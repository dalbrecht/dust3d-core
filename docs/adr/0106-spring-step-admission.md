# ADR 0106: Reject unstable reference spring steps before simulation

Status: Local mitigation prerequisite; core #95 remains open.

Static inspection of SpiderWalk and SpiderRun found a semi-implicit Euler spring
update: v'=(1-c*h)*v-k*h*x and x'=x+h*v'. For positive finite unit-mass
coefficients k,c and interval h, the homogeneous transition matrix has determinant
1-c*h and trace 2-c*h-k*h*h. Its strict Jury conditions reduce to
k*h*h+2*c*h < 4. The positive boundary is h < 4/(sqrt(c*c+4*k)+c).
The helper uses hypot and a scaled denominator to avoid squaring large inputs,
cancellation and denominator overflow. It neither advances state nor allocates.

The reference defaults (walk k=120,c=12; run k=150,c=14) admit h=1/12 but reject
h=1/6 algebraically. This is a plausible contributor, not proof that either was
the unidentified failing clip in #95. Linear homogeneous stability does not prove
bounded forced motion, rig transformations or final output coordinates. Existing
output checks and quarantine remain necessary; no animation sampler was run.

Reference callers reject invalid raw timing before integer conversion, compute
the same float-duration interval as before, then reject invalid/nonpositive spring
coefficients or unstable steps before pose-state allocation. Accepted paths retain
the original integration expressions, defaults and timing; no substeps, rounding,
clamping or stored-document rewrite are introduced. Fractional frame counts and
zero/negative coefficients are now rejected explicitly rather than truncated or
simulated. The reference bool API retains its failure contract. Future native
sampling must surface the typed admission reason and requested clip context.

The missing helper header caused the initial red compile. Pure tests cover both
reference coefficient pairs, strict boundary/interior/exterior admission and
invalid scalar inputs. Both reference translation units pass syntax-only checks.
Fresh Debug build and 110 ordinary CTest contracts pass in 20.40 seconds; all 109
prior commands are unchanged plus the pure admission contract, and the existing
reference manifest check includes the helper source. Scoped helper ASan/UBSan
passes. Measured helper coverage is 9/9 lines, 17/17 regions and 14/14 branches.
This is not coverage or runtime verification of the reference generators.
SwiftPM builds the portable helper without adding sampler targets. Full Qt/runtime
acceptance and new guarded native sampling remain pending.

No crash, allocation-failure, coarse sampling or resource-exhaustion probe ran.
Production pin and all core/Apple quarantines remain unchanged. This stage must
follow reviewed metadata prerequisites with completed clean exact-head automated
review and all CI green before merge. It does not close #95 or full M6 acceptance.


## Pure timing-plan boundary

The reference adapters now delegate raw timing admission and numeric conversion
to planDampedSpringFrames. The pure planner validates finite duration in the legacy
[0.001,600] interval and integral frame count in [2,4999] before either numeric
cast. It retains the reference float duration when computing h, then calls the
spring stability predicate. Rejected plans expose a typed reason and zero values;
accepted plans retain the original count/duration/interval. No simulation runs.

An initial missing-planner compile failed before implementation. Expanded tests
cover invalid raw timing, fractional counts, legacy endpoints, float-duration
compatibility and cleared rejection values. Both reference adapters again pass
syntax-only checks. Fresh Debug build and the unchanged 110-command CTest suite
pass in 20.08 seconds. Scoped pure planner ASan/UBSan and SwiftPM pass; measured
planner coverage is 22/22 lines, 36/36 regions and 30/30 branches. Isolated symbols
contain no sampler, rig, mesh or synthesizer implementation. Reference execution
and full #95 acceptance remain unverified; all quarantines and production pin stay
unchanged. Earlier 9-line coverage applies only to the initial scalar predicate.
