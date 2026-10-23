# ADR 0061: Native owned chart-isolated mip levels

Status: accepted in core #107 after completed exact-head clean automated review and all six CI checks passed.

## Decision

Add an opt-in C loader with source-matched outer and nested option sizes. Zero
maximum levels selects 14, one is base-only, and larger values reject. Existing
loaders retain level-zero sampling. All load errors pass through the existing
serialized exception boundary with their original diagnostic.

Retain original procedural atlas layouts after legacy baking, then generate the
chain while decoded source images, samplers and legacy maps are alive. The owner
stores five independent additional-level vectors. Level zero aliases its existing
base map; borrowed views expire together on owner destruction. Missing maps and
invalid map/level indices return empty views. Consumers must copy every needed
buffer before release and honor the reported partial-chain sampling cap.

The aggregate native quota includes existing geometry/catalogs/triangle storage,
base pixels, additional image-record capacities and all retained mip pixels.
Inline vector roots are charged through the owner data root, not twice. Workspace,
allocator overhead and peak RSS remain excluded. Portable chart/work/retained
limits remain in force. This is not a production ABI stability claim.

## Verification

Test-first compilation failed on the absent C API. Licensed ImportedSurface.ds3
and Capsule.ds3 fixtures retain a real 512-to-256 level with mapped geometry,
policies and tangents. Capsule has no normal map; absence remains explicit.
Tests cover base pointer aliasing, dimensions/spans, input and unrelated-owner
lifetimes, exact/minus-one native quotas, bounded work rejection, real-token
pre-cancellation, nested option sizes, excessive levels, base-only compatibility
and null/out-of-range views. A pure-C consumer compiles and invokes the new API.
No GLB writer/catalog reconstruction or failure/crash probe is used.

The final normal gate passed 51/51 in 37.02 seconds. The ASan/UBSan gate passed
51/51 in 119.92 seconds. SwiftPM builds successfully; inherited UUID seed narrowing
warnings remain. Formatting, whitespace and the source quarantine guard pass.
No whole-engine coverage claim is made. All gates reuse one shared build root.

## Remaining work

Swift SDK copying, renderer upload/partial-chain sampling, export behavior and
independent end-to-end material acceptance remain separate. No Apple production
pin changes or UI changes are included. Existing quarantines stay in force.

## Automated Swift consumer follow-up

Normal native macOS CMake builds now compile the real C import consumer under
Swift 6 complete strict concurrency and warnings as errors, then register it with
CTest. Its owned immutable Sendable value contains all available map levels,
dimensions, copied Data and producer cap. Two independent Capsule bakes agree
after both native owners are destroyed and the first copy crosses a detached
task boundary. No opaque handle or borrowed pointer crosses that boundary, and no
unchecked Sendable is used. The licensed fixture produces 80 triangles and a real
512-to-256 level with cap 1.0.

The focused target initially did not exist (test automation red); the standalone
consumer already passed against the fresh #107 SwiftPM product. CMake integration
then caught formatting differences between repositories, corrected with the local
formatter configuration. The final full normal suite passes 52/52 in 30.67s, with
the new Swift test in 0.93s. No C++ implementation changes are included. Sanitizer
and Linux configurations retain the existing 51 C/C++ contracts; swiftc does not
link the C++ UBSan contract in this build. The reviewed #107 sanitizer evidence
above remains historical, not a rerun for this test-only change.

This improves core boundary coverage without changing the Apple production pin or
claiming SDK binding, native app mip adoption or independent export acceptance.
