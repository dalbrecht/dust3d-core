// SPDX-License-Identifier: MIT
// Atlas construction derived from Dust3D, Copyright (c) 2018 Dust3D Project.
#ifndef DUST3D_UV_UV_ATLAS_BUILDER_H_
#define DUST3D_UV_UV_ATLAS_BUILDER_H_

#include <cstddef>
#include <cstdint>
#include <dust3d/uv/uv_map_packer.h>
#include <optional>
#include <stdexcept>

namespace dust3d {
class Object;

enum class UvAtlasErrorCode {
    invalidInput,
    limitExceeded,
    packingFailed,
    cancelled
};

/// Typed input, quota or atlas failure. Allocation failures may also throw std::bad_alloc.
class UvAtlasError : public std::runtime_error {
public:
    UvAtlasError(UvAtlasErrorCode code, const char* message)
        : std::runtime_error(message)
        , m_code(code)
    {
    }
    UvAtlasErrorCode code() const noexcept { return m_code; }

private:
    UvAtlasErrorCode m_code;
};

/// Image identity/dimensions resolved by the caller; no pixels or registry are borrowed.
struct UvAtlasImage {
    Uuid id;
    size_t width = 0;
    size_t height = 0;
};

/// Explicit component inputs replace Qt color parsing and global image lookups.
struct UvAtlasComponent {
    Color color = Color(1, 1, 1);
    std::optional<UvAtlasImage> image;
};

struct UvAtlasOptions {
    size_t textureSize = 4096;
    size_t maximumPayloadBytes = 0; // Zero selects 64 MiB; maximum 256 MiB.
    bool usePackedChartExtents = false; // Opt in to chart-contained UVs for safe mip ownership.
};

/// A 512-square horizontal gradient, identified by layout ordinal rather than a global image UUID.
struct UvAtlasGradient {
    size_t layoutIndex = 0;
    Color largeColor;
    Color smallColor;
    size_t resolution = 512;
};

/// Independently owned atlas data. Null component IDs and zero known markers remain explicit.
struct UvAtlas {
    std::vector<UvMapPacker::Layout> layouts;
    std::vector<UvAtlasGradient> gradients;
    std::vector<std::array<Vector2, 3>> triangleUvs;
    std::vector<uint8_t> triangleUvKnown;
    std::vector<Uuid> triangleComponentIds;
    double packedTextureSize = 0;
    size_t payloadBytes = 0;
};

class UvAtlasBuilder {
public:
    /// Build inherited atlas metadata without changing the object or using a UI/image framework.
    /// @param object Generated triangles, normalized [0,1] local charts and optional component assignments.
    /// @param components Resolved source colors and image metadata; absent components do not gain charts.
    /// @param options Texture scale (1..8192) and logical output budget (zero defaults to 64 MiB).
    /// @return Owned layouts, seam descriptors and triangle-aligned corner/ownership arrays.
    /// @throws UvAtlasError Invalid geometry/metadata, exceeded quota or nonfinite packed output.
    /// Output accounting includes array/map elements and layout/gradient records, not allocator
    /// overhead, capacity or packing workspace. It is not a hard working-memory/time limit.
    /// Gradient layouts have null image IDs: consumers use the gradient layout ordinal to bake them.
    /// Same-winding cyclic triangle order is resolved and UVs return in actual corner order.
    /// Unknown mappings remain marked zero; a known marker does not assert unique source ownership.
    /// Example: `auto atlas = UvAtlasBuilder::build(object, components);` then inspect
    /// `atlas.triangleUvKnown` before treating a generated surface as fully mapped.
    static UvAtlas build(const Object& object, const std::map<Uuid, UvAtlasComponent>& components, const UvAtlasOptions& options = {});
};
}
#endif
