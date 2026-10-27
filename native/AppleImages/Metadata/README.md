# Image container metadata

Foundation-only PNG/JPEG envelope inspection before platform decoding. This target
has no native engine, ImageIO or CoreGraphics dependency. See the parent provenance
manifest for the byte-identical MIT Apple extraction and consolidation plan.

Build with `swift build --product Dust3DImageMetadata`; import this module for
PNGContainerMetadata/JPEGContainerMetadata and their typed errors. Bounds cover
encoded/final RGBA bytes, not codec workspace. These inspectors do not decode samples
or establish complete codec validity. Parent CMake provenance/sample contracts run
without malformed native image probes. No configuration or external dependencies.
