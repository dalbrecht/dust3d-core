# ADR 0087: Encode baked RGBA pixels as bounded portable PNG

Status: Delivery validated locally; review and full M4/M7 export acceptance pending (#99).

Static textured FBX accepts validated encoded PNG maps, while the portable baker
produces RGBA8 pixels. Add an in-memory encoder using system zlib deflate and a
small PNG chunk wrapper. CMake requires ZLIB and links ZLIB::ZLIB; SwiftPM links
system libz. This adds a native link dependency, not a downloaded package or
copied decoder. The local SDK reports zlib1.2.12. Other hosts need their platform
zlib development package; Linux CI acceptance remains required before merging.

Alternatives considered: promoting the existing old vendored miniz from its
reference-test boundary, an Apple ImageIO-only encoder, a new PNG package or
uncompressed-only PNG. System zlib avoids another vendored implementation and
supports both portable headless export and macOS compression. The wrapper is
MIT; external zlib retains its permissive zlib license and platform ownership.
See the [official manual and license](https://zlib.net/manual.html). Do not
relicense or claim ownership of zlib. No Qt/native asset decoder is linked by this
change and the old reference-codec/miniz test boundary remains unchanged.

Pure count admission validates dimensions1...8192, exact tightly packed
width*height*4 source bytes and output quota57bytes...64MiB (zero uses64MiB).
Dimension bounds precede products and byte access. Stream one filter byte and
one row at a time, using a fixed64KiB deflate output block instead of copying a
full filtered image. Emit RGBA8/noninterlaced IHDR, IDAT chunks with CRCs and IEND.
Preserve all straight-alpha bytes and top-to-bottom order; no gamma conversion,
color-profile tagging, premultiplication or pixel decoding is performed.

Geometric output-capacity growth is bounded by quota. RAII releases compressor
state on normal completion or output-quota rejection. Unexpected zlib errors
retain their numeric status; memory errors map to LIMIT_EXCEEDED and other
library errors to GENERATION_FAILED. Source pixels, zlib workspace and stack
buffers are outside the encoded-output budget; this is not a process RSS cap.
Compression bytes are not promised stable across system zlib versions.

## Initial local verification and limits

The missing API was recorded red. Source audit/quarantine and formatting guards
pass. A fresh Debug CMake build and77/77 contracts pass (16.80s); SwiftPM builds
in0.70s, with inherited UUID narrowing warnings. A fresh strict-warning C++17
ASan/UBSan encoder/driver passes. It instruments this wrapper, not system zlib.
No allocation-failure, native bad-state, crash or exhausted-resource probes run.

Numeric-only cases cover zero/oversized dimensions, SIZE_MAX, exact8192-square
source size without allocating it, byte-count mismatch and quotas. Real tests
encode a distinct2x2 alpha fixture and a128x129 deterministic multi-block image,
check source immutability and same-runtime determinism, and reject an ordinary
insufficient output quota without returning partial bytes.

The optimized-Python acceptance tool independently checks signature, RGBA header,
chunk ordering, every CRC, complete zlib stream, all original pixel/alpha bytes
and row order. Only fresh known-positive encoder outputs reach decompression;
there is no malformed native-image probe or downloaded asset. It uses standard
library modules and is optional when Python is unavailable.

Scoped wrapper coverage is100/107 lines (93.46%),68/76 regions and39/46 branch
outcomes. Unforced paths are zlib initialization/compression failures, their
status mapping and actual allocator over-capacity rejection. These thin external
boundary residuals keep this below the adoption target and must remain explicit;
do not invent unreachable branches or fault probes to inflate the measurement.
This is not whole-core or system-zlib coverage.

Actual baked-map-to-material mapping, imported/source-aware groups, color/normal
shader agreement, aggregate image/output ownership, cancellation/progress,
owned C ABI, atomic destination writes, UI/CLI export and independent generated
textured-scene import remain required. The production Apple pin and all existing
quarantines remain unchanged. This encoder alone does not establish export parity.

## Delivery revalidation

Prepared on tested atlas-alignment delivery `d3824bab`, retaining the reviewed
worker/writer contracts and tested FBX prerequisites. Only the bounded encoder
and fresh-output independent verifier contracts are newly registered. Fresh
ordinary CMake build and 84/84 CTest contracts passed (17.49 seconds), including
the optimized-Python checks of PNG structure, CRCs, complete deflate data and exact
RGBA/alpha/row bytes for both the 2x2 and 128x129 fixtures. Large dimension/count
boundaries remain numeric-only; no large image is allocated for quota tests.

A fresh strict-warning C++17 scoped ASan/UBSan wrapper/driver build passed; system
zlib is not instrumented by that scoped run. SwiftPM built (3.50 seconds), and
manifest lint, formatting, source quarantine and Qt boundary passed. SDK headers
and CMake selected the platform zlib dependency; the SDK header reports 1.2.12.
The official API/license documentation and platform header were checked; no
package or SDK was installed or upgraded. Exact tested source/binary identities,
link paths, decoder versions and test logs are retained with local provenance.
Earlier 77-case and 93.46% scoped coverage results above are historical and were
not remeasured. External-library failure and actual allocator-capacity paths remain
unforced; no fault, exhausted-resource, crash or native bad-state probe ran.
Baked material integration, reviewed dependency adoption and full M4/M7 acceptance
remain pending; production pins and quarantines are unchanged.
