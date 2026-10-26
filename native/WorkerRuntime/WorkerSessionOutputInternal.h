// SPDX-License-Identifier: MIT
#ifndef DUST3D_WORKER_SESSION_OUTPUT_INTERNAL_H_
#define DUST3D_WORKER_SESSION_OUTPUT_INTERNAL_H_
#include "WorkerDecodedInternal.h"
#include "WorkerSessionOutput.h"
#include <Dust3DWorkerSessionOutput.h>
struct Dust3DWorkerSessionOutput {
    Dust3DWorkerSessionOutcome outcome = DUST3D_WORKER_SESSION_EMPTY;
    std::optional<Dust3DWorkerDecodedReply> primary, sourceIssue, statisticsIssue;
    std::optional<dust3d::worker::GenerationStatistics> statistics;
};
namespace dust3d::worker::bridge {
/// Consume a fully codec-validated value without copying geometry. Caller owns
/// the returned root; publication waits for complete construction. Not admission.
Dust3DWorkerSessionOutput* adoptSessionOutput(SessionOutput value);
}
#endif
