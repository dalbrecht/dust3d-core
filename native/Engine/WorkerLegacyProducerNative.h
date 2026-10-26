// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_WORKER_LEGACY_PRODUCER_NATIVE_H_
#define DUST3D_NATIVE_WORKER_LEGACY_PRODUCER_NATIVE_H_
#include "WorkerFailure.h"
#include "WorkerLegacyProducer.h"
namespace dust3d {
namespace worker {
    class LegacyProducerNativeError : public std::runtime_error {
    public:
        explicit LegacyProducerNativeError(FailurePayload failure);
        const FailurePayload& failure() const noexcept { return m_failure; }

    private:
        FailurePayload m_failure;
    };
    /// Prepare referenced images and generate ONE complete native legacy bake.
    /// @param archive Stable complete source archive, borrowed through return.
    /// @param decoder Explicit procedural provider, required only when images exist.
    /// @param options Independent image/native/copy policies; excludes workspace/RSS.
    /// @return Independent geometry, UV/catalog/tangents and optional base maps.
    /// @throws Original metadata/decoder/codec errors, LegacyProducerError or
    /// LegacyProducerNativeError preserving original status/cause, or allocation errors.
    /// No imported/source-baked substitution or parent fallback. Native calls serialize;
    /// platform callbacks execute after the metadata reader releases its engine lock.
    /// Example: auto value = generateLegacyBakedRender(archive, &decoder, options);
    LegacyBakedPayload generateLegacyBakedRender(const std::vector<uint8_t>& archive,
        ProceduralImageDecoder* decoder, const LegacyProducerOptions& options);
}
}
#endif
