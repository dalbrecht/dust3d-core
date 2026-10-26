// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_WORKER_IMPORTED_MATERIALS_COPY_H_
#define DUST3D_NATIVE_WORKER_IMPORTED_MATERIALS_COPY_H_
#include "WorkerImportedMaterials.h"
#include <Dust3DBridge.h>
namespace dust3d {
namespace worker {
    /// Preflight/copy immutable authored facts from one live imported-assets owner.
    /// @param owner Stable borrowed owner retained by caller throughout this call.
    /// @param limits Independent positive record/key/encoded/wire/logical quotas.
    /// @return Owned original material definitions, encoded declarations and instance bindings.
    /// @throws ImportedMaterialsError or allocation errors; no borrowed data escapes.
    /// No geometry generation/image decoding. Example: auto facts = copyNativeImportedMaterials(owner.get(), {});
    ImportedMaterialsPayload copyNativeImportedMaterials(const Dust3DImportedAssets* owner, const ImportedMaterialsLimits& limits);
}
}
#endif
