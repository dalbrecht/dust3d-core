# ADR 0111: Bounded UV worker requests

Status: prepared development stage for #164; no helper dispatch or production adoption.

## Decision

Carry opaque archive bytes and resolved atlas options together in an explicit
big-endian v1 payload. A 32-byte prefix contains version/prefix lengths (u16),
zero flags (u32), texture size (u32), image count (u32), native retained-metadata
budget (u64), and archive byte count (u64). It is followed by 48-byte image
records and then the exact nonempty archive. Each image record contains a
one-byte identity length, 36/38 original UUID bytes and zero padding through byte
39, then width and height (u32 each). No struct dumps, pixel data, native sizes,
or document parsing are involved.

Zero texture size and metadata budget retain native defaults (4096 and 64 MiB).
Explicit texture size is at most 8192; explicit metadata budget is at most
256 MiB. Image dimensions are 1..8192; at most 10000 images are supported.
Images require nonzero engine UUIDs. Preserve original braces/case in returned
values, but reject duplicates by the parsed 16-byte identity, including aliases.
This differs from the reply's optional image association, where empty is valid.

Validate policies, declared sizes/counts, exact spans, all image descriptors and
logical output charge before creating retained archive/image arrays. Validation
uses bounded identity-set workspace; this is not allocation-free. Logical quotas
charge archive bytes, image objects and identity text, excluding root objects,
capacity, validation workspace and RSS. The native metadata budget is a separate
option, not the request quota. None is an OS memory-containment guarantee.

Both encoders and decoders reject unsupported layouts and invalid state with
content-independent typed errors. Input storage must remain stable for the call.
Returned storage independently owns its bytes. No additional dependencies.

## Validation and remaining work

Missing-header red test preceded implementation. Independent prefix/record fields,
owned byte round-trip, exact and under quotas, malformed spans/version/flags/
padding/identities/dimensions, duplicate aliases and nondefault option boundaries
pass as small value-only cases. Fresh scoped ASan/UBSan passes. Measured codec
coverage: 162/162 lines, 129/129 regions, 108/108 branches. Fresh source-audited
CMake build and 116/116 ordinary CTest contracts pass (21.12 seconds). SwiftPM
unchanged-product compatibility build passes incrementally (0.09 seconds); it
does not compile this CMake-only worker module. Changed C++ formatting and
whitespace checks pass. Inherited build warnings are not claimed resolved.

Same-owner native geometry/UV production, opcode/helper dispatch, parent reply
admission, ordinary positive process acceptance, containment and Apple adoption
remain required. No native malformed-input probes, fault/crash/signal/resource
exhaustion tests, protected app control, production pin or quarantine changes.
