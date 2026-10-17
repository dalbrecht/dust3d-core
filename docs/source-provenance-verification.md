# Generated source hints: verification and limits

Tracking: core #25; design: [ADR 0008](adr/0008-generated-source-hints.md).

## Contract

`dust3d_sourced_mesh_load` is opt-in and stateless. Its independent owner lends
geometry, corner/influence arrays, and original node/part strings until destruction.
A copied indexed mesh survives that destruction. Corner i corresponds to expanded
vertex i and indexed triangle index i, including after float-degenerate triangles
are removed. It does not correspond to unique indexed vertex i.

A node hint uses the inherited position quantization (truncation at 1e-5), which
can collide. Weighted hints preserve all finite nonnegative weights as supplied,
including unknown or ambiguous individual identities. Weights are not normalized
by export. UNKNOWN means there is no trustworthy original identity in the
available map; it does not prove a surface was newly generated. AMBIGUOUS means
original keys alias to one engine identity. No exact component ownership is
claimed for Boolean surfaces. Mirrored/interpolated synthetic keys absent from the
original archive remain unknown; generated-key and position collisions can still
produce imprecise hints referencing real original keys.

Options require the exact current struct size; there is no binary ABI guarantee.
Zero source payload budget uses 64 MiB; the maximum is 256 MiB. Logical payload
charges `sizeof(SourceIdentity)` per original node, string bytes with terminators,
`sizeof(Dust3DSourceCorner)` per retained corner, and
`sizeof(Dust3DSourceInfluence)` per influence. Allocation overhead, spare capacity,
lookup workspace and geometry are excluded, so this is not a process-memory cap.
An exact charged budget succeeds and a one-byte-short budget fails descriptively.
No source output is allocated by the unchanged legacy or cached-session API.

## Tests and commands

The first internal test compile failed on missing `SourceProvenance.h` before
production implementation. The resulting `contract_source_provenance` exercises:

- Original opaque node/part keys copied before snapshot ownership transfer;
  braced/unbraced aliases, non-hex legacy IDs, unknown/generated identities.
- Multi-entry weighted associations including unknown/ambiguous entries; invalid
  weights and representable signed-long quantization bounds.
- Capsule, Sword and rounded-difference fixtures; exact source budgets and unchanged
  legacy geometry; source owner survival across other calls; independent indexed
  owner and copied metadata survival after source destruction.
- Actual Cloth output containing multi-node weights; uncombined mirrors and
  interpolated chains; a mixed near/far fixture that demonstrably filters out
  double triangles at exported float precision while preserving source alignment.
- Concurrent legacy/session/source generation through the same native lock;
  missing/short options, oversized budgets, null owners and invalid identity indices.

`contract_c_consumer` imports and calls the new API in a C translation unit.
`tests/swift/verify-session.sh` builds the package cleanly and runs both the
existing session and new source consumers with Swift 6 strict concurrency and
warnings as errors. Swift source arrays and strings are copied during ownership.

Run the ordinary CMake build/CTest suite and repeat with
`-DDUST3D_ENABLE_SANITIZERS=ON`, plus the Swift script, `clang-format` on native/test
files, `swift-format` on the package/new smoke, and the existing Qt boundary check.
Measured local results:

- Debug build and all **29 CTest contracts passed** (19.11 s).
- ASan/UBSan build and all **29 contracts passed** (62.36 s). The initial
  invocation incorrectly forced `detect_leaks=1`, which Apple's runtime rejects
  before executing any test. The successful rerun used macOS defaults; Linux
  hosted CI retains its existing leak-detection configuration.
- Clean SwiftPM build and both Swift 6 strict-concurrency consumers passed;
  Capsule copied 240 corners and 3 original identity keys after source destruction.
- C++/Swift formatting, shell syntax, diff and Qt-boundary checks passed.
- Scoped instrumented `SourceProvenance.cc`: **101/107 lines (94.39%)**, 15/15
  functions, 69/74 branches (93.24%). The six uncovered lines are allocation and
  unexpected standard/nonstandard exception catch fallbacks at the C boundary.
  No production fault switches were added to manufacture coverage. This is an
  adoption gap, not a claim of 100% core coverage; unrelated inherited engine
  code was not instrumented by this scoped measurement.

No UI changed;
no native GPU, VoiceOver or Qt GUI parity result is claimed here.

Remaining work includes cached source export, Swift/editor adoption, broader
modifier differential corpora, exact surface provenance if future consumers need
it, and the inherited engine's coverage/resource-limit adoption gaps.
