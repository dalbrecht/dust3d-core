// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_WORKER_LEGACY_BAKED_COPY_H_
#define DUST3D_NATIVE_WORKER_LEGACY_BAKED_COPY_H_
#include "WorkerLegacyBaked.h"
#include <Dust3DBridge.h>
namespace dust3d {
namespace worker {
    /// Preflight and copy one stable native legacy baked owner, never substitute a source bake.
    /// @param owner Live immutable borrowed baked owner, retained by caller until return.
    /// @param limits Independent supported geometry/UV/tangent/texture and aggregate policies.
    /// @return Owned coherent geometry, charts/gradients, tangents and optional base maps.
    /// @throws LegacyBakedError, original inner errors or allocation errors; no native pointers escape.
    /// Quotas exclude native/workspace/RSS. Example: auto value = copyNativeLegacyBakedMesh(owner.get(), {});
    LegacyBakedPayload copyNativeLegacyBakedMesh(const Dust3DBakedMesh* owner, const LegacyBakedLimits& limits);
}
}
#endif
