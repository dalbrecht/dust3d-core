# 0003 — Positive triangle area at exported precision

Status: Accepted.

## Context

Core #14 found 14 top-opening and 10 bottom-opening zero-area triangles in the
64-face midpoint Cloth fixtures. Most have tiny nonzero area in the inherited
double-precision mesh, then collapse during C ABI float conversion. Filtering
only double-exact degeneracy would miss them; changing the shared double engine
would mix its precision policy with the public float boundary.

## Decision

Stage all three exported vertices after the existing index, finite-position,
finite-normal and conversion-range validation. Calculate the triangle cross
product using double intermediates applied to those float positions. Omit only
exactly zero-area exported triangles. No epsilon or minimum area is introduced,
so small representable triangles remain supported. Keep nonfinite validation
before this cleanup to preserve genuine failures rather than hiding bad data.

If no triangles survive a nonempty generated mesh, return GENERATION_FAILED with
a precision-specific cause. Keep successful empty engine output as NO_GEOMETRY.
Preserve source graph counts, warning text, ownership and serialization semantics.
The policy stays in the sole portable native adapter in dust3d-core; Apple needs
only a reviewed exact dependency pin.

## Verification and limits

Both the opening-area regression and entirely-collapsed export regression failed
before implementation. Real C ABI tests check positive area, bounded finite data,
unit normals, opposing shared-edge winding, maximum two incident triangles per
edge, two connected body/garment components and one degree-two boundary loop for
the configured Cloth hem. Straight-profile export also requires positive area.
A radius-.001 model at origin remains representable; the same model centered at
(1000000,1000000,1000000) fails export and preserves its precise cause.

These are concrete configured-surface contracts, not general manifold guarantees,
mesh repair or agreed Qt differential parity. Internal double geometry, imported
models, interacting wrap combinations and broader corpus acceptance remain
separate work. The ABI layout and status numbering are unchanged.
