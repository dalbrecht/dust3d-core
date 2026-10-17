# ADR 0002: Distinguish successful empty generation

Status: Accepted for the source C API prototype.

Append DUST3D_STATUS_NO_GEOMETRY = 5 while preserving every existing enum value
and function signature. The adapter first checks generator success and a valid
output object, then classifies zero triangles separately from generation errors.
Return NULL plus a descriptive status for that result, retaining existing owner
and borrowed-buffer lifetime rules. Callers that understand the result may clear
a preview; callers that do not still receive a diagnostic.

Do not infer emptiness from Apple graph heuristics or catch every failed
generation as empty. Malformed input, generator failures, missing objects and
nonfinite output must retain their current error paths. No inherited geometry
algorithm or synchronization behavior changes.

Portable C ABI tests cover lone filled/unfilled loops, owner lifetime across
empty loads, optional diagnostics, malformed input, degenerate nonfinite geometry
and refilling. Run the contract matrix, sanitizers and SwiftPM source build.
This does not promise a stable binary ABI release or solve every empty boolean
result in the inherited generator.
