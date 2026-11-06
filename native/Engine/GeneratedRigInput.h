// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_GENERATED_RIG_INPUT_H_
#define DUST3D_NATIVE_GENERATED_RIG_INPUT_H_
#include <dust3d/base/object.h>
#include <dust3d/base/snapshot.h>
#include <memory>
namespace dust3d {
namespace native {
    /// Owns one coherent generator-produced pair. No generation or UV materialization.
    /// Views are borrowed until destruction; engine callers remain serialized.
    class GeneratedRigInput {
    public:
        const Snapshot& snapshot() const noexcept { return *m_snapshot; }
        const Object& object() const noexcept { return *m_object; }
        size_t payloadBytes() const noexcept { return m_payloadBytes; }
        GeneratedRigInput(const GeneratedRigInput&) = delete;
        GeneratedRigInput& operator=(const GeneratedRigInput&) = delete;

    private:
        GeneratedRigInput(std::unique_ptr<Snapshot> snapshot, std::unique_ptr<Object> object, size_t bytes)
            : m_snapshot(std::move(snapshot))
            , m_object(std::move(object))
            , m_payloadBytes(bytes)
        {
        }
        friend std::unique_ptr<GeneratedRigInput> retainGeneratedRigInput(std::unique_ptr<Snapshot>, std::unique_ptr<Object>, size_t);
        std::unique_ptr<Snapshot> m_snapshot;
        std::unique_ptr<Object> m_object;
        size_t m_payloadBytes;
    };
    /// Consume both inputs, including on failure. Caller guarantees they came from the same
    /// generation. Accounts full owned object/container capacity before wrapper allocation.
    /// Zero limit selects64MiB; maximum256MiB. Excludes allocator/container-node overhead,
    /// wrapper and temporary work. Shared source hints charge conservatively per logical owner.
    /// Throws typed DocumentError for absent inputs or quota, propagates allocation errors.
    /// Does not validate semantic rig readiness, mutate inputs or invoke cache materializers.
    std::unique_ptr<GeneratedRigInput> retainGeneratedRigInput(std::unique_ptr<Snapshot>, std::unique_ptr<Object>, size_t maximumBytes = 0);
}
}
#endif
