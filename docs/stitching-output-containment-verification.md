# Stitching diagnostic output containment (#101)

Ordinary Debug builds previously called a private SVG writer after connected-loop
stitching and truncated `/tmp/stitch_loop_debug.svg`. The maintained core removes
that automatic call, its private implementation/declaration and the now-unused
fstream include. No geometry calculation, public ABI, explicit export or license
changes. Inherited source formatting is preserved without mass formatting.

The existing connected-loop synthetic fixture now has an output-containment
scenario. It snapshots a preexisting regular diagnostic file (at most one MiB)
and its modification time without changing it, generates the known-positive
8-node/2-part mesh, and requires existence, bytes and time to stay unchanged.
A missing path must remain missing. Nonregular or oversized paths refuse the
check before mesh generation; unrelated files are never deleted by the test.

## Evidence

The test-first Debug run failed with the precise global-write diagnostic. The
preexisting 33,969-byte SVG was backed up beforehand and restored with its original
bytes and modification time after that expected failure. After removal, all four
stitching contracts pass: lines, loops, fill and output containment. All 53 normal
macOS contracts pass in 46.23 seconds, including the strict Swift consumer from
#108. ASan/UBSan passes all 52 registered C/C++ contracts in 154.27 seconds; the
macOS Swift consumer is intentionally normal-only. Post-gate inspection confirms
the original SVG is unchanged. No allocation-failure, crash or signal probe ran.

## Scope limits

This removes one automatic filesystem side effect. It does not establish a worker
sandbox, parent-death handling, memory/CPU limits or complete filesystem isolation.
Explicit developer wrap dumps and caller-requested save/export paths remain
separate. The Apple full source-bound local-core/app gate is not claimed here;
its production dependency pin stays unchanged. Existing GLB, writer, animation and
rig-pose quarantines remain intact. Public exact-head review and hosted CI are
required before this core change merges. Broader #101/M8 acceptance remains open.
