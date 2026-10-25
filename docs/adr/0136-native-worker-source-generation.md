# ADR 0136: Own native source generation before worker routing

Status: Accepted implementation, pending reviewed delivery.

## Context

Apple #279 requires source associations for node picking before worker backend
adoption. The value codec and atomic geometry/source pair in #213/#215 do not
generate or copy native source output. Existing worker base and UV adapters do
not carry these associations. This stage addresses core #217 without exposing
an opcode before its request/admission/C ownership route exists.

## Decision

Add an internal C++17 `WorkerSourceOperation` adapter. `generateSourcedMesh`
validates policies before calling the existing serialized `dust3d_sourced_mesh_load`
API. Its RAII owner holds both geometry and source storage until independent
copies finish. The configured source decoded-byte allowance also bounds native
source retention through the existing options; that native accounting is distinct
from the copied C++ logical charge and does not bound generation workspace.

`copyNativeSourcedMesh` preflights source counts, readable array presence, bounded
identity lengths, aggregate text, record/string charges and source wire size.
Checked division/subtraction precedes multiplication and aggregation. The source
logical/wire charge reduces the geometry copy policy before either result is
retained. Geometry corner/node counts must match source corners/identities.
Copy preserves all original identity bytes, corner ranges, numeric hint kinds,
sentinels and finite nonnegative weights without normalization. The complete
paired codec validates semantics and confirms exact aggregate charge before return.
No borrowed native pointer crosses the adapter boundary.

Native errors use the established typed failure mapping. Codec/generation errors
retain their bounded original diagnostic with a limit or generation status; no
partial pair is returned. Allocation of a failure reply can itself throw. Temporary
encoding buffers, container capacities and allocator overhead are outside logical
retained quotas. This is not an RSS/CPU/cancellation or isolation guarantee.

No active opcode, public C ABI, Apple pin or editor backend changes. Expanded
geometry is not a claim of indexed/material/mip parity. Source-aware request and
reply routing, owned C/Swift output, remaining parity and authenticated publication
remain required under Apple #279.

## Validation and limits

- TDD red: the new contract failed to compile for missing `WorkerSourceOperation.h`.
- Fresh licensed Capsule generation yields 240 expanded vertices and source
  corners, complete original identities, and an independently round-trippable pair.
  A second known-positive native owner supports bounded copy tests without native
  regeneration for quota rejection. Exact aggregate quotas accept; eleven smaller
  copy policies reject. Every native corner/influence value is checked, and copied
  data remains usable after both owner and input release. Null borrowed owner and
  invalid policy checks remain outside generation.
- Fresh full CMake build and source quarantine guard pass. The initial shell-sandbox
  gate passed 143/147, with four existing XPC fixtures invalidating before peer PID
  establishment. The same complete suite in its established authorized execution
  context passed **147/147 in 32.40 seconds**, including all four healthy XPC cases.
  Changed-source hashes remained unchanged across the gate.
- A fresh scoped ASan/UBSan executable instruments the new adapter and contract,
  linking existing ordinary engine libraries. It passes with leak detection
  disabled. This does not instrument or establish sanitizer coverage for the
  entire inherited engine and makes no leak-check claim.
- Scoped LLVM coverage: **97/120 lines (80.83%), 69/95 regions (72.63%),
  8/9 functions and 46/68 branch outcomes (67.65%)**. Defensive native-storage,
  native-failure and exception-mapping paths remain unexercised. This falls short
  of the standards adoption target; native failure/allocation probes are deliberately
  not introduced to raise it while quarantines remain. Pure codec coverage is
  reported separately in ADRs 0134/0135. No full engine coverage claim.
- SwiftPM `Dust3DWorkerRuntime` packaging passes in the existing shared root.
  C++ and Swift-manifest formatting and whitespace checks pass. The existing
  SwiftPM native-builder deprecation warning remains.

All #67/#69 GLB, #83 writer, #84 fault, #95 animation and Apple #199 rig-pose
quarantines remain. No malformed-native-input, crash, allocation-failure, signal,
broken-pipe, native cancellation or resource-pressure probes are added. No global
SVG, protected QA process/bundle or OS container is modified. Only the single
shared build root is used. Completed exact-head automated review and all green CI
remain required before merge. There is no UI in this stage; screenshots do not apply.
