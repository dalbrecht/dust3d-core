# ADR 0174: Preview archive channels and C/Swift ownership

Status: proposed for reviewed delivery. Tracks #270 and Apple #280.

## Decision

Connect the accepted D3PC archive collector to synchronous Unix descriptor adapters.
The sender validates captured native controls, archive direction, actual readable span,
identity and chunk policy before its first write. Emit the fixed source prefix and
borrowed archive through one bounded writer workspace, reusing the existing bounded
complete-span write helper. Never retain a second whole request envelope. Returning
from the sender means completed writes, not EOF: the host must close or shut down its
own channel. Partial output on error is discarded, never retried as a complete job.

The existing prefix carries archive length and original source policy only. Indexed
policy remains independent host metadata; the receiver must obtain it through host
admission, not infer it from these bytes. This API does not add an indexed wire field.

The receiver constructs its frozen collector before reading. Read through a fixed
64 KiB I/O workspace, retry only EINTR, and supply finishAtEOF only after read returns
zero. A complete end marker alone is insufficient. Return a unique complete owner;
any framing, prefix, I/O or allocation failure destroys the partial owner. Borrowed
descriptors are never opened or closed here, including on error. Negative descriptors
reject in pure preflight without a syscall. Other errors retain the original errno
and input/output stage. EAGAIN is an error, not an implicit wait or busy loop.

This is a blocking single-direction worker-thread operation. The host must establish
signal/resource policy, isolated non-UI execution, concurrent sender/receiver scheduling
for bounded pipes and bounded supervision. This layer supplies no deadline, native
cancellation, authentication, peer identity, current-source admission or containment.
The ordinary positive exercises an anonymous regular descriptor, not pipe/backpressure
or socket failure behavior. No production service route is switched to this adapter.

Expose explicit scope, captured native controls, chunk policy and one immutable opaque
owner through a separate C header in the actual public runtime module map. Read with
a required result pointer or do no work. All exceptions stop at the C boundary with
specific chunk/source/system classification and bounded original descriptions; errno
and I/O stage remain separate fields. Generic/allocation catch boundaries remain
present but are not forced by probes. Getters return copied scalars and a const archive
span valid until matching destruction. Null getters clear size; null destruction is safe.
Only a complete collector is stored, so getters do not invoke fallible state transitions.
Swift must copy bytes and native controls before owner destruction or actor transfer.

Memory accounting remains logical: receiver retains one exact archive, the fixed
prefix, reader header/chunk workspace and an additional fixed 64 KiB I/O workspace.
The C wrapper adds one owner and copied borrowed-view scalars. Sender retains one
chunk plus fixed prefix/header temporaries. Root/capacity/allocator bookkeeping, caller
input/copies and RSS are separate. A shared internal scalar validator avoids divergent
native policy checks in collector and sender. Existing legacy limits/opcodes and the
production Apple dependency remain unchanged.

## Evidence

The new test began red on the missing channel/C header. The existing licensed small
Capsule archive is sent through seven-byte chunks, read to actual descriptor EOF and
compared byte-for-byte through both C++ and C. C-owned bytes survive channel closure;
a caller-owned copy survives C-owner destruction. No archive is parsed by the new test.
Pure preflight cases cover negative descriptors without syscalls, missing input/result,
span/count mismatch, invalid native source policy, zero connection generation and wrong
C++ direction, checking no output was written. Actual empty-channel EOF cannot expose
an owner. A tiny unsupported protocol-prefix version retains its specific source cause
across C without any native archive parsing. Null and missing-size getters are checked.
A pure-C consumer compiles the public header and exercises safe access/preflight.

Strict Swift 6 copies archive bytes plus original zero/default and positive indexed
controls, destroys the C owner, then transfers immutable values through an actor. The
same smoke test passes against both CMake libraries and the actual newly built SwiftPM
static product using the checked-in public module map. No unsafe pointer crosses actors.

Fresh complete CMake build and 220/220 ordinary CTest contracts pass (40.66 seconds).
Actual SwiftPM runtime build passes (0.99 seconds, with its existing native-build-system
deprecation warning), followed by the actual-product Swift smoke. Both clang-format 18
and the newer formatter, strict package/Swift formatting, quarantine guard and twelve
unchanged source/build/test hashes pass. Scoped ASan/UBSan passes for the new channel
and C boundary using the small positive and pure preflight/protocol cases. Native
libraries are linked but not called by this new test and remain uninstrumented;
leak detection is disabled, so this is not a native-engine sanitizer or leak claim.

Measured channel code: 38/43 lines (88.37%), 27/35 regions (77.14%), 3/3 functions,
15/19 branch outcomes (78.95%). C bridge: 69/76 lines (90.79%), 41/45 regions (91.11%),
13/13 functions, 14/16 branch outcomes (87.50%). The existing collector/shared scalar
validator in this narrower test has 65/70 lines, 44/49 regions, 9/9 functions and
23/32 branch outcomes; its broader ordinary contracts also pass. Residual paths include
real read interruption/failure and failed writes, generic/allocation/unknown C catches,
long diagnostic truncation, defensive collector guards and address-space alternatives.
Some C branch misses are opposite I/O stages in separate template instantiations.
These boundaries are inspected but not runtime-forced; no fault/signal/broken-pipe,
allocation failure, pressure, malformed native input or large-resource probe is added.
Coverage remains below the adoption target; no whole-core coverage claim follows.

## Remaining gates

Next implement complete wide session output semantics and incremental transfer under
the native range ledger, preserving real diagnostics/statistics/source fallback and
successful emptiness. Native request dispatch, complete output C/Swift owners, authenticated
peer/current-source/queue admission and Apple adoption remain separate work under #270
and Apple #279/#280. Never silently shrink native ranges or fall back to parent native
generation. Production pin 48b0ce2d, active default routes/caps and all feature quarantines
remain unchanged. M4–M8/full parity are incomplete. Completed clean exact-head automated
review and all hosted CI checks gate merge.
