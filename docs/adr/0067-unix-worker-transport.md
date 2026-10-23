# ADR 0067: Development Unix worker transport

Status: local implementation for #114; public review and hosted CI pending.

## Decision

Add a separately linked parent transport for macOS and Linux hosts with a supported
close-from spawn action. Standard-library protocol/admission targets remain
portable and independent. The transport uses existing OS APIs, no third-party
dependency. Direct posix_spawn avoids shell parsing and fork-time work in a
multithreaded application. A file-redirection wrapper would not verify concurrent
stream transfer or bound diagnostic capture. Swift application orchestration stays
a later layer and must run this blocking development API off the main actor.

The caller provides a trusted absolute helper path without embedded NUL. Request
kind/identity/length/policy validate and encode before creating channels or spawning.
Three Unix stream socket pairs carry stdin, binary stdout and stderr. Descriptors
normalize above standard I/O, carry close-on-exec flags and have nonblocking parent
ends. Spawn actions duplicate only child stdin/stdout/stderr; macOS close-by-default
and Linux close-from prevent unrelated application descriptors from being inherited.
Linux requires glibc's close-from extension (2.34+) detected at configure time;
unsupported hosts omit the development transport without changing portable targets.

The parent sends/drains concurrently with poll and closes input after the complete
request to deliver EOF. Bounded chunks default to 64 KiB; configurable smaller
chunks support ordinary fragmentation checks. Socket-specific SIGPIPE suppression
uses SO_NOSIGPIPE on macOS or MSG_NOSIGNAL on Linux; parent global signal handlers
are not modified. There are no automatic retries.

Collect the fixed reply header first, validating advertised per-kind size and
identity before body growth. Reject bytes beyond advertised reply length. Diagnostic
capture defaults to 4096 original opaque bytes, with a truncation flag; excess is
drained without retention so logging pressure cannot block binary output. Do not
interpret or automatically log child diagnostics. Logical byte limits do not bound
container capacity, request/frame duplication, algorithm workspace or process RSS.

Collect process termination independently with waitpid and drain both output
channels to EOF before return. Returned bytes are not admitted application data:
the caller must supply actual observed outcome and authoritative current identity
to PendingReply, which rejects truncated/semantic-invalid/nonzero/stale completions.
Child failure diagnostics and actual OS error codes remain available separately.

## Lifecycle limits

RAII closes every channel before reaping on exception and prevents zombie children.
This development API deliberately has no deadline, forced cancellation, sandbox,
resource limit or parent-death policy. Cleanup can wait indefinitely for an
uncooperative generator or inherited child channel. It must not run on a UI thread
or be adopted for unrestricted documents. A future lifecycle owner needs bounded
queue/revision orchestration and accepted deadline/termination/resource policy.
No process is intentionally crashed, signalled or exhausted by these tests.

## Verification

The transport test first failed because its public header did not exist. Actual
licensed Capsule and Sword archives cross real channels, compare every geometry
float/source count/warning against the ordinary native C ABI, and pass single-use
admission after observed exit. Tiny invalid and empty archives return identical
original typed causes. Capsule transfers with seven-byte chunks; invalid archives
use one-byte chunks, exercising header/body fragmentation without resource probes.
Path, policy, request-kind and length preflight rejects before spawn. A non-directory
helper path suffix verifies the original ENOTDIR spawn failure without starting a
child. No deliberate broken pipe, signal delivery, allocation failure or crash
case is run. OS setup/interruption/cleanup failure paths and diagnostic truncation
remain unforced coverage gaps, not evidence of production containment.

Full normal macOS CMake/CTest passed 60/60 in 31.82 seconds, followed by a focused
normal rerun with stronger licensed-fixture expectations. Clean ASan/UBSan passed
59/59 in 103.48 seconds; the Swift consumer remains normal-only. Unexpected child
runtime/sanitizer diagnostics fail the transport test and render bounded original
bytes as escaped hex so reports are not hidden or interpreted as terminal control.
A focused sanitizer rerun validates this final test-reporting change. File-scoped
transport instrumentation measures 141/163 lines (86.50%), 145/209 regions, 19/19
functions and 65/108 branch outcomes. These remain below adoption coverage targets;
no fault, signal or resource-exhaustion probe is added to inflate them. Linux paths
still require hosted CI; local execution evidence is macOS only. New implementation
also passes strict -Wall/-Wextra/-Werror syntax checking and formatting.

Quarantines #67/#69, #83, #84, #95 and Apple #199 remain unchanged. No Apple production
pin, SwiftPM dependency or application launch change is included. Full parity and
M7/M8 acceptance remain incomplete.
