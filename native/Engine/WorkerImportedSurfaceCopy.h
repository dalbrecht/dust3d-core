// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_WORKER_IMPORTED_SURFACE_COPY_H_
#define DUST3D_NATIVE_WORKER_IMPORTED_SURFACE_COPY_H_
#include "WorkerImportedSurface.h"
#include <Dust3DBridge.h>
namespace dust3d {
namespace worker {
    /// Copy one live immutable native owner after complete scalar/catalog/aggregate
    /// preflight. No generation, baking, image decoding or provenance inference occurs.
    /// @param owner Borrowed owner stable for the whole call; null rejects.
    /// @param limits Independent supported geometry/metadata/aggregate quotas.
    /// @return Independent geometry, source attributes and exact catalogs.
    /// @throws ImportedSurfaceError, original MeshPayloadError or allocation errors.
    /// Example: auto value = copyNativeImportedSurface(owner, {});
    ImportedSurfacePayload copyNativeImportedSurface(const Dust3DImportedSurfaceMesh* owner, const ImportedSurfaceLimits& limits);
}
}
#endif
