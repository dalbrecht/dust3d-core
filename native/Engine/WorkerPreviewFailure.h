// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_WORKER_PREVIEW_FAILURE_H_
#define DUST3D_NATIVE_WORKER_PREVIEW_FAILURE_H_
#include "WorkerFailure.h"
#include <exception>
namespace dust3d {
namespace worker {
    /// Convert an original process-local preview copy exception to a bounded receipt.
    /// Preserve its typed domain/code and original cause with explicit truncation;
    /// C++ failures have native status0, never a fabricated C status. No native calls.
    /// @param exception Non-null original exception. Borrowed through return.
    /// @return Independent v2 diagnostic; known budget failures select limitExceeded.
    /// @throws ProtocolError for absence; allocation errors may escape receipt copying.
    /// Example: auto failure = copyPreviewGenerationFailure(std::current_exception());
    FailurePayload copyPreviewGenerationFailure(const std::exception_ptr& exception);
}
}
#endif
