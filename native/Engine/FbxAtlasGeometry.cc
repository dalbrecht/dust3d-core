// SPDX-License-Identifier: MIT
#include "FbxAtlasGeometry.h"
#include "EngineInternal.h"
#include <numeric>
namespace dust3d {
namespace native {
    FbxGeometrySelection selectFbxAtlasGeometry(const Object& source, const UvAtlas& atlas, size_t maximumBytes)
    {
        const auto count = source.triangles.size();
        planFbxGeometrySelection(source.vertices.size(), count, count, source.triangleVertexNormals() != nullptr, true, maximumBytes);
        if (atlas.triangleUvs.size() != count || atlas.triangleUvKnown.size() != count)
            throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "FBX atlas UVs do not match original source faces.");
        for (const auto known : atlas.triangleUvKnown) {
            if (known != 1)
                throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "FBX atlas export requires complete known packed UVs.");
        }
        std::vector<size_t> originalFaces(count);
        std::iota(originalFaces.begin(), originalFaces.end(), size_t { 0 });
        return selectFbxGeometry(source, originalFaces, &atlas.triangleUvs, maximumBytes);
    }
}
}
