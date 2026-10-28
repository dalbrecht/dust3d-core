# ADR 0185: Importable Swift worker client foundation

Status: proposed for reviewed delivery. Tracks #270 and Apple #279/#280.

## Structure and API

Move the accepted identity/receipt/admission, metadata queue and immutable-source
coordinator implementations from tests/swift into native/WorkerClient. Keep one canonical
implementation and expose the static SwiftPM product Dust3DWorkerClient. It depends only
on Foundation/Swift; no engine, native runtime, Qt, XPC or external package dependency is
introduced. Exclude its directory from the native Clang target. Keeping the implementation
under tests or copying it into the Apple repository would leave duplicate owners and an
unreviewable package boundary, so neither is used.

Expose host identity, passive original control receipts, typed errors and the externally
serialized source coordinator with documented public methods. Keep the raw metadata queue,
one-shot gate and held-source counter internal. Add an explicit public receipt initializer
that retains original values without normalization/admission. Move the prototype-only
transfer error carrier into its XPC support file; platform transfer concerns do not become
a dependency of the pure client library. No scheduling or native-budget algorithm changes.

CMake builds this same canonical source as a separate Swift module/static library. The
healthy XPC host imports it instead of compiling private copies of the source files.
Existing detailed pure contracts still compile canonical files to inspect internal state.
Actual SwiftPM XCTest cases import the public module, proving consumers can create owned
snapshots, preserve raw policies, complete publication and catch original scope errors
without private declarations. CTest now invokes these public XCTest cases too.

The coordinator is library-only and deliberately not Sendable: one actor/serialized owner
must contain it. Immutable Sendable snapshots can cross into isolated work independently.
It does not launch a process, perform XPC, parse archives, save documents, authenticate
peer observations or publish SwiftUI state. Exact reviewed source pinning remains required;
no binary ABI stability or complete production worker SDK claim is made.

## Verification

Public XCTest first failed with missing Dust3DWorkerClient before extraction. Two importing
XCTest cases now pass. Final fresh CMake build and231/231 CTest pass (36.37s), including the
actual SwiftPM test wrapper and healthy licensed Capsule XPC fixture using the separate
client module. Both current SwiftPM products build (client0.23s, runtime0.05s cached), and a
fresh strict Swift6 XPC host links their actual archives and passes shared-cache/reset,
sandbox/signature/independent peer, actual EOF and normal PID retirement checks.

An initial extra consumer compile assumed the old native-backend module path after
SwiftPM defaulted to a different output layout. Compilation stopped before any consumer
ran. Fix the wrapper to explicitly reuse the established native backend/cache, rerun its
public tests and actual-product consumer, then refresh the full gate above. Retain the
backend deprecation warning; backend/toolchain migration is separate from this extraction.
Every build used the same shared scratch root; no second root was created.

Symbol inspection of the actual SwiftPM client archive finds no native-worker or Qt
symbol dependency. Strict formatting, diff checks and14 frozen source/fixture/manifest
hashes pass. Source stayed frozen during validation. Separate scoped pure profiles report:
admission18/18regions8/8functions46/46lines; queue34/34regions9/9functions49/49lines;
source owner24/24regions16/16functions46/46lines. Swift reports no branch counters.
These are per-file profiles, not summed whole-core/service coverage. No C++ parser/owner
implementation changed, and no new sanitizer claim is made.

## Remaining scope

The production XPC service/client adapter, signed worker bundle/trust distribution,
asynchronous mutable-document producer/backend integration, native-range resource
acceptance and baked/imported profiles remain open. Existing XPC code is still development
infrastructure with private CDHash fixtures, not a production trust policy. Apple pin48b0ce2d,
active app routes and quarantines stay unchanged. M4–M8/full editor parity is incomplete.
No new malformed native input/crash/fault/allocation-failure/signal/broken-pipe/native-cancel/
pressure/exhaustion/denied-peer probes, app control or OS/global SVG changes were used.
