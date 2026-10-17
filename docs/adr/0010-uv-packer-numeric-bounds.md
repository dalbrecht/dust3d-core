# ADR 0010: UV packer numeric bounds

Status: Accepted for implementation; core issue #30.

## Context

Portable atlas extraction calls the inherited ChartPacker/MaxRectangles path that
existing native mesh preview contracts did not exercise. UBSan reproduced signed
integer overflow in occupancy at ordinary 4096-scale charts: 41389592 multiplied
by 41389592 as int before accumulation in a wider variable. Best-area and contact
scores also exceed 32-bit ranges. Float-to-int conversion and float padding can
publish invalid or zero-area charts without an explicit failure.

## Decision

Keep public integer coordinate signatures and inherited paths/format/licenses.
Widen internal scores and best-score sentinels to int64_t, promote products before
multiplication, and use wide occupancy arithmetic. Positive rectangle/bin bounds
and non-null buffers at positive count are validated before packing. Empty positive
bins remain supported; optional occupancy remains optional.

ChartPacker checks finite, positive, representable quantized texture/chart sizes
before integer conversion, validates the rectangle-count conversion, rejects
padding that erases a chart, and clears previous results on a failed attempt.
No numeric clamp silently changes an unrepresentable chart into valid output.

## Consequences

This reusable bug fix is separate from the new portable atlas API and can be
reviewed/offered upstream independently. Correct area scoring may change layout
choices that previously depended on signed overflow; preserving undefined
behavior is not a compatibility contract. No C ABI, dependency, Qt target or Apple
pin changes. Full atlas construction, raster baking and UI adoption remain later
reviewed work. This is not a hard working-memory/time guarantee or full inherited
geometry coverage claim.
