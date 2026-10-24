# ADR 0068: Bounded worker request scheduling

Status: local implementation for #115; public review and hosted CI pending.

## PR #124 review correction

Request admission now receives the queue/transport's configured wire limits rather
than silently applying the default input quota a second time. The constructor's
default remains source-compatible. A header-only regression advertises one byte
above the default 128 MiB input limit, rejects default/too-small/invalid limits,
and admits a tiny typed failure reply using the configured limit. No large payload
is allocated and no large archive/helper or resource-exhaustion probe is run.
Actual helper dispatch still requires a helper configured for compatible quotas;
this change does not increase the production helper's default input limit.

## Decision

Add a standard-library-only RequestQueue beside the existing worker codecs and
PendingReply admission. The serialized owner retains one waiting archive and an
active header; begin transfers the archive to the transport caller. Every valid
submission replaces the waiting request and makes its identity authoritative.
Request numbers increase across the entire queue lifetime, including documents,
so a completed, cancelled or discarded identity cannot be reused. Revision zero
remains valid; document and request identity validation stays in the wire codec.

Validate header, input quota, kind, exact payload length and request sequence before
changing state. Reject invalid work with typed causes, preserving the existing
active/waiting jobs. Replacement returns the discarded waiting identity so callers
can retire their own promises without retaining its archive. Matching cancellation
drops waiting data and retires publication without reviving older work. Close is
idempotent and permanent; it drops pending archives while active transport must
still finish collection and reaping.

Completion releases the matching active slot before decoding or admitting. Stale,
cancelled, closed, malformed, foreign or replayed completions cannot publish data.
Foreign completion leaves the actual active slot intact. Original semantic/codec
and typed failure causes propagate through PendingReply. Transport failures must
be caught by the caller after transport cleanup, followed by abandon(identity),
then surfaced with their original cause. Abandon releases only matching active
work and preserves a newer queued request. It never manufactures successful data.

The queue is deliberately externally serialized. The process transport runs on a
separate I/O executor; its owner applies submissions and collected completions on
one serial executor. No callbacks, thread creation, global state or OS actions are
introduced by queue methods. Do not block that executor inside runWorkerJob.

## Verification

The initial contract compilation failed because WorkerQueue.h did not exist. Pure
contracts cover ownership, replacement, serial admission, cancellation, closure,
monotonic identity, input validation, original opaque failure bytes, foreign and
malformed completion rejection, and continued dispatch after transport failure.
Real ordinary Capsule subprocesses exercise queue-to-transport-to-admission: an
obsolete completed result is rejected, the intermediate waiting request is
discarded, and only the newest archive runs and publishes owned geometry.
Full normal CMake/CTest passed 61/61 in 77.84s; clean ASan/UBSan passed
60/60 in 170.87s. A final focused sanitizer test passed after strengthening typed
queue-error assertions. Strict compiler warnings and formatting pass. Scoped
queue coverage is 61/61 lines, 54/54 regions, 9/9 functions and 33/36 branch
outcomes; this is not a whole-engine coverage claim. Remaining short-circuit
branch outcomes were not forced solely to inflate coverage.

## Limits

This is scheduling/admission integration, not an asynchronous application worker
service. It does not provide forced cancellation, deadlines, parent-death cleanup,
RSS limits or filesystem sandboxing. Actual cancellation still requires transport
to finish normally. No forced signals, crashes, broken pipes, allocation failures
or resource-exhaustion probes are run. Production SwiftPM pin and all existing
quarantines remain unchanged. M7/M8 and full parity acceptance remain incomplete.
