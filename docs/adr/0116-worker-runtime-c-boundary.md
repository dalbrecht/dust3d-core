# ADR 0116: C-compatible descriptor worker runtime

Status: prepared host-integration stage for #167; no production adoption.

## Decision

Publish a separate `Dust3DWorkerRuntime` C interface/static SwiftPM product over
the maintained descriptor job runtime. The public header contains fixed C values,
not STL, C++ references, native object owners or callbacks. `dust3d_worker_serve`
returns a self-owned inline result and catches known codec/protocol/I/O,
allocation, standard and unknown C++ exceptions. Preserve original source/code,
errno, input/output stage, validated request/operation and bounded diagnostic
bytes. Truncation is explicit; no result destructor or borrowed message is needed.
No logging or automatic retry/fallback is introduced by the bridge.

COMPLETE means that a complete reply frame was written; that frame can be a
native operational failure. It is neither geometry success nor a process exit.
Noncomplete results may leave partial output that the host must discard. Never
substitute a service callback for the actual process outcome required by the
existing process admission API. A persistent authenticated transaction needs a
separate completion contract before any parent publication.

The caller owns both descriptors and all startup stdio/SIGPIPE/resource/sandbox
policy, serializes jobs, calls away from UI and closes channels after completion.
The interface performs blocking I/O, not service launch, authentication,
cancellation, deadlines or memory-corruption/signal containment. Its layout
version is one; binary ABI stability is not claimed before release gates. Detail
codes are revision-pinned native diagnostic values, not an independent stable
wire schema. No production dependency or quarantine changes.

Split SwiftPM source ownership into ten runtime/bridge sources and five CLI/
resource-startup sources. Both depend on the same maintained engine; there are no
source overlaps, dependencies added or duplicate Apple adapters. CMake exposes
the corresponding runtime target and ordinary C11/C++/Swift consumers.

## Verification

The C11 smoke failed on the missing header before implementation. New consumers
then verify layout version, preflight rejection, ordinary licensed Capsule base/
UV jobs, 240 corners/80 UV triangles, borrowed descriptor lifetime and original
pre-generation codec identity/code/cause. The existing ordinary directory-read
rejection scenario additionally verifies original EISDIR/stage/context across C,
with unchanged output and descriptor ownership. No new malformed native archive,
crash, signal, broken-pipe, allocation-failure or resource-exhaustion probe is used.

A strict Swift 6 consumer independently builds the small base request fixture,
runs the C entry point off-actor, copies inline metadata and raw diagnostic bytes
into immutable Sendable values, validates independent reply fields and transfers
the owned receipt across an actor. It reports errors with normal exit rather than
fatal assertions. It is a test fixture, not a second production framing adapter.

Fresh source quarantine guard/audit and clean CMake build pass 124 ordinary
contracts initially (21.65 seconds). After extending only two consumer tests,
rebuilt consumers and the final full inventory pass all 124 (15.10 seconds), with
three focused C/C++/Swift contracts in 0.53 seconds. Audited runtime hashes remain
unchanged. SwiftPM builds the runtime static product in 1.95 seconds and helper in
2.07 seconds; the packaged helper passes ordinary base/resource-profile and UV
transport contracts. Source inventory proves the ten/five source split.

Scoped bridge/job-I/O ASan/UBSan pass; inherited engine and operation libraries
remain uninstrumented. Combined C/C++ bridge coverage is 52/72 lines, 27/38 regions
and 11/16 branches. Shared boundary function profile hashes match; only the two
test mains differ. Defensive codec/runtime/allocation and unusual formatter/
truncation paths remain unforced. No whole-core coverage or allocation-failure
guarantee is claimed. C11/C++ warnings, changed formatting and whitespace pass.

Parent bounded stream decoding/admission, a real XPC host/control/lifecycle,
expected release identity, sealed packaging, sandbox/notarization and resource/
parent-death acceptance remain #123/M8 gates. Keep every existing quarantine.


## Serial delivery verification

Replayed after #194 with accepted aggregate text admission and checked pure
metadata fixture access preserved. Current audited runtime, public header and
C/C++/Swift consumers match the inspected saved stage; the inventory contains
125 ordinary contracts, including the accepted pure text-quota regression.
A fresh clean build, twelve pure preflight contracts and the focused C++/Swift
consumers pass. The two focused consumers take 0.46 seconds; all 125 contracts
pass in 20.96 seconds. Directory I/O rejection and malformed UV metadata are
bounded pre-generation checks; native execution uses healthy Capsule base/UV
jobs. No native malformed archive, failed-peer, cancellation, signal, crash,
allocation-failure or pressure pilot is introduced.

Fresh scoped ASan/UBSan instruments the C bridge/job-I/O and ordinary C++ consumer;
inherited operation/engine libraries remain uninstrumented. The earlier combined
coverage figures remain historical prepared-stage evidence, with exceptional
runtime/formatter branches unforced; full inherited coverage is unclaimed.

SwiftPM runtime and helper products build in 1.67 and 2.08 seconds. The manifest
has disjoint ten-source runtime and five-source startup ownership. Fresh healthy
UV dispatch and visual-family verifiers pass against the packaged helper; export
compatibility builds in 0.10 seconds. Strict C++/Swift formatting, whitespace and
recorded executable-source hashes pass. Existing native build-system deprecation
and duplicate resource-library warnings remain. Production pin, release trust,
containment/resource acceptance and every quarantine remain unchanged.
