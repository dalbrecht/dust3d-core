# Changelog

## Unreleased

- Omit triangles that collapse at exported float precision, preserve configured Cloth opening topology, and report a specific generation failure when an entire mesh becomes unrepresentable.

- Keep parallel/near-parallel vector angles finite at floating-point dot-product boundaries, repairing Cloth opening and straight-profile normal generation without weakening native finite-output validation.

- Reject malformed and nonfinite values for every wrapping scalar at the native archive boundary, preserving finite legacy values and inherited generator clamping.

- Extend portable C ABI contracts for configured stitching closures, segment counts, loop caps and isolated/connected fill behavior across the existing Linux/macOS sanitizer matrix.

- Distinguish successful generation with no geometry from genuine generation failures through an explicit C API status and portable ownership/error regression tests.

- Accept legacy comma-decimal numeric attributes at the C boundary while retaining strict input and geometry validation.

- Fix procedural boolean generation where intersection loops share a vertex, including rounded crossing parts.
- Weld original and cut vertices consistently around position-grid boundaries, preventing collapsed sliver triangles.
- Add closed-edge and connected-surface regression checks for union, difference, translation, hard joins and nearby separated parts.
