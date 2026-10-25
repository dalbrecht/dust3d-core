# 0129: C boundary for persistent service admission

Issue: #180. Status: local implementation; serial public review pending.

## Decision

Create service-specific reply collectors before submission, freezing the trusted
expected connection generation with the validated original header. The existing
bounded collector handles read/append/actual-EOF framing. Service collectors must
use service_admit; ordinary reply_decode rejects them. Ordinary collectors cannot
be promoted by supplying completion-time peer values.

The C admission call takes copied active document/request/revision, caller OS peer
observations and scalar service receipt. It uses PendingServiceReply and returns
the existing independent decoded owner. Original admission category/code/cause
cross as ADMISSION_REJECTED/DETAIL_ADMISSION. All exceptions remain caught at the
C ABI; rejection consumes the collector. A NULL result pointer leaves it unused.
Geometry/UV/text/failure ownership and logical quotas retain the existing rules.
The new service path makes a bounded temporary frame copy; budgets are not RSS.

Receipt operation uses uint32_t, not a C enum. The first focused UBSan run found
that reading an out-of-range enum value was undefined before validation. The new
field was changed to a raw integer, validated for exact known codes before enum
conversion; the invalid-enum test cast was removed. Corrected-source ASan/UBSan
and the raw integer regression pass. No old unsafe enum execution is reused.
This was a pure bounded boundary regression, not a native engine/OS crash probe.

The runtime SwiftPM target now includes WorkerAdmission.cc (15 runtime sources,
disjoint from the engine). No new dependency or production Apple pin change.
The C function cannot authenticate supplied observations or publish app state.
The real XPC host and Apple broker still need to capture real trusted connection,
OS peer, actual EOF and current source and invoke this API; current healthy XPC
fixtures continue their previous validation path.

## Evidence and remaining limits

The pure C regression first fails to compile for missing APIs, then C11 and C++
consumers pass. Pure fixtures cover failure bytes, base/UV geometry, collector-
independent lifetime, stale/foreign/current observations, absent EOF, mode bypass,
NULL output, zero generation, invalid header, wide unknown opcode, quotas and
replay. Supported ASan/UBSan passes without leak checking or injected failures.

Focused WorkerDecodedBridge.cc coverage is316/331 lines and98/100 branch outcomes.
WorkerReplyBridge.cc is97/126 lines and23/40 outcomes in that focused consumer run;
the new service-creation function is14/14 lines and6/6 outcomes. Generic allocation/
unknown-exception fallbacks remain unexercised under the fault-probe quarantine.
These are scoped profiles, not whole-core or all-test coverage claims.

Root clang-format/diff checks, fresh build, seven pure C/policy/admission cases
0.76s and all139 source-audited ordinary CTest cases22.70s pass. Source hashes match.
SwiftPM builds Dust3DWorkerRuntime and dust3d-worker; no helper execution follows.
The retained native build-system option emits its existing deprecation warning.
No new real failure/cancellation/denial/multi-peer/overlap, allocation-failure,
signal or resource-exhaustion probes. All quarantines and full editor/M8 gates
remain open. Reviewed app adoption, release trust, cancellation/cleanup/deadlines
and authenticated current-source publication still require separate evidence.

## Serial delivery verification

Replayed onto accepted core #207, retaining the accepted aggregate UV text budget,
checked UV fixture indexing and portable decoded-budget initializer fixes. The
decoded C++ test equals the prepared corrected source plus that initializer fix;
no earlier unsafe enum fixture is used. The quarantine guard and inventory audit
pass with 140 ordinary tests and four shared XPC resource locks. A fresh clean
build in the single shared root and root clang-format checks pass.

Normal and optimized policy suites each pass four tests, followed by 19 pure
C/C++ contracts including C11 consumers. A fresh focused decoded-bridge binary
linked only to pure codecs/collector/admission passes ASan/UBSan with
detect_leaks=0; no leak-check or native-engine sanitizer claim follows. SwiftPM
runtime/helper builds pass using the explicit shared scratch and caches
(1.26/1.18 seconds); the runtime has 15 sources disjoint from 34 engine sources,
and startup has five sources. The existing native-build-system deprecation warning
remains. No helper executable is run.

Four unchanged healthy file/socket base/UV XPC regressions pass in 2.09 seconds.
Socket base returns 240 vertices (host46671/service46672); socket UV returns
240 vertices and 80 triangles (host46682/service46683). Both join writer, actual
EOF decoding and control receipt. These jobs do not adopt service_admit yet.
Owned cleanup follows host exit and service PID disappearance; service exit status
is not independently observed. All 140 ordinary CTests pass in 21.90 seconds.
All 268 source hashes and five fresh fixture/sanitizer binary hashes remain equal.

Earlier coverage remains scoped historical evidence, not a new measurement.
No native cancellation, foreign-peer, retirement, overlap, fault or pressure
probe was run. Pure malformed codec/receipt checks do not parse native input or
induce OS failures. Production pins, quarantines, protected apps, global SVG and
OS-managed containers remain unchanged. Real XPC/app adoption and full acceptance
remain open.
