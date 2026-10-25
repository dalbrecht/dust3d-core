// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_WORKER_UV_OPERATION_H_
#define DUST3D_NATIVE_WORKER_UV_OPERATION_H_
#include "WorkerFailure.h"
#include "WorkerUvMesh.h"
#include "WorkerUvRequest.h"
#include <variant>
namespace dust3d {
namespace worker {
    using UvGenerationResult = std::variant<UvMeshPayload, FailurePayload>;
    /// Generate one correlated geometry/UV pair through the serialized native C ABI.
    /// @param request Stable owned archive/options; validated before native generation.
    /// @param limits Supported independent and aggregate copy/wire policies.
    /// @return Owned pair copied from one native UV owner, or typed original failure.
    /// @throws Request/policy errors before execution; allocation/serialization errors may propagate.
    /// Quotas bound retained copies, not generation workspace/RSS. No pixels are baked.
    /// Example: auto result = generateUvMesh(request, {});
    UvGenerationResult generateUvMesh(const UvRequest& request, const UvMeshPayloadLimits& limits);
}
}
#endif
