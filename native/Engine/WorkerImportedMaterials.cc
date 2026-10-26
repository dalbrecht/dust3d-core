// SPDX-License-Identifier: MIT
#include "WorkerImportedMaterials.h"
#include "WorkerImportedMaterialReader.h"
#include "WorkerUtf8.h"
#include "WorkerWire.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <unordered_map>
#include <unordered_set>
namespace dust3d {
namespace worker {
    namespace {
        static_assert(maximumPayloadBytes <= std::numeric_limits<size_t>::max(), "Imported fact spans must fit size_t.");
        static_assert(sizeof(double) == 8 && std::numeric_limits<double>::is_iec559, "Imported factor wire scalars require IEEE binary64.");
        [[noreturn]] void reject(ImportedMaterialsErrorCode code, const char* cause) { throw ImportedMaterialsError(code, cause); }
        void charge(uint64_t& total, uint64_t count, uint64_t unit, uint64_t maximum)
        {
            if (total > maximum || count > (maximum - total) / unit)
                reject(ImportedMaterialsErrorCode::limitExceeded, "Imported material facts exceed a configured aggregate quota.");
            total += count * unit;
        }
        struct StoragePlan {
            uint64_t wire, logical;
        };
        StoragePlan planStorage(uint64_t materials, uint64_t bindings, uint64_t components, uint64_t text, uint64_t encoded, const ImportedMaterialsLimits& limits)
        {
            validateImportedMaterialsLimits(limits);
            if (materials > limits.maximumMaterials || bindings > limits.maximumBindings || text > limits.maximumTextBytes || encoded > limits.maximumEncodedBytes)
                reject(ImportedMaterialsErrorCode::limitExceeded, "Imported material count/text/image totals exceed configured limits.");
            uint64_t wire = importedMaterialsPrefixBytes, logical = 0;
            charge(wire, materials, importedMaterialBytes, limits.maximumWireBytes);
            charge(wire, bindings, importedBindingBytes, limits.maximumWireBytes);
            charge(wire, text, 1, limits.maximumWireBytes);
            charge(wire, encoded, 1, limits.maximumWireBytes);
            charge(logical, materials, sizeof(ImportedMaterial) + 1, limits.maximumDecodedBytes);
            charge(logical, bindings, sizeof(ImportedBinding) + 1, limits.maximumDecodedBytes);
            charge(logical, components, 1, limits.maximumDecodedBytes);
            charge(logical, text, 1, limits.maximumDecodedBytes);
            charge(logical, encoded, 1, limits.maximumDecodedBytes);
            return { wire, logical };
        }
        void validateIdentity(std::string_view key)
        {
            if (key.empty() || key.size() > 38 || !detail::isIdentityUtf8(key))
                reject(ImportedMaterialsErrorCode::invalidIdentity, "Imported fact identity must be nonempty bounded non-NUL UTF8.");
        }
        bool isUnit(double value) { return std::isfinite(value) && value >= 0 && value <= 1; }
        bool isWrap(uint32_t value) { return value == 10497 || value == 33071 || value == 33648; }
        bool isMagnification(uint32_t value) { return value == 9728 || value == 9729; }
        bool isMinification(uint32_t value) { return isMagnification(value) || (value >= 9984 && value <= 9987); }
        void validateMaterial(const detail::ImportedMaterialView& value, const ImportedMaterialsLimits& limits)
        {
            for (const auto scalar : value.baseColor)
                if (!isUnit(scalar))
                    reject(ImportedMaterialsErrorCode::invalidMaterial, "Imported base color is not finite unit RGBA.");
            for (const auto scalar : value.emissive)
                if (!isUnit(scalar))
                    reject(ImportedMaterialsErrorCode::invalidMaterial, "Imported emissive color is not finite unit RGB.");
            if (!isUnit(value.metallic) || !isUnit(value.roughness) || !std::isfinite(value.alphaCutoff) || value.alphaCutoff < 0 || value.alphaMode > 2 || value.isDoubleSided > 1)
                reject(ImportedMaterialsErrorCode::invalidMaterial, "Imported material factors, alpha mode or sidedness are invalid.");
            if (value.hasSampler > 1 || value.hasMagnification > 1 || value.hasMinification > 1)
                reject(ImportedMaterialsErrorCode::invalidSampler, "Imported sampler presence flags must be zero or one.");
            if (!value.hasSampler) {
                if (value.hasMagnification || value.hasMinification || value.magnification || value.minification || value.wrapS || value.wrapT)
                    reject(ImportedMaterialsErrorCode::invalidSampler, "Absent imported sampler has nonempty filter/wrap fields.");
            } else if (!isWrap(value.wrapS) || !isWrap(value.wrapT)
                || (value.hasMagnification ? !isMagnification(value.magnification) : value.magnification != 0)
                || (value.hasMinification ? !isMinification(value.minification) : value.minification != 0))
                reject(ImportedMaterialsErrorCode::invalidSampler, "Imported filter values/presence or axis wrapping are invalid.");
            if (value.hasImage > 1 || bool(value.hasSampler) != bool(value.hasImage))
                reject(ImportedMaterialsErrorCode::invalidImage, "Imported image and sampler presence are inconsistent.");
            if (!value.hasImage) {
                if (value.imageEncoding || value.imageData || value.imageSize)
                    reject(ImportedMaterialsErrorCode::invalidImage, "Absent imported image has nonempty storage/declaration.");
            } else {
                if ((value.imageEncoding != 1 && value.imageEncoding != 2) || !value.imageData || !value.imageSize)
                    reject(ImportedMaterialsErrorCode::invalidImage, "Imported image needs PNG/JPEG declaration and readable nonempty encoded storage.");
                if (value.imageSize > limits.maximumImageBytes)
                    reject(ImportedMaterialsErrorCode::limitExceeded, "Imported encoded image exceeds its individual quota.");
            }
        }
        class OwnedReader final : public detail::ImportedMaterialReader {
        public:
            explicit OwnedReader(const ImportedMaterialsPayload& value)
                : m_value(value)
            {
            }
            uint64_t materialCount() const noexcept override { return m_value.materials.size(); }
            uint64_t bindingCount() const noexcept override { return m_value.bindings.size(); }
            void rewind() noexcept override { m_material = m_binding = 0; }
            detail::ImportedMaterialView readMaterial() override
            {
                const auto& value = m_value.materials.at(m_material++);
                detail::ImportedMaterialView result;
                result.modelId = value.modelId;
                result.baseColor = value.baseColor;
                result.metallic = value.metallic;
                result.roughness = value.roughness;
                result.emissive = value.emissive;
                result.alphaCutoff = value.alphaCutoff;
                result.alphaMode = value.alphaMode;
                result.isDoubleSided = value.isDoubleSided;
                if (value.sampler) {
                    result.hasSampler = 1;
                    result.hasMagnification = value.sampler->magnification.has_value();
                    result.hasMinification = value.sampler->minification.has_value();
                    result.magnification = value.sampler->magnification.value_or(0);
                    result.minification = value.sampler->minification.value_or(0);
                    result.wrapS = value.sampler->wrapS;
                    result.wrapT = value.sampler->wrapT;
                }
                if (value.image) {
                    result.hasImage = 1;
                    result.imageEncoding = static_cast<uint32_t>(value.image->encoding);
                    result.imageData = value.image->bytes.data();
                    result.imageSize = value.image->bytes.size();
                }
                return result;
            }
            detail::ImportedBindingView readBinding() override
            {
                const auto& value = m_value.bindings.at(m_binding++);
                return { value.modelIndex, value.partId, value.componentId ? std::optional<std::string_view>(*value.componentId) : std::nullopt };
            }
            void finish(const detail::ImportedMaterialsMeasurement&) const override { }

