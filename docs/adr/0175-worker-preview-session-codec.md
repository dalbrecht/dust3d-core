# ADR 0175: Incremental native-budget session semantics

Status: proposed for reviewed delivery. Tracks #270 and Apple #279/#280.

## Decision and compatibility

Add an explicit D3PC session encoder and incremental typed collector using independently
captured archive count and native indexed/source policies. Keep existing session,
indexed-mesh, indexed/source-pair and source-hint field layouts. Canonical small payload
bytes match the independently maintained legacy encoder. Legacy opcodes/defaults and
all legacy wire/decoded/count/text policy validators remain unchanged; this path does
not globally increase maximumPayloadBytes or activate a production backend.

Split record-only accounting from complete transfer accounting. Observed geometry/source
metadata can now be checked before retaining typed records without inventing diagnostics
or statistics. Complete transfer accounting still requires actual independently encoded
or decoded auxiliary observations. Indexed zero/default remains 256 MiB; explicit positive
addressable policies are preserved. Source zero/default remains 64 MiB, configurable up
to 256 MiB. Index count is limited by native UInt32 correspondence and unique vertices
cannot exceed corners. Counts and text follow native record budgets rather than incidental
3M-vertex/9M-index/1M-identity/8MiB-text/1024-byte identity defaults.

Share vertex/triangle/index, source identity/influence/corner and outcome invariants with
legacy codecs. Extracting source and outcome validation changes no legacy semantic rule
or limit. Source kinds, original nonnegative weights, aliases, UTF8, warning bytes and
triangle/index order are preserved, never normalized. Opaque failure/cache cause bytes
and signed status fields remain independent data. Successful emptiness is an explicit
outcome; a noGeometry failure is not silently converted. Missing statistics stay absent,
while measured zero remains available. Source fallback and observation failures retain
their separate original cause/detail/context/truncation metadata.

Existing warning and identity length fields are UInt32. Native source policy at most
256 MiB already bounds each identity below that field range. Current native compatibility
warnings originate from two fixed literals in DocumentReader.cc, copied by generateMesh
and buildIndexedMesh; both fit the existing warning field. The encoder explicitly rejects
a warning outside that field instead of narrowing it. The new profile imposes no incidental
4096-byte warning default. This is current-native compatibility, not a claim that arbitrary
future multi-gigabyte strings can be encoded without a format change.

Native node/part counts come from distinct definitions in the raw model XML span inside
the captured archive. Ds3FileReader::loadItem copies that span without decompression;
snapshot_xml.cc builds one map entry per definition; generateMesh copies map sizes.
Therefore each membership count cannot exceed the independently captured archive byte
count. Apply that derived bound before retaining geometry. It does not bound generated
vertex/index counts, which remain governed by the independent indexed budget.

## Streaming and ownership

The encoder validates the complete owned value and native accounting before its first
sink callback. It then emits fixed prefixes, warning/text spans, individual float/index/
source records and bounded auxiliary sections through one chunk writer. It borrows stable
input for the call and retains no duplicate whole wire body. Callback failures keep their
original type/cause. Completion is not actual channel EOF; the host still finalizes I/O.

The collector binds every chunk to frozen correlation and native policy. An explicit
phase machine handles arbitrarily split prefixes, fields, strings and records using a
64-byte fixed record workspace plus the bounded chunk reader. It checks exact outer and
nested lengths, reserved fields, native record charges, UTF8, finite vertices/weights,
indices/triangle area, membership/source correspondence and complete corner/influence
relationships. Nonfinite source weight bits reject before float interpretation. Source
corner ranges check before influences arrive; full kind correspondence checks after
those records exist. Auxiliary sections are independently bounded and decoded through
existing codecs. Final cross-validation checks the complete value and exact charges.

