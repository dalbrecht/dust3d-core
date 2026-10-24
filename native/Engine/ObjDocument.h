// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_OBJ_DOCUMENT_H_
#define DUST3D_NATIVE_OBJ_DOCUMENT_H_
#include <dust3d/base/object.h>
#include <string_view>
namespace dust3d {
namespace native {
    inline constexpr std::string_view objDocumentHeader = "# Dust3D native geometry OBJ\n";
    struct ObjGeometryPlan {
        size_t maximumPayloadBytes;
        size_t minimumPayloadBytes;
    };
    /// Independent immutable OBJ bytes. Borrowed views expire on move/reassignment/destruction.
    /// Moved-from values may only be destroyed or reassigned.
    class OwnedObjDocument {
    public:
        OwnedObjDocument(const OwnedObjDocument&) = delete;
        OwnedObjDocument& operator=(const OwnedObjDocument&) = delete;
        OwnedObjDocument(OwnedObjDocument&&) noexcept = default;
        OwnedObjDocument& operator=(OwnedObjDocument&&) noexcept = default;
        /// Borrow complete output until move/reassignment/destruction; never throws.
        const std::vector<uint8_t>& bytes() const noexcept { return m_bytes; }
        /// Root plus actual retained byte capacity; excludes formatting workspace/allocator costs.
        size_t payloadBytes() const noexcept { return sizeof(*this) + m_bytes.capacity(); }

    private:
        friend OwnedObjDocument encodeObjDocument(const Object&, size_t);
        explicit OwnedObjDocument(std::vector<uint8_t> bytes) noexcept
            : m_bytes(std::move(bytes))
        {
        }
        std::vector<uint8_t> m_bytes;
    };
    /// Admit vertices1...1M and polygons1...5M without geometry/output allocation.
    /// @param vertices Original shared vertex count.
    /// @param polygons Original triangle/quad count, not triangulated preview count.
    /// @param maximumBytes Root/capacity quota, zero64MiB; maximum256MiB.
    /// @return Resolved quota and conservative minimum possible text/root bytes.
    /// @throws DocumentError NO_GEOMETRY for no polygons, INVALID_DOCUMENT for no vertices,
    /// LIMIT_EXCEEDED for count/quota bounds. Actual text size is checked separately.
    ObjGeometryPlan planObjGeometry(size_t vertices, size_t polygons, size_t maximumBytes = 0);
    /// Encode reference OBJ geometry semantics: original double vertices and triangleAndQuads.
    /// @param source Stable immutable original Object, alive throughout the synchronous call.
    /// @param maximumBytes Root/capacity quota as above.
    /// @return Owned classic-locale max_digits10 double text, one-based original polygon indices.
    /// @throws DocumentError for invalid arity/indices/nonfinite coordinates, bounds/quotas or
    /// inconsistent second-pass size; allocation/formatting exceptions may propagate.
    /// Faces must have3/4 corners; positions are finite within +/-4M. No axis/unit transform,
    /// UVs, normals, materials, rig/animation, globals, native calls or files are included.
    /// No fallback to preview triangles. Example: encodeObjDocument(originalObject).
    OwnedObjDocument encodeObjDocument(const Object& source, size_t maximumBytes = 0);
}
}
#endif
