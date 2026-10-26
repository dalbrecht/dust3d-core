// SPDX-License-Identifier: MIT
#ifndef DUST3D_WORKER_SESSION_OUTPUT_INTERNAL_H_
#define DUST3D_WORKER_SESSION_OUTPUT_INTERNAL_H_
#include "WorkerDecodedInternal.h"
#include "WorkerSessionOutput.h"
#include <Dust3DWorkerSessionOutput.h>
#include <exception>
struct Dust3DWorkerSessionOutput {
    Dust3DWorkerSessionOutcome outcome = DUST3D_WORKER_SESSION_EMPTY;
    std::optional<Dust3DWorkerDecodedReply> primary, sourceIssue, statisticsIssue;
    std::optional<dust3d::worker::GenerationStatistics> statistics;
};
namespace dust3d::worker::bridge {
/// Consume a fully codec-validated value without copying geometry. Caller owns
/// the returned root; publication waits for complete construction. Not admission.
Dust3DWorkerSessionOutput* adoptSessionOutput(SessionOutput value);
/// One C policy builder shared by values-only and completion-admitted paths.
SessionOutputLimits sessionOutputPolicy(Dust3DWorkerSessionOutputLimits value);
/// Preserve a nonnull current_exception captured inside a C boundary catch.
/// Copies causes into fixed diagnostic storage; no native work occurs.
void rejectSessionOutputException(const std::exception_ptr& exception, Dust3DWorkerDecodeResult& result) noexcept;
}
#endif
