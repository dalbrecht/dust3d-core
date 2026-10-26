// SPDX-License-Identifier: MIT
#include "WorkerSourceOperation.h"
#include "WorkerSourceCopy.h"
#include <algorithm>
#include <limits>
#include <memory>
#include <new>

namespace dust3d {
namespace worker {
    namespace {
        static_assert(sizeof(size_t) <= sizeof(uint64_t), "Native source counts must fit wire counts.");
        [[noreturn]] void rejectLimit()
        {
            throw SourceHintPayloadError(SourceHintPayloadErrorCode::limitExceeded, "Native source copy exceeds its configured quota.");
        }
        [[noreturn]] void rejectStorage()
        {
            throw SourceHintPayloadError(SourceHintPayloadErrorCode::invalidInput, "Native source copy requires complete stable storage.");
        }
        void charge(uint64_t& total, uint64_t count, uint64_t unit, uint64_t maximum)
        {
            if (total > maximum || count > (maximum - total) / unit)
                rejectLimit();
            total += count * unit;
        }
        size_t measureText(const char* text, uint64_t maximum)
        {
            if (!text)
                rejectStorage();
            size_t size = 0;
            while (size <= maximum && text[size])
                ++size;
            if (size > maximum)
                rejectLimit();
            return size;
        }
        struct Shape {
            size_t identities, corners, influences;
            uint64_t wire, logical;
        };
        Shape inspectNativeSources(const Dust3DSourcedMesh* owner, const SourceHintPayloadLimits& limits)
        {
            if (!owner)
                rejectStorage();
            Shape shape { dust3d_sourced_mesh_identity_count(owner), dust3d_sourced_mesh_corner_count(owner),
                dust3d_sourced_mesh_influence_count(owner), sourceHintPrefixBytes, 0 };
            if (shape.identities > limits.maximumIdentities || shape.corners > limits.maximumCorners
                || shape.influences > limits.maximumInfluences)
                rejectLimit();
            if (!shape.corners || shape.corners % 3 || !dust3d_sourced_mesh_corners(owner)
                || (shape.influences && !dust3d_sourced_mesh_influences(owner)))
                rejectStorage();
            charge(shape.wire, shape.identities, 8, limits.maximumWireBytes);
            charge(shape.wire, shape.corners, sourceHintCornerBytes, limits.maximumWireBytes);
            charge(shape.wire, shape.influences, sourceHintInfluenceBytes, limits.maximumWireBytes);
            charge(shape.logical, shape.identities, sizeof(SourceHintIdentity) + 2, limits.maximumDecodedBytes);
            charge(shape.logical, shape.corners, sizeof(SourceHintCorner), limits.maximumDecodedBytes);
            charge(shape.logical, shape.influences, sizeof(SourceHintInfluence), limits.maximumDecodedBytes);
            uint64_t text = 0;
            for (size_t index = 0; index < shape.identities; ++index) {
                for (const auto* value : { dust3d_sourced_mesh_node_id(owner, index), dust3d_sourced_mesh_part_id(owner, index) }) {
                    const auto length = measureText(value, limits.maximumIdentityBytes);
                    charge(text, length, 1, limits.maximumTextBytes);
                    charge(shape.wire, length, 1, limits.maximumWireBytes);
                    charge(shape.logical, length, 1, limits.maximumDecodedBytes);
                }
            }
            return shape;
        }
        SourceHintPayload copyNativeSources(const Dust3DSourcedMesh* owner, const Shape& shape)
        {
            SourceHintPayload result;
            result.identities.reserve(shape.identities);
            result.corners.reserve(shape.corners);
            result.influences.reserve(shape.influences);
            for (size_t index = 0; index < shape.identities; ++index)
                result.identities.push_back({ dust3d_sourced_mesh_node_id(owner, index), dust3d_sourced_mesh_part_id(owner, index) });
            const auto* corners = dust3d_sourced_mesh_corners(owner);
            for (size_t index = 0; index < shape.corners; ++index) {
                const auto& source = corners[index];
                result.corners.push_back({ static_cast<SourceHintKind>(source.kind), source.first_influence, source.influence_count });
            }
            const auto* influences = dust3d_sourced_mesh_influences(owner);
            for (size_t index = 0; index < shape.influences; ++index) {
                const auto& source = influences[index];
                const auto identity = source.source_index == SIZE_MAX ? noSourceIdentity : static_cast<uint64_t>(source.source_index);
                result.influences.push_back({ static_cast<SourceHintKind>(source.kind), identity, source.weight });
            }
            return result;
        }
        FailurePayload copyFailure(FailureStatus status, const char* cause)
        {
            size_t size = 0;
            while (size < FailurePayloadLimits {}.maximumCauseBytes && cause[size])
                ++size;
            return { status, { cause, cause + size } };
        }
    }

