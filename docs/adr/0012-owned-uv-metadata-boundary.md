# ADR 0012: Owned UV metadata at the C boundary

Status: Accepted for implementation; core issue #33.

## Context

ADR 0011 exposes portable atlas metadata only through C++. Swift needs owned C
buffers. The native geometry pipeline can remove triangles that collapse when
converted to float: original Object UV ordinals cannot simply accompany exported
vertices. Original component spellings can also differ from reconstructed engine
UUID strings, and multiple original spellings can share one engine identity.

## Decision

Add an opt-in Dust3DUvMesh owner and explicit, source-matched options. It owns
ordinary geometry, the retained portable atlas, C triangle/chart/gradient arrays
and original component keys. Existing geometry, indexed and sourced loads keep
their behavior and costs. A narrow optional pipeline sink prepares atlas metadata
while generated Object/Snapshot owners exist, then appends UV records only after
the same float-collapse filter used for geometry. Snapshot ownership transfers to
the generator only after its constructor succeeds.

Capture original component membership before preprocessing. Return original keys
only for unique membership; alias collisions are AMBIGUOUS and generated identities
absent from membership are UNKNOWN. Chart applicability and component association
are independent. Preserve inherited propagation priority without claiming unique
Boolean surface ownership. Contradictory alias visual inputs reject explicitly.

Parse supported source colors strictly as #RRGGBB/#AARRGGBB before inherited
generation, then resolve actual generated colors again for wrap outputs. Preserve
alpha and source channel encoding without pretending to perform color management.
Named/future encodings fail this opt-in call with UNSUPPORTED_ASSET; ordinary
preview and lossless archive preservation remain independent. Image dimensions
and identities arrive as bounded caller descriptors; no Qt registry or decoder.
Missing dimensions cannot silently turn textured components into solid charts.

One logical metadata quota charges the retained atlas plus C metadata, original
keys and identity lookup records; geometry, allocator/capacity overhead and
transient workspace are excluded. Zero selects 64 MiB, maximum 256 MiB. All C
entry points catch exceptions and share the existing serialized generation lock.
Views expire with the owner; independent indexed geometry copies survive it.

## Alternatives and consequences

Exposing STL or retaining borrowed Object pointers would violate the C boundary.
Flattening original ordinals without the geometry filter would corrupt alignment.
Reconstructing original strings from UUIDs would lose legacy spellings. Reusing
an existing indexed mesh as a textured mesh would merge UV seams because its
sharing key currently contains geometry attributes only; future textured indices
must include UV/material attributes.

This prototype adds a source API, not a stable binary ABI. It is stateless and
opt-in; cache/session integration can follow after correctness. It adds no new
dependency or CI configuration. CPU pixel baking, Apple SDK/editor adoption,
PBR/transparency, decoding/import/export and full Qt acceptance remain open.
