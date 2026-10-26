// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_WORKER_UV_COPY_H_
#define DUST3D_NATIVE_WORKER_UV_COPY_H_
#include "WorkerUv.h"
#include <Dust3DBridge.h>
namespace dust3d {
namespace worker {
    namespace detail {
        struct NativeUvInspection {
            size_t triangles, charts, gradients, components;
            uint64_t wire, logical;
        };
        /// Inspect stable borrowed native UV counts, storage, text and all scalar/reference semantics.
        /// @param owner Live immutable UV owner retained by caller, with stable borrowed buffers.
        /// @param limits Independent supported quotas, excluding root/capacity/validation workspace/RSS.
        /// @return Validated counts and exact wire/logical charges; no retained output arrays.
        /// @throws Original UvPayloadError or bounded validation-workspace allocation errors.
        /// Example: auto shape = inspectNativeUvMetadata(owner, {});
        NativeUvInspection inspectNativeUvMetadata(const Dust3DUvMesh* owner, const UvPayloadLimits& limits);
        /// Independently inspect then copy all metadata; no caller-supplied shape is trusted.
        /// @param owner Live immutable borrowed UV owner, releasable after return.
        /// @param limits Supported independent policies.
        /// @return Owned charts, gradients, triangle associations and exact component/image keys.
        /// @throws Original UvPayloadError or allocation errors; no partial output.
        /// Example: auto value = copyNativeUvMetadata(owner, {});
        UvPayload copyNativeUvMetadata(const Dust3DUvMesh* owner, const UvPayloadLimits& limits);
    }
}
}
#endif
