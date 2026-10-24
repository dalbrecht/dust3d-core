# Project C++ standards

The source of truth for formatting is the existing root `.clang-format`. These rules supplement the pinned coding-standards repository, which does not currently include C++ guidance.

- Use C++17, `.cc`/`.h`, four-space indentation, WebKit braces, left-aligned pointers, and existing `m_` member naming. Preserve inherited identifiers and APIs. Format new adapter files; avoid unrelated changes in inherited or vendored code.
- Keep the modeling core independent of SwiftUI, AppKit, Metal, Qt, and application controllers. Native build inputs are base, mesh, rig, UV, AutoRemesher, and triangle-intersection sources. Generator-free animation catalog/declaration and audio surface-name metadata is also compiled explicitly; generator and sound implementation sources remain uncompiled by native products.
- Own heap objects with RAII (`std::unique_ptr`, containers, lock guards). Raw pointers are borrowed views or compatibility interfaces with explicit ownership transfer. Document MeshGenerator's ownership handoff.
- The C ABI exposes fixed-layout float arrays, sizes, status enums, and opaque handles. No STL classes, C++ references, exceptions, or allocator ownership cross the ABI. Destroy handles with their matching API; borrowed buffers expire with the owner. Swift copies buffers while the owner exists.
- Catch errors at the C boundary, initialize status/diagnostics, preserve the cause, and bound diagnostic copies with `snprintf`. Accessors accept null handles. No unchecked pointer arithmetic, unbounded string formatting, or signed overflow at the native document boundary.
- Validate file/header/payload limits before allocation or arithmetic. Require finite geometry, compatible graph references, unique component identifiers, and bounded XML nesting. Check mesh indices and output float range before rendering.
- Serialize engine calls. Existing UUID generation and globals do not have a documented concurrent-use guarantee. Never share mutable cache contexts across calls or assume a Swift actor protects other C callers.
- Preserve inherited numerical behavior and dependency include order. The TBB shim precedes system TBB. Do not enable fast-math or silence all warnings; record inherited warnings separately.
- Test actual `.ds3` compatibility with independently generated fixtures and repository examples. Cover malformed offsets, missing attributes, XML/graph errors, unsupported imported assets, finite normals/colors, and lifetime across subsequent loads. Sanitizers/fuzzing and a larger wrap/boolean regression corpus remain production-hardening work.
- Preserve original and vendored licenses. The static SwiftPM product is `Dust3DEngine`. New geometry dependencies require a rationale and review.
