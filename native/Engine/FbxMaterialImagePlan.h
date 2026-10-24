// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_FBX_MATERIAL_IMAGE_PLAN_H_
#define DUST3D_NATIVE_FBX_MATERIAL_IMAGE_PLAN_H_
#include <array>
#include <cstddef>

namespace dust3d {
namespace native {
    /// Logical encoded image admission; excludes buffers, temporaries and allocator overhead.
    struct FbxMaterialImagePlan {
        static constexpr size_t maximumEncodedBytes = 64 * 1024 * 1024;
        size_t encodedImageBytes;
        size_t imageCount;
    };
    /// Admit six optional encoded PNG lengths without allocating or inspecting image data.
    /// @param mapBytes Channel lengths; zero means absent, nonzero must fit a PNG signature.
    /// @param hasUvs Whether the mesh reports UVs; the scene builder separately validates completeness.
    /// @return Aggregate bytes and present map count, at most64MiB and six maps.
    /// @throws DocumentError LIMIT_EXCEEDED for aggregate bytes, INVALID_DOCUMENT for short maps/missing UVs.
    /// Does not validate PNG contents or decode pixels. Example: planFbxMaterialImages({}, false).
    FbxMaterialImagePlan planFbxMaterialImages(const std::array<size_t, 6>& mapBytes, bool hasUvs);
}
}
#endif
