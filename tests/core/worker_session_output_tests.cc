// SPDX-License-Identifier: MIT
#include <iostream>
#include <native/Engine/WorkerSessionOutput.h>
#include <native/Engine/WorkerWire.h>
#include <stdexcept>
using namespace dust3d::worker;
void require(bool value)
{
    if (!value)
        throw std::runtime_error("Session output contract");
}
template <typename Action>
void rejects(Action action, SessionOutputErrorCode expected)
{
    bool rejected = false;
    try {
        action();
    } catch (const SessionOutputError& cause) {
        rejected = cause.code() == expected;
    }
    require(rejected);
}
IndexedSourcesPayload sample()
{
    IndexedSourcesPayload value;
    value.geometry.vertices = { { { 0, 0, 0 }, { 0, 0, 1 }, { 1, 1, 1, 1 } }, { { 1, 0, 0 }, { 0, 0, 1 }, { 1, 1, 1, 1 } }, { { 0, 1, 0 }, { 0, 0, 1 }, { 1, 1, 1, 1 } } };
    value.geometry.indices = { 0, 1, 2 };
    value.geometry.nodeCount = 1;
    value.sources.identities = { { "node", "part" } };
    value.sources.influences = { { SourceHintKind::nodeHint, 0, 1.0F } };
    value.sources.corners.assign(3, { SourceHintKind::nodeHint, 0, 1 });
    return value;
}
void verifyContracts()
{
    SessionOutput empty;
    auto bytes = encodeSessionOutput(empty, {});
    auto copy = decodeSessionOutput(bytes.data(), bytes.size(), {});
    require(std::holds_alternative<std::monostate>(copy.outcome) && !copy.statistics && !copy.statisticsIssue);
    empty.statistics = GenerationStatistics {};
    bytes = encodeSessionOutput(empty, {});
    copy = decodeSessionOutput(bytes.data(), bytes.size(), {});
    require(copy.statistics.has_value() && copy.statistics->reusedComponents == 0);
    SessionOutput failure;
    failure.outcome = FailurePayload { FailureStatus::unsupportedAsset, { 0, 255 }, FailureDetail { FailureDomain::native, 2, 2, 0 } };
    failure.sourceIssue = FailurePayload { FailureStatus::generationFailed, { 42 } };
    failure.statisticsIssue = FailurePayload { FailureStatus::limitExceeded, { 43 } };
    bytes = encodeSessionOutput(failure, {});
    copy = decodeSessionOutput(bytes.data(), bytes.size(), {});
    require(std::get<FailurePayload>(copy.outcome).cause == std::vector<uint8_t> { 0, 255 });
    require(copy.sourceIssue->cause == std::vector<uint8_t> { 42 } && copy.statisticsIssue->cause == std::vector<uint8_t> { 43 } && !copy.statistics);
    bytes.assign(bytes.size(), 0);
    require(std::get<FailurePayload>(copy.outcome).detail->code == 2);
    for (bool sourced : { true, false }) {
        SessionOutput geometry;
        auto pair = sample();
        if (sourced)
            geometry.outcome = pair;
        else {
            geometry.outcome = pair.geometry;
            geometry.sourceIssue = FailurePayload { FailureStatus::limitExceeded, { 99 } };
        }
        geometry.statistics = GenerationStatistics {};
        geometry.statistics->reusedComponents = 7;
        auto wire = encodeSessionOutput(geometry, {});
        auto decoded = decodeSessionOutput(wire.data(), wire.size(), {});
        require(encodeSessionOutput(decoded, {}) == wire && decoded.statistics->reusedComponents == 7);
        require(sourced ? std::get<IndexedSourcesPayload>(decoded.outcome).geometry.indices.size() == 3 : std::get<IndexedMeshPayload>(decoded.outcome).indices.size() == 3);
        SessionOutputLimits tiny;
        tiny.maximumDecodedBytes = 1;
        bool limited = false;
        try {
            (void)encodeSessionOutput(geometry, tiny);
        } catch (const std::exception&) {
            limited = true;
        }
        require(limited);
        geometry.sourceIssue = FailurePayload { FailureStatus::unsupportedAsset, {} };
        rejects([&] { (void)encodeSessionOutput(geometry, {}); }, SessionOutputErrorCode::invalidOutcome);
    }
    for (bool sourced : { true, false }) {
        SessionOutput value;
        if (sourced)
            value.outcome = sample();
        else
            value.outcome = sample().geometry;
        value.statistics = GenerationStatistics {};
        value.statistics->cacheCause = { 1 };
        SessionOutputLimits quota;
        quota.maximumDecodedBytes = 1;
        rejects([&] { (void)encodeSessionOutput(value, quota); }, SessionOutputErrorCode::limitExceeded);
    }
    SessionOutput classified;
    classified.statisticsIssue = FailurePayload { FailureStatus::generationFailed, { 1 }, FailureDetail { FailureDomain::generationStatistics, 4, 0, 0 } };
    auto classifiedWire = encodeSessionOutput(classified, {});
    require(decodeSessionOutput(classifiedWire.data(), classifiedWire.size(), {}).statisticsIssue->detail->domain == FailureDomain::generationStatistics);
    auto invalid = empty;
    invalid.statisticsIssue = FailurePayload {};
    rejects([&] { (void)encodeSessionOutput(invalid, {}); }, SessionOutputErrorCode::invalidOutcome);
    invalid = {};
    invalid.outcome = FailurePayload { FailureStatus::noGeometry, {} };
    rejects([&] { (void)encodeSessionOutput(invalid, {}); }, SessionOutputErrorCode::invalidOutcome);
    auto wire = encodeSessionOutput(SessionOutput {}, {});
    rejects([&] { (void)decodeSessionOutput(nullptr, wire.size(), {}); }, SessionOutputErrorCode::invalidInput);
    rejects([&] { (void)decodeSessionOutput(wire.data(), 1, {}); }, SessionOutputErrorCode::truncated);
    auto extra = wire;
    extra.push_back(0);
    rejects([&] { (void)decodeSessionOutput(extra.data(), extra.size(), {}); }, SessionOutputErrorCode::trailingData);
    auto changed = wire;
    changed[1] = 2;
    rejects([&] { (void)decodeSessionOutput(changed.data(), changed.size(), {}); }, SessionOutputErrorCode::unsupportedVersion);
    for (size_t offset : { 3, 7, 40 }) {
        changed = wire;
        changed[offset] = 255;
        rejects([&] { (void)decodeSessionOutput(changed.data(), changed.size(), {}); }, SessionOutputErrorCode::invalidLayout);
    }
    rejects([&] { (void)decodeSessionOutput(wire.data(), wire.size() - 1, {}); }, SessionOutputErrorCode::truncated);
    changed = wire;
    detail::writeUnsigned(changed.data() + 8, UINT64_MAX, 8);
    rejects([&] { (void)decodeSessionOutput(changed.data(), changed.size(), {}); }, SessionOutputErrorCode::limitExceeded);
    changed = wire;
    detail::writeUnsigned(changed.data() + 4, 1, 4);
    rejects([&] { (void)decodeSessionOutput(changed.data(), changed.size(), {}); }, SessionOutputErrorCode::invalidLayout);
    changed = wire;
    changed.resize(sessionOutputPrefixBytes);
    detail::writeUnsigned(changed.data() + 24, 0, 8);
    rejects([&] { (void)decodeSessionOutput(changed.data(), changed.size(), {}); }, SessionOutputErrorCode::invalidLayout);
    SessionOutput diagnostic;
    diagnostic.sourceIssue = FailurePayload { FailureStatus::generationFailed, { 1, 2 } };
    auto diagnosticWire = encodeSessionOutput(diagnostic, {});
    SessionOutputLimits diagnosticQuota;
    diagnosticQuota.maximumDecodedBytes = 1;
    rejects([&] { (void)encodeSessionOutput(diagnostic, diagnosticQuota); }, SessionOutputErrorCode::limitExceeded);
    rejects([&] { (void)decodeSessionOutput(diagnosticWire.data(), diagnosticWire.size(), diagnosticQuota); }, SessionOutputErrorCode::limitExceeded);
    diagnostic.sourceIssue->detail = FailureDetail { FailureDomain::native, 3, 3, 0 };
    diagnostic.sourceIssue->context = FailureResourceContext { FailureResourceRole::importedImage, "id" };
    diagnosticQuota.maximumDecodedBytes = 3;
    rejects([&] { (void)encodeSessionOutput(diagnostic, diagnosticQuota); }, SessionOutputErrorCode::limitExceeded);
    diagnosticQuota.maximumDecodedBytes = 4;
    require(!encodeSessionOutput(diagnostic, diagnosticQuota).empty());
    SessionOutputLimits tight;
    tight.maximumWireBytes = wire.size() - 1;
    rejects([&] { (void)encodeSessionOutput(SessionOutput {}, tight); }, SessionOutputErrorCode::limitExceeded);
    tight = {};
    tight.maximumWireBytes = 0;
    rejects([&] { (void)encodeSessionOutput(SessionOutput {}, tight); }, SessionOutputErrorCode::invalidLimits);
}

int main()
{
    try {
        verifyContracts();
    } catch (const std::exception& cause) {
        std::cerr << cause.what() << '\n';
        return 1;
    }
    return 0;
}
