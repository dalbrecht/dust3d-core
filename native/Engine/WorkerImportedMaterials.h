// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_WORKER_IMPORTED_MATERIALS_H_
#define DUST3D_NATIVE_WORKER_IMPORTED_MATERIALS_H_
#include "WorkerProtocol.h"
#include <optional>
#include <string>
namespace dust3d {
namespace worker {
    constexpr size_t importedMaterialsPrefixBytes = 40, importedMaterialBytes = 128, importedBindingBytes = 24;
    constexpr uint64_t maximumImportedMaterialRecords = 1000000;
    enum class ImportedImageEncoding : uint8_t { png = 1,
        jpeg = 2 };
    /// Opaque declared encoded bytes, never validated/decoded pixels.
    struct ImportedEncodedImage {
        ImportedImageEncoding encoding = ImportedImageEncoding::png;
        std::vector<uint8_t> bytes;
    };
    struct ImportedMaterialSampler {
        std::optional<uint32_t> magnification, minification;
        uint32_t wrapS = 10497, wrapT = 10497;
    };
    struct ImportedMaterial {
        std::string modelId;
        std::array<double, 4> baseColor { 1, 1, 1, 1 };
        double metallic = 1, roughness = 1;
        std::array<double, 3> emissive {};
        double alphaCutoff = 0.5;
        uint32_t alphaMode = 0;
        uint8_t isDoubleSided = 0;
        std::optional<ImportedMaterialSampler> sampler;
        std::optional<ImportedEncodedImage> image;
    };
    struct ImportedBinding {
        uint64_t modelIndex = 0;
        std::string partId;
        std::optional<std::string> componentId;
    };
    struct ImportedMaterialsPayload {
        std::vector<ImportedMaterial> materials;
        std::vector<ImportedBinding> bindings;
    };
    /// Logical records, exact key bytes/terminators and encoded bytes; roots/capacity/workspace/RSS excluded.
    struct ImportedMaterialsLimits {
        uint64_t maximumWireBytes = maximumPayloadBytes, maximumDecodedBytes = 64ULL * 1024 * 1024;
        uint64_t maximumMaterials = 100000, maximumBindings = maximumImportedMaterialRecords;
        uint64_t maximumTextBytes = 8ULL * 1024 * 1024, maximumEncodedBytes = 64ULL * 1024 * 1024;
        uint64_t maximumImageBytes = 16ULL * 1024 * 1024;
    };
    struct ImportedMaterialsInspection {
        uint64_t materialCount, bindingCount, textBytes, encodedBytes, logicalBytes;
    };
    enum class ImportedMaterialsErrorCode { invalidInput,
        invalidLimits,
        unsupportedVersion,
        invalidLayout,
        truncated,
        trailingData,
        limitExceeded,
        invalidMaterial,
        invalidSampler,
        invalidImage,
        invalidIdentity,
        invalidBinding };
    class ImportedMaterialsError : public std::runtime_error {
    public:
        ImportedMaterialsError(ImportedMaterialsErrorCode code, const char* cause);
        ImportedMaterialsErrorCode code() const noexcept { return m_code; }

    private:
        ImportedMaterialsErrorCode m_code;
    };
    /// Validate supported positive count/text/encoded/wire/logical policies without output allocation.
    /// Throws ImportedMaterialsError(invalidLimits). Example: validateImportedMaterialsLimits({});
    void validateImportedMaterialsLimits(const ImportedMaterialsLimits& limits);
    /// Inspect owned immutable facts and all quotas without an encoded output copy.
    /// Returns counts/charges; bounded validation lookup workspace is excluded.
    /// Throws original ImportedMaterialsError or workspace allocation errors.
    /// Example: auto shape = inspectImportedMaterialsPayload(facts, {});
    ImportedMaterialsInspection inspectImportedMaterialsPayload(const ImportedMaterialsPayload& value, const ImportedMaterialsLimits& limits);
    /// Inspect all material/sampler/image declarations, keys, bindings and quotas before wire allocation.
    /// @param value Stable borrowed facts; exact primitive bits, optional presence, bytes and order retained.
    /// @param limits Supported independent policies, excluding validation sets/native workspace/RSS.
    /// @return Complete explicit-endian version-one wire bytes; no pixel decoding or material application.
    /// @throws ImportedMaterialsError or validation/output allocation errors; no partial result.
    /// Example: auto wire = encodeImportedMaterialsPayload(value, {});
    std::vector<uint8_t> encodeImportedMaterialsPayload(const ImportedMaterialsPayload& value, const ImportedMaterialsLimits& limits);
    /// Inspect exact framing and every record/reference before decoded arrays/text/encoded buffers.
    /// @param bytes Stable borrowed complete readable span for this call.
    /// @param size Actual readable length, with no trailing bytes.
    /// @param limits Independent supported quotas; bounded key lookup sets are workspace.
    /// @return Validated counts and exact logical/text/encoded charges.
    /// @throws ImportedMaterialsError or validation-workspace allocation errors; no returned arrays.
    /// Example: auto shape = inspectImportedMaterialsPayload(wire.data(), wire.size(), {});
    ImportedMaterialsInspection inspectImportedMaterialsPayload(const uint8_t* bytes, size_t size, const ImportedMaterialsLimits& limits);
    /// Copy independent values after complete inspection, preserving encoded declarations without decoding.
    /// @param bytes Stable exact readable wire span, releasable after return.
    /// @param size Actual byte count.
    /// @param limits Independent positive quotas excluding roots/capacity/workspace/RSS.
    /// @return Owned materials, bindings and encoded images, with original optional presence.
    /// @throws ImportedMaterialsError or allocation errors; no partial output.
    /// Example: auto facts = decodeImportedMaterialsPayload(wire.data(), wire.size(), {});
    ImportedMaterialsPayload decodeImportedMaterialsPayload(const uint8_t* bytes, size_t size, const ImportedMaterialsLimits& limits);
}
}
#endif
