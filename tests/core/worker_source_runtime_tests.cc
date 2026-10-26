// SPDX-License-Identifier: MIT
#include <Dust3DWorkerRuntime.h>
#include <array>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <native/Engine/WorkerQueue.h>
#include <native/Engine/WorkerSourceOperation.h>
#include <native/Engine/WorkerSourceRequest.h>
#include <native/Engine/WorkerSourcedMesh.h>
#include <native/Engine/WorkerWire.h>
#include <stdexcept>
#include <unistd.h>
using namespace dust3d::worker;
namespace {
void require(bool value, const char* cause)
{
    if (!value)
        throw std::runtime_error(cause);
}
template <typename Action>
void reject(SourceRequestErrorCode code, Action action)
{
    try {
        action();
    } catch (const SourceRequestError& cause) {
        require(cause.code() == code, "Original source request cause retained");
        return;
    }
    throw std::runtime_error("Invalid pure request accepted");
}
void setField(std::vector<uint8_t>& bytes, size_t offset, uint64_t value, size_t width)
{
    require(offset <= bytes.size() && width <= bytes.size() - offset, "Test field span fits");
    detail::writeUnsigned(bytes.data() + offset, value, width);
}
void requests()
{
    SourceRequest value { { 0, 255, 42 }, 65536 };
    const auto bytes = encodeSourceRequest(value, {});
    auto copy = decodeSourceRequest(bytes.data(), bytes.size(), {});
    require(copy.archive == value.archive && copy.maximumSourceBytes == 65536 && bytes.size() == sourceRequestPrefixBytes + 3, "Opaque archive and source budget round trip");
    SourceRequestLimits exact { bytes.size(), 3 };
    require(encodeSourceRequest(copy, exact) == bytes, "Exact wire and retained archive quotas accept");
    require(encodeSourceRequestView({ value.archive.data(), value.archive.size(), value.maximumSourceBytes }, {}) == bytes, "Borrowed encoder owns identical bytes");
    for (auto budget : std::array<uint64_t, 2> { 0, maximumPayloadBytes }) {
        value.maximumSourceBytes = budget;
        const auto encoded = encodeSourceRequest(value, {});
        require(decodeSourceRequest(encoded.data(), encoded.size(), {}).maximumSourceBytes == budget, "Default and maximum budgets retain original setting");
    }
    value.maximumSourceBytes = maximumPayloadBytes + 1;
    reject(SourceRequestErrorCode::invalidOptions, [&] { encodeSourceRequest(value, {}); });
    reject(SourceRequestErrorCode::invalidInput, [&] { encodeSourceRequestView({ nullptr, 3, 0 }, {}); });
    reject(SourceRequestErrorCode::invalidInput, [&] { encodeSourceRequest({}, {}); });
    reject(SourceRequestErrorCode::invalidInput, [&] { decodeSourceRequest(nullptr, bytes.size(), {}); });
    reject(SourceRequestErrorCode::invalidLimits, [&] { decodeSourceRequest(bytes.data(), bytes.size(), { 0, 3 }); });
    reject(SourceRequestErrorCode::invalidLimits, [&] { decodeSourceRequest(bytes.data(), bytes.size(), { maximumPayloadBytes + 1, 3 }); });
    reject(SourceRequestErrorCode::invalidLimits, [&] { decodeSourceRequest(bytes.data(), bytes.size(), { bytes.size(), 0 }); });
    reject(SourceRequestErrorCode::invalidLimits, [&] { decodeSourceRequest(bytes.data(), bytes.size(), { bytes.size(), maximumPayloadBytes + 1 }); });
    reject(SourceRequestErrorCode::limitExceeded, [&] { decodeSourceRequest(bytes.data(), bytes.size(), { bytes.size() - 1, 3 }); });
    reject(SourceRequestErrorCode::limitExceeded, [&] { decodeSourceRequest(bytes.data(), bytes.size(), { bytes.size(), 2 }); });
    for (size_t size = 0; size < bytes.size(); ++size)
        reject(SourceRequestErrorCode::truncated, [&] { decodeSourceRequest(bytes.data(), size, {}); });
    auto changed = bytes;
    changed.push_back(0);
    reject(SourceRequestErrorCode::trailingData, [&] { decodeSourceRequest(changed.data(), changed.size(), {}); });
    changed = bytes;
    setField(changed, 0, 2, 4);
    reject(SourceRequestErrorCode::unsupportedVersion, [&] { decodeSourceRequest(changed.data(), changed.size(), {}); });
    changed = bytes;
    setField(changed, 4, 1, 4);
    reject(SourceRequestErrorCode::invalidLayout, [&] { decodeSourceRequest(changed.data(), changed.size(), {}); });
    changed = bytes;
    setField(changed, 8, maximumPayloadBytes + 1, 8);
    reject(SourceRequestErrorCode::invalidOptions, [&] { decodeSourceRequest(changed.data(), changed.size(), {}); });
    changed = bytes;
    setField(changed, 16, 0, 8);
    reject(SourceRequestErrorCode::invalidInput, [&] { decodeSourceRequest(changed.data(), changed.size(), {}); });
    changed = bytes;
    setField(changed, 16, UINT64_MAX, 8);
    reject(SourceRequestErrorCode::limitExceeded, [&] { decodeSourceRequest(changed.data(), changed.size(), {}); });
    reject(SourceRequestErrorCode::limitExceeded, [&] { encodeSourceRequest(copy, { 1, 3 }); });
    reject(SourceRequestErrorCode::limitExceeded, [&] { encodeSourceRequest(copy, { bytes.size(), 2 }); });
}
Header requestHeader()
{
    Header result;
    result.operation = Operation::generateSourcedMesh;
    result.identity.document[0] = 1;
    result.identity.request = 42;
    result.identity.revision = 7;
    return result;
}
SourcedMeshPayload sample()
{
    SourcedMeshPayload value;
    value.geometry.vertices = { { { 0, 0, 0 }, { 0, 0, 1 }, { 1, 0, 0, 1 } }, { { 1, 0, 0 }, { 0, 0, 1 }, { 0, 1, 0, 1 } }, { { 0, 1, 0 }, { 0, 0, 1 }, { 0, 0, 1, 1 } } };
    value.geometry.nodeCount = 1;
    value.geometry.partCount = 1;
    value.sources.identities = { { "original-κ", "opaque-part" } };
    value.sources.influences = { { SourceHintKind::nodeHint, 0, 2.5F }, { SourceHintKind::unknown, noSourceIdentity, 0.25F }, { SourceHintKind::ambiguous, noSourceIdentity, 0.5F } };
    value.sources.corners = { { SourceHintKind::weightedHint, 0, 1 }, { SourceHintKind::unknown, 1, 1 }, { SourceHintKind::ambiguous, 2, 1 } };
    return value;
}
void pureRequestBridge()
{
    std::vector<uint8_t> archive { 0, 255, 42 };
    Dust3DWorkerRequestIdentity identity {};
    identity.document[0] = 1;
    identity.request = 42;
    Dust3DWorkerRequestResult receipt {};
    using Packet = std::unique_ptr<Dust3DWorkerRequest, decltype(&dust3d_worker_request_destroy)>;
    Packet packet(dust3d_worker_source_request_create(identity, archive.data(), archive.size(), 65536, { 128, 128, 64 }, &receipt), dust3d_worker_request_destroy);
    require(packet && receipt.status == DUST3D_WORKER_REQUEST_OK, "Pure C request copies opaque input without native parsing");
    archive.assign(3, 9);
    size_t size = 0;
    const auto* bytes = dust3d_worker_request_bytes(packet.get(), &size);
    const auto framed = decodeFrame(bytes, size, {});
    const auto value = decodeSourceRequest(framed.payload.data(), framed.payload.size(), {});
    require(value.archive == std::vector<uint8_t>({ 0, 255, 42 }) && value.maximumSourceBytes == 65536 && framed.header.operation == Operation::generateSourcedMesh, "C request bytes are independent and correlated");
    require(!dust3d_worker_source_request_create(identity, archive.data(), archive.size(), 0, { 128, 128, 64 }, nullptr), "Missing result prevents work");
    auto check = [&](const uint8_t* data, size_t count, uint64_t budget, Dust3DWorkerWireLimits limits, Dust3DWorkerRequestStatus status, int32_t code) {
        Packet rejected(dust3d_worker_source_request_create(identity, data, count, budget, limits, &receipt), dust3d_worker_request_destroy);
        require(!rejected && receipt.status == status && receipt.detail_code == code && receipt.message[0], "C source request retains original typed rejection");
    };
    check(nullptr, 3, 0, { 128, 128, 64 }, DUST3D_WORKER_REQUEST_SOURCE_REJECTED, static_cast<int32_t>(SourceRequestErrorCode::invalidInput));
    check(archive.data(), 0, 0, { 128, 128, 64 }, DUST3D_WORKER_REQUEST_SOURCE_REJECTED, static_cast<int32_t>(SourceRequestErrorCode::invalidInput));
    check(archive.data(), SIZE_MAX, 0, { 128, 128, 64 }, DUST3D_WORKER_REQUEST_SOURCE_REJECTED, static_cast<int32_t>(SourceRequestErrorCode::limitExceeded));
    check(archive.data(), 3, maximumPayloadBytes + 1, { 128, 128, 64 }, DUST3D_WORKER_REQUEST_SOURCE_REJECTED, static_cast<int32_t>(SourceRequestErrorCode::invalidOptions));
    check(archive.data(), 3, 0, { 1, 128, 64 }, DUST3D_WORKER_REQUEST_PROTOCOL_REJECTED, static_cast<int32_t>(ProtocolErrorCode::limitExceeded));
    check(archive.data(), 3, 0, { 0, 128, 64 }, DUST3D_WORKER_REQUEST_PROTOCOL_REJECTED, static_cast<int32_t>(ProtocolErrorCode::invalidLimits));
    Frame invalid;
    invalid.header = requestHeader();
    invalid.header.operation = Operation::generateMesh;
    try {
        (void)generateSourcedMeshReply(invalid, {});
        throw std::runtime_error("Wrong source opcode accepted");
    } catch (const ProtocolError& cause) {
        require(cause.code() == ProtocolErrorCode::unsupportedOperation, "Wrong opcode rejects before native execution");
    }
    invalid.header = requestHeader();
    invalid.header.kind = FrameKind::response;
    try {
        (void)generateSourcedMeshReply(invalid, {});
        throw std::runtime_error("Wrong source frame kind accepted");
    } catch (const ProtocolError& cause) {
        require(cause.code() == ProtocolErrorCode::unexpectedResponse, "Wrong kind rejects before native execution");
    }
    invalid.header = requestHeader();
    invalid.header.payloadBytes = 1;
    try {
        (void)generateSourcedMeshReply(invalid, {});
        throw std::runtime_error("Wrong source payload length accepted");
    } catch (const ProtocolError& cause) {
        require(cause.code() == ProtocolErrorCode::invalidInput, "Length rejects before native execution");
    }
}
void serviceBridge()
{
    const auto request = requestHeader();
    auto header = request;
    header.kind = FrameKind::response;
    auto body = encodeSourcedMeshPayload(sample(), {});
    header.payloadBytes = body.size();
    const auto expected = encodeHeader(request, {});
    Dust3DWorkerServiceReceipt receipt {};
    receipt.operation = DUST3D_WORKER_OPERATION_SOURCED_MESH;
    receipt.request_identity = 42;
    receipt.process_identifier = 123;
    Dust3DWorkerRequestIdentity identity {};
    identity.document[0] = 1;
    identity.request = 42;
    identity.revision = 7;
    using Decoded = std::unique_ptr<Dust3DWorkerDecodedReply, decltype(&dust3d_worker_decoded_destroy)>;
    for (int scenario = 0; scenario < 6; ++scenario) {
        auto response = header;
        auto payload = body;
        if (scenario == 1)
            setField(payload, 0, 2, 2);
        if (scenario == 2) {
            const auto meshBytes = encodeMeshPayload(sample().geometry, {});
            setField(payload, sourcedMeshPrefixBytes + meshBytes.size(), 2, 4);
        }
        if (scenario == 3)
            setField(payload, sourcedMeshPrefixBytes, 2, 4);
        if (scenario == 4) {
            response.kind = FrameKind::failure;
            payload = encodeFailurePayload({ FailureStatus::noGeometry, { 'n', 'o' } }, {});
        }
        response.payloadBytes = payload.size();
        const auto scenarioWire = encodeFrame({ response, payload }, {});
        for (bool isQueued : { false, true }) {
            Dust3DWorkerReplyResult frameResult {};
            std::unique_ptr<Dust3DWorkerReplyCollector, decltype(&dust3d_worker_reply_destroy)> owner(nullptr, dust3d_worker_reply_destroy);
            std::unique_ptr<Dust3DWorkerServiceQueue, decltype(&dust3d_worker_service_queue_destroy)> queue(nullptr, dust3d_worker_service_queue_destroy);
            Dust3DWorkerReplyCollector* collector;
            if (isQueued) {
                Dust3DWorkerQueueResult state {};
                queue.reset(dust3d_worker_service_queue_create({ 4096, 4096, 4096 }, &state));
                require(bool(queue), "Pure C service queue created");
                const auto requestWire = encodeFrame({ request, {} }, {});
                require(dust3d_worker_service_queue_submit(queue.get(), requestWire.data(), requestWire.size()).status == DUST3D_WORKER_QUEUE_READY, "Source header scheduled without engine work");
                require(dust3d_worker_service_queue_begin(queue.get(), 17).status == DUST3D_WORKER_QUEUE_BEGUN, "Queue freezes source connection");
                collector = dust3d_worker_service_queue_reply(queue.get());
            } else {
                owner.reset(dust3d_worker_service_reply_create(expected.data(), expected.size(), 17, { 4096, 4096, 4096 }, &frameResult));
                collector = owner.get();
            }
            require(collector && dust3d_worker_reply_append(collector, scenarioWire.data(), scenarioWire.size()).status == DUST3D_WORKER_REPLY_READY, "Pure source service frame appended");
            require(dust3d_worker_reply_finish(collector).status == DUST3D_WORKER_REPLY_FRAMED, "Pure source service EOF supplied");
            Dust3DWorkerDecodeResult result {};
            const uint64_t maximum = scenario == 5 ? 1 : 4096;
            Decoded decoded(isQueued ? dust3d_worker_service_queue_admit(queue.get(), { 17, 123 }, receipt, maximum, &result)
                                     : dust3d_worker_service_admit(collector, identity, { 17, 123 }, receipt, maximum, &result),
                dust3d_worker_decoded_destroy);
            if (scenario == 0)
                require(decoded && result.status == DUST3D_WORKER_DECODE_OK && dust3d_worker_decoded_source_summary(decoded.get()).corner_count == 3, "Both C service routes admit independent sources");
            else if (scenario == 4) {
                require(decoded && dust3d_worker_decoded_summary(decoded.get()).failure_status == 5 && !dust3d_worker_decoded_source_summary(decoded.get()).corner_count, "No geometry remains a distinct owned failure result");
                Dust3DWorkerSourceInfluence missing {};
                require(!dust3d_worker_decoded_source_influence(decoded.get(), 0, &missing), "Wrong decoded kind rejects source getter");
            } else {
                const auto detail = scenario == 1 ? DUST3D_WORKER_DETAIL_SOURCED_MESH : scenario == 2 ? DUST3D_WORKER_DETAIL_SOURCE_HINTS
                                                                                                      : DUST3D_WORKER_DETAIL_MESH;
                require(!decoded && result.status == DUST3D_WORKER_DECODE_CODEC_REJECTED && result.detail_source == detail && result.message[0], "Both C service routes retain original inner rejection without partial output");
            }
        }
    }
}
void ownedOutputAndAdmission()
{
    const auto request = requestHeader();
    auto header = request;
    header.kind = FrameKind::response;
    auto body = encodeSourcedMeshPayload(sample(), {});
    header.payloadBytes = body.size();
    auto wire = encodeFrame({ header, body }, {});
    PendingReply child(request);
    require(std::holds_alternative<SourcedMeshPayload>(child.complete(request.identity, { true, 0 }, wire.data(), wire.size())), "Pure observed child completion admits sources");
    RequestQueue queue;
    queue.submit({ request, {} });
    require(bool(queue.beginService(17)), "Pure service queue begins source request");
    ServiceReceipt receipt;
    receipt.runtimeStatus = 0;
    receipt.admissionStatus = 0;
    receipt.operation = Operation::generateSourcedMesh;
    receipt.request = 42;
    receipt.process = 123;
    require(std::holds_alternative<SourcedMeshPayload>(queue.completeService(request.identity, { 17, 123, true }, receipt, wire.data(), wire.size())), "Pure queue-current source admission retains pair");
    const auto expected = encodeHeader(request, {});
    Dust3DWorkerReplyResult framedResult {};
    std::unique_ptr<Dust3DWorkerReplyCollector, decltype(&dust3d_worker_reply_destroy)> framed(
        dust3d_worker_reply_create(expected.data(), expected.size(), { 4096, 4096, 4096 }, &framedResult), dust3d_worker_reply_destroy);
    require(bool(framed), "Source collector owns framing");
    require(dust3d_worker_reply_append(framed.get(), wire.data(), wire.size()).status == DUST3D_WORKER_REPLY_READY, "Source frame appended");
    require(dust3d_worker_reply_finish(framed.get()).status == DUST3D_WORKER_REPLY_FRAMED, "Actual complete EOF supplied");
    Dust3DWorkerDecodeResult result {};
    std::unique_ptr<Dust3DWorkerDecodedReply, decltype(&dust3d_worker_decoded_destroy)> decoded(
        dust3d_worker_reply_decode(framed.get(), 4096, &result), dust3d_worker_decoded_destroy);
    require(decoded && result.status == DUST3D_WORKER_DECODE_OK, "Complete C source values decoded");
    framed.reset();
    wire.assign(wire.size(), 0);
    body.clear();
    const auto summary = dust3d_worker_decoded_summary(decoded.get());
    const auto sources = dust3d_worker_decoded_source_summary(decoded.get());
    require(summary.kind == DUST3D_WORKER_DECODED_SOURCED_MESH && summary.vertex_count == 3 && sources.identity_count == 1 && sources.corner_count == 3 && sources.influence_count == 3, "Independent C summaries expose sources and geometry");
    Dust3DWorkerSourceCorner corner {};
    Dust3DWorkerSourceInfluence influence {};
    require(dust3d_worker_decoded_source_corner(decoded.get(), 0, &corner) && corner.kind == 2 && corner.first_influence == 0 && corner.influence_count == 1, "Original corner values copied");
    require(dust3d_worker_decoded_source_influence(decoded.get(), 0, &influence) && influence.kind == 1 && influence.identity_index == 0 && influence.weight == 2.5F, "Original unnormalized influence copied");
    require(dust3d_worker_decoded_source_influence(decoded.get(), 1, &influence) && influence.kind == 0 && influence.identity_index == UINT64_MAX && influence.weight == 0.25F, "Unknown influence retains fixed-width sentinel");
    require(dust3d_worker_decoded_source_corner(decoded.get(), 2, &corner) && corner.kind == 3 && corner.first_influence == 2, "Ambiguous corner remains distinct");
    size_t size = 0;
    const auto* node = dust3d_worker_decoded_source_node(decoded.get(), 0, &size);
    require(node && std::string(node, size) == "original-κ", "Opaque UTF8 node bytes retained");
    const auto* part = dust3d_worker_decoded_source_part(decoded.get(), 0, &size);
    require(part && std::string(part, size) == "opaque-part", "Original part bytes retained");
    require(!dust3d_worker_decoded_source_corner(decoded.get(), SIZE_MAX, &corner) && !corner.kind && !corner.influence_count, "Rejected corner clears output");
    require(!dust3d_worker_decoded_source_influence(decoded.get(), SIZE_MAX, &influence) && !influence.weight, "Rejected influence clears output");
    require(!dust3d_worker_decoded_source_corner(decoded.get(), 0, nullptr) && !dust3d_worker_decoded_source_influence(decoded.get(), 0, nullptr), "Null output storage rejects");
    size = 999;
    require(!dust3d_worker_decoded_source_node(decoded.get(), SIZE_MAX, &size) && size == 0 && !dust3d_worker_decoded_source_part(decoded.get(), 0, nullptr), "Invalid borrowed text span rejects");
    require(!dust3d_worker_decoded_source_summary(nullptr).corner_count && !dust3d_worker_decoded_source_corner(nullptr, 0, &corner), "Null source owner is safe");
    size = 999;
    require(!dust3d_worker_decoded_source_part(nullptr, 0, &size) && size == 0, "Null text owner clears borrowed size");
}
void healthyRuntime(const char* path)
{
    std::ifstream input(path, std::ios::binary);
    require(bool(input), "Licensed Capsule fixture opens");
    std::vector<uint8_t> archive { std::istreambuf_iterator<char>(input), {} };
    Dust3DWorkerRequestIdentity identity {};
    identity.document[0] = 1;
    identity.request = 42;
    identity.revision = 7;
    Dust3DWorkerRequestResult receipt {};
    std::unique_ptr<Dust3DWorkerRequest, decltype(&dust3d_worker_request_destroy)> packet(
        dust3d_worker_source_request_create(identity, archive.data(), archive.size(), 0, { maximumPayloadBytes, maximumPayloadBytes, 4096 }, &receipt), dust3d_worker_request_destroy);
    require(packet && receipt.status == DUST3D_WORKER_REQUEST_OK, "C source request owns archive and framing");
    size_t count = 0;
    const auto* bytes = dust3d_worker_request_bytes(packet.get(), &count);
    std::unique_ptr<FILE, decltype(&fclose)> request(tmpfile(), fclose), output(tmpfile(), fclose);
    require(request && output && fwrite(bytes, 1, count, request.get()) == count, "Owned job channels retain request");
    rewind(request.get());
    archive.clear();
    packet.reset();
    const auto job = dust3d_worker_serve(fileno(request.get()), fileno(output.get()));
    require(job.status == DUST3D_WORKER_JOB_COMPLETE && job.operation == DUST3D_WORKER_OPERATION_SOURCED_MESH && job.request_identity == 42, "Healthy source-aware runtime dispatch completes");
    rewind(output.get());
    std::vector<uint8_t> reply;
    std::array<uint8_t, 4096> buffer {};
    while ((count = fread(buffer.data(), 1, buffer.size(), output.get())) != 0)
        reply.insert(reply.end(), buffer.begin(), buffer.begin() + count);
    require(!ferror(output.get()), "Complete ordinary reply is readable");
    const auto framed = decodeFrame(reply.data(), reply.size(), {});
    require(framed.header.kind == FrameKind::response, "Healthy source runtime returns complete geometry");
    const auto pair = decodeSourcedMeshPayload(framed.payload.data(), framed.payload.size(), {});
    require(pair.geometry.vertices.size() == 240 && pair.sources.corners.size() == 240 && !pair.sources.identities.empty(), "Runtime preserves native source correspondence");
}
}
int main(int argc, char** argv)
{
    try {
        requests();
        pureRequestBridge();
        ownedOutputAndAdmission();
        serviceBridge();
        if (argc == 2)
            healthyRuntime(argv[1]);
        else
            require(argc == 1, "Optional licensed fixture only");
        std::cout << "Source-aware worker contracts passed\n";
    } catch (const std::exception& cause) {
        std::cerr << cause.what() << '\n';
        return 1;
    }
}
