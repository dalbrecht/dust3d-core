# ADR 0137: Route source-aware worker results through owned C output

Status: Accepted implementation, pending reviewed delivery.

## Context

Core #213/#215/#217 provide source values, atomic geometry/source pairing and
native generation copying. Those standalone pieces cannot yet support editor
node picking through a worker. Apple #279 requires complete output parity before
backend adoption. Core #219 advances actual runtime routing without changing the
Apple pin or claiming parity for indexed geometry, imported materials or mips.

## Decision

Add operation **3, generateSourcedMesh** while retaining base/UV numbers and the
existing outer protocol version. A source request has a **24-byte big-endian
prefix**: version u32 (1), reserved u32 (0), requested source budget u64, archive
byte count u64, then exact opaque nonempty archive bytes. Zero budget selects
64MiB; values above 256MiB reject. Full framing/options/wire/retained-archive
validation precedes output allocation. Request construction never parses archives
or runs the engine. Borrowed and owned C++ encoders share this codec.

Dispatch decodes the request, validates configuration, then tightens its source
copy/native-retention allowance to the requested/default budget. It uses the
existing serialized same-owner generation adapter and returns a correlated paired
response or original typed native failure. Child, persistent-service and
queue-current admission decode source pairs under independent consumer and
aggregate limits. Parent semantic quotas remain independent of producer request
options: matching identity/header is not proof that a producer honored a source
budget. Callers retain authoritative policy as well as source/transport identity.

The C ABI adds a dedicated source request constructor, appended enum values,
source summaries, record-copy getters and borrowed UTF8 identity spans. Existing
struct layouts and numeric values remain unchanged. Source kind **4** is a new
decoded kind; failure kind **3** and no-geometry failure status **5** remain distinct.
Wrong-kind/null/index access rejects without throwing; required scalar outputs
are initialized. Source strings expire with the decoded owner. Native and frame
owners can already be released. Original unnormalized weights and fixed-width
unknown identity sentinels survive transfer.

ABI version 1 remains the existing layout version and must not be treated as
source-feature negotiation. Matching pinned client/helper sources are required;
an old helper rejects opcode 3. Internal C++ variants/signatures changed, so
source consumers must rebuild. No binary C++ compatibility claim is made.

This route does not enforce CPU, native workspace, container capacity or process
RSS bounds, authenticate caller-supplied observations, or prove failed-peer
containment. It does not activate an editor backend, add a Swift value decoder,
change the Apple pin or complete indexed/material/mip parity. No fallback that
drops source associations is introduced. Those adoption gates remain open.

## Validation and limits

- TDD red: new runtime contract failed to compile for missing WorkerSourceRequest.h.
- Pure codec tests cover opaque archive ownership, borrowed/owned equivalence,
  exact quotas, budget endpoints, unsupported layout/version, null/empty/truncated/
  trailing spans and bounded size rejection without engine parsing.
- Pure C constructor tests verify independent framed bytes and original typed
  diagnostics. Pure child/service/queue tests cover complete source admission,
  pair/source/mesh rejection without partial output, original no-geometry status,
  lifetime after collector release, UTF8 identities, unnormalized weights, unknown
  sentinels and safe null/wrong-kind/out-of-range getters. Synthetic process/EOF
  observations test admission logic, not real transport authentication.
- One fresh known-positive licensed Capsule runtime request verifies the dedicated
  C constructor, descriptor dispatch, operation/request receipt, complete paired
  response and actual 240-corner geometry/source correspondence. No malformed
  archive is passed to native generation. Existing healthy XPC cases remain the
  base/UV pilots; there is no new source-aware XPC acceptance claim.
- Initial full gate passed 147/148: a legacy raw-header test still rejected opcode
  3. Both unknown-opcode assertions now use 4; all three supported operations are
  explicitly round-tripped. After the final null-text-owner check, fresh full build,
  source quarantine and **148/148 ordinary CTests pass in 38.48 seconds** in the
  established authorized execution context. Changed-source hashes remain stable.
- Refreshed scoped ASan/UBSan uses bounded pure codec/state/ownership cases, with
  leak detection disabled. Selected adapter/runtime/codecs are instrumented; linked
  inherited engine libraries are not. No whole-engine or leak coverage claim.
- Scoped request coverage: **55/55 lines, 43/43 regions, 7/7 functions and
  33/34 branch outcomes (97.06%)**. The size_t address-space guard remains
  unexercised on this 64-bit target under supported 256MiB quotas. New source
  getter/helper functions cover **22/22 branch outcomes**. Native failure and
  allocation/exception paths remain unprobed; partial old-route coverage from
  this focused executable is not reported as total runtime/engine coverage.
- SwiftPM runtime packaging, C++/manifest formatting and whitespace checks pass.
  Existing SwiftPM native-builder deprecation remains. All builds/artifacts use
  the existing single shared root. No UI is changed; screenshots do not apply.

All core #67/#69 GLB, #83 writer, #84 fault, #95 animation and Apple #199 rig-pose
quarantines remain. No new malformed-native-input, crash, allocation-failure,
signal, broken-pipe, native cancellation or resource-pressure probes. No global
SVG, protected QA apps/processes or OS containers are modified. Completed clean
exact-head automated review and every CI check passing remain mandatory for merge.
