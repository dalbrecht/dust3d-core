# ADR 0016: Bounded internal GLB decoding

Status: implementation in progress under [core #41](https://github.com/dalbrecht/dust3d-core/issues/41).

The Qt application imports GLB into `MeshGenerator::ImportedModelData`, which the
existing core sweeps along a part spine. Native document loading currently rejects
embedded models. This stage builds the private decoding boundary; it does not
remove either document rejection or advertise end-to-end import support.

The decoder performs no file or network access and returns independent C++ owners.
Input spans expire at return. Errors use the existing typed `DocumentError` status
and original diagnostic. No new C ABI, Swift API, or binary compatibility promise
is introduced. Future adoption must use the engine serialization lock, preserve
archive bytes and account for imported asset revisions and retained payload.

Reuse the already vendored MIT nlohmann JSON 3.1.2 header. Move the unchanged header
and license into `third_party/json`; a forwarding header retains the Qt source
include path. This keeps a portable target from including an application-owned
implementation. No download or version upgrade is introduced. Alternatives were
a new glTF dependency or a second JSON implementation. The inherited version's
age remains a maintenance concern; parser bounds and sanitizer contracts do not
prove the absence of all vulnerabilities.

Initial limits: 64 MiB input, 1 MiB JSON, nesting depth 64, 100,000 parser callback
events, 250,000 aggregate vertices, 500,000 aggregate triangles and 16 MiB encoded
image data. These bound this decoder, not total process memory, allocator overhead,
platform image decoding or inherited geometry generation. Position magnitudes
also stay within checked bounds for the inherited long-based PositionKey.

The initial representation is one mesh and one shared primitive material, with
triangle primitives, finite positions/normals/UVs, unsigned indices and float or
normalized unsigned vertex colors. Missing normals are generated. Absent vertex
colors stay absent to preserve inherited part colors; mixed primitives use neutral
aligned defaults. Material factors, alpha mode/cutoff, double-sided
state, emissive color and sampler filter/wrap enums remain explicit owned metadata.
The decoder does not render them or synthesize texture pixels. Embedded image
bytes remain encoded; platform decoding and image-envelope validation are separate
boundaries. Scene hierarchy, nonidentity transforms, skinning, morph targets,
sparse accessors and unsupported required extensions fail explicitly.

Format authority: [Khronos glTF 2.0 specification](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html).
Reference probes against the inherited reader demonstrate ignored translations,
incorrect vertex strides and first-material selection. Native tests deliberately
expect correct supported stride/material behavior rather than reproducing those
observations. UInt8 indices are an explicit supported improvement over the observed
Qt reader. This does not establish scene, deformation, texture or export parity.

Material/sampler/scene, positive attribute/image ownership and malformed layout
contracts are covered by the current tests. Local final-state validation passes;
fresh automated review/CI remain required before merge. Document/generation integration
and the reviewed Apple pin update remain subsequent stages of issue #41.
