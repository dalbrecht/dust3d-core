// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_WORKER_SOURCE_NATIVE_FAILURE_H_
#define DUST3D_NATIVE_WORKER_SOURCE_NATIVE_FAILURE_H_
#include "WorkerFailure.h"
#include <Dust3DBridge.h>
#include <string_view>
namespace dust3d {
namespace worker {
    /// Copy a source metadata/native diagnostic without generation. Original text is
    /// bounded only by the worker failure quota, with explicit truncation. Preserve
    /// original numeric status/domain; throws ProtocolError for success or allocation errors.
    /// Example: auto failure = copySourceNativeFailure(status, cause);
    FailurePayload copySourceNativeFailure(Dust3DStatus status, std::string_view cause);
}
}
#endif
