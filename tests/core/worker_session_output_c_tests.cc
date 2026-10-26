// SPDX-License-Identifier: MIT
#include <Dust3DWorkerSessionOutput.h>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <native/Engine/WorkerSessionOperation.h>
#include <native/Engine/WorkerSessionOutput.h>
#include <native/Engine/WorkerSourceRequest.h>
#include <native/Engine/WorkerWire.h>
using namespace dust3d::worker;
void require(bool value)
{
    if (!value)
        throw std::runtime_error("Session C ownership contract");
}
IndexedSourcesPayload sample()
{
    IndexedSourcesPayload value;
    value.geometry.vertices = { { { 0, 0, 0 }, { 0, 0, 1 }, { 1, 1, 1, 1 } }, { { 1, 0, 0 }, { 0, 0, 1 }, { 1, 1, 1, 1 } }, { { 0, 1, 0 }, { 0, 0, 1 }, { 1, 1, 1, 1 } } };
    value.geometry.indices = { 0, 1, 2 };
    value.geometry.nodeCount = 1;
    value.geometry.partCount = 1;
    value.sources.identities = { { "node", "part" } };
    value.sources.influences = { { SourceHintKind::nodeHint, 0, 1.0F } };
    value.sources.corners.assign(3, { SourceHintKind::nodeHint, 0, 1 });
    return value;
}
void rejected(const std::vector<uint8_t>& wire, Dust3DWorkerDetailSource source, int32_t code)
{
    Dust3DWorkerDecodeResult result {};
    std::unique_ptr<Dust3DWorkerSessionOutput, decltype(&dust3d_worker_session_output_destroy)> owner(
        dust3d_worker_session_output_decode(wire.data(), wire.size(), { 4096, 4096, 0 }, &result), dust3d_worker_session_output_destroy);
    require(!owner && result.status == DUST3D_WORKER_DECODE_CODEC_REJECTED && result.detail_source == source && result.detail_code == code && result.message[0]);
}
void originalCodecCauses()
{
    auto wire = encodeSessionOutput(SessionOutput {}, {});
    wire[65] = 2;
    rejected(wire, DUST3D_WORKER_DETAIL_GENERATION_STATISTICS, static_cast<int32_t>(GenerationStatisticsErrorCode::unsupportedVersion));
    SessionOutput failure;
    failure.outcome = FailurePayload {};
    wire = encodeSessionOutput(failure, {});
    wire[65] = 9;
    rejected(wire, DUST3D_WORKER_DETAIL_FAILURE, static_cast<int32_t>(FailurePayloadErrorCode::unsupportedVersion));
    SessionOutput ordinary;
    ordinary.outcome = sample().geometry;
    wire = encodeSessionOutput(ordinary, {});
    wire[65] = 9;
    rejected(wire, DUST3D_WORKER_DETAIL_INDEXED_MESH, static_cast<int32_t>(IndexedMeshPayloadErrorCode::unsupportedVersion));
    auto warned = sample().geometry;
    warned.warning = "ok";
    ordinary.outcome = warned;
    wire = encodeSessionOutput(ordinary, {});
    wire[sessionOutputPrefixBytes + indexedMeshPrefixBytes] = 127;
    rejected(wire, DUST3D_WORKER_DETAIL_MESH, static_cast<int32_t>(MeshPayloadErrorCode::invalidText));
    SessionOutput sourced;
    sourced.outcome = sample();
    wire = encodeSessionOutput(sourced, {});
    wire[65] = 9;
    rejected(wire, DUST3D_WORKER_DETAIL_INDEXED_SOURCES, static_cast<int32_t>(IndexedSourcesPayloadErrorCode::unsupportedVersion));
    wire = encodeSessionOutput(sourced, {});
    const auto meshSize = detail::readUnsigned(wire.data() + sessionOutputPrefixBytes + 8, 8);
    wire[sessionOutputPrefixBytes + indexedSourcesPrefixBytes + static_cast<size_t>(meshSize) + 1] = 9;
    rejected(wire, DUST3D_WORKER_DETAIL_SOURCE_HINTS, static_cast<int32_t>(SourceHintPayloadErrorCode::unsupportedVersion));
}
void ordinaryCapsule(const char* fixtures)
{
    std::ifstream input(std::string(fixtures) + "/Capsule.ds3", std::ios::binary);
    require(static_cast<bool>(input));
    std::vector<uint8_t> archive { std::istreambuf_iterator<char>(input), {} };
    Frame request;
    request.header.operation = Operation::generateSessionIndexedSourcedMesh;
    request.header.identity.document[0] = 1;
    request.header.identity.request = 1;
    request.payload = encodeSourceRequest({ archive, 0 }, {});
    request.header.payloadBytes = request.payload.size();
    ConnectionGenerationSession session(request.header.identity.document, 7, 64ULL * 1024 * 1024);
    (void)generateSessionReply(request, session, 7, {});
    ++request.header.identity.request;
    auto reply = generateSessionReply(request, session, 7, {});
    validateResponseIdentity(reply.header, request.header);
    session.close();
    Dust3DWorkerDecodeResult result {};
    using Owner = std::unique_ptr<Dust3DWorkerSessionOutput, decltype(&dust3d_worker_session_output_destroy)>;
    Owner owner(dust3d_worker_session_output_decode(reply.payload.data(), reply.payload.size(), { 1048576, 1048576, 0 }, &result), dust3d_worker_session_output_destroy);
    require(owner && result.status == DUST3D_WORKER_DECODE_OK);
    reply.payload.clear();
    const auto* primary = dust3d_worker_session_output_primary(owner.get());
    const auto summary = dust3d_worker_decoded_summary(primary);
    const auto count = dust3d_worker_decoded_index_count(primary);
    require(summary.kind == DUST3D_WORKER_DECODED_INDEXED_SOURCED_MESH && summary.vertex_count > 0 && count > 0 && count < 1048576
        && dust3d_worker_decoded_source_summary(primary).corner_count == count);
    Dust3DWorkerSessionStatistics stats {};
    require(dust3d_worker_session_output_statistics(owner.get(), &stats, sizeof(stats)) && stats.reused_components > 0);
    std::vector<uint32_t> indices(count);
    require(dust3d_worker_decoded_indices(primary, 0, count, indices.data()));
    owner.reset();
    require(indices.front() < summary.vertex_count);
}
int main(int argc, char** argv)
{
    try {
        require(argc == 2);
        ordinaryCapsule(argv[1]);
        originalCodecCauses();
        SessionOutput value;
        value.statistics = GenerationStatistics {};
        value.statistics->reusedComponents = 7;
        value.statistics->cacheCause = { 0, 255 };
        value.sourceIssue = FailurePayload { FailureStatus::limitExceeded, { 42 } };
        auto wire = encodeSessionOutput(value, {});
        Dust3DWorkerDecodeResult result {};
        using Owner = std::unique_ptr<Dust3DWorkerSessionOutput, decltype(&dust3d_worker_session_output_destroy)>;
        Owner owner(dust3d_worker_session_output_decode(wire.data(), wire.size(), { 4096, 4096 }, &result), dust3d_worker_session_output_destroy);
        require(owner && result.status == DUST3D_WORKER_DECODE_OK);
        wire.assign(wire.size(), 0);
        const auto summary = dust3d_worker_session_output_summary(owner.get());
        require(summary.outcome == DUST3D_WORKER_SESSION_EMPTY && summary.statistics_available == 1);
        require(!dust3d_worker_session_output_primary(owner.get()));
        Dust3DWorkerSessionStatistics stats {};
        require(dust3d_worker_session_output_statistics(owner.get(), &stats, sizeof(stats)) && stats.reused_components == 7);
        size_t count = 0;
        const auto* cause = dust3d_worker_session_output_cache_cause(owner.get(), &count);
        require(cause && count == 2 && cause[0] == 0 && cause[1] == 255);
        const auto* issue = dust3d_worker_session_output_source_issue(owner.get());
        require(issue && dust3d_worker_decoded_summary(issue).kind == DUST3D_WORKER_DECODED_FAILURE);
        cause = dust3d_worker_decoded_failure_cause(issue, &count);
        require(cause && count == 1 && cause[0] == 42);
        require(!dust3d_worker_session_output_statistics_issue(owner.get()));
        SessionOutput failed;
        failed.outcome = FailurePayload { FailureStatus::unsupportedAsset, { 0, 255 } };
        failed.statisticsIssue = FailurePayload { FailureStatus::generationFailed, { 43 } };
        wire = encodeSessionOutput(failed, {});
        Owner failure(dust3d_worker_session_output_decode(wire.data(), wire.size(), { 4096, 4096 }, &result), dust3d_worker_session_output_destroy);
        require(failure && dust3d_worker_session_output_summary(failure.get()).outcome == DUST3D_WORKER_SESSION_FAILURE);
        const auto* primary = dust3d_worker_session_output_primary(failure.get());
        require(primary && dust3d_worker_decoded_summary(primary).failure_status == 2);
        require(!dust3d_worker_session_output_statistics(failure.get(), &stats, sizeof(stats)) && stats.reused_components == 0);
        require(dust3d_worker_session_output_statistics_issue(failure.get()));
        for (bool sourced : { true, false }) {
            SessionOutput geometry;
            if (sourced)
                geometry.outcome = sample();
            else
                geometry.outcome = sample().geometry;
            wire = encodeSessionOutput(geometry, {});
            Owner mesh(dust3d_worker_session_output_decode(wire.data(), wire.size(), { 4096, 4096 }, &result), dust3d_worker_session_output_destroy);
            require(mesh && result.status == DUST3D_WORKER_DECODE_OK);
            wire.clear();
            const auto* view = dust3d_worker_session_output_primary(mesh.get());
            require(view && dust3d_worker_decoded_index_count(view) == 3);
            uint32_t indices[3] {};
            require(dust3d_worker_decoded_indices(view, 0, 3, indices) && indices[2] == 2);
            require(dust3d_worker_decoded_summary(view).vertex_count == 3);
            require(dust3d_worker_decoded_source_summary(view).corner_count == (sourced ? 3 : 0));
            require(dust3d_worker_session_output_summary(mesh.get()).outcome == (sourced ? DUST3D_WORKER_SESSION_SOURCED : DUST3D_WORKER_SESSION_ORDINARY));
        }
        SessionOutput measuredZero;
        measuredZero.statistics = GenerationStatistics {};
        wire = encodeSessionOutput(measuredZero, {});
        Owner zero(dust3d_worker_session_output_decode(wire.data(), wire.size(), { 4096, 4096, 0 }, &result), dust3d_worker_session_output_destroy);
        require(zero && dust3d_worker_session_output_summary(zero.get()).statistics_available == 1
            && dust3d_worker_session_output_statistics(zero.get(), &stats, sizeof(stats)) && stats.reused_components == 0);
        count = 99;
        require(!dust3d_worker_session_output_cache_cause(zero.get(), &count) && count == 0);
        require(!dust3d_worker_session_output_cache_cause(failure.get(), &count) && count == 0);
        wire = encodeSessionOutput(SessionOutput {}, {});
        require(!dust3d_worker_session_output_decode(wire.data(), wire.size(), { 4096, 4096 }, nullptr));
        require(!dust3d_worker_session_output_decode(wire.data(), wire.size(), { 0, 4096 }, &result)
            && result.status == DUST3D_WORKER_DECODE_PROTOCOL_REJECTED);
        require(!dust3d_worker_session_output_decode(nullptr, wire.size(), { 4096, 4096 }, &result)
            && result.status == DUST3D_WORKER_DECODE_CODEC_REJECTED
            && result.detail_source == DUST3D_WORKER_DETAIL_SESSION_OUTPUT
            && result.detail_code == static_cast<int32_t>(SessionOutputErrorCode::invalidInput));
        for (int mode = 0; mode < 5; ++mode) {
            Dust3DWorkerSessionOutputLimits invalid { 4096, 4096, 0 };
            if (!mode)
                invalid.maximum_wire_bytes = 0;
            if (mode == 1)
                invalid.maximum_wire_bytes = maximumPayloadBytes + 1;
            if (mode == 2)
                invalid.maximum_decoded_bytes = 0;
            if (mode == 3)
                invalid.maximum_decoded_bytes = maximumPayloadBytes + 1;
            if (mode == 4)
                invalid.maximum_source_bytes = maximumPayloadBytes + 1;
            require(!dust3d_worker_session_output_decode(wire.data(), wire.size(), invalid, &result)
                && result.status == DUST3D_WORKER_DECODE_PROTOCOL_REJECTED
                && result.detail_code == static_cast<int32_t>(ProtocolErrorCode::invalidLimits));
        }
        Owner explicitSource(dust3d_worker_session_output_decode(wire.data(), wire.size(), { 4096, 4096, 4096 }, &result), dust3d_worker_session_output_destroy);
        require(explicitSource && !dust3d_worker_session_output_summary(explicitSource.get()).statistics_available);
        require(!dust3d_worker_session_output_statistics_issue(nullptr));
        stats.reused_components = 99;
        require(!dust3d_worker_session_output_statistics(nullptr, &stats, sizeof(stats)) && stats.reused_components == 0);
        stats.reused_components = 99;
        require(!dust3d_worker_session_output_statistics(owner.get(), &stats, sizeof(stats) - 1) && stats.reused_components == 99);
        require(!dust3d_worker_session_output_statistics(owner.get(), nullptr, sizeof(stats)));
        require(!dust3d_worker_session_output_cache_cause(owner.get(), nullptr));
        require(dust3d_worker_session_output_summary(nullptr).outcome == DUST3D_WORKER_SESSION_NONE);
        require(!dust3d_worker_session_output_primary(nullptr) && !dust3d_worker_session_output_source_issue(nullptr));
        count = 99;
        require(!dust3d_worker_session_output_cache_cause(nullptr, &count) && count == 0);
        dust3d_worker_session_output_destroy(nullptr);
    } catch (const std::exception& cause) {
        std::cerr << cause.what() << '\n';
        return 1;
    }
    return 0;
}
