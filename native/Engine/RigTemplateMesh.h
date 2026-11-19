// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_RIG_TEMPLATE_MESH_H_
#define DUST3D_NATIVE_RIG_TEMPLATE_MESH_H_
#include <array>
#include <cstdint>
#include <dust3d/base/vector3.h>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
namespace dust3d {
namespace native {
    enum class RigTemplateMeshFailure { invalidInput,
        limitExceeded,
        generationFailed };
    class RigTemplateMeshError : public std::runtime_error {
    public:
        RigTemplateMeshError(RigTemplateMeshFailure failure, std::string message)
            : std::runtime_error(std::move(message))
            , m_failure(failure)
        {
        }
        RigTemplateMeshFailure failure() const noexcept { return m_failure; }

    private:
        RigTemplateMeshFailure m_failure;
    };
    struct RigTemplateBone {
        std::string name;
        Vector3 fromPosition;
        Vector3 toPosition;
        double capsuleRadius = 0;
    };
    enum class RigTemplateVertexRole : uint8_t { bone,
        selectedBone,
        ring };
    struct RigTemplateVertex {
        Vector3 position;
        uint32_t boneIndex;
        RigTemplateVertexRole role;
    };
    struct RigTemplateMesh {
        std::vector<RigTemplateVertex> vertices;
        std::vector<std::array<uint32_t, 3>> triangles;
    };
    struct RigTemplateMeshOptions {
        double startRadius = 0.02;
        bool normalize = true;
        std::string selectedBoneName;
        size_t maximumBones = 256;
        size_t maximumVertices = 100000;
        size_t maximumTriangles = 200000;
        size_t maximumOutputBytes = 32 * 1024 * 1024;
    };
    /// Extracted template-only geometry from Qt RigSkeletonMeshGenerator (core #319).
    /// Inputs/owned outputs never share buffers or invoke rig/pose/document generators.
    /// Requires unique exact names <=1024 bytes, finite coordinates/radii <=1M and
    /// nonzero bone lengths. Tiny lengths <1e-6 preserve the reference ring skip.
    /// Quotas preflight logical output bytes (not allocator/transient process memory).
    /// Throws RigTemplateMeshError atomically for invalid input, quotas or bad output.
    /// Example: buildRigTemplateMesh(bones, options). All types are PRIVATE C++ API;
    /// no STL ownership is exposed through the public C ABI by this extraction.
    RigTemplateMesh buildRigTemplateMesh(const std::vector<RigTemplateBone>& bones,
        const RigTemplateMeshOptions& options = {});
}
}
#endif
