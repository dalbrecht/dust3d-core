// SPDX-License-Identifier: MIT
#include "WorkerLegacyProducer.h"
#include "WorkerImportedRenderValidation.h"
namespace dust3d {
namespace worker {
    LegacyProducerError::LegacyProducerError(LegacyProducerErrorCode code, const char* cause)
        : std::runtime_error(cause)
        , m_code(code)
    {
    }
    void validateLegacyProducerOptions(const LegacyProducerOptions& options)
    {
        validateProceduralImageLimits(options.images);
        validateLegacyBakedLimits(options.output);
        for (const auto quota : { options.maximumDecodedImageBytes, options.maximumUvNativeBytes, options.maximumBakeNativeBytes })
            if (!quota || quota > maximumPayloadBytes)
                throw LegacyProducerError(LegacyProducerErrorCode::invalidLimits, "Legacy bake byte policies are outside supported positive bounds.");
        if (!options.maximumPermittedTextureDimension || options.maximumPermittedTextureDimension > 8192
            || !options.maximumPermittedSpatialGridEntries || options.maximumPermittedSpatialGridEntries > 16000000)
            throw LegacyProducerError(LegacyProducerErrorCode::invalidLimits, "Legacy bake host work caps are outside supported positive bounds.");
        for (const auto dimension : { options.atlasTextureSize, options.colorSize, options.normalSize, options.materialSize })
            if (dimension > options.maximumPermittedTextureDimension)
                throw LegacyProducerError(LegacyProducerErrorCode::invalidLimits, "Legacy bake dimensions exceed the supported range.");
        if (options.maximumSpatialGridEntries > options.maximumPermittedSpatialGridEntries)
            throw LegacyProducerError(LegacyProducerErrorCode::invalidLimits, "Legacy bake spatial grid policy exceeds its supported range.");
    }
    std::vector<ProceduralDecodedImage> prepareProceduralImagePixels(const std::vector<ProceduralImageInput>& inputs,
        ProceduralImageDecoder* decoder, const LegacyProducerOptions& options)
    {
        validateLegacyProducerOptions(options);
        if (inputs.size() > options.images.maximumReferences)
            throw LegacyProducerError(LegacyProducerErrorCode::inputLimitExceeded, "Procedural decode input count exceeds its configured policy.");
        uint64_t encoded = 0;
        for (const auto& input : inputs) {
            if (input.encoded.size() > options.images.maximumEncodedImageBytes || input.encoded.size() > options.images.maximumEncodedTotalBytes - encoded)
                throw LegacyProducerError(LegacyProducerErrorCode::inputLimitExceeded, "Procedural decode inputs exceed an independent encoded quota.");
            encoded += input.encoded.size();
        }
        if (!inputs.empty() && !decoder)
            throw LegacyProducerError(LegacyProducerErrorCode::unavailableDecoder, "Legacy bake requires an explicit procedural image decoder.");
        std::vector<ProceduralDecodedImage> decoded;
        decoded.reserve(inputs.size());
        uint64_t remaining = options.maximumDecodedImageBytes;
        for (const auto& input : inputs) {
            if (!remaining)
                throw LegacyProducerError(LegacyProducerErrorCode::decodedLimitExceeded, "Legacy bake input pixel quota is exhausted.");
            auto image = decoder->decode(input, options.images.maximumEncodedImageBytes, remaining);
            if (image.pixels.size() > remaining)
                throw LegacyProducerError(LegacyProducerErrorCode::decodedLimitExceeded, "Procedural decoder returned pixels beyond its supplied quota.");
            if (image.width > 8192 || image.height > 8192)
                throw LegacyProducerError(LegacyProducerErrorCode::invalidImage, "Procedural decoder returned dimensions beyond the native bake range.");
            detail::validateImportedRenderImage(image.width, image.height, image.pixels.size(), true);
            remaining -= image.pixels.size();
            decoded.push_back(std::move(image));
        }
        return decoded;
    }

}
}
