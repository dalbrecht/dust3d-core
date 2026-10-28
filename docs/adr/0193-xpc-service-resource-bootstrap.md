# ADR 0193: Require explicit resource preparation before XPC service work

## Decision

Reuse the existing C++17 WorkerResources, WorkerResourceSetup and WorkerUnixResources implementations
through one fixed-layout C ABI. The result owns four effective limits, applied/readback counts and
original status/resource/operation/code/errno plus bounded diagnostic bytes and a truncation flag.
Exceptions never cross C. The recording seam and OS calls remain distinct: public preparation mutates
the **current process**, exclusively in a fresh dedicated helper. Neither importing the module nor
constructing a policy, metadata value or synthetic receipt inspects or changes limits.

Schema 2 own-bundle metadata requires exactly seven canonical positive uint64 decimal strings, naming
CPU seconds, regular-file bytes, descriptor ceilings and an explicit startup floor. The unsigned stager
requires a bounded JSON file, rejects duplicate keys, and copies validated values into Info.plist before
signing. There is no runtime file/environment/argument/document override or permissive schema-1 fallback.
The stager is unsigned tooling, not release trust. The running executable reads its own bundle only.

Inspect and validate every mandatory resource before applying any limit. Preserve stricter inherited
limits, check the effective descriptor floor, always disable core dumps, and require exact readback after
application. The four limits are cumulative process CPU seconds, per-regular-file bytes, core bytes zero,
and maximum descriptor number plus one. No address-space/RSS cap or per-job wall deadline is invented.
The explicit startup floor is the embedding policy's responsibility; it is no promise about every future
service allocation. Native platform representability is checked by the existing planner/Unix adapter.

Only complete success constructs the Swift receipt with no public initializer. The listener
requires that receipt and independently checks its sandbox entitlement. Any preflight/readback failure
is terminal for this fresh service bootstrap: report original typed metadata and exit normally before
listener/native work. Partial application is visible and never rolled back, retried or called success.
The original diagnostic bytes stay with the error; stderr logs status, resource, operation, errno and
counts without diagnostic text. Host syntax validation occurs before resource preparation.

## Verification and limits

TDD established missing value/codec/bridge/mapping/receipt/metadata APIs and the missing resource log
classification before implementation. Pure Swift policy/receipt/bundle tests and recording-only C++
contracts cover invalid configuration, stricter inheritance, partial effects and original readback
causes without modifying test-runner limits. A separately compiled dedicated short-lived helper alone
applies and reads back ordinary finite caps. It performs no document, exhaustion, signal or failure probe.
The canonical healthy XPC fixture also prepares explicit trusted QA limits inside its fresh service;
its private signing sidecar remains separate from the actual executable's sealed metadata path.

The actual bundle-only product is built, staged unsigned and its metadata imported by a separate consumer;
that does not prove actual product launch, signed host integration, distribution trust, notarization,
resource exhaustion behavior, hard memory containment or bounded active termination. Those remain
separate acceptance gates. Thin real OS/Security boundaries cannot be covered by fake unit profiles;
coverage claims must identify the measured files and executable, not inherited engine completeness.
No new dependencies, production pin changes or feature-quarantine changes are made. M4–M8/full parity
remain unproven. All negative tests use scalar values/recording systems, not native document probes.

Scoped coverage from the 21 synthetic Swift tests: ResourcePolicy55/55 lines and25/25 regions;
BundleMetadata55/55 and34/34; FailureMetadata47/47 and28/28. ResourceBootstrap81/84 lines,
35/37 regions and19/20 functions; its three-line actual mutating factory is deliberately absent from
unit execution and is exercised only in the fresh healthy service. The recording C++ executable
measures WorkerResourcesBridge93/94 lines,54/56 regions,9/9 functions and24/28 branches. The defensive
application over-count guard remains unforced. The separate real-OS factory has ordinary helper/service
success evidence, not fake coverage of constructor/platform failures. These are scoped measurements,
not combined profiles or claims about inherited geometry coverage. No unsafe probes were added.
