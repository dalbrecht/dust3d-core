// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_WORKER_IMPORTED_PRODUCER_NATIVE_H_
#define DUST3D_NATIVE_WORKER_IMPORTED_PRODUCER_NATIVE_H_
#include "WorkerFailure.h"
#include "WorkerImportedProducer.h"
namespace dust3d {
namespace worker {
    /// Original mapped status/opaque diagnostic from a failed native load.
    class ImportedProducerNativeError : public std::runtime_error {
    public:
        explicit ImportedProducerNativeError(FailurePayload failure);
        const FailurePayload& failure() const noexcept { return m_failure; }

    private:
        FailurePayload m_failure;
    };
    /// Load facts and surface from the same stable unchanged archive and prepare owned rendering data.
    /// @param archive Complete borrowed immutable bytes, retained until this call returns.
    /// @param decoder Explicit platform boundary running in the calling worker, never implicit parent fallback.
    /// @param limits Layered native/copy/preparation quotas, excluding native/decoder workspace and RSS.
    /// @return Independent imported rendering values, or absence when no imported model is rendered.
    /// @throws ImportedProducerNativeError with original status/cause, original copy/decoder errors or allocation errors.
    /// Native entry points serialize inherited globals; no Qt/Apple types or borrowed native buffers escape.
    /// Example: auto output = generateImportedRender(archive, decoder, {});
    std::optional<ImportedRenderPayload> generateImportedRender(const std::vector<uint8_t>& archive,
        ImportedRenderImageDecoder& decoder, const NativeImportedProducerLimits& limits);
}
}
#endif
