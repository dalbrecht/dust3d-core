// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_WORKER_LEGACY_PRODUCER_H_
#define DUST3D_NATIVE_WORKER_LEGACY_PRODUCER_H_
#include "WorkerImportedRender.h"
#include "WorkerLegacyBaked.h"
#include "WorkerProceduralImages.h"
namespace dust3d {
namespace worker {
    /// Identical tight straight RGBA8 storage; the decoder purpose remains distinct.
    using ProceduralDecodedImage = ImportedRenderImage;
    class ProceduralImageDecoder {
    public:
        virtual ~ProceduralImageDecoder() = default;
        /// Decode PNG samples with the existing Qt-compatible procedural grayscale
        /// transparency rule, without orientation/color conversion/premultiplication.
        /// Input/code/context stay live through the synchronous call. Honor independent
        /// quotas before allocation; preserve original exceptions. No native generation.
        virtual ProceduralDecodedImage decode(const ProceduralImageInput& input, uint64_t maximumEncodedBytes, uint64_t maximumDecodedBytes) = 0;
    };
    struct LegacyProducerOptions {
        ProceduralImageLimits images;
        LegacyBakedLimits output;
        uint64_t maximumDecodedImageBytes = maximumPayloadBytes;
        uint64_t maximumUvNativeBytes = 64ULL * 1024 * 1024;
        uint64_t maximumBakeNativeBytes = maximumPayloadBytes;
        uint64_t atlasTextureSize = 0, colorSize = 0, normalSize = 0, materialSize = 0;
        uint64_t maximumSpatialGridEntries = 0;
        // Host admission is separate from the request's selected native defaults.
        uint64_t maximumPermittedTextureDimension = 8192;
        uint64_t maximumPermittedSpatialGridEntries = 16000000;
    };
    enum class LegacyProducerErrorCode { invalidLimits,
        unavailableDecoder,
        decodedLimitExceeded,
        invalidImage,
        inputLimitExceeded };
    class LegacyProducerError : public std::runtime_error {
    public:
        LegacyProducerError(LegacyProducerErrorCode code, const char* cause);
        LegacyProducerErrorCode code() const noexcept { return m_code; }

    private:
        LegacyProducerErrorCode m_code;
    };
    /// Validate policies before archive/native work. Zero dimensions/grid use native
    /// defaults; byte budgets are positive through 256 MiB, dimensions at most 8192 and
    /// grid at most 16000000. Throws original inner errors or LegacyProducerError.
    /// Example: validateLegacyProducerOptions({});
    void validateLegacyProducerOptions(const LegacyProducerOptions& options);
    /// Decode immutable selected facts produced by copyProceduralImageInputs. Identity/key
    /// correspondence must already be validated by that resolver. This function independently
    /// preflights count/encoded totals before callbacks, then validates every owned pixel receipt
    /// and aggregate quota. No archive parsing or native generation occurs. Root/descriptor
    /// workspace is separate from the decoded pixel quota. Throws original decoder/shape
    /// errors, LegacyProducerError or allocation errors. Example: auto pixels = prepareProceduralImagePixels(inputs, &decoder, options);
    std::vector<ProceduralDecodedImage> prepareProceduralImagePixels(const std::vector<ProceduralImageInput>& inputs,
        ProceduralImageDecoder* decoder, const LegacyProducerOptions& options);
}
}
#endif
