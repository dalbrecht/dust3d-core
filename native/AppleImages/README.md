# Apple worker image decoding

Apple-only Swift6 callbacks for the portable worker C ABI. `configuration()` supplies
separate imported PNG/JPEG and procedural PNG providers with straight RGBA8 source
samples. This library does not launch a worker or authenticate/admit its output.
Envelope metadata is a separate Foundation-only Dust3DImageMetadata product so
document/domain consumers never acquire platform/native dependencies. The callback
links system CoreGraphics/ImageIO only; portable C++ products do not depend on it.

Build with `swift build --product Dust3DWorkerAppleImages`. Import the module and
pass a local configuration to `dust3d_worker_serve_with_decoders` synchronously off
UI. Callback input/context are borrowed; any returned C image belongs to its caller
and must be destroyed. No callback context, parent fallback or process policy change.
Quotas exclude platform decode workspace and RSS. Existing 8192-side envelope limits
are retained and remain subject to Apple #280 range acceptance.

`provenance.json` records byte-identical MIT Apple envelope/sample-layout sources
from accepted revision2d24e720. These are deliberately retained here as the worker's
shared implementation candidate; the old pinned Apple app cannot yet import this
new module. A later separately authorized dependency PR must switch Apple consumers
to this module/reexports and remove their duplicate implementations. Until then,
changes must reconcile the provenance manifest and Apple tests explicitly. Do not
silently let two implementations drift. New callbacks use those same sample rules,
including purpose-specific keyed-gray transparency and nearest16-to8 rounding.

CMake builds/runs the licensed positive PNG/JPEG callback contract on macOS. It does
not run malformed-codec, native archive, GLB, fault, cancellation or pressure probes.
No production isolation, material pipeline or distribution acceptance is implied.
