// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_SNAPSHOT_REFERENCES_H_
#define DUST3D_NATIVE_SNAPSHOT_REFERENCES_H_

#include <dust3d/base/snapshot.h>

namespace dust3d {
namespace native {
    /// Resolve known references in a private execution snapshot to authored keys.
    /// Exact matches win; case-preserving UUID fallback requires one source identity.
    /// Map keys/id attributes and unknown fields are retained. Missing references
    /// keep their existing validation policy. Ambiguous fallback throws DocumentError;
    /// callers discard this private copy on failure, retaining the original archive.
    void resolveSnapshotReferences(Snapshot& snapshot);
}
}

#endif
