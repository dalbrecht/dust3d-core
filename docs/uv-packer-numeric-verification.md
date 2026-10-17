# UV packer numeric verification

Core issue #30; ADR 0010. The source-based regression is independent of the
in-progress atlas API and uses existing ChartPacker/MaxRectangles interfaces.

## Failing-first evidence

Atlas extraction under UBSan stopped at max_rectangles.cc occupancy: a
41389592 × 41389592 int product overflowed before its wide accumulator.
The isolated regression also fails without sanitizers: occupancy for two large
rectangles is not the independently known 0.5 value.

Contracts now exercise all five packing heuristics with large integer areas,
bounded positions, maximum-int contact scores/full-bin occupancy, automatic
4096-chart packing, finite/nonfinite/negative/underflowing/unrepresentable scales,
invalid chart widths/heights, invalid rectangle counts/buffers/dimensions,
well-defined empty positive bins and clearing prior layouts after failure.
A further failing-first case reproduces float padding erasing a positive chart
at a large texture scale; the final guard rejects that instead of publishing
zero width. No production fault switches or allocation-failure branches are added
for coverage.

## Final gates

Complete debug build/CTest passes 30/30 (40.22 seconds); complete ASan/UBSan
build/CTest passes 30/30 (113.06 seconds), with halt-on-UB enabled. SwiftPM source
build and both Swift 6 strict-concurrency/warnings-as-errors session/source smoke
consumers pass. New test formatting, changed-range inherited formatting, native
adapter formatting, manifest lint and source Qt-boundary checks pass. Existing
user-cache/deprecated-build-system warnings are not represented as test failures.

## Scope and remaining limits

Changed inherited files retain their copyright/format; formatting checks cover
changed ranges rather than rewriting entire source files. Existing signed one-bit
flag compiler warnings remain inherited. New tests and the full native adapter
format/Qt-free checks pass. The existing Swift consumer script builds the source
product and both strict Swift 6/warnings-as-errors consumers without ABI changes.

Focused LLVM line coverage: ChartPacker 86/91 (94.51%); MaxRectangles 447/517
(86.46%). All five ChartPacker functions and 28/29 rectangle functions execute.
Missed lines include inherited allocation failures, uncommon split/prune paths,
maximum packing attempts and the defensive extreme chart-count guard.
These reports do not establish 100% inherited core coverage or a production
resource bound. Full Qt pixel/atlas differential acceptance remains pending.
