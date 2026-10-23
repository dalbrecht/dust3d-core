// SPDX-License-Identifier: MIT
#include "WorkerOperation.h"
#include <algorithm>
#include <cstdio>
#include <memory>
#include <new>
#include <utility>
namespace dust3d {
namespace worker {
    namespace {
        std::vector<uint8_t> causeBytes(const char* bytes, size_t maximum)
        {
            if (!bytes)
                return {};
            size_t length = 0;
            while (length < maximum && bytes[length])
                ++length;
            return { bytes, bytes + length };
        }

    }
    FailurePayload copyNativeFailure(const Dust3DError& error)
    {
        if (error.status == DUST3D_STATUS_OK)
            throw ProtocolError(ProtocolErrorCode::invalidInput, "Native success is not a failure payload.");
        FailurePayload result;
        result.cause = causeBytes(error.message, sizeof(error.message));
        switch (error.status) {
        case DUST3D_STATUS_INVALID_DOCUMENT:
            result.status = FailureStatus::invalidDocument;
            break;
        case DUST3D_STATUS_UNSUPPORTED_ASSET:
            result.status = FailureStatus::unsupportedAsset;
            break;
        case DUST3D_STATUS_GENERATION_FAILED:
            result.status = FailureStatus::generationFailed;
            break;
        case DUST3D_STATUS_LIMIT_EXCEEDED:
            result.status = FailureStatus::limitExceeded;
            break;
        case DUST3D_STATUS_NO_GEOMETRY:
            result.status = FailureStatus::noGeometry;
            break;
        case DUST3D_STATUS_CANCELLED:
            result.status = FailureStatus::cancelled;
            break;
        default: {
            char prefix[96];
            const int length = std::snprintf(prefix, sizeof(prefix), "Unrecognized native status %d; original cause: ", static_cast<int>(error.status));
            auto original = std::move(result.cause);
            result.cause.assign(prefix, prefix + std::min(static_cast<size_t>(std::max(length, 0)), sizeof(prefix) - 1));
            result.cause.insert(result.cause.end(), original.begin(), original.end());
            result.status = FailureStatus::generationFailed;
            break;
        }
        }
        return result;
    }

    namespace {
        Frame failure(const Header& request, FailurePayload payload)
        {
            auto bytes = encodeFailurePayload(payload, {});
            auto header = request;
            header.kind = FrameKind::failure;
            header.payloadBytes = bytes.size();
            return { header, std::move(bytes) };
        }
        Frame failure(const Header& request, FailureStatus status, const char* cause)
        {
            // Standard exception descriptions here are bounded by the failure schema.
            const auto maximum = FailurePayloadLimits {}.maximumCauseBytes;
            auto bytes = causeBytes(cause, static_cast<size_t>(maximum) + 1);
            return failure(request, { status, std::move(bytes) });
        }
        MeshPayload copyNativeMesh(const Dust3DMesh* owner, const MeshPayloadLimits& limits)
        {
            const auto count = dust3d_mesh_vertex_count(owner);
            const auto* vertices = dust3d_mesh_vertices(owner);
            if (!count || count % 3 || !vertices)
                throw MeshPayloadError(MeshPayloadErrorCode::invalidGeometry, "Native worker returned invalid expanded geometry.");
            const auto nodes = dust3d_mesh_node_count(owner);
            const auto parts = dust3d_mesh_part_count(owner);
            if (count > limits.maximumVertices || nodes > limits.maximumNodeCount || parts > limits.maximumPartCount)
                throw MeshPayloadError(MeshPayloadErrorCode::limitExceeded, "Native worker output exceeds its count limits.");
            const auto* warning = dust3d_mesh_warning(owner);
            if (!warning)
                throw MeshPayloadError(MeshPayloadErrorCode::invalidText, "Native worker warning storage is missing.");
            size_t warningBytes = 0;
            while (warningBytes <= limits.maximumWarningBytes && warning[warningBytes])
                ++warningBytes;
            if (warningBytes > limits.maximumWarningBytes || warningBytes > limits.maximumDecodedBytes
                || count > (limits.maximumDecodedBytes - warningBytes) / sizeof(MeshVertex)
                || meshPrefixBytes + warningBytes > limits.maximumWireBytes
                || count > (limits.maximumWireBytes - meshPrefixBytes - warningBytes) / meshVertexBytes)
                throw MeshPayloadError(MeshPayloadErrorCode::limitExceeded, "Native worker output exceeds its copy/wire limits.");
            MeshPayload result;
            result.nodeCount = nodes;
            result.partCount = parts;
            result.warning.assign(warning, warningBytes);
            result.vertices.reserve(count);
            for (size_t index = 0; index < count; ++index) {
                const auto& vertex = vertices[index];
                result.vertices.push_back({ { vertex.position[0], vertex.position[1], vertex.position[2] },
                    { vertex.normal[0], vertex.normal[1], vertex.normal[2] }, { vertex.color[0], vertex.color[1], vertex.color[2], vertex.color[3] } });
            }
            return result;
        }

    }
    Frame generateMeshReply(const Frame& request, const MeshPayloadLimits& limits)
    {
        (void)encodeHeader(request.header, {});
        if (request.header.kind != FrameKind::request)
            throw ProtocolError(ProtocolErrorCode::unexpectedResponse, "Native worker operation requires a request frame.");
        if (request.header.payloadBytes != request.payload.size())
            throw ProtocolError(ProtocolErrorCode::invalidInput, "Native worker request length does not match its header.");
        validateMeshPayloadLimits(limits);
        try {
            Dust3DError error {};
            std::unique_ptr<Dust3DMesh, decltype(&dust3d_mesh_destroy)> owner(
                dust3d_mesh_load(request.payload.data(), request.payload.size(), &error), dust3d_mesh_destroy);
            if (error.status != DUST3D_STATUS_OK)
                return failure(request.header, copyNativeFailure(error));
            if (!owner)
                return failure(request.header, FailureStatus::generationFailed, "Native success returned no geometry owner.");
            auto mesh = copyNativeMesh(owner.get(), limits);
            owner.reset();
            auto bytes = encodeMeshPayload(mesh, limits);
            auto header = request.header;
            header.kind = FrameKind::response;
            header.payloadBytes = bytes.size();
            return { header, std::move(bytes) };
        } catch (const MeshPayloadError& cause) {
            return failure(request.header, cause.code() == MeshPayloadErrorCode::limitExceeded ? FailureStatus::limitExceeded : FailureStatus::generationFailed, cause.what());
        } catch (const std::bad_alloc& cause) {
            return failure(request.header, FailureStatus::limitExceeded, cause.what());
        } catch (const std::exception& cause) {
            return failure(request.header, FailureStatus::generationFailed, cause.what());
        } catch (...) {
            return failure(request.header, FailureStatus::generationFailed, "Worker operation threw an exception without a diagnostic.");
        }
    }
}
}
