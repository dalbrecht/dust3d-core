# ADR 0162: Explicit Swift session imports and ownership evidence

Status: proposed for reviewed delivery. Tracks #251.

## Decision

Provide an explicit public Dust3DWorkerRuntime module map listing runtime, image,
session-output and session-lifecycle headers. SwiftPM's automatic module used only
the target-named umbrella header, so building the C++ product did not demonstrate
that Swift consumers could import the additive session API. A strict standalone
consumer reproduced that missing API/incomplete-umbrella failure before this fix.
The CMake standalone Swift module also lists the session header explicitly.

Add one ordinary licensed Capsule Swift6 integration consumer. It creates/uses/closes
all C session owners synchronously inside one detached task; opaque handles never
cross an actor boundary. It creates source-budget requests, serves them through
separate regular-file channels and completes dedicated service admission after
observed file EOF. Same-process generation/peer observations test correlation only,
not XPC authentication or process isolation. Channel ownership remains with Swift.

The consumer copies indexed positions/indices, selected original source identity
bytes, actual cache counts and opaque cache diagnostic bytes while owners are live.
It exercises actual reuse on a second request, rebuilding after reset and final
close/destruction. Only independent Sendable values cross into a receiving actor.
This is an importer/ownership contract, not a production Swift service/backend.

## Validation

Initial CMake-module typechecking failed on missing session configuration/functions.
An independent SwiftPM generated-module check also failed before the explicit map.
The first full consumer run caught a test assertion that incorrectly compared source
corner count with unique vertices; existing codec/C tests establish index-corner
correspondence, and the assertion was corrected accordingly. Swift required explicit
Int32-to-Int64 process-ID widening. No runtime behavior was weakened to pass the test.

Full CMake build and207/207 ordinary CTest contracts pass (34.56s). Swift6 strict
concurrency, warnings-as-errors and strict formatting pass. The consumer typechecks
against the explicit public module map and compiles/links/runs against CMake output.
SwiftPM runtime builds (0.13s), and a separately compiled consumer links/runs against
its actual static archive and explicit module map. Three source/build/test hashes
match final validation. Quarantine guard and manifest lint pass.

No C++ implementation, parser, wire or ownership contract changed in this stage.
No new sanitizer/whole-engine coverage claim is made for these Swift integration
checks. The tiny fixture size assertion is a test expectation, not a product range
limit. No new malformed native input, crash/fault/signal/native-cancellation,
pressure/exhaustion probes, XPC/native host probes or app control were performed.
No manual GPU or distribution acceptance follows.

## Remaining work

Actual connection-scoped Swift/XPC lifetime, platform decoding, complete authored/
procedural/source-baked output and supported-range acceptance remain under #251 and
Apple #279/#280. Production pin/backend and all quarantines stay unchanged. This
import/ownership evidence does not establish M4–M8 parity. Completed clean exact-head
automated review and all CI remain required before merge.
