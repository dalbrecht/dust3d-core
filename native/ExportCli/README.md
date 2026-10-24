# dust3d-export

Qt-free static procedural OBJ/FBX export for macOS and Unix-like hosts. Build with
CMake (`cmake --build "$BUILD_DIR" --target dust3d_export`) or the core SwiftPM
package (`swift build --product dust3d-export`). System zlib is required.

```sh
"$BUILD_DIR/dust3d-export" input.ds3 output.obj
"$BUILD_DIR/dust3d-export" input.ds3 output.fbx
"$BUILD_DIR/dust3d-export" --baked --texture-size 256 input.ds3 output.fbx
```

OBJ preserves original double vertices and triangle/quad v/f polygons, with no
UVs/normals/materials. Baked OBJ is explicitly rejected. Geometry is the default. `--baked` embeds a procedural atlas; its default texture
size is256 for all maps. Sizes1...8192 are supported. `--max-output-bytes N`
sets the C API's final retained-output quota; zero uses64MiB, maximum256MiB.
Use `--` before positional filenames that start with a dash. `--help` lists usage.
Decoded external images, imported models, rig/animation and WAV/GLB output
are not supported by this command. Unsupported source visuals reject explicitly.

The command reads a bounded regular archive, validates a distinct `.fbx` or `.obj` target,
then completes encoding before creating a private0600 staging file beside it.
It flushes/closes staging and atomically renames on success. Existing output
survives argument/native/pre-commit failures. Destination symlinks, directories
and source aliases are rejected. The input archive is never opened for writing.
Concurrent filesystem mutation and power-loss directory durability are outside
this prototype's acceptance; this is not a privileged filesystem service.

Exit status0 means committed output (or help),2 invalid usage/destination,3 native
export rejection and4 filesystem/runtime failure. Original native/OS diagnostics
go to stderr; success is reported only after rename. Cleanup is best-effort and
its own errno is surfaced without replacing the primary cause.

Run the source quarantine guard, build fresh and run CTest. The optional Python
CLI contract uses only owned temporary files and the licensed Capsule fixture.
No UI launch, crash, signal, broken-pipe or resource-exhaustion probes are needed.
POSIX-specific publication stays in this executable; portable library/public
headers remain Qt/Apple/POSIX-free. Windows publication and complete exporter
parity remain follow-up work under core#99.

## Recoverable serial batches

Use explicit input/output pairs, with common options applied to every job:

```sh
"$BUILD_DIR/dust3d-export" --batch first.ds3 first.obj second.ds3 second.fbx
```

At most64 pairs are accepted. Preflight rejects known output path/inode collisions
and output aliases of batch inputs before the first commit. Each job also rejects
aliases of earlier committed outputs, including case-equivalent names whose
identity becomes visible only after creation. Each job then reads
and generates its own source snapshot; no preview/cache result is substituted.
Successful jobs stay committed when another fails. Diagnostics identify the job
index, preserve its original cause/exit class, and summarize committed/failed
counts. Exit5 means at least one batch job failed; retry only those failed pairs.
Exit0 means all batch jobs committed; global argument/preflight errors reject the
whole invocation before writing. Aliases detected after an earlier commit are
partial failures and leave that earlier output intact. This is serial, with no parallel memory growth.
External path mutation remains outside the guarantee. The explicit-pair syntax
is new: it does not emulate Qt's -o/-output expansion or enable WAV/GLB/animation.
