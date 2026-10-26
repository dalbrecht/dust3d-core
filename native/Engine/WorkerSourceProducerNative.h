// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_WORKER_SOURCE_PRODUCER_NATIVE_H_
#define DUST3D_NATIVE_WORKER_SOURCE_PRODUCER_NATIVE_H_
#include "WorkerLegacyProducerNative.h"
#include "WorkerSourceBaked.h"
#include "WorkerSourceProducer.h"
namespace dust3d {
namespace worker {
    struct NativeSourceProducerOptions {
        SourceProducerOptions preparation;
        SourceBakedLimits output;
        uint64_t maximumMaterialNativeBytes = 64ULL * 1024 * 1024;
        uint64_t maximumSamplerNativeBytes = 64ULL * 1024 * 1024;
        uint64_t maximumAtlasNativeBytes = 64ULL * 1024 * 1024;
        uint64_t maximumOutputNativeBytes = maximumPayloadBytes;
        uint64_t maximumChartCount = 0, maximumRasterSamples = 0;
        uint64_t maximumPermittedChartCount = 100000, maximumPermittedRasterSamples = 1000000000;
        bool shouldGenerateMips = false;
        uint64_t maximumLevels = 0, maximumPermittedLevels = 14;
    };
    /// Validate native policies without native work; throws typed policy errors.
    /// Example: validateNativeSourceProducerOptions({});
    void validateNativeSourceProducerOptions(const NativeSourceProducerOptions& options);
    /// Resolve both image roles from the same immutable archive, release material facts'
    /// native owner before callbacks, then copy one source-baked owner before destroying it.
    /// Optional mip mode preserves actual levels and sampling LOD. Returns independent
    /// geometry/catalogs/maps; throws original typed errors, contextual SourceImageError,
    /// LegacyProducerNativeError preserving native diagnostic, or allocation errors.
    /// Native calls serialize; callbacks run outside the engine lock. Quotas exclude RSS.
    /// Example: auto result = generateSourceBakedRender(archive, &png, &imported, {});
    SourceBakedPayload generateSourceBakedRender(const std::vector<uint8_t>& archive,
        ProceduralImageDecoder* proceduralDecoder, ImportedRenderImageDecoder* importedDecoder,
        const NativeSourceProducerOptions& options);
}
}
#endif
