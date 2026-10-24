// SPDX-License-Identifier: MIT
#include "FbxNode.h"
#include "EngineInternal.h"
#include <algorithm>
#include <dust3d/base/retained_payload_budget.h>
namespace dust3d {
namespace native {
    FbxNode::FbxNode(std::string_view name, FbxPropertyBlock properties, std::vector<FbxNode> children, FbxNodeLimits limits)
        : m_properties(std::move(properties))
        , m_children(std::move(children))
    {
        constexpr size_t maximumBytes = 256 * 1024 * 1024;
        const auto limit = limits.maximumPayloadBytes == 0 ? 64 * 1024 * 1024 : limits.maximumPayloadBytes;
        if (limit <= sizeof(*this) || limit > maximumBytes || limits.maximumNodes == 0 || limits.maximumNodes > 65536
            || limits.maximumDepth == 0 || limits.maximumDepth > 64)
            throw DocumentError(DUST3D_STATUS_LIMIT_EXCEEDED, "FBX subtree limits exceed supported payload, count or depth bounds.");
        if (name.empty() || name.find('\0') != std::string_view::npos)
            throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "FBX node name must be nonempty and contain no NUL.");
        if (name.size() > 255)
            throw DocumentError(DUST3D_STATUS_LIMIT_EXCEEDED, "FBX node name exceeds its 255-byte wire field.");
        if (m_children.size() > limits.maximumNodes - 1)
            throw DocumentError(DUST3D_STATUS_LIMIT_EXCEEDED, "FBX child count exceeds the subtree node limit.");
        RetainedPayloadBudget payload(limit);
        auto charge = [&](size_t count, size_t width = 1) {
            if (!payload.tryAdd(count, width))
                throw DocumentError(DUST3D_STATUS_LIMIT_EXCEEDED, "FBX retained subtree exceeds its payload quota.");
        };
        charge(sizeof(*this));
        charge(name.size());
        charge(m_properties.payloadBytes() - sizeof(FbxPropertyBlock));
        charge(m_children.capacity(), sizeof(FbxNode));
        RetainedPayloadBudget wire(maximumBytes);
        wire.tryAdd(13);
        wire.tryAdd(name.size());
        wire.tryAdd(m_properties.bytes().size());
        for (const auto& child : m_children) {
            if (child.nodeCount() > limits.maximumNodes - m_nodeCount)
                throw DocumentError(DUST3D_STATUS_LIMIT_EXCEEDED, "FBX descendant count exceeds the subtree node limit.");
            if (child.depth() >= limits.maximumDepth)
                throw DocumentError(DUST3D_STATUS_LIMIT_EXCEEDED, "FBX descendant depth exceeds the subtree limit.");
            m_nodeCount += child.nodeCount();
            m_depth = std::max(m_depth, child.depth() + 1);
            // Child inline roots are already covered by the vector's entire retained capacity.
            charge(child.payloadBytes() - sizeof(FbxNode));
            wire.tryAdd(child.encodedSize());
        }
        if (!m_children.empty())
            wire.tryAdd(13);
        if (wire.isExceeded())
            throw DocumentError(DUST3D_STATUS_LIMIT_EXCEEDED, "FBX subtree wire bytes exceed the supported offset range.");
        m_name.assign(name.begin(), name.end());
        charge(m_name.capacity() - name.size());
        m_payloadBytes = payload.usedBytes();
        m_encodedSize = wire.usedBytes();
    }
}
}
