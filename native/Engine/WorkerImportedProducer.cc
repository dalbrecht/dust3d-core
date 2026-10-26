// SPDX-License-Identifier: MIT
#include "WorkerImportedProducer.h"
#include "WorkerImportedRenderValidation.h"
#include <algorithm>
#include <unordered_map>
namespace dust3d {
namespace worker {
    namespace {
        [[noreturn]] void reject(ImportedProducerErrorCode code, const char* cause) { throw ImportedProducerError(code, cause); }
        void charge(uint64_t& total, uint64_t count, uint64_t width, uint64_t maximum)
        {
            if (total > maximum || count > (maximum - total) / width)
                reject(ImportedProducerErrorCode::decodeLimitExceeded, "Imported preparation exceeds its retained-output quota.");
            total += count * width;
        }
    }
    ImportedProducerError::ImportedProducerError(ImportedProducerErrorCode code, const char* cause)
        : std::runtime_error(cause)
        , m_code(code)
    {
    }
    void validateImportedProducerLimits(const ImportedProducerLimits& limits)
    {
        validateImportedRenderLimits(limits.render);
        validateImportedMaterialsLimits(limits.facts);
        if (!limits.maximumEncodedImageBytes || limits.maximumEncodedImageBytes > maximumPayloadBytes)
            reject(ImportedProducerErrorCode::invalidLimits, "Imported decoder input policy is outside supported positive bounds.");
    }
    std::optional<ImportedRenderPayload> prepareImportedRender(ImportedSurfacePayload surface, ImportedMaterialsPayload facts,
        ImportedRenderImageDecoder& decoder, const ImportedProducerLimits& limits)
    {
        validateImportedProducerLimits(limits);
        // Complete facts validate before unused definitions are discarded.
        (void)encodeImportedMaterialsPayload(facts, limits.facts);
        const auto surfaceWire = encodeImportedSurfacePayload(surface, limits.render.surface);
        if (facts.materials.empty() || std::none_of(surface.triangles.begin(), surface.triangles.end(), [](const ImportedSurfaceTriangle& triangle) { return triangle.modelIndex != noImportedIdentity; }))
            return std::nullopt;
        std::unordered_map<std::string_view, size_t> definitions;
        for (size_t index = 0; index < facts.materials.size(); ++index)
            definitions.emplace(facts.materials[index].modelId, index);
        std::vector<size_t> selection;
        selection.reserve(surface.models.size());
        for (const auto& id : surface.models) {
            const auto found = definitions.find(id);
            if (found == definitions.end())
                reject(ImportedProducerErrorCode::missingMaterial, "Imported rendered model has no matching material definition.");
            selection.push_back(found->second);
        }
        // Finish all borrowed lookup work before moving identity-owning definitions.
        definitions.clear();
        ImportedRenderPayload result;
        result.surface = std::move(surface);
        result.materials.materials.reserve(selection.size());
        for (size_t index : selection)
            result.materials.materials.push_back(std::move(facts.materials[index]));
        const auto materialsWire = encodeImportedMaterialsPayload(result.materials, limits.render.materials);
        uint64_t logical = 0, wire = importedRenderPrefixBytes;
        charge(logical, inspectImportedSurfacePayload(surfaceWire.data(), surfaceWire.size(), limits.render.surface).logicalBytes, 1, limits.render.maximumDecodedBytes);
        charge(logical, inspectImportedMaterialsPayload(materialsWire.data(), materialsWire.size(), limits.render.materials).logicalBytes, 1, limits.render.maximumDecodedBytes);
        charge(logical, selection.size(), sizeof(std::optional<ImportedRenderImage>), limits.render.maximumDecodedBytes);
        charge(wire, surfaceWire.size(), 1, limits.render.maximumWireBytes);
        charge(wire, materialsWire.size(), 1, limits.render.maximumWireBytes);
        charge(wire, selection.size(), importedRenderImageBytes, limits.render.maximumWireBytes);
        uint64_t remaining = std::min({ limits.render.maximumDecodedPixelBytes, limits.render.maximumDecodedBytes - logical, limits.render.maximumWireBytes - wire });
        for (const auto& definition : result.materials.materials)
            if (definition.image && definition.image->bytes.size() > limits.maximumEncodedImageBytes)
                reject(ImportedProducerErrorCode::decodeLimitExceeded, "Imported encoded image exceeds the configured decoder input quota.");
        result.images.reserve(selection.size());
        for (const auto& definition : result.materials.materials) {
            if (!definition.image) {
                result.images.push_back(std::nullopt);
                continue;
            }
            if (!remaining)
                reject(ImportedProducerErrorCode::decodeLimitExceeded, "Imported decoded pixel quota is exhausted.");
            auto decoded = decoder.decode(*definition.image, definition.modelId, limits.maximumEncodedImageBytes, remaining);
            // The injected boundary must enforce allocation limits; this independent
            // receipt check prevents returning an over-budget result from a bad adapter.
            if (decoded.pixels.size() > remaining)
                reject(ImportedProducerErrorCode::decodeLimitExceeded, "Imported decoder returned pixels beyond its supplied quota.");
            detail::validateImportedRenderImage(decoded.width, decoded.height, decoded.pixels.size(), true);
            remaining -= decoded.pixels.size();
            result.images.push_back(std::move(decoded));
        }
        // Independently check dimensions/presence and the complete joined contract.
        (void)encodeImportedRenderPayload(result, limits.render);
        return result;
    }
}
}
