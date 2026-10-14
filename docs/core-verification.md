# Initial portable-core verification

Verified October 14, 2026 with Apple Clang 21 and CMake 4.4.4 on Apple Silicon.

- Full standalone CMake Debug build passed with no Qt, Swift or UI frameworks.
- All seven CTest contracts passed: null handles, invalid archive diagnostics,
  early input-size limit, Capsule and Sword geometry/colors/normals, retained
  mesh ownership across another load, and a pure-C header/link consumer.
- A separate full ASan/UBSan build passed the same seven contracts with
  `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`.
- New adapter/test C++ formatting passed the inherited `.clang-format` rules.
- Original geometry implementation and third-party license files were retained.
  Inherited `sprintf` and signed one-bit field warnings remain visible.

This does not establish full C++ line/branch coverage, all-feature geometry parity,
fuzzing coverage, ABI stability, leak-free execution on every platform, or hard
resource limits in geometry algorithms. Linux/macOS hosted CI is configured
separately; its results must be checked before release.

Follow-up: [core testing roadmap](https://github.com/dalbrecht/dust3d-core/issues/1).
