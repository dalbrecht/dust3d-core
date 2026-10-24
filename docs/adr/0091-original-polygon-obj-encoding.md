# ADR 0091: Encode original double OBJ polygons

Status: Local implementation; review and full M7 acceptance pending (#99).

Upstream application/sources/model_mesh.cc copies Object.vertices and
Object.triangleAndQuads into ModelMesh and exports only v/f records, preserving
polygon order and winding. It does not export OBJ normals, UVs or material files.
An earlier native prototype used float indexed preview triangles and therefore
cannot establish this topology/precision behavior. Use its audited two-pass text
pattern with original double positions and triangle/quad faces instead; no old
binary, test inventory or historical sanitizer receipt is reused.

OwnedObjDocument is move-only with immutable independent bytes. Pure count
admission accepts1M vertices/5M polygons, default64MiB/max256MiB root+capacity
quota, and checks a conservative minimum text size without geometry allocation.
Validate finite positions within+/-4M, three/four-corner faces and every source
index before formatting. Empty original faces return NO_GEOMETRY; do not silently
substitute preview triangles. Use one-based shared vertex references, unchanged
coordinates/units/axes and classic-locale max_digits10 double text. The higher
precision intentionally improves on QString::number's default formatting; no
byte-identical comment/precision claim is made against Qt output.

Count text in a first pass, allocate the exact owned buffer, recheck actual
capacity and encode again with size-consistency checks. Per-record formatting
workspace is bounded by input arity/index/precision limits; allocator overhead
and source storage are excluded from the output quota. No files, globals or
native engine calls occur in the pure encoder. Source must stay alive/unchanged
through both passes. Allocation/format failures propagate to a future C boundary.

Add an opt-in original-object generation output before the source Object is
destroyed. Build requested OBJ and FBX outputs locally, then publish both only
when every preparation and cancellation check succeeds. A failure preparing a
companion result preserves all caller-owned previous output pointers. Default
preview callers are unchanged; no public OBJ C API, CLI dispatch or pin adoption
is included yet.

## Verification

Missing encoder/hook/verifier APIs were recorded red. Source audit/quarantine and
format checks pass. Recovery uses a fresh Debug build;93/93 normal CTest
contracts pass (37.99s). SwiftPM builds the library and executable in8.59s.
A fresh full ASan/UBSan rebuild instruments engine, core, remesher and triangle;
91/91 sanitizer contracts pass (110.88s). The two ordinary macOS Swift consumers
are excluded because swiftc does not link UBSan. System zlib is not instrumented.
Strict-warning syntax checks pass for the new encoder/planner/native driver;
manifest/consumer lint and Qt boundary checks pass. Inherited warnings remain
recorded separately. Exact tested source8621ff27d1fb8df36e61ea70f1a53b1ca6540925,
its tree, driver/output hashes, flags and logs are retained with recovery bundles.

Contracts independently parse v/f text, preserving a quad and triangle, shared
indices/order, exact double round-trip at1000000.123456789 under a decimal-comma
C++ global locale, exact/one-below output quotas, numeric count/budget extremes
without large allocations, invalid arity/index/nonfinite/bounded coordinates,
and real source generation. A post-generation companion FBX quota rejection
proves that an already prepared OBJ does not partially replace prior outputs.

Historical scoped coverage: the pure planner measured18/18 lines,23/23 regions
and16/16 branch outcomes. The encoder measured71/74 lines (95.95%),36/39 regions
and29/32 branch outcomes; these measurements were not repeated during recovery;
actual allocator over-capacity and second-pass inconsistency guards remain
unforced. Combined historical implementation coverage was89/92 lines (96.74%). This is scoped
coverage, not whole-core coverage. No allocation/crash/signal/exhaustion probes.

Only a freshly authored known-positive99-byte quad/triangle OBJ reached
installed Assimp6.0, producing3088bytes of XML during recovery. The optional optimized-Python verifier confirms all imported positions,
unchanged four/three-corner faces, winding and both identity transforms. Four
value-only verifier tests run normally/optimized, rejecting triangulation,
reversed faces, changed positions/transforms and optimized false acceptance.
This is one controlled independent-consumer fixture, not broad Qt/DCC parity.

Reproduce with a fresh build and empty controlled fixture directory:

```sh
"$BUILD_DIR/dust3d_obj_document_tests" tests/core/fixtures "$FIXTURE_DIR/polygons.obj"
assimp dump "$FIXTURE_DIR/polygons.obj" "$FIXTURE_DIR/polygons.assxml"
python3 -B -O tests/core/verify_obj_scene_import.py "$FIXTURE_DIR/polygons.assxml"
```

## Remaining integration and discovered limitation

Source inspection found that generateMesh still rejects an entirely collapsed
float preview before reaching original-geometry export hooks. A source Object
can be meaningful in double precision even when preview coordinates collapse.
Decouple export preparation from preview validity before exposing the OBJ path;
the current static FBX API shares this limitation. No numerically extreme input
was submitted to inherited generation; the next policy/output fix should use
small healthy owned Object values rather than crash-oriented generator trials.

Public OBJ ownership/orchestration, atomic CLI dispatch, native app export,
progress/cancellation and broad source/consumer evidence remain required.
The Apple production pin and all GLB/rig/animation/writer/fault quarantines remain
unchanged. Full M7/editor parity is still incomplete.
