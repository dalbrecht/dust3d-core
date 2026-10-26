// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_WORKER_IMPORTED_RENDER_H_
#define DUST3D_NATIVE_WORKER_IMPORTED_RENDER_H_
#include "WorkerImportedMaterials.h"
#include "WorkerImportedSurface.h"
namespace dust3d {
namespace worker {
    constexpr size_t importedRenderPrefixBytes = 40;
    constexpr size_t importedRenderImageBytes = 16;
    /// Source scanline order, tightly packed straight RGBA8, without material factors.
    struct ImportedRenderImage {
        uint32_t width = 0, height = 0;
        std::vector<uint8_t> pixels;
    };
    /// Definitions retain encoded bytes; images align exactly with surface model order.
    /// Bindings must be empty: this is a render catalog, not the full archive fact table.
    /// Correspondence does not prove provenance; producer must use one unchanged archive.
    struct ImportedRenderPayload {
        ImportedSurfacePayload surface;
        ImportedMaterialsPayload materials;
        std::vector<std::optional<ImportedRenderImage>> images;
    };
    struct ImportedRenderLimits {
        uint64_t maximumWireBytes = maximumPayloadBytes;
        uint64_t maximumDecodedBytes = 128ULL * 1024 * 1024;
        uint64_t maximumDecodedPixelBytes = 64ULL * 1024 * 1024;
        ImportedSurfaceLimits surface;
        ImportedMaterialsLimits materials;
    };
    struct ImportedRenderInspection {
        uint64_t materialCount, pixelBytes, logicalBytes;
    };
    enum class ImportedRenderErrorCode { invalidInput,
        invalidLimits,
        unsupportedVersion,
        invalidLayout,
        truncated,
        trailingData,
        countMismatch,
        invalidImage,
        limitExceeded };
    class ImportedRenderError : public std::runtime_error {
    public:
        ImportedRenderError(ImportedRenderErrorCode code, const char* cause);
        ImportedRenderErrorCode code() const noexcept { return m_code; }

    private:
        ImportedRenderErrorCode m_code;
    };
    /// Validate independent positive supported policies; throws original typed errors.
    /// Example: validateImportedRenderLimits({});
    void validateImportedRenderLimits(const ImportedRenderLimits& limits);
    /// Encode stable owned render values after complete identity/presence/quota validation.
    /// Returns independent explicit-endian bytes; throws ImportedRenderError, original
    /// inner typed errors or allocation errors. Does not decode encoded image content.
    /// Example: auto wire = encodeImportedRenderPayload(value, {});
    std::vector<uint8_t> encodeImportedRenderPayload(const ImportedRenderPayload& value, const ImportedRenderLimits& limits);
    /// Inspect an exact stable readable span before retained decoded arrays. Returns logical
    /// counts/charges excluding roots/capacities/workspace/RSS; callbacks borrow keys only.
    /// Throws original typed errors or workspace allocation errors.
    /// Example: auto shape = inspectImportedRenderPayload(wire.data(), wire.size(), {});
    ImportedRenderInspection inspectImportedRenderPayload(const uint8_t* bytes, size_t size, const ImportedRenderLimits& limits);
    /// Return independent storage after full inspection; original span may then be released.
    /// Throws original typed errors or allocation errors. Example: auto value = decodeImportedRenderPayload(wire.data(), wire.size(), {});
    ImportedRenderPayload decodeImportedRenderPayload(const uint8_t* bytes, size_t size, const ImportedRenderLimits& limits);
}
}
#endif
