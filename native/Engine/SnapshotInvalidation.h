#ifndef DUST3D_NATIVE_SNAPSHOT_INVALIDATION_H_
#define DUST3D_NATIVE_SNAPSHOT_INVALIDATION_H_

#include <dust3d/base/snapshot.h>
#include <set>
#include <string>

namespace dust3d {
namespace native {

    /// Conservative invalidation identities, including removed entries for eventual cache eviction.
    struct SnapshotInvalidation {
        std::set<std::string> dirtyPartIds;
        std::set<std::string> dirtyComponentIds;
        bool isRootDirty = false;
    };

    /// Compare generation snapshots without trusting their persisted __dirty attributes.
    /// @param previous Previous input, or nullptr for a first generation.
    /// @param current Current input. Neither snapshot is modified.
    /// @return Changed parts, component/dependency closure and root invalidation. Global input
    /// changes invalidate everything; unknown attributes conservatively count as changes.
    /// Cyclic metadata terminates, but this analysis does not replace document validation.
    /// External asset revisions and bounded cache ownership remain the caller's responsibility.
    SnapshotInvalidation deriveSnapshotInvalidation(const Snapshot* previous, const Snapshot& current);

}
}

#endif
