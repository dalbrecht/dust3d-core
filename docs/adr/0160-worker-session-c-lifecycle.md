# ADR 0160: C ownership for connection-scoped worker sessions

Status: proposed for reviewed delivery. Tracks #251.

## Decision

Expose Dust3DWorkerSession as an exclusive opaque C owner around the existing
ConnectionGenerationSession. A dedicated configuration contains only the host-selected
16-byte document, positive connection generation and retained-cache allowance.
Request/revision values are not session configuration. The host establishes scope
from its authenticated connection; configuration is not proof of authentication.

Creation validates the existing scope/cache rules before native ownership. Zero cache
allowance disables retention; the existing256MiB ceiling is logical retained payload,
not process RSS. Reset validates the live generation and clears retention without
resetting replay protection. Close is permanent and idempotent, releases native state
after serialized work completes, and leaves the C owner allocated until destroy.
It does not interrupt execution or close any channel. Caller preserves lifetime
through calls, serializes lifecycle changes and never destroys concurrently.

Required DecodeResult pointers make NULL-result operations no-ops. NULL owners reject
reset/close; NULL destruction is harmless. All callable C boundaries catch exceptions.
The shared bounded error mapper adds SESSION_SCOPE31 with the original scope code and
NATIVE_SESSION32 with the original native diagnostic code (normalized outcome only
when original detail is absent). Existing upstream truncation remains explicit;
new fixed-buffer truncation is also preserved. No native work is induced to exercise
error mapping: tests use synthetic typed diagnostic values.

## Validation

The missing public header/API started red. A later synthetic diagnostic regression
failed before correcting normalized-status substitution and truncation loss.
Full CMake build and205/205 ordinary CTest contracts pass (37.01s), including the
independent C11 consumer. SwiftPM runtime builds (0.84s), strict manifest lint,
quarantine guard and clang-format18 pass. Nine implementation/build/test hashes
match the final validation sources.

Scoped ASan/UBSan passes for lifecycle ownership and synthetic diagnostic copying.
The new lifecycle bridge covers41/41 lines,22/22 code regions,7/7 functions and6/6
branch outcomes in its matching profile. This does not imply whole-runtime/engine
coverage: native engine libraries are uninstrumented, leak detection disabled, and
allocation/generic exception recovery and inherited native failure paths remain
unforced. No native malformed-input, fault/crash/signal/cancellation, pressure or
resource-exhaustion probes were run. No display/manual GPU acceptance is claimed.

## Remaining adoption

This stage exposes lifecycle ownership only. Native C channel serving, session request
factories, Swift/XPC ownership, actual authenticated transport and Apple range/output
acceptance remain under #251 and Apple #279/#280. Legacy serving still rejects operation9.
No production pin/backend activation, quarantine removal or feature-parity claim.
Completed clean exact-head automated review and all CI remain merge requirements.
