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
void configuredInputLimits()
{
    auto expected = request();
    expected.payloadBytes = Limits {}.maximumInputBytes + 1;
    Limits limits;
    limits.maximumInputBytes = expected.payloadBytes;
    // Only header metadata is large. The ordinary typed failure payload stays tiny.
    auto wire = reply(expected);
    rejects<ProtocolError>([&] { PendingReply defaults(expected); });
    PendingReply configured(expected, limits);
    auto admitted = configured.complete(expected.identity, { true, 0 }, wire.data(), wire.size(), limits);
    require(std::get<FailurePayload>(admitted).cause == std::vector<uint8_t>({ 0, 255, 'x' }), "Configured input limits admit a small original failure reply");
    require(configured.isRetired(), "Configured completion remains single-use");
    limits.maximumInputBytes--;
    rejects<ProtocolError>([&] { PendingReply tooSmall(expected, limits); });
    limits.maximumInputBytes = maximumPayloadBytes + 1;
    rejects<ProtocolError>([&] { PendingReply invalidLimits(expected, limits); });
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
void serviceCompletion()
{
    const auto expected = request();
    auto wire = reply(expected);
    ServiceReceipt receipt;
    receipt.runtimeStatus = 0;
    receipt.admissionStatus = 0;
    receipt.operation = expected.operation;
    receipt.request = expected.identity.request;
    receipt.process = 101;
    const ServiceObservation observed { 3, 101, true };
    PendingServiceReply pending(expected, 3);
    auto admitted = pending.complete(expected.identity, observed, receipt, wire.data(), wire.size());
    wire.assign(wire.size(), 0);
    require(std::get<FailurePayload>(admitted).cause == std::vector<uint8_t>({ 0, 255, 'x' }), "Service reply owns original failure cause without a process outcome");
    require(pending.isRetired(), "Service admission is single-use");
    rejectsAdmission(AdmissionErrorCode::retired, [&] { pending.complete(expected.identity, observed, receipt, nullptr, 0); });
    wire = reply(expected);
    for (const auto badPeer : { ServiceObservation { 4, 101, true }, ServiceObservation { 3, 0, true }, ServiceObservation { 3, 102, true } }) {
        PendingServiceReply wrongPeer(expected, 3);
        rejectsAdmission(AdmissionErrorCode::unexpectedServicePeer, [&] { wrongPeer.complete(expected.identity, badPeer, receipt, nullptr, 0); });
        require(wrongPeer.isRetired(), "Peer mismatch retires before decoding");
    }
    PendingServiceReply truncated(expected, 3);
    rejectsAdmission(AdmissionErrorCode::incompleteStream, [&] { truncated.complete(expected.identity, { 3, 101, false }, receipt, nullptr, 0); });
    for (size_t field = 0; field < 3; ++field) {
        auto stale = expected.identity;
        if (field == 0)
            ++stale.document[0];
        if (field == 1)
            ++stale.request;
        if (field == 2)
            ++stale.revision;
        PendingServiceReply old(expected, 3);
        rejectsAdmission(AdmissionErrorCode::staleRequest, [&] { old.complete(stale, observed, receipt, nullptr, 0); });
    }
    auto failedReceipt = receipt;
    failedReceipt.runtimeStatus = 7;
    failedReceipt.detailCode = 91;
    PendingServiceReply failed(expected, 3);
    rejectsAdmission(AdmissionErrorCode::unsuccessfulService, [&] { failed.complete(expected.identity, observed, failedReceipt, nullptr, 0); });
    for (size_t field = 0; field < 8; ++field) {
        auto diagnostic = receipt;
        if (field == 0)
            diagnostic.runtimeStatus = 1;
        if (field == 1)
            diagnostic.admissionStatus = 5;
        if (field == 2)
            diagnostic.systemError = 9;
        if (field == 3)
            diagnostic.ioStage = 2;
        if (field == 4)
            diagnostic.detailSource = 3;
        if (field == 5)
            diagnostic.detailCode = 91;
        if (field == 6)
            diagnostic.messageTruncated = 1;
        if (field == 7)
            diagnostic.finalizationError = 9;
        PendingServiceReply rejected(expected, 3);
        rejectsAdmission(AdmissionErrorCode::unsuccessfulService, [&] { rejected.complete(expected.identity, observed, diagnostic, nullptr, 0); });
    }
    PendingServiceReply diagnosticCause(expected, 3);
    try {
        diagnosticCause.complete(expected.identity, observed, failedReceipt, nullptr, 0);
        throw std::runtime_error("Service diagnostic lost");
    } catch (const AdmissionError& cause) {
        require(std::string(cause.what()).find("runtime=7") != std::string::npos && std::string(cause.what()).find("code=91") != std::string::npos, "Original service diagnostic codes remain visible");
    }
    auto operationReceipt = receipt;
    operationReceipt.operation = Operation::generateUvMesh;
    PendingServiceReply operationMismatch(expected, 3);
    rejectsAdmission(AdmissionErrorCode::mismatchedServiceReceipt, [&] { operationMismatch.complete(expected.identity, observed, operationReceipt, nullptr, 0); });
    auto wrongReceipt = receipt;
    ++wrongReceipt.request;
    PendingServiceReply wrong(expected, 3);
    rejectsAdmission(AdmissionErrorCode::mismatchedServiceReceipt, [&] { wrong.complete(expected.identity, observed, wrongReceipt, nullptr, 0); });
    PendingServiceReply cancelled(expected, 3);
    cancelled.retire();
    rejectsAdmission(AdmissionErrorCode::retired, [&] { cancelled.complete(expected.identity, observed, receipt, nullptr, 0); });
    rejectsAdmission(AdmissionErrorCode::invalidServiceSession, [&] { PendingServiceReply invalid(expected, 0); });
    PendingServiceReply malformed(expected, 3);
    rejects<ProtocolError>([&] { malformed.complete(expected.identity, observed, receipt, nullptr, 0); });
    require(malformed.isRetired(), "Bad service frame remains retired");
}

void serviceGeometry()
{
    UvMeshPayload pair;
    pair.geometry.vertices = { { { 0, 0, 0 }, { 0, 0, 1 }, { 1, 1, 1, 1 } }, { { 1, 0, 0 }, { 0, 0, 1 }, { 1, 1, 1, 1 } }, { { 0, 1, 0 }, { 0, 0, 1 }, { 1, 1, 1, 1 } } };
    pair.uv.triangles.resize(1);
    for (const auto operation : { Operation::generateMesh, Operation::generateUvMesh }) {
        auto expected = request();
        expected.operation = operation;
        auto response = expected;
        response.kind = FrameKind::response;
        auto body = operation == Operation::generateMesh ? encodeMeshPayload(pair.geometry, {}) : encodeUvMeshPayload(pair, {});
        response.payloadBytes = body.size();
        auto wire = encodeFrame({ response, body }, {});
        ServiceReceipt receipt;
        receipt.runtimeStatus = receipt.admissionStatus = 0;
        receipt.operation = operation;
        receipt.request = expected.identity.request;
        receipt.process = 101;
        PendingServiceReply pending(expected, 3);
        auto admitted = pending.complete(expected.identity, { 3, 101, true }, receipt, wire.data(), wire.size());
        wire.assign(wire.size(), 0);
        const auto& geometry = operation == Operation::generateMesh ? std::get<MeshPayload>(admitted) : std::get<UvMeshPayload>(admitted).geometry;
        require(geometry.vertices.size() == 3 && geometry.vertices[1].position[0] == 1, "Service geometry survives borrowed wire lifetime");
        wire = encodeFrame({ response, body }, {});
        MeshPayloadLimits meshLimits;
        meshLimits.maximumVertices = 1;
        UvMeshPayloadLimits uvLimits;
        uvLimits.geometry = meshLimits;
        PendingServiceReply limited(expected, 3);
        rejects<MeshPayloadError>([&] { limited.complete(expected.identity, { 3, 101, true }, receipt, wire.data(), wire.size(), {}, meshLimits, {}, uvLimits); });
        require(limited.isRetired(), "Service quota rejection leaves no partial publication");
    }
}

}
int main()
{
    try {
        serviceCompletion();
        serviceGeometry();
        completion();
        meshCompletion();
        configuredInputLimits();
        return 0;
    } catch (const std::exception& cause) {
        std::cerr << cause.what() << '\n';
        return 1;
    }
}
