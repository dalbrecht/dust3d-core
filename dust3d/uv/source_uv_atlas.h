// SPDX-License-Identifier: MIT
#ifndef DUST3D_UV_SOURCE_UV_ATLAS_H_
#define DUST3D_UV_SOURCE_UV_ATLAS_H_
#include <atomic>
#include <dust3d/uv/uv_atlas_builder.h>
#include <string>
namespace dust3d {
/// Normalized packed chart. Transposition preserves corner identity when rotated.
struct SourceUvChart {
    double left = 0;
    double top = 0;
    double width = 1;
    double height = 1;
    bool flipped = false;
    /// Transform a finite unit-square local coordinate; throws UvAtlasError on invalid bounds/input.
    Vector2 map(const Vector2& coordinate) const;
};
struct SourceUvTriangle {
    std::array<Vector2, 3> packedUvs {};
    std::optional<std::array<Vector2, 3>> authoredUvs;
    std::optional<SourceUvChart> importedChart;
    std::string modelKey;
    std::string componentKey;
    bool known = false;
};
struct SourceUvAtlas {
    std::vector<SourceUvTriangle> triangles;
    std::optional<SourceUvChart> legacyChart;
    double packedTextureSize = 0;
    size_t payloadBytes = 0;
};
struct SourceUvAtlasOptions {
    size_t textureSize = 4096;
    size_t maximumPayloadBytes = 0; // Zero selects 64 MiB; maximum 256 MiB.
    size_t maximumChartCount = 50000; // Includes the legacy region; supported range 1..100000.
    double paddingFraction = 0; // Zero retains the inherited per-side packing padding.
    const std::atomic_bool* cancellation = nullptr; // Borrowed only during build().
};
class SourceUvAtlasBuilder {
public:
    /// Pack ordinal-indexed imported triangles alongside one unchanged legacy atlas region.
    /// @param object Original triangles with optional aligned raw model/component keys and authored UVs.
    /// Nonempty model keys identify imports, including textureless models. Imported geometry must
    /// be nondegenerate; source UVs may repeat outside [0,1] and are never normalized in place.
    /// @param legacy Existing normalized procedural atlas; triangle arrays are empty or original-count aligned.
    /// @param options Texture scale, chart-count bound, logical retained quota and cooperative cancellation.
    /// @return Owned identities, authored UVs, packed UVs and explicit per-import/legacy transforms.
    /// Legacy-only known mappings remain unchanged. Unknown procedural triangles remain unknown.
    /// @throws UvAtlasError Invalid/incomplete channels, exceeded quota/count, cancellation or packing failure.
    /// Allocation failures may throw std::bad_alloc. Payload includes root, triangle capacity and
    /// string capacities, excluding allocator overhead, inputs and temporary packing workspace.
    /// Cancellation checkpoints surround each bounded packing attempt, not the packer's inner loops.
    /// Per-triangle charts prioritize source correctness over atlas utilization; no GPU pixel parity claim.
    /// Example: `auto mapped = SourceUvAtlasBuilder::build(object, legacyAtlas);`
    static SourceUvAtlas build(const Object& object, const UvAtlas& legacy = {}, const SourceUvAtlasOptions& options = {});
};
}
#endif