    namespace detail {
        NativeSourceHintInspection inspectNativeSourceHints(const Dust3DSourcedMesh* owner, const SourceHintPayloadLimits& limits)
        {
            validateSourceHintPayloadLimits(limits);
            const auto shape = inspectNativeSources(owner, limits);
            return { shape.identities, shape.corners, shape.influences, shape.wire, shape.logical };
        }
        SourceHintPayload copyNativeSourceHints(const Dust3DSourcedMesh* owner, const SourceHintPayloadLimits& limits)
        {
            validateSourceHintPayloadLimits(limits);
            const auto shape = inspectNativeSources(owner, limits);
            auto result = copyNativeSources(owner, shape);
            (void)measureSourceHintPayload(result, limits);
            return result;
        }
    }
    SourcedMeshPayload copyNativeSourcedMesh(const Dust3DSourcedMesh* owner, const SourcedMeshPayloadLimits& limits)
    {
        validateSourcedMeshPayloadLimits(limits);
        const auto shape = inspectNativeSources(owner, limits.sources);
        const auto* geometry = dust3d_sourced_mesh_geometry(owner);
        if (dust3d_mesh_vertex_count(geometry) != shape.corners || dust3d_mesh_node_count(geometry) != shape.identities)
            throw SourcedMeshPayloadError(SourcedMeshPayloadErrorCode::countMismatch, "Native source counts disagree with geometry.");
        if (shape.logical >= limits.maximumDecodedBytes || sourcedMeshPrefixBytes > limits.maximumWireBytes
            || shape.wire >= limits.maximumWireBytes - sourcedMeshPrefixBytes)
            throw SourcedMeshPayloadError(SourcedMeshPayloadErrorCode::limitExceeded, "Native source pair exceeds its aggregate copy quota.");
        auto geometryLimits = limits.geometry;
        geometryLimits.maximumDecodedBytes = std::min(geometryLimits.maximumDecodedBytes, limits.maximumDecodedBytes - shape.logical);
        geometryLimits.maximumWireBytes = std::min(geometryLimits.maximumWireBytes, limits.maximumWireBytes - sourcedMeshPrefixBytes - shape.wire);
        SourcedMeshPayload result { copyNativeMesh(geometry, geometryLimits), copyNativeSources(owner, shape) };
        // Reuse full semantic validation; bounded temporary encoding workspace is not retained charge.
        (void)encodeSourcedMeshPayload(result, limits);
        return result;
    }

    SourcedGenerationResult generateSourcedMesh(const std::vector<uint8_t>& archive, const SourcedMeshPayloadLimits& limits)
    {
        validateSourcedMeshPayloadLimits(limits);
        static_assert(DUST3D_MAX_SOURCE_PAYLOAD_BYTES <= std::numeric_limits<size_t>::max(), "Native source budget must fit size_t.");
        try {
            Dust3DSourceOptions options { sizeof(options), static_cast<size_t>(limits.sources.maximumDecodedBytes) };
            Dust3DError error {};
            std::unique_ptr<Dust3DSourcedMesh, decltype(&dust3d_sourced_mesh_destroy)> owner(
                dust3d_sourced_mesh_load(archive.data(), archive.size(), &options, &error), dust3d_sourced_mesh_destroy);
            if (error.status != DUST3D_STATUS_OK)
                return copyNativeFailure(error);
            if (!owner)
                return copyFailure(FailureStatus::generationFailed, "Native source success returned no owner.");
            auto result = copyNativeSourcedMesh(owner.get(), limits);
            owner.reset();
            return result;
        } catch (const SourceHintPayloadError& cause) {
            return copyFailure(cause.code() == SourceHintPayloadErrorCode::limitExceeded ? FailureStatus::limitExceeded : FailureStatus::generationFailed, cause.what());
        } catch (const MeshPayloadError& cause) {
            return copyFailure(cause.code() == MeshPayloadErrorCode::limitExceeded ? FailureStatus::limitExceeded : FailureStatus::generationFailed, cause.what());
        } catch (const SourcedMeshPayloadError& cause) {
            return copyFailure(cause.code() == SourcedMeshPayloadErrorCode::limitExceeded ? FailureStatus::limitExceeded : FailureStatus::generationFailed, cause.what());
        } catch (const std::bad_alloc& cause) {
            return copyFailure(FailureStatus::limitExceeded, cause.what());
        } catch (const std::exception& cause) {
            return copyFailure(FailureStatus::generationFailed, cause.what());
        } catch (...) {
            return copyFailure(FailureStatus::generationFailed, "Native source operation threw without a diagnostic.");
        }
    }
    Frame generateSourcedMeshReply(const Frame& request, const SourcedMeshPayloadLimits& limits)
    {
        (void)encodeHeader(request.header, {});
        if (request.header.operation != Operation::generateSourcedMesh)
            throw ProtocolError(ProtocolErrorCode::unsupportedOperation, "Source operation requires its source opcode.");
        if (request.header.kind != FrameKind::request)
            throw ProtocolError(ProtocolErrorCode::unexpectedResponse, "Source operation requires a request frame.");
        if (request.header.payloadBytes != request.payload.size())
            throw ProtocolError(ProtocolErrorCode::invalidInput, "Source request length does not match its header.");
        validateSourcedMeshPayloadLimits(limits);
        const auto decoded = decodeSourceRequest(request.payload.data(), request.payload.size(), {});
        auto policy = limits;
        const auto requested = decoded.maximumSourceBytes ? decoded.maximumSourceBytes : DUST3D_DEFAULT_SOURCE_PAYLOAD_BYTES;
        policy.sources.maximumDecodedBytes = std::min(policy.sources.maximumDecodedBytes, requested);
        auto generated = generateSourcedMesh(decoded.archive, policy);
        auto header = request.header;
        std::vector<uint8_t> bytes;
        if (const auto* pair = std::get_if<SourcedMeshPayload>(&generated)) {
            header.kind = FrameKind::response;
            bytes = encodeSourcedMeshPayload(*pair, policy);
        } else {
            header.kind = FrameKind::failure;
            bytes = encodeFailurePayload(std::get<FailurePayload>(generated), {});
        }
        header.payloadBytes = bytes.size();
        return { header, std::move(bytes) };
    }
}
}
