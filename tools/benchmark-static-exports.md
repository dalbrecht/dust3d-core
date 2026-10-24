# Static export timing receipts

`benchmark_static_exports.py` measures fresh-source ordinary Capsule/Sword exports
as OBJ, geometry FBX and64-square baked FBX. It uses Python's standard library,
runs serially, cleans only its own temporary output directories and records each
process wall time including startup and filesystem work. This is not a stress,
GPU, peak-memory, cancellation, large-document or full-parity benchmark.

Use the same shared build root for all stages. Run the source quarantine guard,
configure/build fresh in Release, and retain the exact build/test logs. For example:

```sh
cmake -P cmake/CheckGlbCatalogQuarantine.cmake
cmake -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release -DDUST3D_ENABLE_SANITIZERS=OFF
cmake --build "$BUILD_DIR" -j2
ctest --test-dir "$BUILD_DIR" --output-on-failure
python3 -B tools/benchmark_static_exports.py --executable "$BUILD_DIR/dust3d-export" \
  --build-root "$BUILD_DIR" --configuration Release --receipt "$BUILD_DIR/static-export-timings.json"
```

Repetitions default1, at most5. The receipt records source revision/tracked-diff
hash, caller-reported configuration, host architecture/OS/Python, fixture and
executable SHA256 values, output counts/hashes, exit status, original diagnostics
and acceptance per sample. Only stable input/executable identities, exit0 and
nonempty output qualify. Failed cases retain their original diagnostics; they
never become successful timing samples. Inspect all hashes before comparing runs.

The receipt must be a regular-file destination inside the shared build root; it
must not be a symlink or an alias of the executable or either fixture. Admission
runs before sampling and again before writing. Each measurement has a 60-second
subprocess deadline. Launch/deadline failures and identity-recheck errors remain
rejected records with original diagnostics; an unavailable exit status is null.
Tests use injected callbacks for these errors, not native failures or signals.
This supervision is not an engine cancellation or resource-limit guarantee.
Sampling stops at the first rejected record, without retrying or running later
cases. The receipt distinguishes planned samples from measured records and
preserves the original rejection. Its fail-fast regression mocks measurements
and Git metadata; it starts no native process.

The source revision is checkout metadata, not an embedded executable attestation;
the retained fresh build log ties the command to the source. Untracked files are
not represented by the tracked-diff hash; commit the harness before baselining.
Independent importer checks are separate acceptance evidence: nonempty output is
not proof of topology/material parity. Single-host samples do not set production
budgets or establish Intel/Apple Silicon equivalence. Keep wider M8 work open.
