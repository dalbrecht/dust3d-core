# ADR 0159: Correlated service admission for session output

Status: proposed for reviewed delivery. Tracks #251.

## Decision

Add PendingServiceReply.completeSession and RequestQueue.completeSessionService.
Both use the existing service control rules rather than a parallel authentication
policy. One shared pending gate retires before validating authoritative current
identity, captured connection generation, independently observed positive OS peer,
actual EOF, exact receipt operation/request and every receipt error field. Queue
completion uses one shared private active-slot collection helper; foreign identities
and wrong completion modes leave active work intact, while matching collection
releases its slot before semantic admission. Newer waiting work remains retained.

After that gate, session completion requires operation9, response kind and exact
operation/document/request/revision framing, then decodes the complete SessionOutput.
A primary generation failure is inside that response envelope with its diagnostics
and observations; a raw failure frame cannot bypass session semantics. Rejections
remain consumed and cannot replay. A cancelled/closed/superseded publication never
becomes current, while transport ownership remains the caller's responsibility.
No I/O, process control, native cancellation or peer authentication occurs here:
the host must establish authentication and supply independent real observations.

## C ownership integration

Dedicated dust3d_worker_session_service_admit consumes an unused EOF-framed service
collector. It rejects child-mode and queue-bound collectors; authoritative current
identity and observed transport lifetime/peer remain caller inputs. Dedicated
session queue admission obtains current source and captured active identity/lifetime
from its private coordinator slot. A NULL result leaves either attempt untouched.
Otherwise rejection retires the collector or releases matching queue work; early
policy/framing rejection abandons only that private matching slot and retains newer
waiting work. Prior framing errors must retain their original receipt and be
abandoned after transport cleanup.

Both APIs adopt the existing independent immutable C session owner. Shared C policy
and typed exception classification serve values-only decoding and the two admitted
paths. Original admission/queue/protocol/codec causes remain self-owned and bounded;
all C++ exceptions are caught. Add C operation9 receipt validation before narrowing
its wide field. Wide aliases remain rejected. Existing generic C decoding/service
admission still reject session semantics; C request factories and native serving
are not enabled by the new receipt enum.

## Verification

C++ and C contracts began red with the missing dedicated methods/entry points.
All 204 ordinary CTest contracts pass (34.09 s), as do the full CMake build,
SwiftPM runtime product (2.14 s), strict manifest lint, quarantine guard and pinned
clang-format18. Fifteen source/build/test hashes match the final validation inputs.

Tiny pure metadata/envelope contracts cover one-shot success/rejection, wrong
connection/peer/current revision/receipt, incomplete EOF, receipt errors, wrong
operation/kind, foreign queue completion, superseded/cancelled/closed work, policy
and codec rejection, queue-bound/child bypass attempts and queue recovery. NULL
result attempts remain usable. Existing legacy tests exercise all shared receipt
flags. The independent C11 consumer calls both new boundaries. The ordinary licensed
Capsule output also passes dedicated C admission and retains its indexed/source
values after native-session close, using explicitly synthetic peer metadata in the
unit test; this is not an actual XPC authentication acceptance test.

Scoped ASan/UBSan passes for new and legacy pure gates, dedicated C entry points
and the ordinary Capsule/owner compatibility path. Separate matching profiles cover
the two new C++ methods/shared helpers at57/57 code regions and44/44 branch outcomes;
legacy tests supply full shared-control coverage. Dedicated C admission covers22/22
regions and16/16 outcomes; dedicated C queue admission covers21/21 regions and11/12
outcomes. Its defensive already-decoded private collector guard remains unforced;
allocation/generic exception and diagnostic truncation recovery guards remain
unforced as documented in ADR0158. No whole-file/whole-engine coverage claim follows
from these selected-function counts. Native engine libraries are uninstrumented;
leak detection is disabled. No new malformed native archive, fault/crash/signal,
native-cancellation, pressure or exhaustion probes were run.

## Remaining work

Native C session lifecycle and channel serving, C request creation, Swift/XPC
ownership, actual authenticated transport evidence and Apple #280 range parity
remain open under #251/#279/#280. Legacy serving remains explicit unsupported for
operation9. Production backend/pin and all quarantines are unchanged. M4–M8 parity
acceptance remains incomplete. Completed clean exact-head automated review and all
CI checks are required before merge.
