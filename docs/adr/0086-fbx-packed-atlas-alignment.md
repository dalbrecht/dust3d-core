# ADR 0086: Align original FBX geometry with packed atlas UVs

Status: Delivery validated locally; review and full M4/M7 acceptance pending (#99).

Original generated local UVs cannot address a composed material atlas. Add a pure
selection operation that copies every original face with the packed atlas's
double corner UVs. Require matching face counts and every known marker exactly1;
reject missing/unknown/invalid markers rather than silently export an unmapped
textured face. Geometry, normal and UV validation and ownership quotas continue
through the existing selection implementation. Source geometry stays unchanged.

The caller establishes common source provenance; equal counts cannot establish
identity. The generation hook obtains the atlas from the UvMetadata prepared
from that same original Object in the synchronous call. A new opt-in flag requires
both geometry and UV outputs before generation. Default and geometry-only callers
remain unchanged. The export owner is published only after complete preparation
and cancellation checks, preserving any prior output owner on rejection.

This uses the ordinary procedural UvMetadata atlas. It is not the imported/source-
baked atlas with per-triangle alpha/double-sided policies; source-aware material
grouping remains required. Unknown mappings reject explicitly rather than claim
full material support. No C ABI, output file, app dependency pin, rig/animation
or quarantined GLB changes are made.

## Initial local verification and remaining work

The missing atlas API began red. Source guard and formatting checks pass; fresh
Debug build and75/75 CTest contracts pass (16.63s). Existing selection tests add
packed-corner preservation, source order, missing arrays, unknown0 and invalid2
markers. The new pure helper measures13/13 lines,10/10 regions and8/8 branch
outcomes using its instrumented object and an uninstrumented driver. This is not
whole-core coverage or independent acceptance of the full exporter.

The ordinary licensed Capsule generation contract now runs a real64-square
procedural color bake. It compares every exported UV against the same original-
face packed atlas, preserving double values rather than using float preview
records. Missing geometry/UV output configurations reject with typed causes;
previous output ownership survives rejection. Existing geometry-only and default
calls still pass. No new fixture, asset decoder or package dependency is added.

Fresh strict-warning scoped ASan/UBSan builds instrument the changed bridge,
geometry selection, atlas helper and each test driver; both drivers pass against
fresh ordinary inherited core libraries. Those inherited libraries are not
instrumented by this scoped run. SwiftPM builds in3.25s with the inherited UUID
narrowing warning. No fault, crash, resource-exhaustion or quarantined fixture
probes run.

The existing independent selected-face and authored-material importer checks
remain in their earlier ADRs; no new independent generated/baked-scene re-import
claim is made here. Encoding actual baked images, source-aware material groups,
owned C ABI, atomic destination writes, cancellation/progress, UI/CLI commands
and independent textured generated-scene acceptance remain open. M7 export
parity and the complete editor plan are not established by this integration.

## Delivery revalidation

Prepared on tested original-geometry delivery `51c1fc70`, retaining reviewed
worker/writer contracts and tested FBX prerequisites. Existing selection and
Capsule generation contracts extend their real behavior checks; no new fixture
or unsafe test target is registered. Fresh ordinary build and 82/82 CTest
contracts passed (17.15 seconds). A fresh full ASan/UBSan rebuild and all 81
sanitizer-mode contracts passed (54.40 seconds), including the real 64-square
procedural bake and exact copied packed-UV comparison. The additional ordinary
Swift consumer is excluded in sanitizer mode. Instrumentation flags for all core
libraries are retained with local provenance; no leak-sanitizer claim is made.

SwiftPM built (4.00 seconds), and manifest lint, formatting, source quarantine and
Qt boundary passed. Earlier 75-case, scoped instrumentation and helper coverage
measurements above are historical and were not remeasured. These are configured
ordinary-fixture tests, not whole-core coverage or universal safety acceptance.
No fault, exhausted-resource, signal, crash or quarantined-fixture probe ran.
No new independent generated/baked FBX import is claimed here. Source-aware
material grouping, image encoding, caught C ownership, app/CLI adoption and full
M4/M7 acceptance remain pending; production pins and quarantines are unchanged.
