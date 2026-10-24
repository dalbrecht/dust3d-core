# Changelog

## Unreleased

- Restore bounded owned FBX7400 container encoding and known-positive structural compatibility tests against the existing reference codec (#99).

- Restore immutable owned FBX node trees with checked depth, count, retained capacity and encoded-size metadata (#99).

- Restore bounded, owned FBX7400 property encoding with explicit little-endian bytes and append-state preservation (#99).

- Restore checked rigid-transform validation and FBX XYZ Euler extraction as pure value operations, with explicit column-major matrix ownership (#99).

- Restore source-audited, bounded FBX time and Euler-angle conventions independently of serializers and quarantined animation evaluation (#99).

- Bound snapshot XML component traversal and encoded output, preserve ordinary bytes, and stage writes before publication; quarantined writer failure fixtures remain disabled (#83).

- Add parent-side supervised resource launch and bounded report admission with owned original completion errors; Apple adoption and strong containment remain pending (#123).

- Add an explicit versioned helper resource launch with compiled-runtime checks, helper-only limit installation/readback and ordinary fixture acceptance; production profiles and Apple adoption remain pending (#123).

- Add private worker resource setup and checked Unix conversion libraries with original OS-error preservation; helper activation remains pending (#123).

- Add pure worker resource-profile planning with explicit finite units, inherited-limit preservation and sanitizer compatibility checks; OS application and production adoption remain pending (#123).

- Add an opt-in supervised Unix operation with pre-start cancellation and retained retirement outcomes; active OS termination remains unverified and production adoption is pending (#123).

- Add a pure bounded worker-lifetime decision policy with single-use stop/drain actions and publication retirement; process integration remains pending (#123).

- Bound worker scheduling to one active request and one latest waiting archive, retiring stale completions before typed reply admission (#115).

- Add a development Unix parent transport with concurrent bounded channel transfer, original diagnostics and observed process completion (#114). Production lifecycle policy remains pending.

- Add single-use worker completion admission requiring clean exit, current identity and validated owned payloads (#113).

- Add a Unix single-job native helper with bounded framed input, isolated binary output and separately testable native execution/status mapping (#112). Parent transport and production containment remain pending; pins and quarantines are unchanged.

- Add typed worker non-success payloads that preserve bounded original diagnostic bytes, including non-UTF-8 causes, and distinguish no-geometry/cancellation outcomes (#111). Process transport remains pending; pins and quarantines are unchanged.

- Add explicit owned worker base-mesh payload encoding and semantic admission for geometry, source counts and bounded UTF-8 warnings (#110). Process transport and source-aware results remain separate; no pin or quarantine changes.

- Add a separately tested portable worker frame codec with versioned byte order, bounded lengths and document/request/revision checks (#109). Process isolation and result-schema admission remain pending; quarantines and pins are unchanged.

- Stop debug stitching generation from automatically creating or overwriting a global SVG outside caller-owned outputs (#101). Geometry and explicit exports are unchanged.

- Expose opt-in owned native atlas mip levels with borrowed per-level views, preserved procedural island ownership and aggregate output accounting (#59). The existing loader stays base-only; Apple adoption remains pending.

- Add opt-in owned atlas mip chains with isolated imported and procedural islands, linear-light color filtering, normalized normals, actual per-map LOD admission and cumulative quotas (#59). Existing native sampling limits and Apple pins remain unchanged.

- Preserve typed invalid-document errors, geometry field names and original numeric conversion diagnostics for malformed or out-of-range scalar metadata (#104).

- Quarantine standalone and embedded allocation-failure probes from default builds, retaining ordinary ownership checks and non-injecting JSON comparison contracts with compile-time signature checks (#84).

- Propagate allocation failure from glTF JSON parsing and string comparisons instead of terminating the process. Object, array, and string payloads are published before their type, and string literals are no longer no-throw numeric scalars (#69).

- Block accidental restoration of the quarantined experimental GLB material C API, including its SwiftPM and CMake source-list entries, during configuration (#69).

- Preserve the full finite nonnegative glTF alpha-cutoff range in source baking, including mask cutoffs above one, while retaining raw combined alpha and exact owned policy metadata (#61).

- Add an owned opt-in native source-bake C API with caller-decoded imported images, exact final geometry/UV/material alignment, procedural composition, bounded ownership and explicit atlas LOD metadata. Existing load/bake contracts and Apple pins remain unchanged (#58).

- Add owned CPU source-aware atlas baking with linear imported color factors, derivative mip selection, per-triangle alpha/sidedness policies, PBR maps and legacy-map composition. Isolate chart gutters, require bilinear padding and expose the output LOD-zero limit; chart-isolated atlas mips remain #59. Native/Apple adoption remains pending (#56).

- Add ordinal imported atlas layout with independent authored UVs, exact model/component identities, geometry-derived non-overlapping charts and a separate procedural atlas transform. Native bake adoption remains pending (#54).

- Add bounded, owned CPU sRGB color sampling with authored wrapping, all glTF filter modes, straight alpha and odd-dimension mip reduction as an imported-baking foundation. Atlas/bake integration remains pending (#52).

- Add an owned C surface-source API with float-retained triangle alignment, authored repeat UVs, straight RGBA with explicit linear/sRGB encoding, resolved model/component catalogs and bounded retained ownership. Atlas sampling, baking and Apple adoption remain separate.

- Preserve imported per-corner UV seams and resolved model ownership through spine deformation, mirrored winding, Boolean clipping, recombination, previews and retained caches. Public imported UV/bake rendering integration remains pending.

- Add an opt-in owned C material/image inspection API for required imported models,
  preserving original instance bindings, glTF factors/samplers and encoded storage
  under a separate output quota; pixel decoding and application adoption remain
  subsequent stages.


- Sweep imported geometry across circular spines' closing edges with periodic cross-section frames and matching normal derivatives.

- Apply authored-versus-smoothed imported normals per component instance, including coincident instances sharing the same part.

- Reject nonfinite/out-of-range position quantization before integer conversion, and preserve distinct imported component fallback colors when multiple instances share one part.

- Preserve imported vertex-color seams, per-node directional scales and component-scoped source hints, including distinct original component aliases.

- Preserve imported colors and normals per owning component through Boolean/cache aggregation, including coincident uncombined parts, component-color fallback and group previews.

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
