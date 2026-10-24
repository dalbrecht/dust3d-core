# ADR 0070: Opt-in supervised Unix worker operation

Status: local development mitigation; active stop/escalation OS paths unverified.
Refs #123. Not production containment or Apple adoption.

## Decision

Keep runWorkerJob as the existing development operation. Add the opt-in
runSupervisedWorkerJob using the same framed channel collector and WorkerLifetime
policy. Default supported budgets are ten minutes of work and one second each for
stop grace and post-exit drain; callers may supply explicit validated values.
The monotonic work interval starts after successful spawn, not during setup,
request encoding or OS spawn. Decisions are observed around a 50ms poll interval;
this is not a real-time scheduling guarantee.

A caller-owned atomic WorkerCancellation flag may be requested from another thread
and must outlive the operation. Cancellation already observed before encoding or
spawn throws WorkerStopped without starting a child. Active retirement closes
request input and requests SIGTERM on the directly owned helper; after the grace
interval, a still-uncollected helper receives a single SIGKILL request. Real
waitpid exit state is collected independently. Post-exit drain expiry closes the
remaining channels. Retired output never returns as an admissible CollectedJob:
WorkerStopped retains the first retirement reason and owned bounded reply,
diagnostic bytes/truncation flag and actual process outcome. No automatic retry
or logging of opaque child diagnostics occurs.

The parent must exclusively own child reaping; an external SIGCHLD/waitpid reaper
violates the contract. Observed ECHILD clears the PID before propagating the
original wait error, preventing later cleanup from targeting a PID after that
ownership loss. On exceptions, channels close before the supervised child owner
requests force-stop and reaps. The original transport/codec/allocation exception
remains primary. Exceptional force-stop/reap failures are best-effort and do not
replace the original exception; their independent reporting is still incomplete.
The legacy owner does not acquire this exceptional force-stop behavior.

## Verification and limits

Test-first compilation fails on the missing supervision API. An ordinary licensed
Capsule crosses the supervised transport and matches the native C API vertex bits,
source counts and warning, then passes single-use PendingReply admission. Existing
Sword/invalid/empty ordinary cases remain on the legacy path. An already-requested
cancellation with a nonexistent helper path returns the typed cancellation and no
collected child outcome, proving pre-start retirement rather than spawn failure.
Policy rejection precedes spawning. A pure exception-container contract verifies
independent ownership of original binary diagnostics, reply and synthetic outcome;
it is not evidence of an OS termination event.

Full final normal CMake/CTest passes 62/62 in 32.90s; fresh ASan/UBSan passes all
61 registered C/C++ contracts in 82.23s. Strict warnings and formatting pass.
Ordinary transport instrumentation measures WorkerTransport.cc at 175/217 lines
(80.65%), 186/277 regions, 22/23 functions and 92/160 branch outcomes. Its header
accessors/cancellation methods measure six additional lines/functions. This does
not meet later infrastructure coverage targets. Active cancellation, timeout,
SIGTERM/SIGKILL delivery, exceptional ownership-loss/cleanup, interrupted OS calls
and drain-pressure paths are deliberately unforced. Pure lifecycle policy coverage
in ADR 0069 does not prove those OS paths. No crash, signal-delivery, deliberately
broken-pipe, allocation-failure or resource-exhaustion probe runs.

Only a directly owned child is stopped; descendant process containment is absent.
OS spawn/reap in uninterruptible kernel states cannot be promised bounded, and
exceptional reap can still block. CPU/RSS/file/descriptor limits, filesystem
sandbox, parent-death handling, signed trusted-helper packaging and full lifecycle
acceptance remain required. Never run this on a UI actor or adopt unrestricted
production documents based on these tests. The Apple pin and all GLB #67/#69,
writer #83, fault #84, animation #95 and Apple #199 quarantines are unchanged.

## Rebased delivery verification

Rebased onto reviewed core master 69b428bd; retained #124 request-limit
propagation in transport preflight. On f9a23f7b, fresh normal CMake/CTest
passes 62/62 in 34.55s and fresh ASan/UBSan passes 61/61 in 102.54s.
The source quarantine guard and changed-file formatting pass. No leak-sanitizer
claim is made on macOS. These reruns use only the ordinary registered contracts;
the unforced OS paths and production-adoption limitations above remain open.
CMake products are cleaned after the terminal gates while retaining the single
shared Swift compiler cache and live QA bundle.
