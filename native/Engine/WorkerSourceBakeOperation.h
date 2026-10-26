// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_WORKER_SOURCE_BAKE_OPERATION_H_
#define DUST3D_NATIVE_WORKER_SOURCE_BAKE_OPERATION_H_
#include "WorkerSourceProducerNative.h"
namespace dust3d {
namespace worker {
    /// Produce one complete correlated source-bake response or original contextual failure.
    /// Request/policy errors throw before native work. No fallback or platform decoding.
    /// Example: auto reply = generateSourceBakedReply(request, &png, &imported, {});
    Frame generateSourceBakedReply(const Frame& request, ProceduralImageDecoder* proceduralDecoder,
        ImportedRenderImageDecoder* importedDecoder, const NativeSourceProducerOptions& limits);
}
}
#endif
