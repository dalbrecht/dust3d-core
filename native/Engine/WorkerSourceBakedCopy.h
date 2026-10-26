// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_WORKER_SOURCE_BAKED_COPY_H_
#define DUST3D_NATIVE_WORKER_SOURCE_BAKED_COPY_H_
#include "WorkerSourceBaked.h"
#include <Dust3DBridge.h>
namespace dust3d {
namespace worker {
    /// Preflight and copy full rendering values from ONE stable source-baked owner.
    /// @param owner Live immutable borrowed owner, retained by caller throughout this call.
    /// @param limits Independent inner/aggregate copy quotas; native workspace/RSS excluded.
    /// @return Independent geometry, records/catalogs and pixels/cap in this owner's order.
    /// @throws SourceBakedError, original inner errors or allocation errors; no native pointers escape.
    /// No native generation/image decoding occurs. Example: auto value = copyNativeSourceBakedMesh(owner.get(), {});
    SourceBakedPayload copyNativeSourceBakedMesh(const Dust3DSourceBakedMesh* owner, const SourceBakedLimits& limits);
}
}
#endif
