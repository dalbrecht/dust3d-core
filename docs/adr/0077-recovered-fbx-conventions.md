# ADR 0077: Recover the FBX convention foundation independently

Status: Delivery validated locally; review and full export acceptance pending (#99).

The archived native FBX work must be reintroduced in source-audited stages against
the current core. Recover only the pure convention helpers and their ordinary
value tests from the saved FBX convention source. The private recovery checkpoint
records the exact archive identity for reproducibility.
Do not restore its old target manifests, fixtures, binary artifacts, serializers,
animation evaluator or dependency pins. Keep the Qt exporter as the source reference
and retain the original MIT copyright.

The contract converts finite nonnegative seconds through 60 to 46,186,158,000
ticks per second with truncation toward zero. Angles are finite and bounded to
one million degrees. Normalize into (-180,180], retain signed half-turn ties in
shortest differences, and unwrap relative to the prior angle without repeated
360-degree loops. Check the resulting bound before returning. Original typed
DocumentError status/cause conventions remain; no new public C ABI is exposed.

The current-source test first failed because the helper header was absent. Tests
compare known times/ties and a small value grid with the inherited loop formula,
check a pure numeric continuity sequence, and reject nonfinite/out-of-policy
scalar values. They do not load any clip, rig, serializer or archive. These are
value-admission tests, not crash, timing, allocation or exhaustion probes.

This is an enabling layer for the full M7 export plan. Restored source and narrow
passing math tests do not establish full FBX output, re-import, animation or
editor parity. All existing quarantines and production dependency pins remain.

## Initial local verification

The source quarantine guard and formatter dry-run pass. Compared with the safe
base, only the audited pure math contract is newly registered. A clean Debug
CMake rebuild and all 63 CTest contracts pass (32.47 seconds). A fresh standalone
C++17 strict-warning ASan/UBSan build of the math source and its contracts passes;
this is scoped instrumentation, not a whole-core sanitizer claim. Scoped compiler
coverage reports 39/39 lines, 28/28 regions and 18/18 branch outcomes. The core
SwiftPM product builds with the added source; inherited UUID narrowing warnings
remain outside this change. No archived executable or unsafe fixture was run.

## Delivery revalidation

Recovered onto reviewed master `bdb36e72`, which includes the separate writer
mitigation and reviewed worker-resource stages. Only the pure math contract is
newly registered relative to this base. The fresh ordinary CMake build and all
69 CTest contracts passed (16.33 seconds). Fresh standalone C++17 strict-warning
ASan/UBSan math contracts and the SwiftPM product build (0.73 seconds) passed,
along with formatter, manifest lint, source quarantine and Qt boundary checks.
The earlier 63-case and scoped coverage measurements above are historical;
coverage was not remeasured for this delivery. No rig/clip/archive evaluation,
serializer, fault probe or production pin change was introduced.
