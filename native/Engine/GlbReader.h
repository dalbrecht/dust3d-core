// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_GLB_READER_H_
#define DUST3D_NATIVE_GLB_READER_H_
#include <array>
#include <cstddef>
#include <cstdint>
#include <dust3d/mesh/mesh_generator.h>
#include <optional>
#include <string>
#include <vector>

namespace dust3d {
namespace native {
    struct GlbEncodedImage {
        std::string mimeType;
        std::vector<uint8_t> bytes;
    };
    enum class GlbAlphaMode { Opaque,
        Mask,
        Blend };
    struct GlbSampler {
        std::optional<uint32_t> magnification;
        std::optional<uint32_t> minification;
        uint32_t wrapS = 10497;
        uint32_t wrapT = 10497;
    };
    struct GlbAsset {
        MeshGenerator::ImportedModelData model;
        std::array<double, 4> baseColor { 1, 1, 1, 1 };
        double metallic = 1;
        double roughness = 1;
        std::array<double, 3> emissive { 0, 0, 0 };
        GlbAlphaMode alphaMode = GlbAlphaMode::Opaque;
        double alphaCutoff = 0.5;
        bool isDoubleSided = false;
        std::optional<GlbSampler> sampler;
        std::optional<GlbEncodedImage> image;
    };
    /// Decode a bounded, single-mesh GLB into independent owners. No file/network I/O.
    /// @param bytes Borrowed GLB storage, valid for the duration of this call.
    /// @param size Number of readable bytes, limited to 64 MiB.
    /// @return Owned mesh/material data; encoded image pixels are not decoded or validated.
    /// @throws DocumentError for malformed, unsupported or over-budget input.
    // Throws DocumentError for malformed, unsupported or over-budget input; allocation
    // failures propagate to the existing exception-containing C boundary on adoption.
    // Scene transforms, skinning, morph targets and multiple materials are explicit
    // unsupported representations, never silently flattened or omitted.
    GlbAsset readGlb(const uint8_t* bytes, size_t size);
}
}
#endif
