// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_WORKER_BAKED_TEXTURES_H_
#define DUST3D_NATIVE_WORKER_BAKED_TEXTURES_H_
#include "WorkerProtocol.h"
namespace dust3d {
namespace worker {
    constexpr size_t bakedTexturesPrefixBytes = 64;
    constexpr size_t bakedTextureRoleCount = 5;
    constexpr size_t maximumTextureLevels = 14;
    /// Tight, source-encoded straight RGBA8 pixels; no orientation/color/alpha conversion.
    struct TextureLevel {
        uint32_t width = 0, height = 0;
        std::vector<uint8_t> pixels;
    };
    /// Roles: color, normal, metallic, roughness, emissive. Color is mandatory.
    /// Each present chain includes its base. The producer's chart-safe cap is never inferred.
    struct BakedTexturesPayload {
        std::array<std::vector<TextureLevel>, bakedTextureRoleCount> maps;
        double maximumSamplingLod = 0;
    };
    /// Logical charge: retained TextureLevel records plus pixel bytes. Root objects,
    /// vector capacity, codec/native workspace and RSS are excluded. Prototype bounds
    /// do not establish current editor supported-range parity (Apple #280).
    struct BakedTexturesLimits {
        uint64_t maximumWireBytes = maximumPayloadBytes;
        uint64_t maximumDecodedBytes = 128ULL * 1024 * 1024;
        uint64_t maximumPixelBytes = 128ULL * 1024 * 1024;
    };
    struct BakedTexturesInspection {
        uint64_t levelCount, pixelBytes, logicalBytes;
    };
    enum class BakedTexturesErrorCode { invalidInput,
        invalidLimits,
        invalidImage,
        invalidChain,
        invalidSamplingLod,
        unsupportedVersion,
        invalidLayout,
        truncated,
        trailingData,
        limitExceeded };
    class BakedTexturesError : public std::runtime_error {
    public:
        BakedTexturesError(BakedTexturesErrorCode code, const char* cause);
        BakedTexturesErrorCode code() const noexcept { return m_code; }

    private:
        BakedTexturesErrorCode m_code;
    };
    /// Require positive supported wire, logical and pixel bounds; no generation/allocation.
    /// @throws BakedTexturesError(invalidLimits). Example: validateBakedTexturesLimits({});
    void validateBakedTexturesLimits(const BakedTexturesLimits& limits);
    /// Validate all levels/quotas before allocating complete explicit-endian version-one bytes.
    /// @param value Stable borrowed five-role owned value; exact pixel and LOD bits retained.
    /// @param limits Independent supported bounds; no native engine/image decoder is called.
    /// @return Independent wire bytes, with role and level ordering preserved.
    /// @throws BakedTexturesError or allocation errors; no partial output.
    /// Example: auto wire = encodeBakedTexturesPayload(value, {});
    std::vector<uint8_t> encodeBakedTexturesPayload(const BakedTexturesPayload& value, const BakedTexturesLimits& limits);
    /// Inspect complete framing, dimensions, tight spans, chain shape and cap before decoded arrays.
    /// @param bytes Stable borrowed exact readable span for this call.
    /// @param size Actual readable count; trailing or incomplete bytes reject.
    /// @param limits Independent supported bounds; fixed stack inspection excludes decoded allocation.
    /// @return Total level, pixel and logical charges.
    /// @throws BakedTexturesError; no owned pixel/level arrays are constructed.
    /// Example: auto summary = inspectBakedTexturesPayload(wire.data(), wire.size(), {});
    BakedTexturesInspection inspectBakedTexturesPayload(const uint8_t* bytes, size_t size, const BakedTexturesLimits& limits);
    /// Copy independent pixels/levels only after complete inspection succeeds.
    /// @param bytes Stable borrowed complete version-one wire span, releasable after return.
    /// @param size Exact readable byte count.
    /// @param limits Supported quotas excluding root/capacity/workspace/RSS.
    /// @return Owned five-role values and the original chart-safe LOD.
    /// @throws BakedTexturesError or allocation errors; no partial result.
    /// Example: auto value = decodeBakedTexturesPayload(wire.data(), wire.size(), {});
    BakedTexturesPayload decodeBakedTexturesPayload(const uint8_t* bytes, size_t size, const BakedTexturesLimits& limits);
}
}
#endif
