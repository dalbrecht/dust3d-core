// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_WORKER_SOURCE_BAKED_H_
#define DUST3D_NATIVE_WORKER_SOURCE_BAKED_H_
#include "WorkerBakedTextures.h"
#include "WorkerMesh.h"
#include <limits>
namespace dust3d {
namespace worker {
    constexpr size_t sourceBakedPrefixBytes = 32;
    constexpr size_t bakedSurfacePrefixBytes = 40;
    constexpr size_t bakedSurfaceTriangleBytes = 96;
    constexpr uint64_t noBakedIdentity = std::numeric_limits<uint64_t>::max();
    /// One final expanded triangle's packed mapping, tangent and optional rendering policy.
    /// Unknown policy leaves alpha fields opaque; unknown mapping requires zero UVs.
    struct BakedSurfaceTriangle {
        std::array<std::array<double, 2>, 3> uv {};
        std::array<float, 3> tangent {};
        float handedness = 1;
        double alphaCutoff = 0;
        uint64_t modelIndex = noBakedIdentity, componentIndex = noBakedIdentity;
        uint8_t uvKnown = 0, policyKnown = 0, isDoubleSided = 0, tangentKnown = 0;
        uint32_t alphaMode = 0;
    };
    struct BakedSurfaceComponent {
        std::string id;
        uint32_t kind = 0; // Original 1 or generated 2; membership does not prove unique provenance.
    };
    struct BakedSurfacePayload {
        std::vector<BakedSurfaceTriangle> triangles;
        std::vector<std::string> models;
        std::vector<BakedSurfaceComponent> components;
    };
    /// One coherent source-baked owner; this is not legacy chart or authored sampler metadata.
    struct SourceBakedPayload {
        MeshPayload geometry;
        BakedSurfacePayload surface;
        BakedTexturesPayload textures;
    };
    struct BakedSurfaceLimits {
        uint64_t maximumWireBytes = maximumPayloadBytes;
        uint64_t maximumDecodedBytes = 128ULL * 1024 * 1024;
        uint64_t maximumTriangles = 1000000;
        uint64_t maximumModels = 100000, maximumComponents = 100000;
        uint64_t maximumTextBytes = 8ULL * 1024 * 1024;
    };
    /// Aggregate logical charge includes inner records/text/pixels, excluding roots,
    /// capacity and bounded validation sets/wire/native workspace/RSS (Apple #280).
    struct SourceBakedLimits {
        uint64_t maximumWireBytes = maximumPayloadBytes;
        uint64_t maximumDecodedBytes = 128ULL * 1024 * 1024;
        MeshPayloadLimits geometry;
        BakedSurfaceLimits surface;
        BakedTexturesLimits textures;
    };
    struct SourceBakedInspection {
        uint64_t vertexCount, triangleCount, pixelBytes, logicalBytes;
    };
    enum class SourceBakedErrorCode { invalidInput,
        invalidLimits,
        unsupportedVersion,
        invalidLayout,
        truncated,
        trailingData,
        limitExceeded,
        countMismatch,
        invalidMetadata,
        invalidIdentity };
    class SourceBakedError : public std::runtime_error {
    public:
        SourceBakedError(SourceBakedErrorCode code, const char* cause);
        SourceBakedErrorCode code() const noexcept { return m_code; }

    private:
        SourceBakedErrorCode m_code;
    };
    /// Validate independent positive supported policies, with original inner limit errors.
    /// Throws SourceBakedError or original inner errors. Example: validateSourceBakedLimits({});
    void validateSourceBakedLimits(const SourceBakedLimits& limits);
    /// Encode a matching geometry/surface/texture tuple with explicit-endian nested schemas.
    /// @param value Stable borrowed complete rendering values; bits/keys/order are retained.
    /// @param limits Independent inner and aggregate quotas excluding transient workspace/RSS.
    /// @return Independent complete wire bytes after full semantic/count/quota validation.
    /// @throws SourceBakedError, original MeshPayloadError/BakedTexturesError or allocation errors.
    /// Example: auto wire = encodeSourceBakedPayload(value, {});
    std::vector<uint8_t> encodeSourceBakedPayload(const SourceBakedPayload& value, const SourceBakedLimits& limits);
    /// Validate every inner span, correspondence, record/key and aggregate before decoded arrays.
    /// @param bytes Stable borrowed exact readable span; no native engine executes.
    /// @param size Actual readable length, without trailing bytes.
    /// @param limits Independent quotas; bounded duplicate-key validation sets are workspace.
    /// @return Matching counts and aggregate logical charge; no retained output arrays.
    /// @throws SourceBakedError, original inner errors or validation-workspace allocation errors.
    /// Example: auto shape = inspectSourceBakedPayload(wire.data(), wire.size(), {});
    SourceBakedInspection inspectSourceBakedPayload(const uint8_t* bytes, size_t size, const SourceBakedLimits& limits);
    /// Copy independent arrays, keys and pixels after complete aggregate inspection succeeds.
    /// @param bytes Stable borrowed complete wire span; releasable after return.
    /// @param size Exact readable byte count.
    /// @param limits Supported quotas, excluding transient workspace/RSS.
    /// @return One owned coherent source-baked rendering value, without inferred provenance.
    /// @throws SourceBakedError, original inner errors or allocation errors; no partial result.
    /// Example: auto value = decodeSourceBakedPayload(wire.data(), wire.size(), {});
    SourceBakedPayload decodeSourceBakedPayload(const uint8_t* bytes, size_t size, const SourceBakedLimits& limits);
}
}
#endif
