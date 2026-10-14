# Contract fixtures

These MIT project fixtures come from the standalone viewer at commit
`4f88ebf8`. `Capsule.ds3` was produced with the repository's independent Python
archive writer; `Sword.ds3` was generated from
`tools/agent_modeling/examples/sword.json`.

Expected source semantics: Capsule has 3 nodes and 1 part; Sword has 18 nodes and
5 parts. Tests check finite geometry, unit normals, retained colors and ownership;
they deliberately do not assert incidental vertex ordering or exact triangulation.

The archive files contain byte offsets and a CRLF header. Do not text-normalize
them. Future fixtures must include source/license, expected behavior, tolerance and
upstream revision metadata. Keep minimized crash reproducers as regressions.
