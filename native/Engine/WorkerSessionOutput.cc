// SPDX-License-Identifier: MIT
#include "WorkerSessionOutput.h"
#include "WorkerWire.h"
#include <algorithm>
#include <limits>
namespace dust3d {
namespace worker {
    namespace {
        static_assert(maximumPayloadBytes <= std::numeric_limits<size_t>::max(), "Session wire spans must fit size_t.");
        [[noreturn]] void reject(SessionOutputErrorCode code, const char* cause) { throw SessionOutputError(code, cause); }
        void validateLimits(const SessionOutputLimits& limits)
        {
            if (!limits.maximumWireBytes || limits.maximumWireBytes > maximumPayloadBytes || !limits.maximumDecodedBytes || limits.maximumDecodedBytes > maximumPayloadBytes)
                reject(SessionOutputErrorCode::invalidLimits, "Session output aggregate quotas are unsupported.");
            validateIndexedSourcesPayloadLimits(limits.sourced);
            validateIndexedMeshPayloadLimits(limits.ordinary);
            (void)encodeFailurePayload(FailurePayload {}, limits.failure);
            (void)encodeGenerationStatistics(std::nullopt, limits.statistics);
        }
        void validateOutcome(const SessionOutput& value, uint32_t kind)
        {
            if (value.sourceIssue && (kind == 1 || (value.sourceIssue->status != FailureStatus::generationFailed && value.sourceIssue->status != FailureStatus::limitExceeded)))
                reject(SessionOutputErrorCode::invalidOutcome, "Session source issue requires an ordinary fallback outcome and original status3/4.");
            if (value.statistics && value.statisticsIssue)
                reject(SessionOutputErrorCode::invalidOutcome, "Session statistics cannot be both available and failed.");
            if (const auto* failure = std::get_if<FailurePayload>(&value.outcome); failure && failure->status == FailureStatus::noGeometry)
                reject(SessionOutputErrorCode::invalidOutcome, "Session successful emptiness requires its explicit outcome.");
        }
        uint32_t kindOf(const SessionOutput& value)
        {
            if (value.outcome.valueless_by_exception())
                reject(SessionOutputErrorCode::invalidOutcome, "Session outcome has no owned value.");
            constexpr uint32_t kinds[] { 3, 1, 2, 4 };
            return kinds[value.outcome.index()];
        }
        void charge(uint64_t& total, uint64_t bytes, uint64_t maximum)
        {
            if (total > maximum || bytes > maximum - total)
                reject(SessionOutputErrorCode::limitExceeded, "Session output exceeds its aggregate quota.");
            total += bytes;
        }
        void chargeFailure(uint64_t& total, const FailurePayload& value, uint64_t maximum)
        {
            charge(total, value.cause.size(), maximum);
            if (value.context)
                charge(total, value.context->identity.size(), maximum);
        }
        uint64_t remainingDecoded(const SessionOutput& value, const SessionOutputLimits& limits)
        {
            uint64_t count = 0;
            if (value.sourceIssue)
                chargeFailure(count, *value.sourceIssue, limits.maximumDecodedBytes);
            if (value.statisticsIssue)
                chargeFailure(count, *value.statisticsIssue, limits.maximumDecodedBytes);
            if (value.statistics)
                charge(count, value.statistics->cacheCause.size(), limits.maximumDecodedBytes);
            if (const auto* failure = std::get_if<FailurePayload>(&value.outcome))
                chargeFailure(count, *failure, limits.maximumDecodedBytes);
            return limits.maximumDecodedBytes - count;
        }
        IndexedSourcesPayloadLimits sourcedPolicy(const SessionOutputLimits& limits, uint64_t decoded, uint64_t wire)
        {
            if (!decoded || !wire)
                reject(SessionOutputErrorCode::limitExceeded, "Session source geometry has no aggregate allowance.");
            auto result = limits.sourced;
            result.maximumDecodedBytes = std::min(result.maximumDecodedBytes, decoded);
            result.maximumWireBytes = std::min(result.maximumWireBytes, wire);
            return result;
        }
        IndexedMeshPayloadLimits ordinaryPolicy(const SessionOutputLimits& limits, uint64_t decoded, uint64_t wire)
        {
            if (!decoded || !wire)
                reject(SessionOutputErrorCode::limitExceeded, "Session ordinary geometry has no aggregate allowance.");
            auto result = limits.ordinary;
            result.maximumDecodedBytes = std::min(result.maximumDecodedBytes, decoded);
            result.maximumWireBytes = std::min(result.maximumWireBytes, wire);
            return result;
        }
    }
    SessionOutputError::SessionOutputError(SessionOutputErrorCode code, const char* cause)
        : std::runtime_error(cause)
        , m_code(code)
    {
    }
    std::vector<uint8_t> encodeSessionOutput(const SessionOutput& value, const SessionOutputLimits& limits)
    {
        validateLimits(limits);
        const auto kind = kindOf(value);
        validateOutcome(value, kind);
        std::array<std::vector<uint8_t>, 4> sections;
        if (value.sourceIssue)
            sections[1] = encodeFailurePayload(*value.sourceIssue, limits.failure);
        sections[2] = encodeGenerationStatistics(value.statistics, limits.statistics);
        if (value.statisticsIssue)
            sections[3] = encodeFailurePayload(*value.statisticsIssue, limits.failure);
        const auto decoded = remainingDecoded(value, limits);
        uint64_t count = sessionOutputPrefixBytes;
        for (size_t i = 1; i < sections.size(); ++i)
            charge(count, sections[i].size(), limits.maximumWireBytes);
        const auto remainingWire = limits.maximumWireBytes - count;
        if (kind == 1)
            sections[0] = encodeIndexedSourcesPayload(std::get<IndexedSourcesPayload>(value.outcome), sourcedPolicy(limits, decoded, remainingWire));
        else if (kind == 2)
            sections[0] = encodeIndexedMeshPayload(std::get<IndexedMeshPayload>(value.outcome), ordinaryPolicy(limits, decoded, remainingWire));
        else if (kind == 4)
            sections[0] = encodeFailurePayload(std::get<FailurePayload>(value.outcome), limits.failure);
        charge(count, sections[0].size(), limits.maximumWireBytes);
        std::vector<uint8_t> wire(static_cast<size_t>(count), 0);
        detail::writeUnsigned(wire.data(), 1, 2);
        detail::writeUnsigned(wire.data() + 2, sessionOutputPrefixBytes, 2);
        detail::writeUnsigned(wire.data() + 4, kind, 4);
        size_t offset = sessionOutputPrefixBytes;
        for (size_t i = 0; i < sections.size(); ++i) {
            detail::writeUnsigned(wire.data() + 8 + i * 8, sections[i].size(), 8);
            std::copy(sections[i].begin(), sections[i].end(), wire.begin() + offset);
            offset += sections[i].size();
        }
        return wire;
    }
    SessionOutput decodeSessionOutput(const uint8_t* bytes, size_t size, const SessionOutputLimits& limits)
    {
        validateLimits(limits);
        if (!bytes)
            reject(SessionOutputErrorCode::invalidInput, "Session output storage is missing.");
        if (size < sessionOutputPrefixBytes)
            reject(SessionOutputErrorCode::truncated, "Session output prefix is incomplete.");
        if (detail::readUnsigned(bytes, 2) != 1)
            reject(SessionOutputErrorCode::unsupportedVersion, "Session output version is unsupported.");
        const auto kind = static_cast<uint32_t>(detail::readUnsigned(bytes + 4, 4));
        if (detail::readUnsigned(bytes + 2, 2) != sessionOutputPrefixBytes || kind < 1 || kind > 4 || std::any_of(bytes + 40, bytes + sessionOutputPrefixBytes, [](uint8_t value) { return value != 0; }))
            reject(SessionOutputErrorCode::invalidLayout, "Session output layout or reserved fields are invalid.");
        std::array<size_t, 4> lengths;
        uint64_t count = sessionOutputPrefixBytes;
        for (size_t i = 0; i < lengths.size(); ++i) {
            const auto length = detail::readUnsigned(bytes + 8 + i * 8, 8);
            charge(count, length, limits.maximumWireBytes);
            lengths[i] = static_cast<size_t>(length);
        }
        if (size < count)
            reject(SessionOutputErrorCode::truncated, "Session output sections are incomplete.");
        if (size > count)
            reject(SessionOutputErrorCode::trailingData, "Session output has trailing bytes.");
        if ((kind == 3) != (lengths[0] == 0) || lengths[2] < generationStatisticsPrefixBytes)
            reject(SessionOutputErrorCode::invalidLayout, "Session output outcome or statistics section is missing.");
        std::array<const uint8_t*, 4> sections;
        size_t offset = sessionOutputPrefixBytes;
        for (size_t i = 0; i < sections.size(); ++i) {
            sections[i] = bytes + offset;
            offset += lengths[i];
        }
        SessionOutput value;
        if (lengths[1])
            value.sourceIssue = decodeFailurePayload(sections[1], lengths[1], limits.failure);
        value.statistics = decodeGenerationStatistics(sections[2], lengths[2], limits.statistics);
        if (lengths[3])
            value.statisticsIssue = decodeFailurePayload(sections[3], lengths[3], limits.failure);
        if (kind == 4)
            value.outcome = decodeFailurePayload(sections[0], lengths[0], limits.failure);
        validateOutcome(value, kind);
        const auto decoded = remainingDecoded(value, limits);
        if (kind == 1)
            value.outcome = decodeIndexedSourcesPayload(sections[0], lengths[0], sourcedPolicy(limits, decoded, lengths[0]));
        else if (kind == 2)
            value.outcome = decodeIndexedMeshPayload(sections[0], lengths[0], ordinaryPolicy(limits, decoded, lengths[0]));
        return value;
    }
}
}
