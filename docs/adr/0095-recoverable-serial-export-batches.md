# ADR 0095: Add recoverable serial headless export batches

Status: Local implementation; review and full M7 acceptance pending (#99).

M7 requires partial multi-file failures to be disclosed and recoverable. Add
--batch with explicit input/output pairs and a bounded64-job admission policy.
Common geometry/bake/output-quota options apply to every job; OBJ baked mode
continues to reject explicitly. Keep ordinary two-path invocation behavior.

Before any export, resolve path identities and inspect existing inode/device
identities. Reject known overlapping output ownership and output aliases of
batch input, including inputs used by later jobs. This prevents a successful job
from destroying another job's source. Global preflight/usage failure commits
nothing. This metadata check does not promise safety against concurrent external
filesystem mutation; each job retains existing publication revalidation.

Recheck each destination against earlier committed output identities before
export. Missing names that differ only by filesystem case rules may not alias
until the first file exists; an ordinary owned-directory regression exposed the
initial command reporting two commits for one physical output. The later job now
fails explicitly, preserves the first output and contributes to partial-failure
reporting. Case-sensitive filesystems still accept distinct case-variant names.
Capture the staged inode/device through fstat before rename, and allocate all
batch bookkeeping before the first publication. Recording a completed identity
uses only nonthrowing scalar assignments; no metadata lookup/allocation after
rename can turn a committed file into a reported failure. External filesystem
mutation remains outside the guarantee.

Execute jobs serially through the same export use case. Each reads/generates its
own current archive, completes encoding, then atomically commits independently.
Catch per-job typed native/filesystem failures, retain their original diagnostics
and job index, continue remaining jobs and disclose committed/failed counts.
Exit5 means partial/all batch failure;0 means all committed. No rollback of earlier
successful files is claimed. Users can retry the disclosed failed pairs. Ordinary
single-job exit codes2/3/4 remain unchanged. No process concurrency, unbounded job
list, cached/stale preview output or production dependency update is introduced.

Upstream Qt main.cc uses -o/-output target accumulation and special single-source
expansion. This stage provides a new explicit-pair headless workflow rather than
claiming exact flag compatibility. Legacy syntax, model-plus-WAV, rig/animation
and quarantined GLB paths remain outside this stage.

TDD first observed --batch rejection through the freshly built parent command.
Recovery adds stronger original-cause/job-index assertions, a corrected failed-
pair retry preserving earlier commits, normalized-path aliases and the case-
identity regression described above. All 16 ordinary CLI contracts pass on both
CMake and SwiftPM products. The case-insensitive branch ran on this host; the
case-sensitive branch remains for a corresponding CI volume, without claiming
both filesystem behaviors were measured locally.

A fresh Debug build and full CTest pass 98/98 contracts (28.38s final recovery
run). SwiftPM passes (1.73s). A fresh strict-warning ASan/UBSan command instruments
the CLI and OBJ boundary; all 16 filesystem contracts pass (1.241s). Inherited
libraries and system zlib are not instrumented in this scoped run. Formatting,
manifest/consumer lint and Qt/quarantine checks pass. Exact tested recovery source
is 0e3405f4777f6be7024a6d77492909dcc649b3ba; source bundles, red/green logs and
source/binary/output hashes are retained. No native fault/crash/resource-
exhaustion probes ran; the 65-pair check is ordinary argument admission with no
generation.

Fresh CMake and SwiftPM mixed batches produce byte-identical 2,492-byte OBJ and
9,706-byte geometry FBX outputs with complete commit summaries. The committed
optimized OBJ verifier passes on both fresh Assimp imports; it compares emitted
text, not independent generator precision. This run checks the FBX container
and equality, not a new full independent FBX geometry/material acceptance. Those
format boundaries retain their separately documented fixture evidence.

Historical scoped command coverage (not remeasured after recovery/case fix):
246/278 lines88.49%,252/313 regions80.51%,151/188 branch
outcomes80.32%. Preflight:18/18 lines,14/14 regions,12/12 branch outcomes100%.
Parser:43/43 lines and64/64 regions100%,47/50 branch outcomes94%. Batch execution:
19/22 lines86.36%,15/17 regions88.24%,6/6 branch outcomes100%. Unforced OS-error,
interruption, cleanup and generic-exception paths remain gaps. These measurements
are not whole-core coverage. Pins and all quarantines remain unchanged; native
editor adoption and full M7 acceptance are still open.
