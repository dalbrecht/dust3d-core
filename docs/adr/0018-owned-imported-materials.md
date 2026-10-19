# ADR 0018: Owned imported-material inspection

Status: locally verified for core #46; automated review and hosted CI pending.

Core #43 established required embedded-model geometry previews. Applying imported
materials also needs a safe way for Apple code to copy encoded images and glTF
factors without exposing C++ owners or adding Apple/Qt decoding to the core.

An opt-in C API returns an immutable `Dust3DImportedAssets` owner. Required models
use the same bounded archive/GLB resolver as geometry. Inspection does not generate
a mesh, decode pixels, apply materials, or invoke optional generation caches. The
shared engine lock still serializes inherited globals. Input remains unchanged and
is borrowed only during loading.

Each unique normalized model reference owns one material record and encoded image
buffer. Scalar factors retain double precision and unpremultiplied source values.
Alpha mode/cutoff, sidedness and sampler filter presence/wrapping remain explicit.
The MIME declaration is not a claim that the encoded image is valid. Borrowed
string/byte views remain valid until their owner is destroyed; independent Swift
copies are required before that point. Existing geometry warnings and UV/bake
import rejection remain in place.

Bindings retain original opaque component/part spellings. Distinct accepted UUID
aliases do not collapse. Multiple instances share a model index while retaining
their separate bindings; an imported part with no component has a binding with a
null component ID and a valid part ID. Unused procedural import metadata does not
become a required asset.

Material inspection accepts an empty graph because an empty editor document has
a valid empty material set. A private reader/validator option relaxes only the need
for geometry: identifier, number, graph and reference validation still run.
Geometry-loading entry points retain their existing nonempty requirement.

The output logical quota defaults to 64 MiB and has a 256 MiB hard maximum. It
charges the owner, reserved record capacities, owned identity/string capacities,
and encoded image capacity. Model geometry is discarded before retaining material
records. Container/allocator bookkeeping, input/decoder allocations, temporary
indexes and process RSS are excluded. Existing per-GLB and aggregate decoded-owner
limits remain separate. Required asset validation precedes output-quota checks;
invalid caller options are rejected at the entry boundary. Counts are charged
before reserve/summing, and actual extra capacity is charged afterward.

Alternatives considered:
- Decode PNG/JPEG in portable C++: would add a decoder dependency and duplicate the
  established Apple ImageIO boundary. No new dependency is needed for inspection.
- Expose `GlbAsset`/STL or borrow archive bytes: violates C ownership and prevents
  independent output lifetime.
- Extend ordinary geometry loads to imply material support: hides the distinction
  between transferred metadata and applied/rendered materials.

The next stages still require bounded Swift copies/ImageIO decoding, corner
UV/material ownership, imported-image/scalar UV and bake integration, PBR/alpha
rendering, a reviewed Apple pin, file preview/edit/save evidence and textured
export comparisons. This API does not complete #41 or M4.
