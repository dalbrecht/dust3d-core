// SPDX-License-Identifier: MIT
#include <iostream>
#include <native/Engine/WorkerAdmission.h>
#include <stdexcept>
#include <type_traits>

namespace {
using namespace dust3d::worker;
void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}
template <typename Error, typename Action>
void rejects(Action action)
{
    try {
        action();
    } catch (const Error&) {
        return;
    }
    throw std::runtime_error("Invalid completion was admitted");
}
template <typename Action>
void rejectsAdmission(AdmissionErrorCode code, Action action)
{
    try {
        action();
    } catch (const AdmissionError& cause) {
        require(cause.code() == code && cause.what()[0], "Admission preserves typed rejection cause");
        return;
    }
    throw std::runtime_error("Invalid admission state was accepted");
}
Header request()
{
    Header value;
    value.identity.document[0] = 1;
    value.identity.request = 7;
    value.identity.revision = 9;
    return value;
}
std::vector<uint8_t> reply(const Header& expected)
{
    auto header = expected;
    header.kind = FrameKind::failure;
    auto body = encodeFailurePayload({ FailureStatus::invalidDocument, { 0, 255, 'x' } }, {});
    header.payloadBytes = body.size();
    return encodeFrame({ header, body }, {});
}
void meshCompletion()
{
    const auto expected = request();
    MeshPayload mesh;
    mesh.vertices = {
        { { 0, 0, 0 }, { 0, 0, 1 }, { 1, 1, 1, 1 } },
        { { 1, 0, 0 }, { 0, 0, 1 }, { 1, 1, 1, 1 } },
        { { 0, 1, 0 }, { 0, 0, 1 }, { 1, 1, 1, 1 } }
    };
    auto header = expected;
    header.kind = FrameKind::response;
    auto body = encodeMeshPayload(mesh, {});
    header.payloadBytes = body.size();
    auto wire = encodeFrame({ header, body }, {});
    PendingReply pending(expected);
    auto admitted = pending.complete(expected.identity, { true, 0 }, wire.data(), wire.size());
    wire.assign(wire.size(), 0);
    require(std::get<MeshPayload>(admitted).vertices[1].position[0] == 1, "Admitted mesh owns independent geometry");
    wire = encodeFrame({ header, body }, {});
    require(std::get<MeshPayload>(admitted).vertices.size() == 3, "Valid triangle publishes owned mesh");
    MeshPayloadLimits small;
    small.maximumVertices = 1;
    PendingReply limited(expected);
    rejects<MeshPayloadError>([&] { limited.complete(expected.identity, { true, 0 }, wire.data(), wire.size(), {}, small); });
    require(limited.isRetired(), "Consumer semantic quota retires rejected reply");
    body[1] = 2;
    wire = encodeFrame({ header, body }, {});
    PendingReply malformed(expected);
    rejects<MeshPayloadError>([&] { malformed.complete(expected.identity, { true, 0 }, wire.data(), wire.size()); });
    require(malformed.isRetired(), "Invalid semantics cannot publish");
    wire = reply(expected);
    wire[headerBytes + 5] = 0;
    PendingReply badFailure(expected);
    rejects<FailurePayloadError>([&] { badFailure.complete(expected.identity, { true, 0 }, wire.data(), wire.size()); });
}
void completion()
{
    static_assert(!std::is_copy_constructible<PendingReply>::value, "Pending ownership cannot duplicate");
    static_assert(!std::is_move_constructible<PendingReply>::value, "Pending identity remains stable");
    const auto expected = request();
    auto wire = reply(expected);
    PendingReply pending(expected);
    auto admitted = pending.complete(expected.identity, { true, 0 }, wire.data(), wire.size());
    wire.assign(wire.size(), 0);
    require(pending.isRetired(), "Completed work retires before publication");
    require(std::get<FailurePayload>(admitted).cause == std::vector<uint8_t>({ 0, 255, 'x' }), "Admitted failure owns opaque cause bytes");
    rejectsAdmission(AdmissionErrorCode::retired, [&] { pending.complete(expected.identity, { true, 0 }, nullptr, 0); });
    for (const auto outcome : { ProcessOutcome { false, 0 }, ProcessOutcome { true, 65 } }) {
        PendingReply failed(expected);
        rejectsAdmission(AdmissionErrorCode::unsuccessfulExit, [&] { failed.complete(expected.identity, outcome, nullptr, 0); });
        require(failed.isRetired(), "Unsuccessful process outcome retires without parsing");
    }
    for (size_t field = 0; field < 3; ++field) {
        auto active = expected.identity;
        if (field == 0)
            ++active.document[0];
        if (field == 1)
            ++active.request;
        if (field == 2)
            ++active.revision;
        PendingReply stale(expected);
        rejectsAdmission(AdmissionErrorCode::staleRequest, [&] { stale.complete(active, { true, 0 }, nullptr, 0); });
        require(stale.isRetired(), "Changed active identity retires stale completion");
    }
    wire = reply(expected);
    PendingReply truncated(expected);
    rejects<ProtocolError>([&] { truncated.complete(expected.identity, { true, 0 }, wire.data(), wire.size() - 1); });
    require(truncated.isRetired(), "Malformed completion cannot be replayed");
    auto other = expected;
    ++other.identity.request;
    wire = reply(other);
    PendingReply wrong(expected);
    rejects<ProtocolError>([&] { wrong.complete(expected.identity, { true, 0 }, wire.data(), wire.size()); });
    PendingReply cancelled(expected);
    cancelled.retire();
    cancelled.retire();
    rejectsAdmission(AdmissionErrorCode::retired, [&] { cancelled.complete(expected.identity, { true, 0 }, wire.data(), wire.size()); });
    auto invalid = expected;
    invalid.kind = FrameKind::response;
    rejectsAdmission(AdmissionErrorCode::invalidRequest, [&] { PendingReply bad(invalid); });
    invalid = expected;
    invalid.identity.request = 0;
    rejects<ProtocolError>([&] { PendingReply bad(invalid); });
}
}
int main()
{
    try {
        completion();
        meshCompletion();
        return 0;
    } catch (const std::exception& cause) {
        std::cerr << cause.what() << '\n';
        return 1;
    }
}
