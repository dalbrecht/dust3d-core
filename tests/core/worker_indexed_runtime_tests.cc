// SPDX-License-Identifier: MIT
#include <Dust3DWorkerRuntime.h>
#include <array>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <native/Engine/WorkerAdmission.h>
#include <native/Engine/WorkerIndexedSourceOperation.h>
#include <native/Engine/WorkerSourceRequest.h>
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
using Decoded = std::unique_ptr<Dust3DWorkerDecodedReply, decltype(&dust3d_worker_decoded_destroy)>;
using Packet = std::unique_ptr<Dust3DWorkerRequest, decltype(&dust3d_worker_request_destroy)>;
void setField(std::vector<uint8_t>& bytes, size_t offset, uint64_t value, size_t width)
{
    require(offset <= bytes.size() && width <= bytes.size() - offset, "Test field fits readable storage");
    detail::writeUnsigned(bytes.data() + offset, value, width);
}
IndexedSourcesPayload sample()
{
    IndexedSourcesPayload value;
    value.geometry.vertices = { { { -0.0F, 0, 0 }, { 0, 0, 1 }, { 1, 0, 0, 0.5F } }, { { 1, 0, 0 }, { 0, 0, 1 }, { 0, 1, 0, 1 } },
        { { 1, 1, 0 }, { 0, 0, 1 }, { 0, 0, 1, 1 } }, { { 0, 1, 0 }, { 0, 0, 1 }, { 1, 1, 1, 1 } } };
    value.geometry.indices = { 0, 1, 2, 0, 2, 3 };
    value.geometry.nodeCount = 1;
    value.geometry.partCount = 1;
    value.geometry.warning = "original warning";
    value.sources.identities = { { "original-κ", "part" } };
    value.sources.influences = { { SourceHintKind::nodeHint, 0, 2.5F } };
    value.sources.corners.assign(6, { SourceHintKind::weightedHint, 0, 1 });
    return value;
}
Header requestHeader(bool withSources)
{
    Header result;
    result.operation = withSources ? Operation::generateIndexedSourcedMesh : Operation::generateIndexedMesh;
    result.identity.document[0] = 1;
    result.identity.request = 42;
    result.identity.revision = 7;
    return result;
}
Dust3DWorkerRequestIdentity identity()
{
    Dust3DWorkerRequestIdentity value {};
    value.document[0] = 1;
    value.request = 42;
    value.revision = 7;
    return value;
}
Packet packet(const std::vector<uint8_t>& archive, bool withSources, uint64_t budget = 0)
{
    Dust3DWorkerRequestResult result {};
    Packet owner(withSources ? dust3d_worker_indexed_source_request_create(identity(), archive.data(), archive.size(), budget, { 1048576, 1048576, 4096 }, &result)
                             : dust3d_worker_request_create(DUST3D_WORKER_OPERATION_INDEXED_MESH, identity(), archive.data(), archive.size(), nullptr, { 1048576, 1048576, 4096 }, &result),
        dust3d_worker_request_destroy);
    require(owner && result.status == DUST3D_WORKER_REQUEST_OK, "Independent indexed request created without archive parsing");
    return owner;
}
void requests()
{
    for (bool withSources : { false, true }) {
        std::vector<uint8_t> archive { 0, 255, 42 };
        auto owner = packet(archive, withSources, 65536);
        archive.assign(3, 9);
        size_t size = 0;
        const auto* bytes = dust3d_worker_request_bytes(owner.get(), &size);
        const auto value = decodeFrame(bytes, size, {});
        require(value.header.operation == requestHeader(withSources).operation, "C request operation is correlated");
        if (withSources) {
            const auto options = decodeSourceRequest(value.payload.data(), value.payload.size(), {});
            require(options.maximumSourceBytes == 65536 && options.archive == std::vector<uint8_t>({ 0, 255, 42 }), "Indexed source request preserves original budget/archive");
        } else
            require(value.payload == std::vector<uint8_t>({ 0, 255, 42 }), "Ordinary indexed request owns raw archive bytes");
    }
    const uint8_t archive[] { 1 };
    Dust3DWorkerRequestResult result {};
    Dust3DWorkerRequestUvOptions options {};
    Packet rejected(dust3d_worker_request_create(DUST3D_WORKER_OPERATION_INDEXED_MESH, identity(), archive, 1, &options, { 128, 128, 64 }, &result), dust3d_worker_request_destroy);
    require(!rejected && result.status == DUST3D_WORKER_REQUEST_PROTOCOL_REJECTED, "Ordinary indexed request cannot carry unrelated UV options");
    require(!dust3d_worker_indexed_source_request_create(identity(), archive, 1, 0, { 128, 128, 64 }, nullptr), "Missing source receipt prevents work");
    Frame invalid;
    invalid.header = requestHeader(false);
    try {
        generateIndexedSourcesReply(invalid, {});
        throw std::runtime_error("Wrong indexed-source opcode accepted");
    } catch (const ProtocolError& cause) {
        require(cause.code() == ProtocolErrorCode::unsupportedOperation, "Wrong opcode rejects before native generation");
    }
    invalid.header = requestHeader(true);
    invalid.header.kind = FrameKind::response;
    try {
        generateIndexedSourcesReply(invalid, {});
        throw std::runtime_error("Wrong indexed-source kind accepted");
    } catch (const ProtocolError& cause) {
        require(cause.code() == ProtocolErrorCode::unexpectedResponse, "Wrong kind rejects before native generation");
    }
    invalid.header = requestHeader(false);
    invalid.header.payloadBytes = 1;
    try {
        generateIndexedMeshReply(invalid, {});
        throw std::runtime_error("Wrong ordinary indexed length accepted");
    } catch (const ProtocolError& cause) {
        require(cause.code() == ProtocolErrorCode::invalidInput, "Wrong payload length rejects before native generation");
    }
}
void ownedOutput(bool withSources, int mode)
{
    const auto request = requestHeader(withSources);
    auto response = request;
    response.kind = FrameKind::response;
    auto body = withSources ? encodeIndexedSourcesPayload(sample(), {}) : encodeIndexedMeshPayload(sample().geometry, {});
    if (mode == 1)
        setField(body, withSources ? indexedSourcesPrefixBytes : 0, 2, 2);
    if (mode == 2 && withSources)
        setField(body, 0, 1, 2);
    if (mode == 3) {
        response.kind = FrameKind::failure;
        body = encodeFailurePayload({ FailureStatus::noGeometry, { 'n', 'o' } }, {});
    }
    response.payloadBytes = body.size();
    auto wire = encodeFrame({ response, body }, {});
    const auto expected = encodeHeader(request, {});
    for (int route = 0; route < 3; ++route) {
        Dust3DWorkerReplyResult framedResult {};
        std::unique_ptr<Dust3DWorkerReplyCollector, decltype(&dust3d_worker_reply_destroy)> owner(nullptr, dust3d_worker_reply_destroy);
        std::unique_ptr<Dust3DWorkerServiceQueue, decltype(&dust3d_worker_service_queue_destroy)> queue(nullptr, dust3d_worker_service_queue_destroy);
        Dust3DWorkerReplyCollector* collector;
        if (route == 2) {
            Dust3DWorkerQueueResult state {};
            queue.reset(dust3d_worker_service_queue_create({ 4096, 4096, 4096 }, &state));
            require(bool(queue), "Pure service coordinator owns source queue");
            const auto requestWire = encodeFrame({ request, {} }, {});
            require(dust3d_worker_service_queue_submit(queue.get(), requestWire.data(), requestWire.size()).status == DUST3D_WORKER_QUEUE_READY, "Indexed request scheduled");
            require(dust3d_worker_service_queue_begin(queue.get(), 17).status == DUST3D_WORKER_QUEUE_BEGUN, "Indexed connection generation frozen");
            collector = dust3d_worker_service_queue_reply(queue.get());
        } else {
            owner.reset(route == 1 ? dust3d_worker_service_reply_create(expected.data(), expected.size(), 17, { 4096, 4096, 4096 }, &framedResult)
                                   : dust3d_worker_reply_create(expected.data(), expected.size(), { 4096, 4096, 4096 }, &framedResult));
            collector = owner.get();
        }
        require(collector && dust3d_worker_reply_append(collector, wire.data(), wire.size()).status == DUST3D_WORKER_REPLY_READY, "Pure indexed reply appended");
        require(dust3d_worker_reply_finish(collector).status == DUST3D_WORKER_REPLY_FRAMED, "Complete EOF supplied");
        Dust3DWorkerServiceReceipt receipt {};
        receipt.operation = withSources ? DUST3D_WORKER_OPERATION_INDEXED_SOURCED_MESH : DUST3D_WORKER_OPERATION_INDEXED_MESH;
        receipt.request_identity = 42;
        receipt.process_identifier = 123;
        Dust3DWorkerDecodeResult result {};
        if (route == 0 && (mode == 0 || mode == 3)) {
            PendingReply pending(request);
            const auto cpp = pending.complete(request.identity, { true, 0 }, wire.data(), wire.size());
            require(mode || (withSources ? std::holds_alternative<IndexedSourcesPayload>(cpp) : std::holds_alternative<IndexedMeshPayload>(cpp)), "Pure child admission retains indexed alternative");
        }
        Decoded decoded(route == 0 ? dust3d_worker_reply_decode(collector, mode == 4 ? 1 : 4096, &result)
                : route == 1       ? dust3d_worker_service_admit(collector, identity(), { 17, 123 }, receipt, mode == 4 ? 1 : 4096, &result)
                                   : dust3d_worker_service_queue_admit(queue.get(), { 17, 123 }, receipt, mode == 4 ? 1 : 4096, &result),
            dust3d_worker_decoded_destroy);
        if (mode == 1 || (mode == 2 && withSources) || mode == 4) {
            require(!decoded && result.status == DUST3D_WORKER_DECODE_CODEC_REJECTED
                    && result.detail_source == (mode == 2 ? DUST3D_WORKER_DETAIL_INDEXED_SOURCES : DUST3D_WORKER_DETAIL_INDEXED_MESH),
                "All C routes preserve indexed codec rejection without partial output");
            continue;
        }
        require(decoded && result.status == DUST3D_WORKER_DECODE_OK, "All C indexed routes decode independent values");
        owner.reset();
        queue.reset();
        const auto summary = dust3d_worker_decoded_summary(decoded.get());
        if (mode == 3) {
            require(summary.kind == DUST3D_WORKER_DECODED_FAILURE && summary.failure_status == 5 && dust3d_worker_decoded_index_count(decoded.get()) == 0, "No geometry remains a distinct owned result");
            require(!dust3d_worker_decoded_indices(decoded.get(), 0, 0, nullptr), "Wrong decoded kind never exposes an index buffer");
            continue;
        }
        require(summary.kind == (withSources ? DUST3D_WORKER_DECODED_INDEXED_SOURCED_MESH : DUST3D_WORKER_DECODED_INDEXED_MESH)
                && summary.vertex_count == 4 && summary.triangle_count == 2 && summary.node_count == 1 && summary.part_count == 1
                && dust3d_worker_decoded_index_count(decoded.get()) == 6,
            "Unique table and triangle-index dimensions stay distinct");
        std::array<uint32_t, 6> indices {};
        require(dust3d_worker_decoded_indices(decoded.get(), 0, indices.size(), indices.data()) && indices == std::array<uint32_t, 6> { 0, 1, 2, 0, 2, 3 }, "Every original index is copied in order");
        indices.at(0) = 123;
        require(!dust3d_worker_decoded_indices(decoded.get(), SIZE_MAX, 1, indices.data()) && indices.at(0) == 123, "Invalid starting index does not write");
        require(!dust3d_worker_decoded_indices(decoded.get(), 0, SIZE_MAX, indices.data()) && !dust3d_worker_decoded_indices(decoded.get(), 0, 1, nullptr), "Invalid count/output storage reject");
        require(dust3d_worker_decoded_indices(decoded.get(), 6, 0, nullptr), "Valid empty range needs no storage");
        Dust3DWorkerVertex vertices[4] {};
        require(dust3d_worker_decoded_vertices(decoded.get(), 0, 4, vertices) && vertices[2].position[1] == 1 && vertices[0].color[3] == 0.5F && std::signbit(vertices[0].position[0]), "Indexed full-attribute vertices remain independently copied");
        size_t size = 0;
        const auto* warning = dust3d_worker_decoded_warning(decoded.get(), &size);
        require(warning && std::string(warning, size) == "original warning", "Indexed warning text survives framing owner release");
        const auto sources = dust3d_worker_decoded_source_summary(decoded.get());
        require(sources.corner_count == (withSources ? 6 : 0), "Optional hints are explicit rather than fabricated");
        if (withSources) {
            Dust3DWorkerSourceInfluence influence {};
            require(dust3d_worker_decoded_source_influence(decoded.get(), 0, &influence) && influence.weight == 2.5F, "Indexed source weights retain original values");
        }
    }
}
void expandedCompatibility()
{
    auto request = requestHeader(false);
    request.operation = Operation::generateMesh;
    auto response = request;
    response.kind = FrameKind::response;
    const auto indexed = sample().geometry;
    MeshPayload expanded;
    expanded.nodeCount = indexed.nodeCount;
    expanded.partCount = indexed.partCount;
    expanded.warning = "legacy warning";
    for (const auto index : indexed.indices)
        expanded.vertices.push_back(indexed.vertices.at(index));
    auto body = encodeMeshPayload(expanded, {});
    response.payloadBytes = body.size();
    const auto wire = encodeFrame({ response, body }, {});
    const auto expected = encodeHeader(request, {});
    Dust3DWorkerReplyResult receipt {};
    std::unique_ptr<Dust3DWorkerReplyCollector, decltype(&dust3d_worker_reply_destroy)> owner(
        dust3d_worker_reply_create(expected.data(), expected.size(), { 4096, 4096, 4096 }, &receipt), dust3d_worker_reply_destroy);
    require(owner && dust3d_worker_reply_append(owner.get(), wire.data(), wire.size()).status == DUST3D_WORKER_REPLY_READY, "Expanded fixture is framed independently");
    require(dust3d_worker_reply_finish(owner.get()).status == DUST3D_WORKER_REPLY_FRAMED, "Expanded fixture reaches EOF");
    Dust3DWorkerDecodeResult result {};
    Decoded decoded(dust3d_worker_reply_decode(owner.get(), 4096, &result), dust3d_worker_decoded_destroy);
    require(decoded && result.status == DUST3D_WORKER_DECODE_OK, "Existing expanded alternative remains supported");
    owner.reset();
    Dust3DWorkerVertex vertices[6] {};
    require(dust3d_worker_decoded_vertices(decoded.get(), 0, 6, vertices) && vertices[5].position[1] == 1, "Shared vertex helper preserves expanded corner semantics");
    require(!dust3d_worker_decoded_index_count(decoded.get()) && !dust3d_worker_decoded_indices(decoded.get(), 0, 0, nullptr), "Expanded geometry does not fabricate indexed storage");
    require(!dust3d_worker_decoded_vertices(nullptr, 0, 0, nullptr), "Shared vertex helper safely rejects a missing owner");
}
void healthyRuntime(const char* fixture, bool withSources)
{
    std::ifstream input(fixture, std::ios::binary);
    require(bool(input), "Licensed Capsule fixture opens");
    const std::vector<uint8_t> archive { std::istreambuf_iterator<char>(input), {} };
    auto request = packet(archive, withSources);
    size_t size = 0;
    const auto* bytes = dust3d_worker_request_bytes(request.get(), &size);
    require(bytes && size > headerBytes, "Complete owned request span is readable");
    std::unique_ptr<FILE, decltype(&fclose)> source(tmpfile(), fclose), output(tmpfile(), fclose);
    require(source && output && fwrite(bytes, 1, size, source.get()) == size, "Owned ordinary job channels retain complete request");
    rewind(source.get());
    request.reset();
    const auto result = dust3d_worker_serve(fileno(source.get()), fileno(output.get()));
    require(result.status == DUST3D_WORKER_JOB_COMPLETE && result.request_identity == 42
            && result.operation == (withSources ? DUST3D_WORKER_OPERATION_INDEXED_SOURCED_MESH : DUST3D_WORKER_OPERATION_INDEXED_MESH),
        "Healthy indexed runtime dispatch completes with original identity");
    rewind(output.get());
    std::vector<uint8_t> reply;
    std::array<uint8_t, 4096> buffer {};
    while ((size = fread(buffer.data(), 1, buffer.size(), output.get())) != 0) {
        require(reply.size() <= 1048576 && size <= 1048576 - reply.size(), "Known-positive fixture reply remains small");
        reply.insert(reply.end(), buffer.begin(), buffer.begin() + size);
    }
    require(!ferror(output.get()), "Complete ordinary indexed response readable");
    const auto framed = decodeFrame(reply.data(), reply.size(), {});
    validateResponseIdentity(framed.header, requestHeader(withSources));
    require(framed.header.kind == FrameKind::response, "Known-positive job returns geometry");
    if (withSources) {
        const auto value = decodeIndexedSourcesPayload(framed.payload.data(), framed.payload.size(), {});
        require(value.geometry.indices.size() == 240 && value.sources.corners.size() == 240, "Indexed/source worker correspondence remains intact");
    } else
        require(decodeIndexedMeshPayload(framed.payload.data(), framed.payload.size(), {}).indices.size() == 240, "Ordinary indexed route supports future optional-hint recovery");
}
}
int main(int argc, char** argv)
{
    try {
        requests();
        expandedCompatibility();
        for (bool sources : { false, true })
            for (int mode = 0; mode < 5; ++mode)
                ownedOutput(sources, mode);
        require(!dust3d_worker_decoded_index_count(nullptr) && !dust3d_worker_decoded_indices(nullptr, 0, 0, nullptr), "Null indexed owner is safe");
        if (argc == 2) {
            healthyRuntime(argv[1], true);
            healthyRuntime(argv[1], false);
        } else
            require(argc == 1, "Optional licensed fixture only");
        std::cout << "Indexed worker runtime contracts passed\n";
    } catch (const std::exception& cause) {
        std::cerr << cause.what() << '\n';
        return 1;
    }
}
