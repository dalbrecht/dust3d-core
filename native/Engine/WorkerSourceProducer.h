// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_WORKER_SOURCE_PRODUCER_H_
#define DUST3D_NATIVE_WORKER_SOURCE_PRODUCER_H_
#include "WorkerImportedProducer.h"
#include "WorkerLegacyProducer.h"
#include <exception>
namespace dust3d {
namespace worker {
    struct SourceProducerOptions {
        LegacyProducerOptions procedural;
        ImportedMaterialsLimits materials;
        uint64_t maximumModelImages = 10000;
        uint64_t maximumEncodedImageBytes = 32ULL * 1024 * 1024;
        uint64_t maximumEncodedTotalBytes = 128ULL * 1024 * 1024;
        uint64_t maximumDecodedImageBytes = 64ULL * 1024 * 1024;
    };
    struct PreparedModelBakeImage {
        std::string modelId;
        ImportedRenderImage image;
    };
    struct PreparedSourceBakeImages {
        std::vector<ProceduralDecodedImage> procedural;
        std::vector<PreparedModelBakeImage> models;
    };
    enum class SourceImageRole { procedural,
        imported };
    /// Resource context plus the unchanged original typed exception.
    class SourceImageError : public std::runtime_error {
    public:
        SourceImageError(SourceImageRole role, std::string identity, std::exception_ptr cause);
        SourceImageRole role() const noexcept { return m_role; }
        const std::string& identity() const noexcept { return m_identity; }
        std::exception_ptr cause() const noexcept { return m_cause; }

    private:
        SourceImageRole m_role;
        std::string m_identity;
        std::exception_ptr m_cause;
    };
    /// Prepare all declared images from matching immutable archive facts. Procedural
    /// identities must already be resolved by copyProceduralImageInputs. Admit combined
    /// encoded quotas and both providers before any callback, then decode procedural
    /// followed by imported images with one remaining pixel quota. Returns owned pixels;
    /// throws original policy errors or SourceImageError preserving callback/receipt cause.
    /// No native generation or platform APIs. Example: auto images = prepareSourceBakeImages(inputs, facts, &png, &imported, {});
    PreparedSourceBakeImages prepareSourceBakeImages(const std::vector<ProceduralImageInput>& procedural,
        const ImportedMaterialsPayload& materials, ProceduralImageDecoder* proceduralDecoder,
        ImportedRenderImageDecoder* importedDecoder, const SourceProducerOptions& options);
}
}
#endif
