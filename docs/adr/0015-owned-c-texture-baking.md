# ADR 0015: Owned C texture baking

Status: implementation in progress; core issue #39. This document does not
establish a released bake ABI or Apple renderer adoption.

## Context

The portable CPU baker landed in PR #38. Swift cannot consume its STL containers
or exception types. Generated Object and Snapshot values currently expire inside
native generation; retaining them in a second app-side adapter would duplicate
ownership and bypass output accounting.

## Direction

Add an opt-in opaque baked owner with borrowed geometry/UV metadata, RGBA8 map
views and float-retained-triangle tangent records. Resolve document materials and
procedural settings while the generated snapshot exists; bake while the generated
Object exists. Destroy those temporary generation values before returning.
The original preview and UV-only entry points retain their behavior.

Use sized options, bounded borrowed decoded image descriptors and explicit output
quotas. C++ exceptions terminate at the C boundary, preserving typed status/cause.
No partial owner escapes. Pixel encoding, alpha policy, map absence, independent
owner lifetime and precise logical quota inclusions must be documented and tested
before publishing the ABI. Exact accounting for temporary C++ tangent storage and
retained float-aligned tangent copies is an implementation gate, not a process
memory guarantee.

Cancellation uses an opaque one-shot atomic token. Cancel/query operations do not
acquire the engine mutex; destruction requires the caller to finish the load and
all concurrent token operations. Append CANCELLED without renumbering existing
statuses. Check phase boundaries around inherited generation, and pass the token
to the baker's cooperative checkpoints. Inherited generation remains synchronous
and does not promise a bounded cancellation latency.

## Verification gates

Failing-first pure C and C++ contracts cover options, image dimensions/strides/spans,
null accessors, exact/insufficient quotas, original identity ambiguity, cancellation,
no partial ownership, lifetime across later loads and float triangle filtering.
A strict Swift 6 consumer must copy borrowed outputs before destruction. Run full
Debug and sanitizer suites, formatting, Qt boundary checks and automated review.
Apple exact-pin adoption and PBR rendering follow in separately reviewed stages.

Current local evidence includes actual Capsule owner lifetime/exact-quota tests,
malformed option/image span tests, cancelled calls, cancellation independent of
the engine lock, and strict Swift 6 copies of Capsule/Sword buffers. Full Debug
passed 34/34. Image/material corpus and float-filter alignment coverage, measured
adapter coverage and the complete review/release gates remain unfinished.

The C quota counts final pixel bytes plus float-aligned C tangent records. The
C++ baker temporarily produces original-triangle double tangent records; its
budget is adjusted by the checked difference between temporary and retained
records, with its own 256 MiB ceiling. Mandatory C color/tangent capacity is
checked before baking. Temporary original indices and borrowed input pixel views
are discarded before returning the owner. Container capacity/allocator overhead
and concurrent construction workspace remain outside logical output accounting.
