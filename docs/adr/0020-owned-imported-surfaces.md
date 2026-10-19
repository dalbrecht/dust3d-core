# ADR 0020: Owned imported surface source attributes

Status: accepted for implementation; tracked by #50.

## Context

Imported source UVs and model identities now survive generation internally.
Consumers need these facts alongside copied material/image metadata, without
confusing repeated authored UVs with normalized packed atlas coordinates. The
existing atlas boundary requires local coordinates in 0–1 and cannot safely be
used as an imported source channel.

## Decision

Add an opt-in C owner containing ordinary generated geometry and one source
record per triangle retained after float conversion and degeneracy filtering.
Each record carries optional authored UVs, straight source RGBA, explicit RGB
encoding, and model/component catalog indices. Resolved model identities remain
separate from original/generated component catalog membership. Unknown source
faces remain explicit; do not infer a model or UV from nearby geometry.

Capture the glTF COLOR_0 encoding policy before imported geometry is moved into
the generator. glTF vertex RGB is linear; document/default RGB is sRGB. No
conversion, premultiplication or material multiplication occurs in this API.
Alpha remains linear. Boolean interpolation retains the generator's numerical
behavior; this boundary does not assert physically correct cross-encoding
blending for unknown source faces.

Use a named internal GenerationOutputs bundle for optional generation observers
rather than extending positional source/UV/bake parameters again. Existing callers
keep their prior behavior. The shared sourceCornerColor helper ensures source
metadata uses the same generator colors as ordinary geometry, while preserving
source alpha independently of opaque preview policy.

Validate sized options, finite values and aligned channels; serialize generation;
catch exceptions at the C boundary. Borrowed views expire only with their owner.
Charge actual retained capacities, owner roots and catalog strings against a
64 MiB default / 256 MiB maximum quota. Generation workspace, allocator overhead
and decoder quotas are separate; this is not a process-memory limit.

## Consequences

Swift can adopt source facts through a reviewed exact dependency pin. Texture
sampling, imported atlas integration, baking, PBR rendering and export still need
separate implementations and acceptance evidence. Existing UV/bake rejection
remains in place until those paths can handle imported material semantics.
No public binary ABI stability or full inherited coverage is claimed.
