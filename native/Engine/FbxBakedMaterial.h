// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_FBX_BAKED_MATERIAL_H_
#define DUST3D_NATIVE_FBX_BAKED_MATERIAL_H_
#include "FbxMeshScene.h"
#include <dust3d/uv/texture_baker.h>
namespace dust3d {
namespace native {
    /// Borrowed baked RGBA8 images: color, normal, metallic, roughness, emissive, AO.
    /// Pointees stay alive/unchanged through makeFbxBakedMaterial; null means absent.
    using FbxBakedImageViews = std::array<const TextureImage*, 6>;
    /// Encode baked maps into an independent single-atlas material without copying raw pixels.
    /// @param images Complete baked color map and optional aligned material-map images.
    /// Caller establishes common atlas provenance; differing map resolutions are supported.
    /// @param maximumBytes Aggregate retained PNG buffer-capacity budget, zero64MiB; max64MiB.
    /// @return Owned PNG maps with neutral color/opacity/roughness factors and metallic1 when mapped,
    /// otherwise dielectric metallic0. No absent channel is synthesized; emission is map-driven.
    /// @throws DocumentError INVALID_DOCUMENT for missing color/invalid RGBA spans;
    /// LIMIT_EXCEEDED for dimensions/quotas; compression errors retain their zlib status.
    /// Allocation failures propagate. Input pixels, compressor workspace and later FBX bytes
    /// are excluded from this logical budget. No asset decoding, files, globals or native calls.
    /// Example: makeFbxBakedMaterial({ &color, nullptr, &metallic, &roughness, nullptr, nullptr }).
    FbxSceneMaterial makeFbxBakedMaterial(const FbxBakedImageViews& images, size_t maximumBytes = 0);
}
}
#endif
