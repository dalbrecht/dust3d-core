# ADR 0017: Primary embedded-model geometry resolution

Status: locally verified implementation under [core #41](https://github.com/dalbrecht/dust3d-core/issues/41), pending review and CI.

The bounded GLB decoder is now connected to geometry-preview generation. Expanded,
sourced and session mesh loads resolve required `ImportedModel` parts before any
optional cache preparation. Indexed copies keep their existing ownership contract.
No C ABI types or function signatures change.

`ImportedModels` owns decoded assets for one operation. The reader provides only
already validated archive spans, and the resolver decodes each referenced UUID
once. Missing/malformed required assets and ambiguous aliases fail the primary
load with the original status/cause. Cache observers remain noexcept and optional;
they cannot downgrade required import failures to successful cached geometry.

Required identities contain 36 hexadecimal UUID characters, optionally braced.
`models/<uuid>.glb` names bind brace aliases while preserving case. Duplicate
bindings for a required identity fail rather than choose one. Unreferenced model
assets remain opaque; known procedural roles can retain unused import metadata.
An unknown role carrying an import reference fails explicitly rather than fall
back to procedural geometry. Only the derived generation snapshot normalizes
reference braces; original archive bytes remain untouched.

The aggregate decoded-owner quota is 64 MiB. Accounting includes map values/keys,
vector capacities and nested faces/UV records, plus encoded image strings/bytes.
The existing per-GLB limits remain in force. Parser/generator workspace, binding
indexes, transfer temporaries, allocator bookkeeping and process RSS are outside
this logical quota. A decoded asset may allocate within its individual limits
before the aggregate check rejects it. This is not a hard process-memory ceiling.

Geometry is moved into the inherited generator and uses its actual spine,
width/thickness, rotation and mirror implementation. Tests establish straight
spine width deformation and finite zero-width flattening after correcting the
inherited coupled-scale division; the separate development probe covers a bent spine.
Broader modifier/differential acceptance is still required. Result buffers are
copied into their existing opaque owners and survive import-context/session
teardown. All engine entry points continue using the shared engine lock.

Existing session asset revision collection already compares complete asset names,
attributes and bytes. Any change resets retained generated components before
reuse, including replacement under the same model UUID. Primary decode happens
before optional cache work, including with retention disabled or too small.

This stage is a geometry preview. GLB materials, encoded textures and transparency
are explicitly disclosed as unapplied in the mesh warning, alongside existing
image warnings. UV/bake entry points retain their rejection until material/image
integration can be complete; they must not silently emit incomplete textured
output. Pixel decoding, material ownership through C/Swift, reviewed Apple pinning,
rendered file previews, lossless editor interactions and textured exports remain
subsequent stages of #41 and M4/M7. The Apple apps are not updated by this core PR.
