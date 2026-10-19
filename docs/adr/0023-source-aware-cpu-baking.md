# 0023: Source-aware CPU texture baking

Status: Proposed implementation for core #56; native adapter adoption remains pending.

## Context

The CPU sampler and ordinal atlas layout now preserve imported source semantics.
A bake must sample authored UVs rather than packed output coordinates, multiply
linear material/vertex/texture colors once, retain material draw policy and compose
existing procedural output. Reusing the legacy encoded-color painter for imported
texels would change colors and lose repeat/mip/alpha information.

## Decision

Add a portable `SourceTextureBaker` consuming an owned SourceUvAtlas, aligned caller-
resolved corner colors, exact-key material records and borrowed immutable sampler
owners. The caller explicitly declares whether imported corner RGB is linear glTF
vertex color or encoded component fallback color. Decode before interpolation;
apply linear base-color and sampled texture factors once, then encode output RGB.
Alpha is straight and linear. Opaque forces one; mask/blend retain the combined
alpha and a per-triangle policy with cutoff and sidedness. Do not threshold mask
alpha during baking, because subsequent texture filtering still needs those values.

Derive color mip LOD from the affine authored-UV derivatives per destination texel
and original sampler dimensions. Constant UVs select magnification. Retain authored
repeat coordinates. Reject nonfinite footprint/interpolation arithmetic. Rasterize
normalized packed triangles in original corner order, supporting either winding.

Return owned color and emissive sRGB maps, linear metallic/roughness maps, optional
normal output and aligned optional triangle policies. Unknown procedural surfaces
remain explicitly policy-unknown. Legacy encoded images are resampled with nearest
filtering through their independent region transform, preserving the existing
procedural color policy. Missing legacy scalar maps use metallic zero, roughness
one and emissive zero. A legacy normal map enables normal output; imports use flat
tangent normals because supported source materials have no normal texture channel.

For a transposed legacy region, both tangent-normal XY channels swap and negate.
The baker's tangent convention has bitangent opposite increasing top-down V. If
old basis is (T,B,N), transposition produces T'=-B and B'=-T; preserving the world
normal therefore requires (x',y',z')=(-y,-x,z). Encoded channel complementation
preserves this relation including byte quantization.

## Bounds and failure behavior

Validate material enums/factors, image spans, coherent chart ownership, finite
normalized output UVs and source colors. A sweep detects overlapping chart rectangles
in O(n log n). A second sweep requires disjoint one-texel collars at the smallest
output resolution. Rectangle/collar ownership is assigned before color filling;
gap ownership then expands from those rectangles. Dilation never crosses an owner,
including alpha-zero source texels and uncovered triangle corners. Shared output
texel claims are rejected rather than silently selecting a source. Each imported triangle and the legacy region must cover a texel in every
enabled output map. Insufficient resolution fails explicitly and asks for larger
map dimensions; no partially missing material result escapes.

The retained output quota defaults to and is capped at 256 MiB, including the root,
policy capacity and pixel capacities. Inputs, allocator bookkeeping and temporary
overlap/coverage/dilation workspace are excluded; this is not an RSS cap. Independent
texel-work accounting covers initialization, raster candidates, legacy composition
and dilation (250 million default, supported maximum one billion). Caller chart
limits are bounded by 100,000; material catalogs by 10,000. Cancellation is checked
through validation, between rows and every 4096 texel operations. This is cooperative
cancellation, not a hard latency guarantee. No source or cancellation pointer escapes.

The returned `maximumSamplingLod=0` is an explicit output contract: the maps support
base-level bilinear filtering. A globally generated mixed-chart mip chain can
reintroduce contamination and must not be sampled. Source-image mip selection by
authored UV derivatives remains independent and supported. Chart-isolated atlas
minification is required remaining M4 work in core #59; callers must keep the output
LOD clamp until that stage establishes safe mip footprints and padding.

## Consequences and remaining adoption

No dependency, Qt/Apple type, C ABI, document-loader or Apple pin change is needed.
The existing procedural baker remains unchanged. Native integration must resolve
encoded-image decoding with owned caller buffers, map glTF sampler/material catalogs,
prepare the procedural base, retain final float-filtered triangle alignment and
publish these maps and policies through a bounded owned API. Apple rendering/export
adoption and independent textured re-import remain required. Core #41 and Apple #143
stay open. Atlas density, large-model throughput and broader texture channels remain
explicit production acceptance work.

Tests use independent numeric colors, alpha policies, repeated/mip UVs, scalar and
emissive values, both windings, mixed legacy composition, tangent transposition,
transparent gutters and typed malformed/quota/resolution failures. The actual MIT
.ds3 fixture runs through document decoding, generation and layout before baking
with a synthetic decoded image; it does not claim PNG decoding inside the core.
