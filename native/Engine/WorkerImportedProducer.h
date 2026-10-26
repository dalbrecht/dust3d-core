// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_WORKER_IMPORTED_PRODUCER_H_
#define DUST3D_NATIVE_WORKER_IMPORTED_PRODUCER_H_
#include "WorkerFailure.h"
#include "WorkerImportedRender.h"
#include <string_view>
namespace dust3d {
namespace worker {
    /// Portable original decoder failure receipt; implemented by platform adapters.
    class ImportedImageDecoderFailure : public std::runtime_error {
    public:
        using std::runtime_error::runtime_error;
        virtual FailurePayload failure() const = 0;
    };
    /// Injected platform boundary, executed in the worker calling thread. Implementation
    /// must honor input/output quotas before allocation, return straight tight RGBA8 in
    /// source scanline order and preserve original errors. No platform types cross here.
    class ImportedRenderImageDecoder {
    public:
        virtual ~ImportedRenderImageDecoder() = default;
        virtual ImportedRenderImage decode(const ImportedEncodedImage& encoded, std::string_view modelId, uint64_t maximumEncodedBytes, uint64_t maximumDecodedBytes) = 0;
    };
    struct ImportedProducerLimits {
        ImportedRenderLimits render;
        ImportedMaterialsLimits facts;
        uint64_t maximumEncodedImageBytes = 16ULL * 1024 * 1024;
    };
    struct NativeImportedProducerLimits {
        ImportedProducerLimits preparation;
        uint64_t maximumMaterialNativeBytes = 64ULL * 1024 * 1024;
        uint64_t maximumSurfaceNativeBytes = 64ULL * 1024 * 1024;
    };
    /// Validate independent positive native/preparation policies without executing native code.
    /// Throws original typed policy errors. Example: validateNativeImportedProducerLimits({});
    void validateNativeImportedProducerLimits(const NativeImportedProducerLimits& limits);
    enum class ImportedProducerErrorCode { invalidLimits,
        missingMaterial,
        decodeLimitExceeded };
    class ImportedProducerError : public std::runtime_error {
    public:
        ImportedProducerError(ImportedProducerErrorCode code, const char* cause);
        ImportedProducerErrorCode code() const noexcept { return m_code; }

    private:
        ImportedProducerErrorCode m_code;
    };
    /// Validate independent positive policies; throws original inner or producer errors.
    /// Example: validateImportedProducerLimits({});
    void validateImportedProducerLimits(const ImportedProducerLimits& limits);
    /// Prepare surface-ordered render values from owned immutable-generation facts.
    /// @param surface Matching authored geometry/attributes, moved into result.
    /// @param facts Complete same-archive definitions/bindings, validated before selection.
    /// @param decoder Required platform implementation; only rendered encoded images decoded.
    /// @param limits Layered quotas; workspace/temporary encoding/RSS excluded.
    /// @return Independent render data or absence when no imported model is rendered.
    /// @throws Original inner/decoder errors, ImportedProducerError or allocation errors.
    /// Caller must establish same-archive provenance; identity matching cannot authenticate it.
    /// Example: auto value = prepareImportedRender(std::move(surface), std::move(facts), decoder, {});
    std::optional<ImportedRenderPayload> prepareImportedRender(ImportedSurfacePayload surface, ImportedMaterialsPayload facts,
        ImportedRenderImageDecoder& decoder, const ImportedProducerLimits& limits);
}
}
#endif
