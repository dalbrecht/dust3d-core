# ADR 0097: Record reproducible ordinary static export timings

Status: Local timing harness; wider M8 acceptance pending (#99).

M8 needs measured performance evidence instead of guesses. Add a serial Python
standard-library harness for the known-positive licensed Capsule/Sword fixtures,
with OBJ, geometry FBX and64-square baked FBX. Default one sample per case;
repetitions are bounded1...5. It uses the existing shared build root and deletes
only its own temporary output directories. There are no fault/crash/exhaustion
inputs, native app launches or dependency/pin changes.

Record per-process elapsed wall time including startup/IO, executable/fixture
SHA256 before/after, output count/hash, exit status and original diagnostics.
Accept only stable executable/input identity, exit0 and nonempty committed output.
A native failure or success without bytes is never reported as a valid timing.
Checkout revision/tracked-diff hash, caller-reported configuration and nonpersonal
host architecture/OS/Python metadata contextualize the receipt. These are not
embedded executable attestations; retain the matching fresh source build logs.
Commit the harness before baselining because untracked files are not in the diff
hash. Inspect all case hashes before comparing or aggregating runs.

TDD first observed the missing module. Additional boundary regressions then
failed on missing deadline/receipt admission and unhandled callback errors. Eight
owned value/filesystem contracts now pass normally and with Python optimization.
They cover false success, original diagnostics, changed/lost identities, baked
settings, hashes/cleanup, unsafe receipt paths and fail-fast admission. All
failure/deadline cases use injected callbacks, without native fault execution.
Fresh final Debug CMake/CTest passes 100/100 entries (40.20s), including enabled
C++ assertions. The Release build/contract smoke gate passes 100/100 (6.64s);
Release NDEBUG may disable inherited assertions, so it does not replace Debug
semantic evidence. No new native production code or dependency is introduced.

Wall time does not measure peak RSS, GPU upload, editing responsiveness, full
pipeline phase timing, large documents or cross-hardware equivalence. Nonempty
output does not replace independent importer verification. No production budgets,
whole-core coverage or M8 completion are claimed. Wider performance, renderer,
accessibility, distribution and full parity acceptance remain open.

Recovery adds a 60-second measurement deadline and failure-aware handling for
launch errors, deadline diagnostics and an unreadable identity after the run.
These failure tests use injected callbacks only, with no native fault, timeout or
signal delivery. Native positive samples remain limited to the known fixtures.
Receipt admission confines writes to the shared build root and rejects symlinks,
directories and input/executable hardlink aliases before any measurement, then
rechecks before writing. Raw tracked-diff hashing disables external diff/text
conversion. All sample temporary directories remain within the shared root.
Recovery receipts below use freshly qualified measurements. The older saved
prototype receipts are historical and are not reused as recovery evidence.
Sampling now stops after the first rejected sample instead of continuing to
later modes/fixtures. A regression using mocked measurements (no native calls)
first observed six calls instead of one. Planned/measured counts and the stop
state are explicit, and partial/rejected runs cannot return success.

## Fresh recovery receipts

Final samples use committed source `0cbb683c2cfc233247bf4333b3343500091a2c5c` with a clean tracked
tree on arm64 Darwin 27.0.0, Python 3.14.5.
The single shared CMake root was freshly rebuilt in Release with sanitizers OFF;
exact flags, build logs, executable/fixture/output hashes and original diagnostics
are retained with the receipts. All six planned samples qualify, with no native
rejection, deadline or signal delivery. The earlier pre-fail-fast-control sample
set is retained separately; it also had no rejected native samples and is not
the table below.

| Fixture | OBJ wall ms | Geometry FBX wall ms | Baked FBX wall ms |
| --- | ---: | ---: | ---: |
| Capsule.ds3 | 2.918 | 2.819 | 2.979 |
| Sword.ds3 | 6.073 | 5.747 | 7.357 |

These are one sample per case, including startup/IO on a non-isolated host, not
statistical or production budgets. Capsule outputs are 2,492/9,706/17,494 bytes;
Sword outputs are 11,550/32,746/54,696 bytes. No peak-RSS/denied timing query,
native crash/fault probe, stale binary or app-control operation was used. No
independent parity or full M8 acceptance follows from nonempty timing output.


## Delivery preparation on the legacy-argument PR

Replay the harness onto PR151 head88e61598, retaining the corrected OBJ scene
binding verifier and accepted-parent evidence. Runtime/harness sources match the
original audited ba82aaea stage byte-for-byte; negative benchmark cases remain
injected callbacks with no native launch, timeout or signal probes.

Fresh Debug build and100/100 CTest pass19.11s. A fresh Release build and100/100
smoke contracts pass6.44s; inherited NDEBUG assertions remain a limitation and
this does not replace Debug semantics. Six serial positive samples from the clean
committed38246145 source qualify, with stable executable/input hashes and output
counts/hashes. Capsule/Sword each cover OBJ, geometry FBX and64-square baked FBX.
Single wall samples range2.5–6.8ms on this host, including process startup and IO.
No rejected/native failure case ran during measurement. The receipt and matching
build/test logs are retained locally with executable/source provenance.

One sample per case is not a statistical budget, cross-hardware comparison, peak
memory/GPU/phase measurement, independent import verification or M8 acceptance.
No new native production code, dependency, production pin or quarantine changes.
Before public delivery, verify source/tree identity on accepted master. Require
completed clean exact-head automated review and all CI green before merging.
