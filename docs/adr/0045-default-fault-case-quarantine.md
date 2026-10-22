# ADR 0045: Quarantine default allocation-failure probes

Status: Mitigation implementation; public review pending. Tracking: #84.
GLB catalog containment remains #69; re-enablement remains blocked in #67.

## Audit correction

The resumed PR #73 validation ran 47 CTest targets before their contents were
audited. Four otherwise ordinary executables contained deliberate allocation
failure injection. Calling that entire run ordinary, or saying no allocation
failure probe ran, was incorrect. The pass count is an observed target result,
not evidence that injection was excluded. No additional historical fault case
is authorized for replay. The guard changes from #73 already reached master
through #98, so the duplicate PR was closed rather than merged over newer work.

## Decision

Remove test-owned allocator overrides, injected-failure functions/calls, and
inline fault cases from imported-asset, imported-surface, source-bake, and
generation-session contracts. Retain their ordinary behavior, validation,
ownership, cache, and lifetime checks. Remove the standalone JSON allocation
probe and its registration from active sources. Exact removed code remains in
Git history at `942e28150f53f09d30ac503ef99015c727ec097b`.

Keep ordinary JSON parse and comparison contracts in a separate executable.
Unevaluated compile-time `noexcept` assertions reject the historical string
comparison signature without creating allocation failure or executing that
historical implementation. Numeric scalar comparisons retain their no-throw
contract. These checks do not prove parser allocation-failure containment.

The source quarantine guard rejects restored standalone probe files or manifest
registrations, and known allocator overrides/fault identifiers in the four
affected default sources. Regression fixtures are inert text read by CMake;
they are never compiled or executed. This is a known-case restoration guard,
not a general C++ parser or sandbox. The retained-payload allocation observer
remains: it counts successful allocations and never deliberately fails them.

No runtime opt-in, allocator switch, production parser change, or inert coverage
branch is added. Restoring fault coverage requires a separately reviewed change
and explicit authorization. The GLB material catalog stays unavailable, the
failing writer fixture in #83 must not be reconstructed or replayed, and Apple
production dependency bytes remain unchanged.

## Verification and limits

Harmless source fixtures first failed because registration and physical probe
restoration were accepted, then passed after the guard changes. The comparison
contract produced six static assertion failures with the historical header and
compiled with the current header using `-fsyntax-only`; neither check ran a
native program. Current ordinary gate results belong in the delivery PR.

Do not launch pre-containment test binaries. Any later ordinary gate must use
a fresh build after auditing source and CTest registration. Allocation-failure
and exception-path coverage is explicitly deferred. Ordinary test passes do not
establish full inherited-core coverage, memory safety, or safe GLB re-enablement.
