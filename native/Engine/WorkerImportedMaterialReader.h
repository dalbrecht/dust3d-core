// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_WORKER_IMPORTED_MATERIAL_READER_H_
#define DUST3D_NATIVE_WORKER_IMPORTED_MATERIAL_READER_H_
#include "WorkerImportedMaterials.h"
#include <string_view>
namespace dust3d {
namespace worker {
    namespace detail {
        /// Borrowed actual readable views; scalar flags remain raw to reject inconsistent native fields.
        struct ImportedMaterialView {
            std::string_view modelId;
            std::array<double, 4> baseColor {};
            double metallic = 0, roughness = 0;
            std::array<double, 3> emissive {};
            double alphaCutoff = 0;
            uint32_t alphaMode = 0, isDoubleSided = 0;
            uint32_t hasSampler = 0, hasMagnification = 0, hasMinification = 0;
            uint32_t magnification = 0, minification = 0, wrapS = 0, wrapT = 0;
            uint32_t hasImage = 0, imageEncoding = 0;
            const uint8_t* imageData = nullptr;
            uint64_t imageSize = 0;
        };
        struct ImportedBindingView {
            uint64_t modelIndex;
            std::string_view partId;
            std::optional<std::string_view> componentId;
        };
        struct ImportedMaterialsMeasurement {
            uint64_t materialCount, bindingCount, componentCount, textBytes, encodedBytes, wireBytes, logicalBytes;
        };
        /// Internal stable-table boundary. Readers own cursors only; sources/spans remain immutable
        /// and actually readable until all inspection/copy calls return. No arbitrary pointer validation.
        class ImportedMaterialReader {
        public:
            virtual ~ImportedMaterialReader() = default;
            virtual uint64_t materialCount() const noexcept = 0;
            virtual uint64_t bindingCount() const noexcept = 0;
            virtual void rewind() noexcept = 0;
            virtual ImportedMaterialView readMaterial() = 0;
            virtual ImportedBindingView readBinding() = 0;
            virtual void finish(const ImportedMaterialsMeasurement& measured) const = 0;
        };
        /// Validate count/minimum-record quotas before a reader borrows array elements.
        /// Throws ImportedMaterialsError; no output allocation. Example: validateImportedMaterialsCounts(models, bindings, limits);
        void validateImportedMaterialsCounts(uint64_t materials, uint64_t bindings, const ImportedMaterialsLimits& limits);
        /// Rewind and validate complete stable reader semantics/quotas using bounded key workspace.
        /// @param reader Trusted adapter for immutable actually readable owned/wire/native spans.
        /// @param limits Supported independent policies.
        /// @return Exact counts/charges, with no retained output arrays.
        /// @throws ImportedMaterialsError or validation-workspace allocation errors.
        /// Example: auto shape = inspectImportedMaterialReader(reader, {});
        ImportedMaterialsMeasurement inspectImportedMaterialReader(ImportedMaterialReader& reader, const ImportedMaterialsLimits& limits);
        /// Independently inspect, rewind and copy; no caller-supplied measurement is trusted.
        /// @param reader Stable adapter/source, retained throughout this synchronous call.
        /// @param limits Supported independent policies.
        /// @return Owned facts; no borrowed span escapes.
        /// @throws ImportedMaterialsError or allocation errors. Example: auto facts = copyImportedMaterialReader(reader, {});
        ImportedMaterialsPayload copyImportedMaterialReader(ImportedMaterialReader& reader, const ImportedMaterialsLimits& limits);
    }
}
}
#endif
