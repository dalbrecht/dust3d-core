# ADR 0201: Hollow tube endpoints

Tube generation uses the existing solid path at thickness 0 and 1. Between
these endpoints it creates an inner surface and the end rims. The thickness-one
endpoint avoids a collapsed inner wall and produces the same geometry as zero.

The indexed ownership prerequisite preserves original corner identity through
shell generation and its downstream consumers. Tests cover shell winding and
topology, near-collapse input, mirrored ownership, Boolean operations, and
the exact solid endpoint. The 1.1.6 Qt binary produced solid output for the
synthetic Capsule at all three tested thicknesses. This change implements the
hollow interval and the agreed solid endpoint for the native editor.

Profiles with the origin strictly inside their visibility kernel retain radial
scaling. Other custom profiles use a mitered inset with the same vertex order.
The builder finds a nonintersecting rim at a bounded depth, then applies the
requested fraction of that depth and checks the result. Custom profiles that
need this fallback are limited to 1,024 points to bound pairwise validation.
If validation cannot establish a contained cavity, generation fails for that
part rather than emitting intersecting surfaces. At thickness 1 the solid
path still applies.
