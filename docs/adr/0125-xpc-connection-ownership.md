# 0125: Connection-owned development XPC worker lifecycle

Issue: #176. Status: local implementation; serial public review pending.

## Decision

Issue a process-local candidate token during listener setup, but claim ownership
only on the first service method invocation, after Foundation's configured peer
requirement check. Candidate issuance alone cannot reserve or retire the worker.
The scheduler serializes ownership and native admission with the same lock.
Unclaimed or foreign invalidation cannot retire the owner or its active job;
foreign completion cannot release its lease. Owner retirement seals admission and
queues normal shutdown behind its accepted work. Completion may release that
owner's in-flight lease after retirement. Generation exhaustion does not wrap.

These tokens route lifetime events; they are not security credentials. One owning
connection lasts for the service lifetime. A future application broker can
multiplex documents on it. Reconnection, cancellation, authenticated current-source
publication, release trust and resource policy remain separate work.

## Evidence and limits

The regression first failed for the missing ownership type. Strict Swift 6
concurrency compilation, formatting, four pure policy/metadata checks, all four
healthy isolated XPC base/UV file/socket cases and all 139 source-audited ordinary
CTest cases pass (full suite 22.66 seconds). Source hashes match the clean build.
Ownership measures 24/24 lines and 15/15 regions; scheduler measures 86/88 lines
and 26/28 regions in pure recording tests. The default Dispatch adapter is outside
that coverage run and exercised by healthy integration. Swift reports no branch
outcomes; these numbers do not establish branch or whole-core coverage.

Other-client claims, completion, disconnect and retirement use only pure values
and recorded execution. No real multi-peer, rejected-peer, overlap, pressure,
crash, allocation-failure or signal probe ran. No C++ runtime/parser change or new
native sanitizer claim. Production pin and all quarantines remain unchanged.

## Serial delivery verification

Replayed onto accepted core #203 while preserving the accepted UV aggregate text
budget, checked UV fixture indexing and portable decoded-budget initializer fixes.
The quarantine guard and source audit pass with 140 ordinary registrations,
including the accepted pure quota regression; all four XPC cases retain their
shared development-host resource lock. A fresh clean build in the single shared
root passes strict Swift 6 compilation and formatting.

Normal and optimized policy suites each pass four tests. Pure ownership,
admission, recorded scheduling and channel metadata checks pass before native
work, followed by 19 pure C/C++ contracts. All four healthy file/socket base/UV
XPC cases pass in 2.00 seconds. Socket base returns 240 vertices
(host20590/service20591); socket UV returns 240 vertices and 80 triangles
(host20600/service20601). Both join writer completion, actual EOF semantic
decoding and control receipt. File cases also pass with accepted admission and
actual peer identity. Owned cleanup follows host exit and service PID disappearance;
the service exit status is not independently observed.

All 140 ordinary CTests pass in 22.29 seconds. All 268 audited source hashes and
four fresh Swift fixture binary hashes remain unchanged. Prior scoped coverage
above is historical evidence for these unchanged pure implementation bytes;
no new coverage, sanitizer or SwiftPM execution is claimed. Portable runtime,
parser, C++ ownership and SwiftPM inputs are unchanged. No real foreign-peer,
retirement, overlap, native cancellation or fault probes were run. Production
pins, quarantines, protected apps, global SVG and OS-managed containers remain
unchanged. Reconnection, release trust, resource policy, application adoption
and full editor acceptance remain open.
