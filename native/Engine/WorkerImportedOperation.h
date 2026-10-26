// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_WORKER_IMPORTED_OPERATION_H_
#define DUST3D_NATIVE_WORKER_IMPORTED_OPERATION_H_
#include "WorkerImportedProducerNative.h"
namespace dust3d {
namespace worker {
    /// Generate a correlated operation-six reply through the explicit worker decoder.
    /// @param request Stable raw archive request, exact kind/identity/length.
    /// @param decoder Borrowed exclusive worker-side decoder; null yields explicit unsupported failure before native execution.
    /// @param limits Independent validated native/preparation/output quotas.
    /// @return Owned render bytes, empty response for absence, or original typed detailed failure.
    /// @throws Protocol/policy errors before execution or reply-allocation errors; no parent fallback.
    /// Example: auto reply = generateImportedRenderReply(request, &decoder, {});
    Frame generateImportedRenderReply(const Frame& request, ImportedRenderImageDecoder* decoder, const NativeImportedProducerLimits& limits);
}
}
#endif
