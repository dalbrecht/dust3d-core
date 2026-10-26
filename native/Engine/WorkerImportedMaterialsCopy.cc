// SPDX-License-Identifier: MIT
#include "WorkerImportedMaterialsCopy.h"
#include "WorkerImportedMaterialReader.h"
#include <algorithm>
namespace dust3d {
namespace worker {
    namespace {
        static_assert(sizeof(size_t) <= sizeof(uint64_t), "Native count conversions require the supported address space.");
        [[noreturn]] void reject(ImportedMaterialsErrorCode code, const char* cause) { throw ImportedMaterialsError(code, cause); }
        std::string_view readIdentity(const char* bytes, ImportedMaterialsErrorCode code)
        {
            if (!bytes)
                reject(code, "Native imported fact text storage is missing.");
            size_t count = 0;
            while (count <= 38 && bytes[count])
                ++count;
            if (count > 38)
                reject(code, "Native imported fact text exceeds its bounded readable length.");
            return { bytes, count };
        }
        void chargeMinimum(uint64_t& total, uint64_t count, uint64_t unit, uint64_t maximum)
        {
            if (total > maximum || count > (maximum - total) / unit)
                reject(ImportedMaterialsErrorCode::invalidLayout, "Native imported material counts exceed reported retained storage.");
            total += count * unit;
        }
        class NativeReader final : public detail::ImportedMaterialReader {
        public:
            NativeReader(const Dust3DImportedAssets* owner, const ImportedMaterialsLimits& limits)
                : m_owner(owner)
                , m_materials(dust3d_imported_assets_model_count(owner))
                , m_bindings(dust3d_imported_assets_binding_count(owner))
            {
                detail::validateImportedMaterialsCounts(m_materials, m_bindings, limits);
                const auto payload = dust3d_imported_assets_payload_bytes(owner);
                if (!payload || payload > DUST3D_MAX_IMPORTED_ASSET_PAYLOAD_BYTES)
                    reject(ImportedMaterialsErrorCode::invalidLayout, "Native imported material retained payload is outside supported bounds.");
                uint64_t minimum = 0;
                chargeMinimum(minimum, m_materials, sizeof(Dust3DImportedMaterialView), payload);
                chargeMinimum(minimum, m_bindings, sizeof(Dust3DImportedBindingView), payload);
            }
            uint64_t materialCount() const noexcept override { return m_materials; }
            uint64_t bindingCount() const noexcept override { return m_bindings; }
            void rewind() noexcept override { m_material = m_binding = 0; }
            detail::ImportedMaterialView readMaterial() override
            {
                const auto raw = dust3d_imported_assets_material(m_owner, m_material++);
                detail::ImportedMaterialView value;
                value.modelId = readIdentity(raw.model_id, ImportedMaterialsErrorCode::invalidIdentity);
                std::copy(raw.base_color, raw.base_color + 4, value.baseColor.begin());
                value.metallic = raw.metallic;
                value.roughness = raw.roughness;
                std::copy(raw.emissive, raw.emissive + 3, value.emissive.begin());
                value.alphaMode = static_cast<uint32_t>(raw.alpha_mode);
                value.alphaCutoff = raw.alpha_cutoff;
                value.isDoubleSided = raw.is_double_sided;
                value.hasSampler = raw.has_sampler;
                value.hasMagnification = raw.has_magnification_filter;
                value.hasMinification = raw.has_minification_filter;
                value.magnification = raw.magnification_filter;
                value.minification = raw.minification_filter;
                value.wrapS = raw.wrap_s;
                value.wrapT = raw.wrap_t;
                value.imageData = raw.image.bytes;
                value.imageSize = raw.image.byte_count;
                if (raw.image.mime_type) {
                    value.hasImage = 1;
                    const auto mime = readIdentity(raw.image.mime_type, ImportedMaterialsErrorCode::invalidImage);
                    if (mime == "image/png")
                        value.imageEncoding = 1;
                    else if (mime == "image/jpeg")
                        value.imageEncoding = 2;
                    else
                        reject(ImportedMaterialsErrorCode::invalidImage, "Native imported image declaration is neither PNG nor JPEG.");
                }
                return value;
            }
            detail::ImportedBindingView readBinding() override
            {
                const auto raw = dust3d_imported_assets_binding(m_owner, m_binding++);
                const auto part = readIdentity(raw.part_id, ImportedMaterialsErrorCode::invalidIdentity);
                return { raw.model_index, part, raw.component_id ? std::optional<std::string_view>(readIdentity(raw.component_id, ImportedMaterialsErrorCode::invalidIdentity)) : std::nullopt };
            }
            void finish(const detail::ImportedMaterialsMeasurement&) const override { }

        private:
            const Dust3DImportedAssets* m_owner;
            uint64_t m_materials, m_bindings;
            size_t m_material = 0, m_binding = 0;
        };
    }
    ImportedMaterialsPayload copyNativeImportedMaterials(const Dust3DImportedAssets* owner, const ImportedMaterialsLimits& limits)
    {
        validateImportedMaterialsLimits(limits);
        if (!owner)
            reject(ImportedMaterialsErrorCode::invalidInput, "Native imported material owner is missing.");
        NativeReader reader(owner, limits);
        return detail::copyImportedMaterialReader(reader, limits);
    }
}
}
