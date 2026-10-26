// SPDX-License-Identifier: MIT
#include "WorkerImportedRenderValidation.h"
#include "WorkerSourceProducerNative.h"
#include <algorithm>
namespace dust3d {
namespace worker {
    void validateNativeSourceProducerOptions(const NativeSourceProducerOptions& options)
    {
        validateLegacyProducerOptions(options.preparation.procedural);
        validateImportedMaterialsLimits(options.preparation.materials);
        validateSourceBakedLimits(options.output);
        if (!options.preparation.maximumModelImages || options.preparation.maximumModelImages > 10000)
            throw LegacyProducerError(LegacyProducerErrorCode::invalidLimits, "Source model image count policy exceeds native bounds.");
        for (const auto quota : { options.maximumMaterialNativeBytes, options.maximumSamplerNativeBytes, options.maximumAtlasNativeBytes, options.maximumOutputNativeBytes,
                 options.preparation.maximumEncodedImageBytes, options.preparation.maximumEncodedTotalBytes, options.preparation.maximumDecodedImageBytes })
            if (!quota || quota > maximumPayloadBytes)
                throw LegacyProducerError(LegacyProducerErrorCode::invalidLimits, "Source bake byte policy exceeds supported positive bounds.");
        if (!options.maximumPermittedChartCount || options.maximumPermittedChartCount > 100000
            || !options.maximumPermittedRasterSamples || options.maximumPermittedRasterSamples > 1000000000
            || !options.maximumPermittedLevels || options.maximumPermittedLevels > 14
            || options.maximumChartCount > options.maximumPermittedChartCount
            || options.maximumRasterSamples > options.maximumPermittedRasterSamples
            || options.maximumLevels > options.maximumPermittedLevels)
            throw LegacyProducerError(LegacyProducerErrorCode::invalidLimits, "Source bake work settings exceed independent host caps.");
    }
    SourceImageError::SourceImageError(SourceImageRole role, std::string identity, std::exception_ptr cause)
        : std::runtime_error("Source bake image preparation failed; original cause is retained.")
        , m_role(role)
        , m_identity(std::move(identity))
        , m_cause(std::move(cause))
    {
    }
    PreparedSourceBakeImages prepareSourceBakeImages(const std::vector<ProceduralImageInput>& procedural,
        const ImportedMaterialsPayload& materials, ProceduralImageDecoder* proceduralDecoder,
        ImportedRenderImageDecoder* importedDecoder, const SourceProducerOptions& options)
    {
        validateLegacyProducerOptions(options.procedural);
        for (const auto quota : { options.maximumEncodedImageBytes, options.maximumEncodedTotalBytes, options.maximumDecodedImageBytes })
            if (!quota || quota > maximumPayloadBytes)
                throw LegacyProducerError(LegacyProducerErrorCode::invalidLimits, "Source bake image policy exceeds supported positive bounds.");
        if (!options.maximumModelImages || options.maximumModelImages > 10000)
            throw LegacyProducerError(LegacyProducerErrorCode::invalidLimits, "Source model image count policy exceeds native bounds.");
        // Validate declarations without duplicating encoded image storage.
        (void)inspectImportedMaterialsPayload(materials, options.materials);
        if (procedural.size() > options.procedural.images.maximumReferences)
            throw LegacyProducerError(LegacyProducerErrorCode::inputLimitExceeded, "Source bake procedural reference count exceeds policy.");
        uint64_t encoded = 0, proceduralEncoded = 0;
        const auto admit = [&](uint64_t size, uint64_t perImage) {
            if (size > perImage || size > options.maximumEncodedImageBytes || size > options.maximumEncodedTotalBytes - encoded)
                throw LegacyProducerError(LegacyProducerErrorCode::inputLimitExceeded, "Source bake combined encoded image quota is exceeded.");
            encoded += size;
        };
        for (const auto& input : procedural) {
            admit(input.encoded.size(), options.procedural.images.maximumEncodedImageBytes);
            if (input.encoded.size() > options.procedural.images.maximumEncodedTotalBytes - proceduralEncoded)
                throw LegacyProducerError(LegacyProducerErrorCode::inputLimitExceeded, "Source bake procedural encoded quota is exceeded.");
            proceduralEncoded += input.encoded.size();
        }
        uint64_t importedImageCount = 0;
        for (const auto& material : materials.materials)
            if (material.image) {
                if (++importedImageCount > options.maximumModelImages)
                    throw LegacyProducerError(LegacyProducerErrorCode::inputLimitExceeded, "Source model images exceed native descriptor admission.");
                admit(material.image->bytes.size(), options.materials.maximumImageBytes);
            }
        if ((!procedural.empty() && !proceduralDecoder) || (importedImageCount && !importedDecoder))
            throw LegacyProducerError(LegacyProducerErrorCode::unavailableDecoder, "Source bake requires both declared image decoder roles.");
        PreparedSourceBakeImages result;
        uint64_t remaining = options.maximumDecodedImageBytes;
        const auto validateReceipt = [&](const ImportedRenderImage& image) {
            if (image.pixels.size() > remaining)
                throw LegacyProducerError(LegacyProducerErrorCode::decodedLimitExceeded, "Source image exceeds its remaining pixel quota.");
            if (image.width > 8192 || image.height > 8192)
                throw LegacyProducerError(LegacyProducerErrorCode::invalidImage, "Source image exceeds native dimension bounds.");
            detail::validateImportedRenderImage(image.width, image.height, image.pixels.size(), true);
            remaining -= image.pixels.size();
        };
        for (const auto& input : procedural) {
            try {
                if (!remaining)
                    throw LegacyProducerError(LegacyProducerErrorCode::decodedLimitExceeded, "Source pixel quota is exhausted.");
                auto image = proceduralDecoder->decode(input, std::min(options.maximumEncodedImageBytes, options.procedural.images.maximumEncodedImageBytes), remaining);
                validateReceipt(image);
                result.procedural.push_back(std::move(image));
            } catch (...) {
                throw SourceImageError(SourceImageRole::procedural, input.identity, std::current_exception());
            }
        }
        for (const auto& material : materials.materials) {
            if (!material.image)
                continue;
            try {
                if (!remaining)
                    throw LegacyProducerError(LegacyProducerErrorCode::decodedLimitExceeded, "Source pixel quota is exhausted.");
                auto image = importedDecoder->decode(*material.image, material.modelId, std::min(options.maximumEncodedImageBytes, options.materials.maximumImageBytes), remaining);
                validateReceipt(image);
                result.models.push_back({ material.modelId, std::move(image) });
            } catch (...) {
                throw SourceImageError(SourceImageRole::imported, material.modelId, std::current_exception());
            }
        }
        return result;
    }
}
}