All retained records remain private. Arrays are not reserved from declared counts;
actual validated record bytes grow owned values incrementally. Strings grow only from
actual bytes after prefix/remainder checks. First mutation failure seals the owner and
survives later append/EOF/take attempts. An incomplete ownership query does not mutate
collection. Only complete semantics plus terminal framing and actual EOF permit one
nothrow move of the complete value. No pointer into the collector escapes, no partial
publication or parent native fallback occurs, and no native generation runs here.

Logical storage includes private typed records/text, bounded auxiliary buffers, fixed
prefix/string work and chunk workspace. Encoder preflight/final cross-validation may
hold additional small independently bounded auxiliary encodings. Capacity, roots,
allocator bookkeeping, caller input/output copies, RSS and CPU remain separate. This
profile is semantic/ownership validation, not peer authentication, current-source/queue
admission, OS supervision or a high-resource containment guarantee.

## Verification

The test began red on the missing API. A later protocol-only regression demonstrated
that impossible node membership metadata was accepted; the archive-derived bound now
rejects it before publication. Its failing log is retained in the shared build cache.
Canonical byte equality with the legacy encoder covers sourced/ordinary/failure/empty
outcomes, original source fallback, binary/contextual diagnostics and optional actual
statistics. One-byte input, whole-span input and chunk sizes 1/4/64/65536 exercise split
and combined fields. Multiple identities and triangles, absent hints, opaque hint kinds,
original weights, 1500-byte identity text and 5000-byte warning text round trip. Scalar
accounting alone covers native ranges above incidental worker defaults; tiny actual
values also pass under the largest addressable explicit indexed policy.

Small protocol cases reject unsupported/reserved layouts, inconsistent lengths, invalid
indices/vertices/weights/identity text, unclaimed text, out-of-range corners, oversized
auxiliary sections, arithmetic overflow and incomplete semantic/framing state. No result
escapes before EOF or more than once. Typed semantic and fake sink causes survive.
These tests never parse a synthetic archive or call the native generator, and introduce
no crash/fault/allocation-failure/signal/broken-pipe/cancellation/large-resource probes.

Fresh complete build and 221/221 ordinary CTest contracts pass (56.15 seconds). Actual
SwiftPM runtime build passes (1.77 seconds, with the existing native-build-system
deprecation warning); the complete C++ codec test also passes against that actual static
product. Both clang-format 18 and the newer local formatter, strict package formatting,
quarantine guard and fifteen unchanged source/build/test hashes pass.

Fresh scoped ASan/UBSan passes for the new codec and existing ordinary pure record/source
contracts. Only protocol code is linked: no native engine library, generation or OS fault
path is present. Leak detection is disabled and other existing protocol code remains
uninstrumented. New codec: 480/488 lines (98.36%), 317/323 regions (98.14%), 37/37 functions,
237/260 branch outcomes (91.15%). Record ledger: 82/83 lines, 67/68 regions, 8/8 functions,
51/56 branches. Shared source validation: 32/32 lines, 36/36 regions, 6/6 functions and
30/30 branches. Shared outcome validation: 14/15 lines, 20/21 regions, 3/3 functions,
16/18 branches. Combined scoped figures: 608/618 lines, 440/448 regions, 54/54 functions,
334/364 branches. Residual paths include warning-field overflow requiring a prohibited
large string probe, defensive phase/workspace/final consistency checks, valueless-variant
state and 32-bit/address-space alternatives. No artificial fault branch is added for
coverage, and these are not whole-core or large-runtime acceptance figures.

## Remaining delivery gates

Next connect actual native session generation/copy to the full native budgets without
legacy consumer narrowing, then add real output channels, immutable C/Swift owners and
authenticated peer/current-source/queue admission. Ordinary small licensed end-to-end
comparison remains necessary. Apple adoption is a separate reviewed dependency/backend
decision. Production pin 48b0ce2d, active routes/caps and core/Apple quarantines remain
unchanged. #270, Apple #279/#280 and full M4–M8 parity remain open. Completed clean
exact-head automated review and all hosted CI checks gate this PR's merge.
