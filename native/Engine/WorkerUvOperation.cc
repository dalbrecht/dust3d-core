// SPDX-License-Identifier: MIT
#include "WorkerUvOperation.h"
#include "WorkerOperation.h"
#include "WorkerUvCopyBudget.h"
#include <algorithm>
#include <memory>
#include <new>
namespace dust3d {
namespace worker {
    namespace {
        [[noreturn]] void limit()
        {
            throw UvPayloadError(UvPayloadErrorCode::limitExceeded, "Native worker UV output exceeds its copy/wire quota.");
        }
        [[noreturn]] void invalid()
        {
            throw UvPayloadError(UvPayloadErrorCode::invalidMetadata, "Native worker UV storage is invalid.");
        }
        void add(uint64_t& total, uint64_t value, uint64_t maximum)
        {
            if (value > maximum || total > maximum - value)
                limit();
            total += value;
        }
        size_t textLength(const char* text, uint64_t maximum)
        {
            if (!text)
                invalid();
            size_t length = 0;
            while (length <= maximum && text[length])
                ++length;
            if (length > maximum)
                limit();
            return length;
        }
        uint64_t componentIndex(Dust3DUvComponentKind kind, size_t index, size_t count)
        {
            if (kind == DUST3D_UV_COMPONENT && index < count)
                return index;
            if ((kind == DUST3D_UV_UNKNOWN || kind == DUST3D_UV_AMBIGUOUS) && index == SIZE_MAX)
                return noUvComponent;
            invalid();
        }
        struct Shape {
            size_t triangles, charts, gradients, components;
            uint64_t wire, logical;
        };
        Shape inspect(const Dust3DUvMesh* owner, const UvPayloadLimits& limits)
        {
            Shape shape { dust3d_uv_mesh_triangle_count(owner), dust3d_uv_mesh_chart_count(owner),
                dust3d_uv_mesh_gradient_count(owner), dust3d_uv_mesh_component_count(owner), uvPrefixBytes, 0 };
            if (!shape.triangles || shape.triangles > limits.maximumTriangles || shape.charts > limits.maximumCharts
                || shape.gradients > limits.maximumGradients || shape.components > limits.maximumComponents)
                limit();
            if (!dust3d_uv_mesh_triangles(owner) || (shape.charts && !dust3d_uv_mesh_charts(owner))
                || (shape.gradients && !dust3d_uv_mesh_gradients(owner)))
                invalid();
            add(shape.wire, shape.triangles * uvTriangleBytes, limits.maximumWireBytes);
            add(shape.wire, shape.charts * uvChartBytes, limits.maximumWireBytes);
            add(shape.wire, shape.gradients * uvGradientBytes, limits.maximumWireBytes);
            add(shape.wire, shape.components * 4, limits.maximumWireBytes);
            add(shape.logical, shape.triangles * sizeof(UvTriangle), limits.maximumDecodedBytes);
            add(shape.logical, shape.charts * sizeof(UvChart), limits.maximumDecodedBytes);
            add(shape.logical, shape.gradients * sizeof(UvGradient), limits.maximumDecodedBytes);
            add(shape.logical, shape.components * sizeof(std::string), limits.maximumDecodedBytes);
            detail::UvCopyTextBudget textBudget(limits.maximumTextBytes);
            for (size_t i = 0; i < shape.components; ++i) {
                const auto length = textLength(dust3d_uv_mesh_component_id(owner, i), limits.maximumKeyBytes);
                textBudget.charge(length);
                add(shape.wire, length, limits.maximumWireBytes);
                add(shape.logical, length, limits.maximumDecodedBytes);
            }
            for (size_t i = 0; i < shape.charts; ++i) {
                const auto length = textLength(dust3d_uv_mesh_charts(owner)[i].image_id, 38);
                textBudget.charge(length);
                add(shape.logical, length, limits.maximumDecodedBytes);
            }
            return shape;
        }
        UvPayload copy(const Dust3DUvMesh* owner, const Shape& shape)
        {
            UvPayload result;
            result.packedTextureSize = dust3d_uv_mesh_texture_size(owner);
            result.components.reserve(shape.components);
            for (size_t i = 0; i < shape.components; ++i)
                result.components.emplace_back(dust3d_uv_mesh_component_id(owner, i));
            result.triangles.reserve(shape.triangles);
            for (size_t i = 0; i < shape.triangles; ++i) {
                const auto& source = dust3d_uv_mesh_triangles(owner)[i];
                UvTriangle target;
                for (size_t corner = 0; corner < 3; ++corner)
                    std::copy(source.uv[corner], source.uv[corner] + 2, target.uv[corner].begin());
                target.known = source.known;
                target.componentIndex = componentIndex(source.component_kind, source.component_index, shape.components);
                target.association = static_cast<UvAssociation>(source.component_kind);
                result.triangles.push_back(target);
            }
            result.charts.reserve(shape.charts);
            for (size_t i = 0; i < shape.charts; ++i) {
                const auto& source = dust3d_uv_mesh_charts(owner)[i];
                UvChart target;
                target.left = source.left;
                target.top = source.top;
                target.width = source.width;
                target.height = source.height;
                std::copy(source.color, source.color + 4, target.color.begin());
                target.flipped = source.flipped;
                target.componentIndex = componentIndex(source.component_kind, source.component_index, shape.components);
                target.association = static_cast<UvAssociation>(source.component_kind);
                target.imageID = source.image_id;
                result.charts.push_back(std::move(target));
            }
            result.gradients.reserve(shape.gradients);
            for (size_t i = 0; i < shape.gradients; ++i) {
                const auto& source = dust3d_uv_mesh_gradients(owner)[i];
                UvGradient target;
                target.chartIndex = source.chart_index;
                std::copy(source.large_color, source.large_color + 4, target.largeColor.begin());
                std::copy(source.small_color, source.small_color + 4, target.smallColor.begin());
                target.resolution = source.resolution;
                result.gradients.push_back(target);
            }
            return result;
        }
        FailurePayload failure(FailureStatus status, const char* message)
        {
            size_t size = 0;
            while (size < FailurePayloadLimits {}.maximumCauseBytes && message[size])
                ++size;
            return { status, { message, message + size } };
        }
    }
    UvGenerationResult generateUvMesh(const UvRequest& request, const UvMeshPayloadLimits& limits)
    {
        validateUvRequest(request, {});
        validateUvMeshPayloadLimits(limits);
        try {
            std::vector<Dust3DUvImage> images;
            images.reserve(request.images.size());
            for (const auto& source : request.images) {
                Dust3DUvImage target {};
                std::copy(source.identity.begin(), source.identity.end(), target.image_id);
                target.width = source.width;
                target.height = source.height;
                images.push_back(target);
            }
            Dust3DUvOptions options { sizeof(options), static_cast<size_t>(request.metadataBudget), request.textureSize, images.data(), images.size() };
            Dust3DError error {};
            std::unique_ptr<Dust3DUvMesh, decltype(&dust3d_uv_mesh_destroy)> owner(
                dust3d_uv_mesh_load(request.archive.data(), request.archive.size(), &options, &error), dust3d_uv_mesh_destroy);
            if (error.status != DUST3D_STATUS_OK)
                return copyNativeFailure(error);
            if (!owner)
                return failure(FailureStatus::generationFailed, "Native UV success returned no owner.");
            const auto shape = inspect(owner.get(), limits.uv);
            const auto* geometry = dust3d_uv_mesh_geometry(owner.get());
            if (dust3d_mesh_vertex_count(geometry) / 3 != shape.triangles)
                invalid();
            if (shape.logical >= limits.maximumDecodedBytes || uvMeshPrefixBytes > limits.maximumWireBytes
                || shape.wire >= limits.maximumWireBytes - uvMeshPrefixBytes)
                limit();
            auto geometryLimits = limits.geometry;
            geometryLimits.maximumDecodedBytes = std::min(geometryLimits.maximumDecodedBytes, limits.maximumDecodedBytes - shape.logical);
            geometryLimits.maximumWireBytes = std::min(geometryLimits.maximumWireBytes, limits.maximumWireBytes - uvMeshPrefixBytes - shape.wire);
            UvMeshPayload result { copyNativeMesh(geometry, geometryLimits), copy(owner.get(), shape) };
            owner.reset();
            // Full semantic validation and exact aggregate confirmation after owner destruction.
            (void)encodeUvMeshPayload(result, limits);
            return result;
        } catch (const UvPayloadError& cause) {
            return failure(cause.code() == UvPayloadErrorCode::limitExceeded ? FailureStatus::limitExceeded : FailureStatus::generationFailed, cause.what());
        } catch (const MeshPayloadError& cause) {
            return failure(cause.code() == MeshPayloadErrorCode::limitExceeded ? FailureStatus::limitExceeded : FailureStatus::generationFailed, cause.what());
        } catch (const UvMeshPayloadError& cause) {
            return failure(cause.code() == UvMeshPayloadErrorCode::limitExceeded ? FailureStatus::limitExceeded : FailureStatus::generationFailed, cause.what());
        } catch (const std::bad_alloc& cause) {
            return failure(FailureStatus::limitExceeded, cause.what());
        } catch (const std::exception& cause) {
            return failure(FailureStatus::generationFailed, cause.what());
        } catch (...) {
            return failure(FailureStatus::generationFailed, "Native UV operation threw without a diagnostic.");
        }
    }
    Frame generateUvMeshReply(const Frame& request, const UvMeshPayloadLimits& limits)
    {
        (void)encodeHeader(request.header, {});
        if (request.header.operation != Operation::generateUvMesh)
            throw ProtocolError(ProtocolErrorCode::unsupportedOperation, "UV operation requires the UV opcode.");
        if (request.header.kind != FrameKind::request)
            throw ProtocolError(ProtocolErrorCode::unexpectedResponse, "UV operation requires a request frame.");
        if (request.header.payloadBytes != request.payload.size())
            throw ProtocolError(ProtocolErrorCode::invalidInput, "UV request length does not match its header.");
        const auto decoded = decodeUvRequest(request.payload.data(), request.payload.size(), {});
        auto generated = generateUvMesh(decoded, limits);
        auto header = request.header;
        std::vector<uint8_t> payload;
        if (std::holds_alternative<UvMeshPayload>(generated)) {
            header.kind = FrameKind::response;
            payload = encodeUvMeshPayload(std::get<UvMeshPayload>(generated), limits);
        } else {
            header.kind = FrameKind::failure;
            payload = encodeFailurePayload(std::get<FailurePayload>(generated), {});
        }
        header.payloadBytes = payload.size();
        return { header, std::move(payload) };
    }

}
}
