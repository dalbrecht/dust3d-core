# ADR 0212: Owned generated-rig C boundary

Status: Development version1 ABI, core #330 / Apple M5 epic #144.

Expose procedural archive generation through one immutable opaque owner. Borrow
archive/type/template/options/token spans only for the call. Template records
carry exact non-NUL UTF8 names/parents and finite closing angles; positions are
derived from assignments and radii recomputed. Validate version/exact size,
booleans/reserved/counts/limits/text/unique names/resolved acyclic forest before
archive generation. Maximum256 bones applies to input AND final post-eyelid count.

Read/generate under engineMutex with no imported context and rigInputOnly;
release the lock before generateFinalRig (it internally locks). Reacquire only
for final grounded Object preview preparation. Procedural loading rejects model
assets before GLB decoding. This is a declared first scope, not full import parity.
Stage all output in a unique owner and publish only after final cancellation.

C joints preserve final source order with UINT32_MAX root parent, Float32 heads,
tails/radius/closing angle, zero reserved word and two column-major16-double
rest-world/inverse-bind arrays. C layout is296bytes, matrix offsets40/168. Names
remain owner-backed, byte-distinct UTF8. Four-slot32-byte weight records preserve
indices/weights/unbound slots; join them through the exact original vertex map
from final preview preparation, including float-collapse filtering. Validate map
count and every index before copying. Geometry, names, records and all views die
with the owner. Getters/destroy are NULL-safe; borrowed mesh must not be destroyed
separately. No STL or C++ exception crosses C. Copy views in Swift while owned.

Default limits:256 bones,1M original vertices,15M draw corners,64MiB output and
64MiB retained input; each byte limit caps at256MiB. The private final Object/data/
rest stage reuses maximum_output_bytes as a SEPARATE temporary quota. Before
preview allocation, admit original-face worst-case draw/map staging with checked
arithmetic: joint records +NUL names/warning +3*originalTriangles*(Dust3DVertex +
32-byte weight +size_t map). Conservative admission may reject a quota that could
fit only after collapse filtering. Reported retained output charges mesh vertex
capacity, logical joint/weight records and NUL text, excluding container/allocator
headers. It excludes temporary mapping/native values/generation workspace; these
limits are not total RSS/time containment. Vertex binding polls its borrowed
atomic flag; other inherited stages/allocation/sort retain no fixed latency.

Catch DocumentError, typed RigFinalDataError (invalid/limit/cancel), guarded
PositionQuantizationError (limit), bad_alloc, std and unknown exceptions, preserving
original text where available. Clear optional diagnostics on success. Never expose
partial owners. C pointer/span memory validity remains the caller's obligation.

New narrow C++ consumer derives an assigned Capsule in memory from the existing
licensed fixture (adds two Body edge attributes and rebuilds only its byte header,
no engine writer). It checks actual bone extent, all draw weights, inverse identity,
exact names, invalid options/forest/NUL/quota, forward parent order, prior-owner
isolation and copied lifetime. Pure C consumer loads the unchanged unassigned
Capsule, verifies explicit unbound data/layout/lifetime. Both normal and ASan/UBSan
CMake contracts pass. Swift6 strict-concurrency/warnings-as-errors consumer links
the ACTUAL SwiftPM static product/module map, loads Capsule, copies views and uses
them after destroy. Product and engine builds pass; inherited uuid/vendor warnings
remain. Source guards precede scoped runs. Independent review found no correctness
defect; quota documentation clarified per review.

No broad inventories, old Apple199 controller replay, pose/animation evaluation,
writer/export, GLB decoding, Qt, signals, fault injection or exhaustion probe runs.
C ABI positive eyelids/custom Unicode and large model corpus are not established
by these consumers (private helper fixtures cover eyelids/Unicode separately).
No full measured inherited coverage, stable binary distribution, active Apple route,
physical GUI/HIG/VoiceOver acceptance or complete Qt parity claim. Production core
pin and quarantines remain unchanged. Swift app adoption is the next integration.
