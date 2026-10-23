# ADR 0072: Pure worker resource-profile planning

Status: local development policy, not enforced process containment.
Refs #123; implements the pure-policy portion of ADR 0071.

## Decision

Add WorkerResources to the portable development worker protocol library. A
ResourceProfile explicitly requests finite soft/hard CPU-second, regular-file-byte
and descriptor-number caps and optionally a virtual-address byte cap. Core dumps
always receive a zero-byte plan. No production budget defaults are selected.

A trusted future adapter supplies a ResourceEnvironment: per-resource support,
maximum convertible finite value, inherited optional soft/hard limits and an
explicit ordinary or address-sanitized runtime. nullopt denotes infinity; no
numeric budget is reinterpreted as an infinity sentinel. Runtime defaults to
unknown and is rejected, as are invalid runtime enum values and requested virtual
caps for ASan. The ASan-compatible profile without that cap proves no memory
containment. No resident-memory cap is exposed.

planWorkerResources returns CPU/file/core/descriptor entries in stable setup
order, followed by the optional virtual cap. It rejects nonpositive requested
budgets (except core zero), reversed budgets, unsupported requested capabilities,
unrepresentable finite values and malformed inherited observations. Each finite
effective soft/hard value is independently the stricter of requested/inherited.
An inherited zero is retained; helper startup viability must be checked separately.
A descriptor-number ceiling is not a count of existing open files and does not
close inherited descriptors. Planning never mutates either input or any OS state,
including when validation of a later entry fails.

ResourceError preserves a typed cause and resource. The function has no runtime
profile or OS autodetection: honest capability and runtime observations remain
an adapter precondition. It does not read untrusted archive data, invoke an engine,
select a production profile, apply limits or change the current no-argument helper
launch contract. Invalid inherited observations cannot be silently clamped away.

## Verification and remaining gates

Test-first compilation fails on the missing API. Ordinary requested/inherited
profiles verify the actual planned values/order and unchanged inputs. Contracts
exercise all five resources' support, finite bounds and malformed inherited
states, both infinity positions, stricter zero inheritance, maximum uint64 finite
values, no implicit memory cap, incompatible instrumented profiles and late
validation failure followed by a successful independent plan.

A separate regression showed the initial ordinary runtime default admitted an
unspecified build profile; it now defaults to unknown and rejects it. Focused
strict-warning ASan/UBSan execution passes. LLVM instrumentation reports all
36/36 lines, 53/53 regions, 5/5 functions and 38/38 branch outcomes in
WorkerResources.cc plus its error accessors. This is scoped policy coverage, not
whole-core coverage or proof of any resource limit. Final fresh ordinary CMake/
CTest passes 63/63 in 26.25 seconds after the runtime-default correction.

No getrlimit/setrlimit calls, OS mutation, resource exhaustion, allocator failure,
crash, signal-delivery or deliberately broken-pipe probes were used. A thin
helper-only adapter, inherited-descriptor/startup preflight, original OS setup-error
reporting, versioned trusted launch/profile identity and ordinary effective-limit
observations remain required before activation. Platform RSS/descendant/filesystem
sandboxing, parent-death handling, active stop/reap acceptance and signed helper
packaging remain unresolved. Apple adoption and its pinned core revision remain
unchanged, as do all GLB/writer/fault/animation/rig-pose quarantines. #123/M8 and
full native editor parity remain incomplete.