        private:
            const ImportedMaterialsPayload& m_value;
            size_t m_material = 0, m_binding = 0;
        };
        double readDouble(const uint8_t* bytes)
        {
            const uint64_t bits = detail::readUnsigned(bytes, 8);
            double value;
            std::memcpy(&value, &bits, sizeof(value));
            return value;
        }
        void writeDouble(uint8_t* bytes, double value)
        {
            uint64_t bits;
            std::memcpy(&bits, &value, sizeof(bits));
            detail::writeUnsigned(bytes, bits, 8);
        }
        class WireReader final : public detail::ImportedMaterialReader {
        public:
            WireReader(const uint8_t* bytes, size_t size, const ImportedMaterialsLimits& limits)
                : m_bytes(bytes)
                , m_size(size)
            {
                validateImportedMaterialsLimits(limits);
                if (!bytes)
                    reject(ImportedMaterialsErrorCode::invalidInput, "Imported material fact bytes are missing.");
                if (size < importedMaterialsPrefixBytes)
                    reject(ImportedMaterialsErrorCode::truncated, "Imported material fact prefix is incomplete.");
                if (detail::readUnsigned(bytes, 2) != 1)
                    reject(ImportedMaterialsErrorCode::unsupportedVersion, "Imported material fact version is unsupported.");
                if (detail::readUnsigned(bytes + 2, 2) != importedMaterialsPrefixBytes || detail::readUnsigned(bytes + 4, 2) != importedMaterialBytes || detail::readUnsigned(bytes + 6, 2) != importedBindingBytes)
                    reject(ImportedMaterialsErrorCode::invalidLayout, "Imported material fact prefix/record strides are invalid.");
                m_materials = detail::readUnsigned(bytes + 8, 8);
                m_bindings = detail::readUnsigned(bytes + 16, 8);
                m_text = detail::readUnsigned(bytes + 24, 8);
                m_encoded = detail::readUnsigned(bytes + 32, 8);
                const auto plan = planStorage(m_materials, m_bindings, 0, m_text, m_encoded, limits);
                if (size < plan.wire)
                    reject(ImportedMaterialsErrorCode::truncated, "Imported material fact records/storage are incomplete.");
                if (size > plan.wire)
                    reject(ImportedMaterialsErrorCode::trailingData, "Imported material facts have trailing bytes.");
            }
            uint64_t materialCount() const noexcept override { return m_materials; }
            uint64_t bindingCount() const noexcept override { return m_bindings; }
            void rewind() noexcept override { m_offset = importedMaterialsPrefixBytes; }
            detail::ImportedMaterialView readMaterial() override
            {
                const auto* bytes = take(importedMaterialBytes);
                detail::ImportedMaterialView value;
                for (size_t index = 0; index < 4; ++index)
                    value.baseColor[index] = readDouble(bytes + index * 8);
                value.metallic = readDouble(bytes + 32);
                value.roughness = readDouble(bytes + 40);
                for (size_t index = 0; index < 3; ++index)
                    value.emissive[index] = readDouble(bytes + 48 + index * 8);
                value.alphaCutoff = readDouble(bytes + 72);
                value.alphaMode = static_cast<uint32_t>(detail::readUnsigned(bytes + 80, 4));
                value.magnification = static_cast<uint32_t>(detail::readUnsigned(bytes + 84, 4));
                value.minification = static_cast<uint32_t>(detail::readUnsigned(bytes + 88, 4));
                value.wrapS = static_cast<uint32_t>(detail::readUnsigned(bytes + 92, 4));
                value.wrapT = static_cast<uint32_t>(detail::readUnsigned(bytes + 96, 4));
                const auto keyBytes = detail::readUnsigned(bytes + 100, 4);
                value.imageSize = detail::readUnsigned(bytes + 104, 8);
                const uint8_t flags = bytes[112];
                if (flags & ~uint8_t(31))
                    reject(ImportedMaterialsErrorCode::invalidLayout, "Imported material presence flags contain reserved bits.");
                value.hasSampler = flags & 1;
                value.hasMagnification = (flags >> 1) & 1;
                value.hasMinification = (flags >> 2) & 1;
                value.hasImage = (flags >> 3) & 1;
                value.isDoubleSided = (flags >> 4) & 1;
                value.imageEncoding = bytes[113];
                zeros(bytes + 114, 14);
                value.modelId = text(keyBytes);
                if (value.imageSize)
                    value.imageData = take(value.imageSize);
                return value;
            }
            detail::ImportedBindingView readBinding() override
            {
                const auto* bytes = take(importedBindingBytes);
                const auto model = detail::readUnsigned(bytes, 8);
                const auto part = detail::readUnsigned(bytes + 8, 4), component = detail::readUnsigned(bytes + 12, 4);
                const auto hasComponent = bytes[16];
                zeros(bytes + 17, 7);
                if (hasComponent > 1 || (!hasComponent && component))
                    reject(ImportedMaterialsErrorCode::invalidBinding, "Imported binding component presence/length is inconsistent.");
                const auto partId = text(part);
                std::optional<std::string_view> componentId;
                if (hasComponent)
                    componentId = text(component);
                return { model, partId, componentId };
            }
            void finish(const detail::ImportedMaterialsMeasurement& measured) const override
            {
                // Exact record framing plus text accounting already determines encoded bytes.
                if (m_offset != m_size || measured.textBytes != m_text)
                    reject(ImportedMaterialsErrorCode::invalidLayout, "Imported material fact text/image totals or exact framing are inconsistent.");
            }

