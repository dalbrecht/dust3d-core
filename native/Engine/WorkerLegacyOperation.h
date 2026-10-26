// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_WORKER_LEGACY_OPERATION_H_
#define DUST3D_NATIVE_WORKER_LEGACY_OPERATION_H_
#include "WorkerLegacyProducerNative.h"
namespace dust3d {
namespace worker {
    /// Generate one correlated complete legacy-bake reply from its explicit request codec.
    /// @param request Stable exact kind/identity/length and complete budgeted body.
    /// @param decoder Exclusive procedural provider; absent is supported only without referenced images.
    /// @param limits Authoritative byte/copy/image receiver policy; request settings select bounded dimensions/grid.
    /// @return Owned complete legacy bytes or original typed detailed failure; no parent fallback.
    /// @throws Original framing/request/policy errors before execution or reply allocation errors.
    /// Example: auto reply = generateLegacyBakedReply(request, &decoder, {});
    Frame generateLegacyBakedReply(const Frame& request, ProceduralImageDecoder* decoder, const LegacyProducerOptions& limits);
}
}
#endif
