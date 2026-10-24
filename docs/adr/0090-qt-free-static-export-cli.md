# ADR 0090: Publish static FBX through a Qt-free headless command

Status: Local implementation; review and full M7 acceptance pending (#99).

Expose the owned C API through dust3d-export, an actual headless .ds3-to-FBX
workflow independent of the Apple production pin. CMake builds this executable
on Unix; the macOS SwiftPM package exposes the same source as an executable
product. POSIX file handling stays private to native/ExportCli. The portable
library/public C headers gain no OS types, Qt dependency or second adapter.

Default geometry mode preserves original generated double geometry. --baked
embeds one procedural atlas, with an explicit256-square default for all maps;
--texture-size accepts1...8192. --max-output-bytes forwards the final retained
C API quota. Imported model assets and unsupported visuals reject before writing.
Rig/animation/WAV/OBJ/GLB, bulk batch policy and decoded external-image CLI inputs
remain open. No quarantined decoder/writer or production pin changes are made.

Read only regular archive files, bounded to256MiB, with checked positive sizes
and bounded streaming growth. Hold owned archive bytes through the synchronous
native call. Require an .fbx output, a directory parent and a regular distinct
existing target; reject destination symlinks and source inode aliases. Literal
dash-prefixed filenames are supported after --. No shell, external process or
unsanitized command execution is used by the command.

Encode completely before creating a private0600 mkstemp file beside the target.
Write bounded blocks, fsync and explicitly close it before atomic rename. Recheck
the destination before commit. RAII closes descriptors and removes uncommitted
staging. Cleanup errors surface their errno without replacing the original cause.
Success is reported only after rename through nonthrowing C reporting, so C++
logging allocation cannot turn a committed file into a failed transaction.
This is atomic local publication, not power-loss directory durability or a
privileged service against concurrent filesystem mutation. Native/runtime
workspace and cancellation latency remain the C API's documented limits.

Exit codes are0 success/help,2 usage/invalid destination,3 native export rejection,
4 filesystem/runtime failure. Preserve original native status/message and OS
errno in diagnostics. Read-only source descriptors never write archive bytes.
The CLI adds no signal handler, termination path or fault-injection mechanism.

## Verification and limits

The missing command was recorded red. New/changed registered sources were
inspected and source quarantine/format checks pass. A fresh Debug CMake build
and90/90 CTest contracts pass (28.08s final recovery run). SwiftPM builds the
library and executable in8.37s. Initial recovery manifest lint identified a
missing trailing comma in the now-multiline products collection; it was fixed
after the running gate ended, then lint and the complete normal gate passed.
Inherited warnings remain recorded separately. The exact tested source revision
isf6c9f69620dcac624d848cf4afd52a41b0b3fbd0; output hashes, logs and source
bundles are retained in the recovery checkpoint.

Nine CLI cases cover geometry replacement, real baked output, UTF-8/spaced paths,
literal dash filenames/uppercase extension, invalid/overflow/missing numeric
options/help, malformed archive/native output quota preserving old output,
source hardlink/destination symlink rejection, directory/missing-parent outputs,
missing/nonregular input and an unwritable owned staging directory. The last case
restores directory permissions in finally and skips explicitly under root; it
ran successfully on this host. Source bytes and absence of leaked staging files
are checked. No disk exhaustion, descriptor corruption, crash, broken-pipe or
signal-delivery probes run.

Fresh strict-warning scoped ASan/UBSan instruments the CLI shell and all nine
cases pass; the previously audited native libraries are fresh ordinary builds,
not instrumented by this scoped run. Historical scoped CLI coverage was169/196 lines
(86.22%),175/227 regions and99/130 branch outcomes; it was not remeasured
during recovery. Unforced paths include
oversized/growing input, interrupted/failed read/write/flush/close/rename,
cleanup/capacity guards, parser-warning reporting and generic exception fallback.
This remains below adoption goals. Thin OS-boundary failure/durability acceptance
and pure-policy extraction remain follow-up work; do not force unsafe failures
to inflate coverage.

Both the current CMake and SwiftPM executable exported the original licensed
Capsule through the public C API with --baked --texture-size64. Each17494-byte
FBX re-imported through installed Assimp6.0 during recovery; the two executable
outputs and the source-oracle driver output are byte-identical. An independent optimized-Python verifier
compared all80 faces/240 original double corners, normals/winding, packed UVs,
identity transforms, neutral factors, exact embedded color pixels and material/
UV/embedded-image bindings against an oracle from pre-encoding source/bake data.
The controlled test driver gains --default-material only for this oracle; its
ordinary four-map acceptance remains unchanged. No generated scene is treated
as a baseline or complete renderer/Qt parity proof. Mach-O dependencies are
system zlib, libc++ and libSystem; no Qt linkage.

## Remaining delivery

The current C API still prepares a float preview before retaining original double
geometry. Export therefore inherits preview admission restrictions until the
separate original-export generation-purpose stage. This remains an explicit
limitation before general export acceptance.

The native app still needs a reviewed core pin, Swift export service, destination
UI and progress/cancellation UX. Imported material policy groups, baked normal/AO
and alpha/shader agreement, broader consumers, Windows publication, bulk batch,
rig/animation/audio and other formats remain required. This command establishes
one usable static headless path, not full M7 or editor feature parity.
