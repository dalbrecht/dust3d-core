# Owned native bake verification

Scope: core issue #39, ADR 0015. This stage exposes portable pixels through C and
proves strict Swift 6 copies. It does not establish Apple rendering or full editor
parity, a stable distribution ABI, or complete upstream compatibility.

## Actual behavior exercised

- Real Capsule archives produce 80 aligned UV/tangent records and owned RGBA8
  output. Exact final budgets succeed; one byte less fails without a partial owner.
  Later successful/failed loads do not change an existing owner's borrowed pixels.
- An independently encoded archive resolves borrowed image pixels and generated
  metallic/roughness/emission attributes into actual maps. Straight alpha remains
  128. Altering the caller image buffer after loading leaves output unchanged.
  Missing decoded images fail explicitly. Malformed/out-of-range material scalars
  retain their specific diagnostic at the C boundary.
- Analytic padded-row images cover a readable last row without requiring trailing
  padding. Mixed-map and normal-map exact quotas account for retained C tangent
  records rather than temporary original-triangle double records.
- A generated precision fixture actually drops a prefix of triangles at float
  export. Borrowed C geometry/UVs/tangents agree with independently selected
  original C++ bake frames. A separate analytic case distinguishes unknown dropped
  UVs from the surviving X tangent, preventing accidental prefix-copy acceptance.
- Snapshot pattern type, color, scale, belly, seed, wrinkle strength and serialized
  folds match independently constructed resolved C++ inputs, including the
  reference UV path's minimum pattern scale of 0.005.
- Null owners/accessors, unknown map selectors, malformed sized options, duplicate
  image identities, dimensions, descriptor arrays, stride/span overflow and
  independent UV/optional-map quotas reject safely with typed causes.
- Cancellation and querying complete while the engine mutex is held by another
  thread. Cancelled calls preserve CANCELLED and their original diagnostic. Tokens
  are one-shot and destruction requires all concurrent users to finish.
- A genuine C translation unit consumes the new declarations. The strict Swift 6
  smoke copies Capsule/Sword geometry, UVs, pixels and 80/320 tangent records before
  releasing each owner, then compares independent loads.

## Failing-first regressions

The initial owner contract failed to link before its implementation. Subsequent
behavioral tests first reproduced unknown active patterns becoming plain color
and harmless alias labels/dirty flags being rejected. They now reject unsupported
patterns explicitly and compare only relevant alias visual definitions.

ASan reproduced an 88-byte copy from a foreign options buffer containing only
struct_size. The factory now validates that field before any full struct copy.
UBSan reproduced an invalid-enum load for an unknown map selector. The C function
now transports the selector as uint32_t while retaining named map constants, so
all foreign bit patterns can be safely rejected. Neither fix uses a runtime fault
switch or an artificial coverage branch.

## Gate and scope

The full CMake Debug suite passed 34/34 (24.94 s). Strict Swift 6 consumers passed,
including both baked fixtures. The full ASan/UBSan suite passed 34/34 (70.34 s).
Seven relevant optional Qt comparisons passed (21.27 s); the underlying baker and
reference paths are unchanged by this adapter stage. Native linkage inspection of
the new contract executable lists only libc++ and libSystem, with no Qt/Apple UI
framework. New source formatting, strict Swift lint and diff checks passed.

LLVM instrumentation of BakedMesh.cc and Cancellation.cc was linked into the
actual boundary contract executable. All 19 functions ran. BakedMesh.cc measured
194/207 executable lines (93.72%), Cancellation.cc 16/17 (94.12%): aggregate 210/224
lines (93.75%), 182/204 regions (89.22%) and 120/148 branches (81.08%). This scope
excludes headers, the generation bridge, inherited kernels and the complete core.
Allocation/catch-all recovery and defensive invariant branches remain uncovered;
no unreachable behavior was added to improve the report. The standards' coverage
targets remain unmet by this measurement and are tracked by core issue #1 under
the repository's documented adoption policy.

Logical quotas count final map bytes and aligned tangent records, separately from
UV metadata. Temporary C++ tangent output has its own 256 MiB ceiling. Input
storage, allocator capacity, generation and lookup/raster workspace are excluded;
this is not a process-memory or cancellation-latency guarantee. Inherited document,
generation and UV work observe phase boundaries rather than arbitrary preemption.
Apple exact-pin adoption, asset decoding, PBR/transparency/AO, textured export,
UI/GPU checks and broader M3/M4 acceptance remain separate reviewed stages.


## Review regression: legacy decimal separators

Automated review found that classic-locale full-consumption parsing rejected
legacy comma-decimal scalars accepted by normal generation. A failing-first real
archive test confirms ordinary preview accepts comma-decimal metallic/roughness/
emission before requiring the same baked values. The scalar boundary now
normalizes comma to period before strict parsing, retaining finite/range checks
and rejecting trailing junk or mixed separators. Pattern scale, belly and wrinkle
strength have pixel-equivalence checks between comma and period spellings.

After this fix, Debug passed 34/34 (24.94 s), ASan/UBSan passed 34/34 (70.34 s),
strict Swift consumers passed, and scoped coverage above was refreshed. Formatting,
manifest lint and diff checks passed. No inherited kernel/reference path changed.
