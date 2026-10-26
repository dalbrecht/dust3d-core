// SPDX-License-Identifier: MIT
#include "WorkerIndexedSourceOperation.h"
#include "WorkerIndexedCopyBudget.h"
#include "WorkerOperation.h"
#include "WorkerSourceCopy.h"
#include "WorkerSourceRequest.h"
#include <algorithm>
#include <limits>
#include <memory>
#include <new>
namespace dust3d {
namespace worker {
    namespace {
        static_assert(sizeof(Dust3DVertex) == sizeof(MeshVertex), "Native and owned vertex record charges must agree.");
        static_assert(maximumPayloadBytes <= std::numeric_limits<size_t>::max(), "Native indexed budget must fit size_t.");
        [[noreturn]] void rejectLimit()
        {
            throw IndexedMeshPayloadError(IndexedMeshPayloadErrorCode::limitExceeded, "Native indexed output exceeds its copy quota.");
        }
        void charge(uint64_t& total, uint64_t count, uint64_t unit, uint64_t maximum)
        {
            if (total > maximum || count > (maximum - total) / unit)
                rejectLimit();
            total += count * unit;
        }
        size_t measureWarning(const char* value, uint64_t maximum)
        {
            if (!value)
                throw MeshPayloadError(MeshPayloadErrorCode::invalidText, "Native indexed warning storage is missing.");
            size_t size = 0;
            while (size <= maximum && value[size])
                ++size;
            if (size > maximum)
                rejectLimit();
            return size;
        }
        IndexedMeshPayloadLimits tightenGeometryPolicy(const detail::NativeSourceHintInspection& source, const IndexedSourcesPayloadLimits& limits)
        {
            if (source.logicalBytes >= limits.maximumDecodedBytes || indexedSourcesPrefixBytes > limits.maximumWireBytes
                || source.wireBytes >= limits.maximumWireBytes - indexedSourcesPrefixBytes)
                throw IndexedSourcesPayloadError(IndexedSourcesPayloadErrorCode::limitExceeded, "Native indexed/source pair exceeds its aggregate copy quota.");
            auto result = limits.geometry;
            result.maximumDecodedBytes = std::min(result.maximumDecodedBytes, limits.maximumDecodedBytes - source.logicalBytes);
            result.maximumWireBytes = std::min(result.maximumWireBytes, limits.maximumWireBytes - indexedSourcesPrefixBytes - source.wireBytes);
            return result;
        }
        size_t calculateNativeIndexBudget(const Dust3DMesh* source, uint64_t corners, const IndexedMeshPayloadLimits& limits)
        {
            if (!source || dust3d_mesh_vertex_count(source) != corners)
                throw IndexedSourcesPayloadError(IndexedSourcesPayloadErrorCode::countMismatch, "Native source corners disagree with expanded geometry.");
            const auto warning = measureWarning(dust3d_mesh_warning(source), limits.maximumWarningBytes);
            return static_cast<size_t>(calculateIndexedCopyBudget(corners, warning, dust3d_mesh_node_count(source), dust3d_mesh_part_count(source), limits));
        }
        FailurePayload copyFailure(FailureStatus status, const char* cause)
        {
            size_t size = 0;
            while (size < FailurePayloadLimits {}.maximumCauseBytes && cause[size])
                ++size;
            return { status, { cause, cause + size } };
        }
        void validateRequest(const Frame& request, Operation operation)
        {
            (void)encodeHeader(request.header, {});
            if (request.header.operation != operation)
                throw ProtocolError(ProtocolErrorCode::unsupportedOperation, "Indexed request requires its exact opcode.");
            if (request.header.kind != FrameKind::request)
                throw ProtocolError(ProtocolErrorCode::unexpectedResponse, "Indexed operation requires a request frame.");
            if (request.header.payloadBytes != request.payload.size())
                throw ProtocolError(ProtocolErrorCode::invalidInput, "Indexed request length does not match its header.");
        }
    }
    IndexedMeshPayload copyNativeIndexedMesh(const Dust3DIndexedMesh* owner, const IndexedMeshPayloadLimits& limits)
    {
        validateIndexedMeshPayloadLimits(limits);
        if (!owner)
            throw IndexedMeshPayloadError(IndexedMeshPayloadErrorCode::invalidInput, "Native indexed owner is missing.");
        const auto vertexCount = dust3d_indexed_mesh_vertex_count(owner), indexCount = dust3d_indexed_mesh_index_count(owner);
        const auto nodes = dust3d_indexed_mesh_node_count(owner), parts = dust3d_indexed_mesh_part_count(owner);
        const auto* vertices = dust3d_indexed_mesh_vertices(owner);
        const auto* indices = dust3d_indexed_mesh_indices(owner);
        if (!vertexCount || !indexCount || indexCount % 3 || !vertices || !indices)
            throw IndexedMeshPayloadError(IndexedMeshPayloadErrorCode::invalidGeometry, "Native indexed output requires complete stable triangles.");
        if (vertexCount > limits.maximumVertices || indexCount > limits.maximumIndices || nodes > limits.maximumNodeCount || parts > limits.maximumPartCount)
            rejectLimit();
        const auto* text = dust3d_indexed_mesh_warning(owner);
        const auto warning = measureWarning(text, limits.maximumWarningBytes);
        uint64_t wire = indexedMeshPrefixBytes, logical = 0;
        charge(wire, warning, 1, limits.maximumWireBytes);
        charge(wire, vertexCount, meshVertexBytes, limits.maximumWireBytes);
        charge(wire, indexCount, 4, limits.maximumWireBytes);
        charge(logical, warning, 1, limits.maximumDecodedBytes);
        charge(logical, vertexCount, sizeof(MeshVertex), limits.maximumDecodedBytes);
        charge(logical, indexCount, sizeof(uint32_t), limits.maximumDecodedBytes);
        IndexedMeshPayload result;
        result.nodeCount = nodes;
        result.partCount = parts;
        result.warning.assign(text, warning);
        result.vertices.reserve(vertexCount);
        for (size_t index = 0; index < vertexCount; ++index) {
            const auto& value = vertices[index];
            result.vertices.push_back({ { value.position[0], value.position[1], value.position[2] },
                { value.normal[0], value.normal[1], value.normal[2] }, { value.color[0], value.color[1], value.color[2], value.color[3] } });
        }
        result.indices.assign(indices, indices + indexCount);
        (void)encodeIndexedMeshPayload(result, limits);
        return result;
    }
    IndexedSourcesPayload copyNativeIndexedSourcedMesh(const Dust3DSourcedMesh* source, const Dust3DIndexedMesh* indexed, const IndexedSourcesPayloadLimits& limits)
    {
        validateIndexedSourcesPayloadLimits(limits);
        const auto sources = detail::inspectNativeSourceHints(source, limits.sources);
        if (!indexed)
            throw IndexedMeshPayloadError(IndexedMeshPayloadErrorCode::invalidInput, "Native indexed owner is missing.");
        if (sources.cornerCount != dust3d_indexed_mesh_index_count(indexed) || sources.identityCount != dust3d_indexed_mesh_node_count(indexed))
            throw IndexedSourcesPayloadError(IndexedSourcesPayloadErrorCode::countMismatch, "Native indexed/source dimensions disagree.");
        const auto policy = tightenGeometryPolicy(sources, limits);
        IndexedSourcesPayload result { copyNativeIndexedMesh(indexed, policy), detail::copyNativeSourceHints(source, limits.sources) };
        (void)encodeIndexedSourcesPayload(result, limits);
        return result;
    }
    static IndexedSourceGenerationResult generateIndexedSourcedMeshImpl(Dust3DGenerationSession* session, const std::vector<uint8_t>& archive, const IndexedSourcesPayloadLimits& limits)
    {
        validateIndexedSourcesPayloadLimits(limits);
        try {
            Dust3DError error {};
            Dust3DSourceOptions options { sizeof(options), static_cast<size_t>(limits.sources.maximumDecodedBytes) };
            std::unique_ptr<Dust3DSourcedMesh, decltype(&dust3d_sourced_mesh_destroy)> source(
                session ? dust3d_generation_session_load_sourced(session, archive.data(), archive.size(), &options, &error)
                        : dust3d_sourced_mesh_load(archive.data(), archive.size(), &options, &error),
                dust3d_sourced_mesh_destroy);
            if (error.status != DUST3D_STATUS_OK)
                return copyNativeFailure(error);
            if (!source)
                return copyFailure(FailureStatus::generationFailed, "Native source success returned no owner.");
            const auto sources = detail::inspectNativeSourceHints(source.get(), limits.sources);
            const auto policy = tightenGeometryPolicy(sources, limits);
            const auto* geometry = dust3d_sourced_mesh_geometry(source.get());
            const auto budget = calculateNativeIndexBudget(geometry, sources.cornerCount, policy);
            std::unique_ptr<Dust3DIndexedMesh, decltype(&dust3d_indexed_mesh_destroy)> indexed(
                dust3d_mesh_create_indexed(geometry, budget, &error), dust3d_indexed_mesh_destroy);
            if (error.status != DUST3D_STATUS_OK)
                return copyNativeFailure(error);
            if (!indexed)
                return copyFailure(FailureStatus::generationFailed, "Native indexed success returned no owner.");
            auto result = copyNativeIndexedSourcedMesh(source.get(), indexed.get(), limits);
            indexed.reset();
            source.reset();
            return result;
        } catch (const IndexedSourcesPayloadError& cause) {
            return copyFailure(cause.code() == IndexedSourcesPayloadErrorCode::limitExceeded ? FailureStatus::limitExceeded : FailureStatus::generationFailed, cause.what());
        } catch (const IndexedMeshPayloadError& cause) {
            return copyFailure(cause.code() == IndexedMeshPayloadErrorCode::limitExceeded ? FailureStatus::limitExceeded : FailureStatus::generationFailed, cause.what());
        } catch (const SourceHintPayloadError& cause) {
            return copyFailure(cause.code() == SourceHintPayloadErrorCode::limitExceeded ? FailureStatus::limitExceeded : FailureStatus::generationFailed, cause.what());
        } catch (const MeshPayloadError& cause) {
            return copyFailure(cause.code() == MeshPayloadErrorCode::limitExceeded ? FailureStatus::limitExceeded : FailureStatus::generationFailed, cause.what());
        } catch (const std::bad_alloc& cause) {
            return copyFailure(FailureStatus::limitExceeded, cause.what());
        } catch (const std::exception& cause) {
            return copyFailure(FailureStatus::generationFailed, cause.what());
        } catch (...) {
            return copyFailure(FailureStatus::generationFailed, "Native indexed/source operation threw without a diagnostic.");
        }
    }
    static IndexedGenerationResult generateIndexedMeshImpl(Dust3DGenerationSession* session, const std::vector<uint8_t>& archive, const IndexedMeshPayloadLimits& limits)
    {
        validateIndexedMeshPayloadLimits(limits);
        try {
            Dust3DError error {};
            std::unique_ptr<Dust3DMesh, decltype(&dust3d_mesh_destroy)> source(
                session ? dust3d_generation_session_load(session, archive.data(), archive.size(), &error)
                        : dust3d_mesh_load(archive.data(), archive.size(), &error),
                dust3d_mesh_destroy);
            if (error.status != DUST3D_STATUS_OK)
                return copyNativeFailure(error);
            if (!source)
                return copyFailure(FailureStatus::generationFailed, "Native indexed generation returned no source owner.");
            const auto budget = calculateNativeIndexBudget(source.get(), dust3d_mesh_vertex_count(source.get()), limits);
            std::unique_ptr<Dust3DIndexedMesh, decltype(&dust3d_indexed_mesh_destroy)> indexed(
                dust3d_mesh_create_indexed(source.get(), budget, &error), dust3d_indexed_mesh_destroy);
            if (error.status != DUST3D_STATUS_OK)
                return copyNativeFailure(error);
            if (!indexed)
                return copyFailure(FailureStatus::generationFailed, "Native indexed success returned no owner.");
            source.reset();
            auto result = copyNativeIndexedMesh(indexed.get(), limits);
            indexed.reset();
            return result;
        } catch (const IndexedMeshPayloadError& cause) {
            return copyFailure(cause.code() == IndexedMeshPayloadErrorCode::limitExceeded ? FailureStatus::limitExceeded : FailureStatus::generationFailed, cause.what());
        } catch (const MeshPayloadError& cause) {
            return copyFailure(cause.code() == MeshPayloadErrorCode::limitExceeded ? FailureStatus::limitExceeded : FailureStatus::generationFailed, cause.what());
        } catch (const std::bad_alloc& cause) {
            return copyFailure(FailureStatus::limitExceeded, cause.what());
        } catch (const std::exception& cause) {
            return copyFailure(FailureStatus::generationFailed, cause.what());
        } catch (...) {
            return copyFailure(FailureStatus::generationFailed, "Native indexed operation threw without a diagnostic.");
        }
    }
    IndexedSourceGenerationResult generateIndexedSourcedMesh(const std::vector<uint8_t>& archive, const IndexedSourcesPayloadLimits& limits)
    {
        return generateIndexedSourcedMeshImpl(nullptr, archive, limits);
    }
    IndexedGenerationResult generateIndexedMesh(const std::vector<uint8_t>& archive, const IndexedMeshPayloadLimits& limits)
    {
        return generateIndexedMeshImpl(nullptr, archive, limits);
    }
    IndexedSourceGenerationResult generateSessionIndexedSourcedMesh(Dust3DGenerationSession* session, const std::vector<uint8_t>& archive, const IndexedSourcesPayloadLimits& limits)
    {
        if (!session)
            throw ProtocolError(ProtocolErrorCode::invalidInput, "Cached source generation requires a session owner.");
        return generateIndexedSourcedMeshImpl(session, archive, limits);
    }
    IndexedGenerationResult generateSessionIndexedMesh(Dust3DGenerationSession* session, const std::vector<uint8_t>& archive, const IndexedMeshPayloadLimits& limits)
    {
        if (!session)
            throw ProtocolError(ProtocolErrorCode::invalidInput, "Cached indexed generation requires a session owner.");
        return generateIndexedMeshImpl(session, archive, limits);
    }
    Frame generateIndexedSourcesReply(const Frame& request, const IndexedSourcesPayloadLimits& limits)
    {
        validateRequest(request, Operation::generateIndexedSourcedMesh);
        validateIndexedSourcesPayloadLimits(limits);
        const auto decoded = decodeSourceRequest(request.payload.data(), request.payload.size(), {});
        auto policy = limits;
        const auto requested = decoded.maximumSourceBytes ? decoded.maximumSourceBytes : DUST3D_DEFAULT_SOURCE_PAYLOAD_BYTES;
        policy.sources.maximumDecodedBytes = std::min(policy.sources.maximumDecodedBytes, requested);
        auto generated = generateIndexedSourcedMesh(decoded.archive, policy);
        auto header = request.header;
        std::vector<uint8_t> bytes;
        if (const auto* value = std::get_if<IndexedSourcesPayload>(&generated)) {
            header.kind = FrameKind::response;
            bytes = encodeIndexedSourcesPayload(*value, policy);
        } else {
            header.kind = FrameKind::failure;
            bytes = encodeFailurePayload(std::get<FailurePayload>(generated), {});
        }
        header.payloadBytes = bytes.size();
        return { header, std::move(bytes) };
    }
    Frame generateIndexedMeshReply(const Frame& request, const IndexedMeshPayloadLimits& limits)
    {
        validateRequest(request, Operation::generateIndexedMesh);
        auto generated = generateIndexedMesh(request.payload, limits);
        auto header = request.header;
        std::vector<uint8_t> bytes;
        if (const auto* value = std::get_if<IndexedMeshPayload>(&generated)) {
            header.kind = FrameKind::response;
            bytes = encodeIndexedMeshPayload(*value, limits);
        } else {
            header.kind = FrameKind::failure;
            bytes = encodeFailurePayload(std::get<FailurePayload>(generated), {});
        }
        header.payloadBytes = bytes.size();
        return { header, std::move(bytes) };
    }
}
}
