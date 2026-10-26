// SPDX-License-Identifier: MIT
#include "WorkerLegacyBakedCopy.h"
#include "WorkerBakedTextureViews.h"
#include "WorkerMeshCopyInspection.h"
#include "WorkerOperation.h"
#include "WorkerTangentValidation.h"
#include "WorkerUvCopy.h"
namespace dust3d {
namespace worker {
    namespace {
        [[noreturn]] void reject(LegacyBakedErrorCode code, const char* cause) { throw LegacyBakedError(code, cause); }
        void charge(uint64_t& total, uint64_t count, uint64_t unit, uint64_t maximum)
        {
            if (total > maximum || count > (maximum - total) / unit)
                reject(LegacyBakedErrorCode::limitExceeded, "Native legacy bake exceeds its aggregate quota.");
            total += count * unit;
        }
        TextureTangent readNativeTangent(const Dust3DTextureTangent& value)
        {
            return { { value.direction[0], value.direction[1], value.direction[2] }, value.handedness, value.known };
        }
        detail::BakedTextureViews readNativeTextures(const Dust3DBakedMesh* owner)
        {
            detail::BakedTextureViews views;
            for (size_t role = 0; role < bakedTextureRoleCount; ++role) {
                const auto view = dust3d_baked_mesh_texture(owner, static_cast<uint32_t>(role));
                if (!view.pixels) {
                    if (view.width || view.height || view.row_stride || view.byte_count)
                        throw BakedTexturesError(BakedTexturesErrorCode::invalidImage, "Native legacy absent texture has nonempty storage.");
                    continue;
                }
                if (!view.width || view.width > 8192 || !view.height || view.height > 8192 || view.row_stride != view.width * 4)
                    throw BakedTexturesError(BakedTexturesErrorCode::invalidImage, "Native legacy texture dimensions or tight stride are invalid.");
                views.counts[role] = 1;
                views.maps[role][0] = { static_cast<uint32_t>(view.width), static_cast<uint32_t>(view.height), view.byte_count, view.pixels };
            }
            return views;
        }
    }
    LegacyBakedPayload copyNativeLegacyBakedMesh(const Dust3DBakedMesh* owner, const LegacyBakedLimits& limits)
    {
        validateLegacyBakedLimits(limits);
        if (!owner)
            reject(LegacyBakedErrorCode::invalidInput, "Native legacy baked owner is missing.");
        const auto* nativeUv = dust3d_baked_mesh_uvs(owner);
        const auto* nativeGeometry = dust3d_uv_mesh_geometry(nativeUv);
        if (!nativeUv || !nativeGeometry)
            reject(LegacyBakedErrorCode::invalidLayout, "Native legacy geometry/UV storage is missing.");
        const auto uv = detail::inspectNativeUvMetadata(nativeUv, limits.uv);
        const auto geometry = detail::inspectNativeMeshCopy(nativeGeometry, limits.geometry);
        const auto count = dust3d_baked_mesh_tangent_count(owner);
        const auto* tangents = dust3d_baked_mesh_tangents(owner);
        if (!tangents || !count || count != uv.triangles || count != geometry.vertexCount / 3)
            reject(LegacyBakedErrorCode::countMismatch, "Native legacy geometry/UV/tangent correspondence is invalid.");
        if (count > limits.maximumTangents)
            reject(LegacyBakedErrorCode::limitExceeded, "Native legacy tangent count exceeds configured limits.");
        for (size_t index = 0; index < count; ++index) {
            const auto tangent = readNativeTangent(tangents[index]);
            if (!detail::isValidTextureTangent(tangent.direction, tangent.handedness, tangent.known, dust3d_uv_mesh_triangles(nativeUv)[index].known))
                reject(LegacyBakedErrorCode::invalidTangent, "Native legacy tangent scalars, mapping or unit length are invalid.");
        }
        const auto views = readNativeTextures(owner);
        const auto maps = detail::inspectBakedTextureViews(views, limits.textures);
        const auto nativePayload = dust3d_baked_mesh_payload_bytes(owner);
        if (!nativePayload || nativePayload > maximumPayloadBytes)
            reject(LegacyBakedErrorCode::invalidLayout, "Native legacy bake payload is outside supported bounds.");
        uint64_t minimum = 0;
        charge(minimum, count, sizeof(Dust3DTextureTangent), nativePayload);
        for (size_t role = 0; role < bakedTextureRoleCount; ++role)
            if (views.counts[role])
                charge(minimum, views.maps[role][0].byteCount, 1, nativePayload);
        uint64_t wire = legacyBakedPrefixBytes, logical = 0;
        charge(wire, geometry.wire, 1, limits.maximumWireBytes);
        charge(wire, uv.wire, 1, limits.maximumWireBytes);
        charge(wire, count, textureTangentBytes, limits.maximumWireBytes);
        charge(wire, maps.wireBytes, 1, limits.maximumWireBytes);
        charge(logical, geometry.logical, 1, limits.maximumDecodedBytes);
        charge(logical, uv.logical, 1, limits.maximumDecodedBytes);
        charge(logical, count, sizeof(TextureTangent), limits.maximumDecodedBytes);
        charge(logical, maps.logicalBytes, 1, limits.maximumDecodedBytes);
        LegacyBakedPayload result;
        result.geometry = copyNativeMesh(nativeGeometry, limits.geometry);
        result.uv = detail::copyNativeUvMetadata(nativeUv, limits.uv);
        result.tangents.reserve(count);
        for (size_t index = 0; index < count; ++index)
            result.tangents.push_back(readNativeTangent(tangents[index]));
        result.textures = detail::copyBakedTextureViews(views, limits.textures);
        return result;
    }
}
}
