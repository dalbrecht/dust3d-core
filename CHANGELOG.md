# Changelog

## Unreleased

- Transform imported normals with the local spine Jacobian, including cut rotation, nonuniform scales and taper; singular deformations retain geometry-derived normals.

- Keep imported-model spine width and thickness scales independent, so zero width produces finite flattened geometry without a division by zero.

- Resolve required embedded GLB assets for expanded/sourced/session geometry previews with an aggregate decoded-owner quota, independent outputs, same-UUID cache invalidation and explicit material limitations; UV/bake and Apple adoption remain separate.

- Add a private bounded Qt-free GLB mesh/material decoder with owned image bytes and malformed-layout contracts; embedded `.ds3` model generation and Apple adoption remain separate stages.

- Add an opt-in owned C baked-mesh boundary with borrowed RGBA8 views, float-aligned tangent records, independent UV/bake quotas, legacy comma-decimal settings and one-shot cooperative cancellation; Apple renderer adoption remains separate.

- Add borrowed fold-evaluation checkpoints so dense overlapping segment queries and all normal derivative samples can cooperate with cancellation while preserving original evaluation APIs.

- Add opt-in synchronous construction checkpoints to fold and surface-pattern kernels; portable baking can cancel preparation, grid insertion/sorting and covariance work while legacy constructors remain available.

- Add a portable C++ CPU texture-bake API with owned RGBA8 maps, explicit image views, material scalars, world-space patterns/fold normals and triangle tangents; C/Swift adoption and renderer integration remain separate.

- Add optional isolated offscreen Qt bake reference probes, including numerical portable UV comparison, procedural maps and explicit alpha/material observations; native targets remain Qt-free.

- Add opt-in owned C UV metadata with float-retained triangle alignment, original component identities, explicit ambiguity, bounded resolved image descriptors and strict Swift 6 copy consumers; baking and Apple app adoption remain separate.

- Add owned portable C++ UV atlas metadata with explicit missing mappings, seam-gradient descriptors, cyclic corner ordering and checked logical output budgets; pixel baking and C/Swift adoption follow separately.

- Fix UV rectangle area/contact-score overflow, validate quantized chart dimensions before conversion, reject charts lost to padding precision, and clear stale layouts after packing failure.

- Add sourced generation-session loads with actual component cache reuse, current original identities, independent output budgets/lifetimes and preserved cache-only fallback diagnostics.

- Add opt-in bounded per-corner source hints with original opaque document keys, complete wrapping weights, explicit unknown/ambiguous identities, independent geometry ownership and C/Swift consumers.

- Add optional bounded generation-session C owners with semantic invalidation, exact asset revisions, actual cache reuse metrics, independent mesh lifetime, and fresh fallback for cache-only failures.

- Add checked retained-payload accounting for actual geometry/cache owners, reserved buffers, nested metadata and wrapping results, with explicit borrowed-view handling before persistent sessions are enabled.

- Add a portable semantic snapshot-invalidation foundation for native component caches, deriving old/new ownership and dependency changes without trusting saved dirty flags. Persistent cache activation follows separately.

- Add optional independently owned indexed C ABI buffers with full-attribute sharing, copied source metadata, explicit payload budgets and portable lifetime/seam/concurrency contracts.

- Omit triangles that collapse at exported float precision, preserve configured Cloth opening topology, and report a specific generation failure when an entire mesh becomes unrepresentable.

- Keep parallel/near-parallel vector angles finite at floating-point dot-product boundaries, repairing Cloth opening and straight-profile normal generation without weakening native finite-output validation.

- Reject malformed and nonfinite values for every wrapping scalar at the native archive boundary, preserving finite legacy values and inherited generator clamping.

- Extend portable C ABI contracts for configured stitching closures, segment counts, loop caps and isolated/connected fill behavior across the existing Linux/macOS sanitizer matrix.

- Distinguish successful generation with no geometry from genuine generation failures through an explicit C API status and portable ownership/error regression tests.

- Accept legacy comma-decimal numeric attributes at the C boundary while retaining strict input and geometry validation.

- Fix procedural boolean generation where intersection loops share a vertex, including rounded crossing parts.
- Weld original and cut vertices consistently around position-grid boundaries, preventing collapsed sliver triangles.
- Add closed-edge and connected-surface regression checks for union, difference, translation, hard joins and nearby separated parts.
