# ADR 0013: Isolated offscreen bake reference

Status: Accepted for implementation; core issue #35.

## Context

Portable atlas and C/Swift metadata stages have analytic/ownership contracts but
no actual reference texture execution. Reading Qt orchestration cannot establish
image rotation, premultiplied rounding, scalar conversion or procedural map
semantics. A full Qt app interaction is unnecessary for these algorithm checks.

## Decision

Add a standalone optional tests/reference CMake project. It links existing native
archives plus developer-installed Qt Core/Gui and compiles preserved application
UV/image/model data sources without editing them. It forces the offscreen platform,
uses public APIs, and emits JSON plus optional explicitly requested diagnostics.
Configuration verifies matching native checkout/sanitizer modes and the reference
build refreshes native engine archives before linking, preventing stale-code
comparisons. The native CMake/SwiftPM dependency graph and standard CI configuration stay
unchanged; native products never depend on the reference target or Qt.

Probe solid/image/seam/material/procedural/alpha cases and compare original-object
portable UVs with Qt on the existing image-free corpus. Validate finite/triangular
comparison data so NaNs cannot masquerade as zero error. Bound file reads before
allocation. Keep original resource ownership and independently scoped image
registration. No private access macros, production switches, geometry replacements
or local snapshot baselines are needed.

## Alternatives and consequences

Analytic-only tests establish a chosen algorithm but cannot confirm Qt semantics.
GUI screenshots add platform/display and interaction dependencies without exposing
numerical UVs or precise pixels. Adding Qt to normal native builds would violate
the product boundary. The optional offscreen harness supplies focused comparison
without doing that. Qt retains its separate license; new harness code remains MIT.
No Qt code or linked reference binary is redistributed with native products.

Probe success means the stated checks passed, not full editor or texture parity.
The corpus shares the validated reader/generator, so it isolates UV orchestration
rather than certifying independent Qt file loading or all generator settings.
Original-object arrays differ in scope from float-filtered exported geometry.
Reference alpha/channel quirks are recorded as observations, not automatically
adopted as desired semantics. Future portable baking needs explicit pixel format,
color-space, coverage, resource and GPU acceptance contracts.
