// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_WORKER_LEGACY_BAKED_H_
#define DUST3D_NATIVE_WORKER_LEGACY_BAKED_H_
#include "WorkerBakedTextures.h"
#include "WorkerMesh.h"
#include "WorkerUv.h"
namespace dust3d {
namespace worker {
    constexpr size_t legacyBakedPrefixBytes = 48, textureTangentBytes = 24;
    struct TextureTangent {
        std::array<float, 3> direction {};
        float handedness = 1;
        uint8_t known = 0;
    };
    /// Coherent legacy procedural output; charts/gradients are not source-baked policies.
    struct LegacyBakedPayload {
        MeshPayload geometry;
        UvPayload uv;
        std::vector<TextureTangent> tangents;
        BakedTexturesPayload textures;
    };
    /// Logical records/text/pixels only: roots/capacity/validation/wire/native workspace/RSS excluded.
    struct LegacyBakedLimits {
        uint64_t maximumWireBytes = maximumPayloadBytes, maximumDecodedBytes = 128ULL * 1024 * 1024;
        uint64_t maximumTangents = maximumUvRecords;
        MeshPayloadLimits geometry;
        UvPayloadLimits uv;
        BakedTexturesLimits textures;
    };
    struct LegacyBakedInspection {
        uint64_t vertexCount, triangleCount, logicalBytes;
    };
    enum class LegacyBakedErrorCode { invalidInput,
        invalidLimits,
        unsupportedVersion,
        invalidLayout,
        truncated,
        trailingData,
        limitExceeded,
        countMismatch,
        invalidTangent,
        invalidTextures };
    class LegacyBakedError : public std::runtime_error {
    public:
        LegacyBakedError(LegacyBakedErrorCode code, const char* cause);
        LegacyBakedErrorCode code() const noexcept { return m_code; }

    private:
        LegacyBakedErrorCode m_code;
    };
    /// Require independent supported positive quotas without generation/output allocation.
    /// Throws LegacyBakedError or original inner policy errors. Example: validateLegacyBakedLimits({});
    void validateLegacyBakedLimits(const LegacyBakedLimits& limits);
    /// Encode matching legacy geometry/UV/tangent/base-texture values with explicit endian fields.
    /// @param value Stable borrowed owned values; exact ordering, keys and bits are preserved.
    /// @param limits Independent inner/aggregate policies, excluding transient workspace/RSS.
    /// @return Complete independent wire value after validation; inner wire spans are temporary workspace.
    /// @throws LegacyBakedError, original inner errors or allocation errors; no partial result.
    /// Example: auto wire = encodeLegacyBakedPayload(value, {});
    std::vector<uint8_t> encodeLegacyBakedPayload(const LegacyBakedPayload& value, const LegacyBakedLimits& limits);
    /// Inspect complete inner spans, tangent/UV correspondence and aggregate before decoded arrays.
    /// @param bytes Stable borrowed exact readable span throughout this call.
    /// @param size Actual byte count, without trailing data.
    /// @param limits Supported quotas, excluding bounded UV validation workspace.
    /// @return Matching geometry/triangle counts and logical charge; no retained output arrays.
    /// @throws LegacyBakedError, original inner errors or validation-workspace allocation errors.
    /// Example: auto shape = inspectLegacyBakedPayload(wire.data(), wire.size(), {});
    LegacyBakedInspection inspectLegacyBakedPayload(const uint8_t* bytes, size_t size, const LegacyBakedLimits& limits);
    /// Copy independent legacy values only after complete aggregate inspection.
    /// @param bytes Stable complete wire span, releasable after return.
    /// @param size Exact readable byte count.
    /// @param limits Independent supported quotas; root/capacity/workspace/RSS excluded.
    /// @return Coherent owned geometry, charts/gradients, tangents and base maps.
    /// @throws LegacyBakedError, original inner errors or allocation errors; no partial output.
    /// Example: auto value = decodeLegacyBakedPayload(wire.data(), wire.size(), {});
    LegacyBakedPayload decodeLegacyBakedPayload(const uint8_t* bytes, size_t size, const LegacyBakedLimits& limits);
}
}
#endif
