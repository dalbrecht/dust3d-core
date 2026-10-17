# Changelog

## Unreleased

- Extend portable C ABI contracts for configured stitching closures, segment counts, loop caps and isolated/connected fill behavior across the existing Linux/macOS sanitizer matrix.

- Distinguish successful generation with no geometry from genuine generation failures through an explicit C API status and portable ownership/error regression tests.

- Accept legacy comma-decimal numeric attributes at the C boundary while retaining strict input and geometry validation.

- Fix procedural boolean generation where intersection loops share a vertex, including rounded crossing parts.
- Weld original and cut vertices consistently around position-grid boundaries, preventing collapsed sliver triangles.
- Add closed-edge and connected-surface regression checks for union, difference, translation, hard joins and nearby separated parts.
