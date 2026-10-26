// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_WORKER_SESSION_OPERATION_H_
#define DUST3D_NATIVE_WORKER_SESSION_OPERATION_H_
#include "WorkerConnectionSession.h"
#include "WorkerSessionOutput.h"
namespace dust3d {
namespace worker {
    /// Generate a correlated session response through an existing host-owned cache.
    /// Requires exact operation9 request framing and a complete source-budget body.
    /// Host supplies the trusted connection lifetime; request bytes never create an
    /// owner. Reserves bounded diagnostic allowance before generation, independently
    /// of geometry/source quotas. Request source budget can only tighten host policy.
    /// Returns one owned response envelope, including primary failure/empty outcomes
    /// and actual final-call statistics. No parent/stateless fallback or I/O occurs.
    /// Throws original protocol/request/scope/output/copy errors; no partial frame.
    /// This is not host publication admission, process containment or C/Swift adoption.
    /// @param request Stable exact source-budget request, borrowed through return.
    /// @param session Existing exclusive host-owned connection/document cache.
    /// @param connectionGeneration Trusted host lifetime, never receipt-derived.
    /// @param limits Inner/aggregate output policy; reserves worst-case diagnostics,
    /// reducing geometry allowance explicitly. This does not establish Apple range parity.
    /// @return Correlated independently owned response envelope.
    /// Example: auto reply = generateSessionReply(request, session, generation, {});
    Frame generateSessionReply(const Frame& request, ConnectionGenerationSession& session, uint64_t connectionGeneration, const SessionOutputLimits& limits);
}
}
#endif
