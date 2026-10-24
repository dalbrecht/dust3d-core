// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_FBX_NODE_H_
#define DUST3D_NATIVE_FBX_NODE_H_
#include "FbxPropertyBlock.h"
#include <string_view>
#include <utility>
namespace dust3d {
namespace native {
    /// Aggregate subtree limits. Payload zero selects64MiB/max256MiB. Nodes must
    /// be1...65,536 and depth1...64; lower values are useful for constrained exports.
    struct FbxNodeLimits {
        size_t maximumPayloadBytes = 0;
        size_t maximumNodes = 65536;
        size_t maximumDepth = 64;
    };
    /// Immutable owned FBX7400 node. Child/property sources are consumed; names
    /// are copied during construction. References expire on move/destruction.
    /// Moved-from values may only be destroyed/reassigned. No engine/file work.
    class FbxNode {
    public:
        /// Validate/publish one complete subtree, including spare child capacity,
        /// name/property buffers and every descendant's dynamic storage. Input
        /// temporaries and allocator overhead excluded; no RSS limit. Names are
        ///1...255 non-NUL bytes; strings inside properties retain their binary form.
        /// Throws DocumentError INVALID_DOCUMENT for invalid names or LIMIT_EXCEEDED
        /// for quota/count/depth/wire-size excess; allocation failures propagate.
        /// Child/property arguments may be consumed even if construction fails.
        /// Example: FbxNode leaf("Model", std::move(properties)).
        explicit FbxNode(std::string_view name, FbxPropertyBlock properties = FbxPropertyBlock(),
            std::vector<FbxNode> children = {}, FbxNodeLimits limits = {});
        FbxNode(const FbxNode&) = delete;
        FbxNode& operator=(const FbxNode&) = delete;
        FbxNode(FbxNode&&) noexcept = default;
        FbxNode& operator=(FbxNode&&) noexcept = default;
        /// Borrow exact validated name bytes. Never throws.
        const std::vector<uint8_t>& name() const noexcept { return m_name; }
        /// Borrow immutable property bytes/count. Never throws.
        const FbxPropertyBlock& properties() const noexcept { return m_properties; }
        /// Borrow immutable children in source order. Never throws.
        const std::vector<FbxNode>& children() const noexcept { return m_children; }
        /// Return maximum subtree depth, including this node. Never throws.
        size_t depth() const noexcept { return m_depth; }
        /// Return aggregate node count, including this node. Never throws.
        size_t nodeCount() const noexcept { return m_nodeCount; }
        /// Return exact7400 record bytes, including a child-list terminator only
        /// when children exist; excludes document header/footer. Never throws.
        size_t encodedSize() const noexcept { return m_encodedSize; }
        /// Return accounted root/capacity/subtree bytes. Never throws.
        size_t payloadBytes() const noexcept { return m_payloadBytes; }

    private:
        std::vector<uint8_t> m_name;
        FbxPropertyBlock m_properties;
        std::vector<FbxNode> m_children;
        size_t m_depth = 1;
        size_t m_nodeCount = 1;
        size_t m_encodedSize = 0;
        size_t m_payloadBytes = 0;
    };
}
}
#endif
