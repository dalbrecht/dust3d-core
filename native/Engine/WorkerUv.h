// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_WORKER_UV_H_
#define DUST3D_NATIVE_WORKER_UV_H_
#include "WorkerProtocol.h"
#include <limits>
#include <string>

namespace dust3d {
namespace worker {
    constexpr size_t uvPrefixBytes = 56;
    constexpr size_t uvTriangleBytes = 64;
    constexpr size_t uvChartBytes = 120;
    constexpr size_t uvGradientBytes = 80;
    /// Unknown/ambiguous associations always carry this wire-sized sentinel.
    constexpr uint64_t noUvComponent = std::numeric_limits<uint64_t>::max();
    constexpr uint64_t maximumUvRecords = 1000000;
    enum class UvAssociation : uint8_t { unknown = 0,
        component = 1,
        ambiguous = 2 };
    struct UvTriangle {
        std::array<std::array<double, 2>, 3> uv {};
        uint8_t known = 0;
        UvAssociation association = UvAssociation::unknown;
        uint64_t componentIndex = noUvComponent;
    };
    struct UvChart {
        double left = 0, top = 0, width = 0, height = 0;
        std::array<double, 4> color {};
        uint8_t flipped = 0;
        UvAssociation association = UvAssociation::unknown;
        uint64_t componentIndex = noUvComponent;
        std::string imageID;
    };
    struct UvGradient {
        uint64_t chartIndex = 0;
        std::array<double, 4> largeColor {}, smallColor {};
        uint64_t resolution = 0;
    };
    /// Owned atlas metadata only; it neither contains pixels nor proves source ownership.
    struct UvPayload {
        std::vector<UvTriangle> triangles;
        std::vector<UvChart> charts;
        std::vector<UvGradient> gradients;
        std::vector<std::string> components;
        double packedTextureSize = 0;
    };
    /// Logical quota counts records, component string objects and retained text bytes.
    /// Excludes root objects, capacity, validation workspace and process RSS.
    struct UvPayloadLimits {
        uint64_t maximumWireBytes = maximumPayloadBytes;
        uint64_t maximumDecodedBytes = 128ULL * 1024 * 1024;
        uint64_t maximumTriangles = maximumUvRecords;
        uint64_t maximumComponents = 100000;
        uint64_t maximumCharts = maximumUvRecords;
        uint64_t maximumGradients = maximumUvRecords;
        uint64_t maximumTextBytes = 4ULL * 1024 * 1024;
        uint64_t maximumKeyBytes = 4096;
    };
    enum class UvPayloadErrorCode { invalidLimits,
        invalidInput,
        unsupportedVersion,
        invalidLayout,
        truncated,
        trailingData,
        limitExceeded,
        invalidMetadata,
        invalidText };
    /// Typed metadata rejection; messages never interpolate worker/document contents.
    class UvPayloadError : public std::runtime_error {
    public:
        UvPayloadError(UvPayloadErrorCode code, const char* message);
        UvPayloadErrorCode code() const noexcept { return m_code; }

    private:
        UvPayloadErrorCode m_code;
    };
    /// Encode explicit v1 big-endian binary64 metadata after full validation.
    /// @param payload Borrowed metadata; exact double bits and component key bytes are preserved.
    /// @param expectedTriangles Required nonzero geometry triangle count; no geometry is executed.
    /// @param limits Independent positive supported wire/logical/count/text admission budgets.
    /// @return Independently owned complete metadata bytes; input remains unchanged.
    /// @throws UvPayloadError or allocation errors; no partial wire result is returned.
    /// Example: auto wire = encodeUvPayload(metadata, geometryTriangleCount, {});
    std::vector<uint8_t> encodeUvPayload(const UvPayload& payload, uint64_t expectedTriangles, const UvPayloadLimits& limits);
    /// Validate supported positive policies without inspecting metadata or executing an engine.
    /// @param limits Independent supported positive byte/count/text quotas.
    /// @throws UvPayloadError with invalidLimits; no worker/document content is logged.
    /// Example: validateUvPayloadLimits({});
    void validateUvPayloadLimits(const UvPayloadLimits& limits);

    /// Validate an exact metadata payload without constructing retained arrays/strings.
    /// @param bytes Stable borrowed complete metadata storage for this call.
    /// @param size Exact span; truncation/trailing bytes reject.
    /// @param expectedTriangles Nonzero count from the independently validated geometry.
    /// @param limits Supported wire/logical/count/text admission policy.
    /// @return Logical decoded byte charge, including original keys and image identity text.
    /// Bounded validation workspace is permitted; this is not process-memory containment.
    /// @throws The same UvPayloadError causes as decodeUvPayload.
    /// Example: auto charge = inspectUvPayload(bytes.data(), bytes.size(), triangleCount, {});
    uint64_t inspectUvPayload(const uint8_t* bytes, size_t size, uint64_t expectedTriangles, const UvPayloadLimits& limits);

    /// Validate exact spans/fields/quotas before allocating retained output; return independent values.
    /// @param bytes Borrowed complete metadata bytes, valid only for this call.
    /// @param size Exact byte span; trailing bytes reject.
    /// @param expectedTriangles Required geometry count correlation; identity/frame admission is separate.
    /// @param limits Independent wire/logical/count/text budgets; workspace/RSS are excluded.
    /// @return Independent metadata, including original key spellings; no pixels or ownership proof.
    /// @throws UvPayloadError or allocation errors; no partial output is returned.
    /// Example: auto metadata = decodeUvPayload(wire.data(), wire.size(), triangleCount, {});
    UvPayload decodeUvPayload(const uint8_t* bytes, size_t size, uint64_t expectedTriangles, const UvPayloadLimits& limits);
}
}
#endif
