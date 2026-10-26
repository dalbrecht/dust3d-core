// SPDX-License-Identifier: MIT
#include "WorkerImportedRender.h"
#include "WorkerImportedCatalogViews.h"
#include "WorkerImportedRenderValidation.h"
#include "WorkerWire.h"
#include <algorithm>
namespace dust3d {
namespace worker {
    namespace {
        [[noreturn]] void reject(ImportedRenderErrorCode code, const char* cause) { throw ImportedRenderError(code, cause); }
        void charge(uint64_t& total, uint64_t count, uint64_t width, uint64_t maximum)
        {
            if (total > maximum || count > (maximum - total) / width)
                reject(ImportedRenderErrorCode::limitExceeded, "Imported render output exceeds its configured quota.");
            total += count * width;
        }
        void image(uint64_t width, uint64_t height, uint64_t bytes, bool present)
        {
            if (!present) {
                if (width || height || bytes)
                    reject(ImportedRenderErrorCode::invalidImage, "Absent imported image has nonempty storage.");
                return;
            }
            if (!width || !height || width > maximumPayloadBytes / 4 || height > maximumPayloadBytes / (width * 4) || bytes != width * height * 4)
                reject(ImportedRenderErrorCode::invalidImage, "Imported image is not bounded tightly packed RGBA8.");
        }
        struct CatalogEntry {
            std::string_view id;
            bool hasImage = false;
        };
        struct Description {
            size_t surface, materials, images;
            std::vector<CatalogEntry> catalog;
            ImportedRenderInspection summary;
        };
        Description describe(const uint8_t* bytes, size_t size, const ImportedRenderLimits& limits)
        {
            validateImportedRenderLimits(limits);
            if (!bytes)
                reject(ImportedRenderErrorCode::invalidInput, "Imported render bytes are missing.");
            if (size < importedRenderPrefixBytes)
                reject(ImportedRenderErrorCode::truncated, "Imported render prefix is incomplete.");
            if (detail::readUnsigned(bytes, 2) != 1)
                reject(ImportedRenderErrorCode::unsupportedVersion, "Imported render version is unsupported.");
            if (detail::readUnsigned(bytes + 2, 2) != importedRenderPrefixBytes || detail::readUnsigned(bytes + 4, 4))
                reject(ImportedRenderErrorCode::invalidLayout, "Imported render prefix/reserved fields are invalid.");
            const auto surface = detail::readUnsigned(bytes + 8, 8), materials = detail::readUnsigned(bytes + 16, 8);
            const auto count = detail::readUnsigned(bytes + 24, 8), pixels = detail::readUnsigned(bytes + 32, 8);
            uint64_t total = importedRenderPrefixBytes;
            charge(total, surface, 1, limits.maximumWireBytes);
            charge(total, materials, 1, limits.maximumWireBytes);
            const auto images = total;
            charge(total, count, importedRenderImageBytes, limits.maximumWireBytes);
            charge(total, pixels, 1, limits.maximumWireBytes);
            if (surface < importedSurfacePrefixBytes || materials < importedMaterialsPrefixBytes)
                reject(ImportedRenderErrorCode::invalidLayout, "Imported render inner spans lack complete prefixes.");
            if (size < total)
                reject(ImportedRenderErrorCode::truncated, "Imported render body is incomplete.");
            if (size > total)
                reject(ImportedRenderErrorCode::trailingData, "Imported render has trailing bytes.");
            if (pixels > limits.maximumDecodedPixelBytes)
                reject(ImportedRenderErrorCode::limitExceeded, "Imported render decoded pixels exceed their aggregate quota.");
            const auto* surfaceBytes = bytes + importedRenderPrefixBytes;
            const auto* materialBytes = surfaceBytes + surface;
            const auto source = inspectImportedSurfacePayload(surfaceBytes, static_cast<size_t>(surface), limits.surface);
            const auto facts = inspectImportedMaterialsPayload(materialBytes, static_cast<size_t>(materials), limits.materials);
            if (facts.materialCount != count || facts.bindingCount)
                reject(ImportedRenderErrorCode::countMismatch, "Imported render material/image count or binding table is inconsistent.");
            uint64_t logical = 0;
            charge(logical, source.logicalBytes, 1, limits.maximumDecodedBytes);
            charge(logical, facts.logicalBytes, 1, limits.maximumDecodedBytes);
            charge(logical, count, sizeof(std::optional<ImportedRenderImage>), limits.maximumDecodedBytes);
            charge(logical, pixels, 1, limits.maximumDecodedBytes);
            std::vector<CatalogEntry> catalog;
            detail::visitImportedSurfaceModels(surfaceBytes, static_cast<size_t>(surface), limits.surface, [&](uint64_t index, std::string_view id) {
                if (index >= count)
                    reject(ImportedRenderErrorCode::countMismatch, "Imported render surface has extra model identities.");
                catalog.push_back({ id, false });
            });
            if (catalog.size() != count)
                reject(ImportedRenderErrorCode::countMismatch, "Imported render surface lacks matching model identities.");
            detail::visitImportedMaterialModels(materialBytes, static_cast<size_t>(materials), limits.materials, [&](uint64_t index, std::string_view id, bool present) {
                if (catalog[static_cast<size_t>(index)].id != id)
                    reject(ImportedRenderErrorCode::countMismatch, "Imported render material order/identity does not match the surface.");
                catalog[static_cast<size_t>(index)].hasImage = present;
            });
            size_t offset = static_cast<size_t>(images);
            uint64_t observed = 0;
            for (const auto& entry : catalog) {
                if (size - offset < importedRenderImageBytes)
                    reject(ImportedRenderErrorCode::truncated, "Imported render image prefix is incomplete.");
                const auto width = detail::readUnsigned(bytes + offset, 4), height = detail::readUnsigned(bytes + offset + 4, 4), length = detail::readUnsigned(bytes + offset + 8, 8);
                offset += importedRenderImageBytes;
                image(width, height, length, entry.hasImage);
                if (length > size - offset)
                    reject(ImportedRenderErrorCode::truncated, "Imported render pixel span is incomplete.");
                charge(observed, length, 1, limits.maximumDecodedPixelBytes);
                offset += static_cast<size_t>(length);
            }
            // Exact outer size minus fixed prefixes makes full cursor consumption
            // equivalent to the declared pixel sum; do not duplicate that invariant.
            if (offset != size)
                reject(ImportedRenderErrorCode::invalidLayout, "Imported render image framing/aggregate is inconsistent.");
            return { static_cast<size_t>(surface), static_cast<size_t>(materials), static_cast<size_t>(images), std::move(catalog), { count, pixels, logical } };
        }
    }
    namespace detail {
        void validateImportedRenderImage(uint64_t width, uint64_t height, uint64_t bytes, bool present) { image(width, height, bytes, present); }
    }
    ImportedRenderError::ImportedRenderError(ImportedRenderErrorCode code, const char* cause)
        : std::runtime_error(cause)
        , m_code(code)
    {
    }
    void validateImportedRenderLimits(const ImportedRenderLimits& limits)
    {
        if (!limits.maximumWireBytes || limits.maximumWireBytes > maximumPayloadBytes
            || !limits.maximumDecodedBytes || limits.maximumDecodedBytes > maximumPayloadBytes
            || !limits.maximumDecodedPixelBytes || limits.maximumDecodedPixelBytes > maximumPayloadBytes)
            reject(ImportedRenderErrorCode::invalidLimits, "Imported render policy is outside supported positive bounds.");
        validateImportedSurfaceLimits(limits.surface);
        validateImportedMaterialsLimits(limits.materials);
    }
    std::vector<uint8_t> encodeImportedRenderPayload(const ImportedRenderPayload& value, const ImportedRenderLimits& limits)
    {
        validateImportedRenderLimits(limits);
        const auto count = value.surface.models.size();
        if (value.materials.materials.size() != count || value.images.size() != count || !value.materials.bindings.empty())
            reject(ImportedRenderErrorCode::countMismatch, "Imported render catalogs/images are not aligned or bindings are nonempty.");
        uint64_t pixels = 0;
        for (size_t index = 0; index < count; ++index) {
            if (value.materials.materials[index].modelId != value.surface.models[index])
                reject(ImportedRenderErrorCode::countMismatch, "Imported render material identity/order is inconsistent.");
            const auto& decoded = value.images[index];
            if (decoded.has_value() != value.materials.materials[index].image.has_value())
                reject(ImportedRenderErrorCode::invalidImage, "Imported render encoded/decoded image presence is inconsistent.");
            image(decoded ? decoded->width : 0, decoded ? decoded->height : 0, decoded ? decoded->pixels.size() : 0, decoded.has_value());
            if (decoded)
                charge(pixels, decoded->pixels.size(), 1, limits.maximumDecodedPixelBytes);
        }
        const auto surface = encodeImportedSurfacePayload(value.surface, limits.surface);
        const auto materials = encodeImportedMaterialsPayload(value.materials, limits.materials);
        uint64_t logical = 0;
        charge(logical, inspectImportedSurfacePayload(surface.data(), surface.size(), limits.surface).logicalBytes, 1, limits.maximumDecodedBytes);
        charge(logical, inspectImportedMaterialsPayload(materials.data(), materials.size(), limits.materials).logicalBytes, 1, limits.maximumDecodedBytes);
        charge(logical, count, sizeof(std::optional<ImportedRenderImage>), limits.maximumDecodedBytes);
        charge(logical, pixels, 1, limits.maximumDecodedBytes);
        uint64_t size = importedRenderPrefixBytes;
        charge(size, surface.size(), 1, limits.maximumWireBytes);
        charge(size, materials.size(), 1, limits.maximumWireBytes);
        charge(size, count, importedRenderImageBytes, limits.maximumWireBytes);
        charge(size, pixels, 1, limits.maximumWireBytes);
        std::vector<uint8_t> bytes(static_cast<size_t>(size), 0);
        detail::writeUnsigned(bytes.data(), 1, 2);
        detail::writeUnsigned(bytes.data() + 2, importedRenderPrefixBytes, 2);
        detail::writeUnsigned(bytes.data() + 8, surface.size(), 8);
        detail::writeUnsigned(bytes.data() + 16, materials.size(), 8);
        detail::writeUnsigned(bytes.data() + 24, count, 8);
        detail::writeUnsigned(bytes.data() + 32, pixels, 8);
        auto output = std::copy(surface.begin(), surface.end(), bytes.begin() + importedRenderPrefixBytes);
        output = std::copy(materials.begin(), materials.end(), output);
        size_t offset = static_cast<size_t>(output - bytes.begin());
        for (const auto& decoded : value.images) {
            if (decoded) {
                detail::writeUnsigned(bytes.data() + offset, decoded->width, 4);
                detail::writeUnsigned(bytes.data() + offset + 4, decoded->height, 4);
                detail::writeUnsigned(bytes.data() + offset + 8, decoded->pixels.size(), 8);
                std::copy(decoded->pixels.begin(), decoded->pixels.end(), bytes.begin() + offset + importedRenderImageBytes);
            }
            offset += importedRenderImageBytes + (decoded ? decoded->pixels.size() : 0);
        }
        return bytes;
    }
    ImportedRenderInspection inspectImportedRenderPayload(const uint8_t* bytes, size_t size, const ImportedRenderLimits& limits) { return describe(bytes, size, limits).summary; }
    ImportedRenderPayload decodeImportedRenderPayload(const uint8_t* bytes, size_t size, const ImportedRenderLimits& limits)
    {
        const auto shape = describe(bytes, size, limits);
        ImportedRenderPayload value;
        value.surface = decodeImportedSurfacePayload(bytes + importedRenderPrefixBytes, shape.surface, limits.surface);
        value.materials = decodeImportedMaterialsPayload(bytes + importedRenderPrefixBytes + shape.surface, shape.materials, limits.materials);
        value.images.reserve(shape.catalog.size());
        size_t offset = shape.images;
        for (const auto& entry : shape.catalog) {
            const auto width = static_cast<uint32_t>(detail::readUnsigned(bytes + offset, 4)), height = static_cast<uint32_t>(detail::readUnsigned(bytes + offset + 4, 4));
            const auto length = static_cast<size_t>(detail::readUnsigned(bytes + offset + 8, 8));
            offset += importedRenderImageBytes;
            if (entry.hasImage)
                value.images.push_back(ImportedRenderImage { width, height, { bytes + offset, bytes + offset + length } });
            else
                value.images.push_back(std::nullopt);
            offset += length;
        }
        return value;
    }
}
}
