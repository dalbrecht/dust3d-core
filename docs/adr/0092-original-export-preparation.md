# ADR 0092: Separate original export from float preview preparation

Status: Local implementation; review and full M7 acceptance pending (#99).

ADR0091 identified that original double exports still depended on successful
float preview conversion. A valid small triangle can lose all area after that
conversion. Exporting original geometry must not require a renderable preview.

GenerationPurpose defaults to preview. The originalExport mode requires an OBJ
or FBX owner and rejects render-only source, surface and source-bake descriptors.
It prepares original export owners without allocating preview vertex storage.
Optional UV/bake outputs prepare export maps; baked FBX requires aligned atlas
outputs. Validate and stage original geometry before baking. Publish requested
export owners together only after all preparation and cancellation checks pass.
The public static FBX API selects this mode for geometry and baked exports.

Extract preparation of an immutable healthy Object from inherited generation.
The caller serializes access, keeps borrowed inputs alive and supplies empty
preview storage. Existing node/part/warning metadata is retained. Default preview
conversion, finite/index checks, float-collapse filtering and render descriptors
remain unchanged. Public C ABI layout and Apple pins are unchanged.

The regression uses three owned positions near one million with a separation of
0.001: double area remains nonzero while float positions coincide. This controlled
value object never enters inherited mesh generation. Preview rejects it; original
OBJ/FBX retain its double coordinates and preview capacity stays zero. Additional
contracts cover invalid purpose/output combinations, cancellation, companion
quota failure preserving previous owners, fresh storage and polygon-only OBJ.
No extreme-model generation, crash, allocation-failure or quarantined probes ran.

Recovery uses a fresh Debug CMake build;94/94 normal contracts pass (38.87s).
A fresh full ASan/UBSan rebuild instruments engine, core, remesher and triangle;
92/92 sanitizer contracts pass (114.99s). Two ordinary macOS Swift consumers are
excluded because swiftc does not link UBSan; system zlib is not instrumented.
SwiftPM passes (8.62s). Strict-warning syntax checks pass for the changed bridge
and regression driver; formatting, manifest/consumer lint and Qt boundary pass.
Inherited warnings remain recorded separately. Exact tested recovery source is
9ca339a527fbc2bbf60525912a2d25bf49f8f372; tree, flags, driver/output hashes and
logs are retained with source bundles. Fresh CMake and SwiftPM baked CLI outputs
are byte-identical17494-byte FBX and independently re-imported through Assimp6.0.
Optimized Python matches original corners, UVs, exact baked pixels, material
bindings and identity transforms against the pre-encoding source oracle.

Historical scoped coverage (not remeasured during recovery): three ordinary
drivers linked the same instrumented Dust3DBridge.cc object.
prepareExports: 17/17 lines, 8/8 regions, 6/6 branch outcomes. Output validation:
15/16 lines, 40/41 regions, 31/36 branch outcomes. prepareGeneratedObject: 93/112
lines (83.04%), 90/110 regions, 61/88 branch outcomes. Entire scoped bridge file:
175/229 lines (76.42%), 173/232 regions, 118/172 branch outcomes. These are scoped
measurements, not whole-core coverage. Unforced inherited preparation/fallback
and unrelated public-preview paths remain gaps; no fault probes fill them.

Public OBJ ownership/CLI dispatch and native app export adoption remain next.
Imported, rigged, animated and GLB export remain outside this stage. Preserve all
quarantines; do not claim full precision/workflow acceptance or M7 completion.
