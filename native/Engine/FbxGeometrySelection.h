// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_FBX_GEOMETRY_SELECTION_H_
#define DUST3D_NATIVE_FBX_GEOMETRY_SELECTION_H_
#include <array>
#include <dust3d/base/object.h>
#include <memory>

namespace dust3d {
namespace native {
    using FbxTriangleUvs = std::array<Vector2, 3>;
    /// Logical payload budget, including temporary corner streams but excluding allocator overhead.
    struct FbxGeometrySelectionPlan {
        size_t maximumPayloadBytes;
        size_t stagingBytes;
    };
    /// Owns selected positions/topology/corner streams and source-relative face correspondence.
    /// Other Object metadata is deliberately excluded. Moved-from instances may only be destroyed/reassigned.
    class FbxGeometrySelection {
    public:
        FbxGeometrySelection(FbxGeometrySelection&&) noexcept = default;
        FbxGeometrySelection& operator=(FbxGeometrySelection&&) noexcept = default;
        FbxGeometrySelection(const FbxGeometrySelection&) = delete;
        FbxGeometrySelection& operator=(const FbxGeometrySelection&) = delete;
        /// Borrow geometry until this owner is moved or destroyed.
        const Object& object() const noexcept { return *m_object; }
        /// Borrow strictly increasing face indices relative to the supplied source Object.
        const std::vector<size_t>& sourceTriangleIndices() const noexcept { return m_sourceTriangles; }
        /// Retained object/container capacities; excludes caller storage and allocator overhead.
        size_t payloadBytes() const noexcept { return m_payloadBytes; }

    private:
        FbxGeometrySelection() = default;
        friend FbxGeometrySelection selectFbxGeometry(const Object&, const std::vector<size_t>&, const std::vector<FbxTriangleUvs>*, size_t);
        std::unique_ptr<Object> m_object;
        std::vector<size_t> m_sourceTriangles;
        size_t m_payloadBytes = 0;
    };
    /// Admit counts without allocating/traversing geometry. Zero quota selects64MiB; maximum256MiB.
    /// @param vertices Shared positions (1...1M), all retained without compaction.
    /// @param sourceTriangles Source face count (1...5M).
    /// @param selectedTriangles Selected face count (1...sourceTriangles).
    /// @param hasNormals Whether selected corner normals will be copied.
    /// @param hasUvs Whether selected/override corner UVs will be copied.
    /// @param maximumBytes Logical payload limit; zero uses the default.
    /// @return Resolved limit and conservative logical staging estimate.
    /// @throws DocumentError INVALID_DOCUMENT for empty counts; LIMIT_EXCEEDED for bounds/quota.
    FbxGeometrySelectionPlan planFbxGeometrySelection(size_t vertices, size_t sourceTriangles, size_t selectedTriangles, bool hasNormals, bool hasUvs, size_t maximumBytes = 0);
    /// Copy selected triangles and all double-precision positions without changing vertex identities.
    /// @param source Immutable source; only selected corner streams are inspected.
    /// @param indices Nonempty strictly increasing source face indices.
    /// @param overrides Optional per-selected-face UV replacement, independent of source UVs.
    /// @param maximumBytes Payload quota, as in planFbxGeometrySelection.
    /// @return Independent geometry owner; no colors, rig or document metadata is copied.
    /// @throws DocumentError INVALID_DOCUMENT for shape/order/nonfinite data; LIMIT_EXCEEDED for bounds/quota.
    /// Example: auto selected = selectFbxGeometry(source, {1}); encodeFbxMeshScene(selected.object()).
    FbxGeometrySelection selectFbxGeometry(const Object& source, const std::vector<size_t>& indices, const std::vector<FbxTriangleUvs>* overrides = nullptr, size_t maximumBytes = 0);
}
}
#endif
