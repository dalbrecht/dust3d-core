// SPDX-License-Identifier: MIT
#ifndef DUST3D_WORKER_IMAGE_DECODER_ADAPTER_H_
#define DUST3D_WORKER_IMAGE_DECODER_ADAPTER_H_
#include "WorkerImportedProducer.h"
#include <Dust3DWorkerImageDecoder.h>
namespace dust3d {
namespace worker {
    /// Bounded original callback receipt; raw bytes survive embedded NUL/non-UTF8.
    class WorkerImageDecoderError : public ImportedImageDecoderFailure {
    public:
        const Dust3DWorkerImageResult& result() const noexcept { return m_result; }
        /// Copy original status/code/truncation/opaque cause for explicit worker failure transport.
        FailurePayload failure() const override;

    private:
        friend class CallbackImportedImageDecoder;
        explicit WorkerImageDecoderError(const Dust3DWorkerImageResult& result);
        Dust3DWorkerImageResult m_result;
    };
    /// Explicit synchronous worker-side platform adapter; context remains caller-owned.
    class CallbackImportedImageDecoder final : public ImportedRenderImageDecoder {
    public:
        /// Copy validated configuration; throws WorkerImageDecoderError on invalid callback/layout.
        /// Example: CallbackImportedImageDecoder decoder(configuration);
        explicit CallbackImportedImageDecoder(Dust3DWorkerImageDecoder configuration);
        /// Validate input/policies, invoke callback once, own/destroy its receipt, and move pixels.
        /// Returns independent straight RGBA8. Throws original bounded WorkerImageDecoderError,
        /// original semantic errors or allocation errors. Context/input live throughout the call.
        ImportedRenderImage decode(const ImportedEncodedImage& encoded, std::string_view modelId, uint64_t maximumEncodedBytes, uint64_t maximumDecodedBytes) override;

    private:
        Dust3DWorkerImageDecoder m_configuration;
    };
}
}
#endif