        private:
            static void zeros(const uint8_t* bytes, size_t size)
            {
                for (size_t index = 0; index < size; ++index)
                    if (bytes[index])
                        reject(ImportedMaterialsErrorCode::invalidLayout, "Imported material reserved bytes must be zero.");
            }
            const uint8_t* take(uint64_t count)
            {
                if (count > m_size - m_offset)
                    reject(ImportedMaterialsErrorCode::truncated, "Imported material record/key/image span is incomplete.");
                const auto* result = m_bytes + m_offset;
                m_offset += static_cast<size_t>(count);
                return result;
            }
            std::string_view text(uint64_t count) { return { reinterpret_cast<const char*>(take(count)), static_cast<size_t>(count) }; }
            const uint8_t* m_bytes;
            size_t m_size, m_offset = importedMaterialsPrefixBytes;
            uint64_t m_materials = 0, m_bindings = 0, m_text = 0, m_encoded = 0;
        };
        ImportedMaterialsInspection summary(const detail::ImportedMaterialsMeasurement& value)
        {
            return { value.materialCount, value.bindingCount, value.textBytes, value.encodedBytes, value.logicalBytes };
        }
    }
    ImportedMaterialsError::ImportedMaterialsError(ImportedMaterialsErrorCode code, const char* cause)
        : std::runtime_error(cause)
        , m_code(code)
    {
    }
    void validateImportedMaterialsLimits(const ImportedMaterialsLimits& limits)
    {
        if (!limits.maximumWireBytes || limits.maximumWireBytes > maximumPayloadBytes || !limits.maximumDecodedBytes || limits.maximumDecodedBytes > maximumPayloadBytes
            || !limits.maximumMaterials || limits.maximumMaterials > maximumImportedMaterialRecords || !limits.maximumBindings || limits.maximumBindings > maximumImportedMaterialRecords
            || !limits.maximumTextBytes || limits.maximumTextBytes > maximumPayloadBytes || !limits.maximumEncodedBytes || limits.maximumEncodedBytes > maximumPayloadBytes
            || !limits.maximumImageBytes || limits.maximumImageBytes > maximumPayloadBytes)
            reject(ImportedMaterialsErrorCode::invalidLimits, "Imported material policies are outside supported positive bounds.");
    }
    namespace detail {
        void validateImportedMaterialsCounts(uint64_t materials, uint64_t bindings, const ImportedMaterialsLimits& limits) { (void)planStorage(materials, bindings, 0, 0, 0, limits); }
        ImportedMaterialsMeasurement inspectImportedMaterialReader(ImportedMaterialReader& reader, const ImportedMaterialsLimits& limits)
        {
            const auto materials = reader.materialCount(), bindings = reader.bindingCount();
            validateImportedMaterialsCounts(materials, bindings, limits);
            reader.rewind();
            uint64_t text = 0, encoded = 0, components = 0;
            std::unordered_set<std::string_view> modelKeys, componentKeys, instancedParts, uninstancedParts;
            std::unordered_map<std::string_view, uint64_t> partModels;
            for (uint64_t index = 0; index < materials; ++index) {
                const auto value = reader.readMaterial();
                validateIdentity(value.modelId);
                if (!modelKeys.emplace(value.modelId).second)
                    reject(ImportedMaterialsErrorCode::invalidIdentity, "Imported material identities are duplicated by exact bytes.");
                validateMaterial(value, limits);
                charge(text, value.modelId.size(), 1, limits.maximumTextBytes);
                charge(encoded, value.imageSize, 1, limits.maximumEncodedBytes);
            }
            for (uint64_t index = 0; index < bindings; ++index) {
                const auto value = reader.readBinding();
                if (value.modelIndex >= materials)
                    reject(ImportedMaterialsErrorCode::invalidBinding, "Imported binding references a missing material.");
                validateIdentity(value.partId);
                charge(text, value.partId.size(), 1, limits.maximumTextBytes);
                const auto previous = partModels.emplace(value.partId, value.modelIndex);
                if (!previous.second && previous.first->second != value.modelIndex)
                    reject(ImportedMaterialsErrorCode::invalidBinding, "One imported part binds different models.");
                if (value.componentId) {
                    validateIdentity(*value.componentId);
                    charge(text, value.componentId->size(), 1, limits.maximumTextBytes);
                    ++components;
                    if (uninstancedParts.count(value.partId) || !componentKeys.emplace(*value.componentId).second)
                        reject(ImportedMaterialsErrorCode::invalidBinding, "Imported component binding is duplicated or mixes instancing modes.");
                    instancedParts.emplace(value.partId);
                } else {
                    if (instancedParts.count(value.partId) || !uninstancedParts.emplace(value.partId).second)
                        reject(ImportedMaterialsErrorCode::invalidBinding, "Imported uninstanced binding is duplicated or mixes instancing modes.");
                }
            }
            const auto plan = planStorage(materials, bindings, components, text, encoded, limits);
            ImportedMaterialsMeasurement result { materials, bindings, components, text, encoded, plan.wire, plan.logical };
            reader.finish(result);
            return result;
        }
        ImportedMaterialsPayload copyImportedMaterialReader(ImportedMaterialReader& reader, const ImportedMaterialsLimits& limits)
        {
            const auto shape = inspectImportedMaterialReader(reader, limits);
            reader.rewind();
            ImportedMaterialsPayload result;
            result.materials.reserve(static_cast<size_t>(shape.materialCount));
            result.bindings.reserve(static_cast<size_t>(shape.bindingCount));
            for (uint64_t index = 0; index < shape.materialCount; ++index) {
                const auto raw = reader.readMaterial();
                ImportedMaterial value;
                value.modelId = std::string(raw.modelId);
                value.baseColor = raw.baseColor;
                value.metallic = raw.metallic;
                value.roughness = raw.roughness;
                value.emissive = raw.emissive;
                value.alphaMode = raw.alphaMode;
                value.alphaCutoff = raw.alphaCutoff;
                value.isDoubleSided = static_cast<uint8_t>(raw.isDoubleSided);
                if (raw.hasSampler) {
                    ImportedMaterialSampler sampler;
                    if (raw.hasMagnification)
                        sampler.magnification = raw.magnification;
                    if (raw.hasMinification)
                        sampler.minification = raw.minification;
                    sampler.wrapS = raw.wrapS;
                    sampler.wrapT = raw.wrapT;
                    value.sampler = sampler;
                }
                if (raw.hasImage)
                    value.image = ImportedEncodedImage { static_cast<ImportedImageEncoding>(raw.imageEncoding), { raw.imageData, raw.imageData + static_cast<size_t>(raw.imageSize) } };
                result.materials.push_back(std::move(value));
            }
            for (uint64_t index = 0; index < shape.bindingCount; ++index) {
                const auto raw = reader.readBinding();
                result.bindings.push_back({ raw.modelIndex, std::string(raw.partId), raw.componentId ? std::optional<std::string>(std::string(*raw.componentId)) : std::nullopt });
            }
            reader.finish(shape);
            return result;
        }
    }
    std::vector<uint8_t> encodeImportedMaterialsPayload(const ImportedMaterialsPayload& value, const ImportedMaterialsLimits& limits)
    {
        OwnedReader reader(value);
        const auto shape = detail::inspectImportedMaterialReader(reader, limits);
        reader.rewind();
        std::vector<uint8_t> bytes(static_cast<size_t>(shape.wireBytes), 0);
        detail::writeUnsigned(bytes.data(), 1, 2);
        detail::writeUnsigned(bytes.data() + 2, importedMaterialsPrefixBytes, 2);
        detail::writeUnsigned(bytes.data() + 4, importedMaterialBytes, 2);
        detail::writeUnsigned(bytes.data() + 6, importedBindingBytes, 2);
        detail::writeUnsigned(bytes.data() + 8, shape.materialCount, 8);
        detail::writeUnsigned(bytes.data() + 16, shape.bindingCount, 8);
        detail::writeUnsigned(bytes.data() + 24, shape.textBytes, 8);
        detail::writeUnsigned(bytes.data() + 32, shape.encodedBytes, 8);
        size_t offset = importedMaterialsPrefixBytes;
        auto copyText = [&](std::string_view text) { std::copy(text.begin(), text.end(), bytes.begin() + offset); offset += text.size(); };
        for (uint64_t index = 0; index < shape.materialCount; ++index) {
            const auto raw = reader.readMaterial();
            auto* output = bytes.data() + offset;
            for (size_t axis = 0; axis < 4; ++axis)
                writeDouble(output + axis * 8, raw.baseColor[axis]);
            writeDouble(output + 32, raw.metallic);
            writeDouble(output + 40, raw.roughness);
            for (size_t axis = 0; axis < 3; ++axis)
                writeDouble(output + 48 + axis * 8, raw.emissive[axis]);
            writeDouble(output + 72, raw.alphaCutoff);
            detail::writeUnsigned(output + 80, raw.alphaMode, 4);
            detail::writeUnsigned(output + 84, raw.magnification, 4);
            detail::writeUnsigned(output + 88, raw.minification, 4);
            detail::writeUnsigned(output + 92, raw.wrapS, 4);
            detail::writeUnsigned(output + 96, raw.wrapT, 4);
            detail::writeUnsigned(output + 100, raw.modelId.size(), 4);
            detail::writeUnsigned(output + 104, raw.imageSize, 8);
            output[112] = static_cast<uint8_t>(raw.hasSampler | (raw.hasMagnification << 1) | (raw.hasMinification << 2) | (raw.hasImage << 3) | (raw.isDoubleSided << 4));
            output[113] = static_cast<uint8_t>(raw.imageEncoding);
            offset += importedMaterialBytes;
            copyText(raw.modelId);
            if (raw.imageSize) {
                std::copy_n(raw.imageData, static_cast<size_t>(raw.imageSize), bytes.data() + offset);
                offset += static_cast<size_t>(raw.imageSize);
            }
        }
        for (uint64_t index = 0; index < shape.bindingCount; ++index) {
            const auto raw = reader.readBinding();
            auto* output = bytes.data() + offset;
            detail::writeUnsigned(output, raw.modelIndex, 8);
            detail::writeUnsigned(output + 8, raw.partId.size(), 4);
            detail::writeUnsigned(output + 12, raw.componentId ? raw.componentId->size() : 0, 4);
            output[16] = raw.componentId.has_value();
            offset += importedBindingBytes;
            copyText(raw.partId);
            if (raw.componentId)
                copyText(*raw.componentId);
        }
        reader.finish(shape);
        return bytes;
    }
    ImportedMaterialsInspection inspectImportedMaterialsPayload(const uint8_t* bytes, size_t size, const ImportedMaterialsLimits& limits)
    {
        WireReader reader(bytes, size, limits);
        return summary(detail::inspectImportedMaterialReader(reader, limits));
    }
    ImportedMaterialsPayload decodeImportedMaterialsPayload(const uint8_t* bytes, size_t size, const ImportedMaterialsLimits& limits)
    {
        WireReader reader(bytes, size, limits);
        return detail::copyImportedMaterialReader(reader, limits);
    }
}
}
