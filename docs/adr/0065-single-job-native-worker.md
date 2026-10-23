# ADR 0065: Single-job native worker prototype

Status: implemented locally for #112; public review and hosted CI pending.

## Decision and build graph

Turn the existing frame, mesh and failure codecs into a real single-job Unix
helper. `dust3d_worker` is a CMake executable on Unix. Its OS entry point depends on
`dust3d_worker_operation`, a separately testable native execution adapter, which
depends on the existing serialized C ABI and standard-library-only protocol codecs.
No engine or Qt implementation is duplicated. The operation adapter is portable
C++17; descriptor handling stays in the executable. Keeping native files together
preserves the current CI formatter/Qt checks. No third-party dependency is added.
A pure iostream-only entry point would not isolate backend C stdio; an Apple-only
process service belongs in the later application transport layer.

The executable accepts no arguments. Stdin is exactly one version-one request
frame whose operation is generateMesh and whose body is raw complete `.ds3` bytes,
followed by EOF. Header identity, kind and advertised size validate before body
allocation; body truncation or trailing bytes reject before generation. The default
input limit is 128 MiB. The caller must close the input after the request; this
prototype has no in-process deadline for an input that never completes.

Generation calls the existing serialized C entry point. Before copying native
buffers, the execution adapter checks vertex/source counts, bounded warning length
and logical output policy. All borrows are copied while the owner lives; the native
owner is released before semantic payload encoding. Input storage is released
before the outer response copy. Success emits the owned base-mesh schema; native
non-success emits its explicitly mapped status and exact bounded reported cause.
Unknown native status numbers preserve their original number/cause as generation
failure; success cannot be passed into the pure failure mapper. Invalid requests
and output configurations validate before execution. Smaller consumer output
policies can return typed limit outcomes after ordinary bounded generation.

## Channels, diagnostics and exits

Reserve a close-on-exec duplicate of original stdout for binary replies. Redirect
ordinary stdout to `/dev/null` before parsing/generation, so inherited std::cout and
C stdout diagnostics cannot corrupt the protocol. Suppress C++ engine stderr at
the stream-buffer boundary, restoring it afterward. Keep the actual stderr
descriptor available for C runtime/sanitizer reports; redirecting it entirely would
hide validation findings. Native operation causes remain binary data, not logs.
Bounded transport diagnostics use stage, code, request and operation fields and
preserve available protocol/errno descriptions. Secondary diagnostic failure does
not change the primary result/exit decision. A parent must still treat all child
stderr as untrusted bounded data, not automatically persist or publish it.

| Exit | Meaning |
| --- | --- |
| 0 | One complete mesh or typed non-success reply was written |
| 64 | Unsupported command-line arguments |
| 65 | Invalid/incomplete/extra request framing |
| 70 | Unrecoverable transport/encoding/internal exception |
| 74 | Descriptor/read/write failure |

No reply is written until the full request is admitted and the owned response is
encoded. Kernel I/O can still fail after a partial write. A parent must require a
complete correctly identified and semantically valid reply plus exit zero before
publication, discarding fragments or abnormal exits without automatic retry.
SIGPIPE is ignored in this child so closed output pipes can report an I/O failure;
no signal delivery or broken-pipe case is deliberately forced in current tests.

## Verification

The black-box contract first failed because the helper did not exist. Fifteen
real subprocess cases now verify licensed Capsule and Sword bit-for-bit against
the ordinary native C API, opt-in existing seam-report diagnostics without binary
stream corruption, invalid/empty archives with exact native failure bytes, empty
input, incomplete header/body, trailing bytes, response-as-request, unknown
version, oversized advertised input, zero identity, invalid arguments and ordinary
directory-as-stdin read failure. The directory returns errno 21 and controlled
exit 74 with no partial reply. Every subprocess exits normally through its defined
policy; no crash, allocator failure, signal delivery or resource exhaustion is
induced. CMake file redirection tests actual process separation; fragmented pipes,
back-pressure and parent cancellation are not claimed tested here.

A separate operation contract checks all six native status mappings, bounded
nonterminated diagnostic arrays, unknown numeric status preservation, invalid
success-as-failure, actual Capsule output, small vertex/decoded/wire budgets,
ordinary invalid input, wrong request kind/length and invalid output policy. These
are normal value/policy cases, not fake native handles or allocator hooks.

Final full normal macOS CMake/CTest passes 58/58 in 33.31 seconds. ASan/UBSan passes
57/57 in 85.62 seconds; the Swift consumer remains normal-only. Instrumented
subprocess/operation runs pass. WorkerOperation.cc measures 108/120 lines (90%),
70/79 regions, 6/6 functions and 48/62 branch outcomes. WorkerMain.cc measures
94/126 lines (74.60%), 89/122 regions, 9/11 functions and 35/60 branch outcomes.
These do not meet the later coverage adoption targets. Unforced setup/interrupted
I/O, broken-pipe, allocation/unclassified-exception and native-invariant guards,
plus currently unused C++ stderr sink callbacks, remain explicit gaps. No unsafe
probe or artificial hook is added for a coverage number. Metrics are file-scoped,
not inherited-core or release-containment coverage. The helper links no Qt or Apple
UI framework. All builds/profiles use one shared root and profiling products are
cleaned afterward.

## Remaining acceptance

This is a working development helper, not production containment or full M7/M8.
Logical output limits apply after native generation and do not cap its algorithm
workspace/RSS. No queue/deadline/resource/sandbox/parent-death policy, hard
cancellation, parent response retirement or application integration is provided.
The helper invokes no save/export API; that is not proof of complete filesystem
isolation of inherited algorithms. Source-aware UV/material/rig/animation schemas,
full corpus/performance acceptance and signed packaging remain required. Existing
GLB #67/#69, writer #83, fault #84, animation #95 and Apple #199 quarantines and the
Apple production pin remain unchanged.
