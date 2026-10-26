// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_WORKER_SESSION_OUTPUT_COPY_H_
#define DUST3D_NATIVE_WORKER_SESSION_OUTPUT_COPY_H_
#include "WorkerNativeSession.h"
#include "WorkerSessionOutput.h"
namespace dust3d {
namespace worker {
    /// Move exclusive native-session output into a portable envelope. Require one
    /// final outcome; preserve original native/codec/standard statistics exceptions
    /// as separate bounded detailed failures, never fabricated measurements.
    /// Explicit cause truncation honors positive diagnostic policies; resource/native
    /// classifications survive. Does not serialize, generate or authenticate output.
    /// Throws SessionOutputError, FailurePayloadError or allocation errors. Caller
    /// admits diagnostic policy before native work; this consumes the input value.
    /// Example: auto value = copySessionOutput(std::move(generated), {});
    SessionOutput copySessionOutput(NativeSessionGeneration generated, const FailurePayloadLimits& limits);
}
}
#endif
