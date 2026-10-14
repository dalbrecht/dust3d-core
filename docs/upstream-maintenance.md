# Maintaining the upstream relationship

This public repository is a maintained downstream of
[huxingyi/dust3d](https://github.com/huxingyi/dust3d), with its full Git ancestry.
It is a GitHub history-preserving duplicate, not a second network fork. The
existing [Apple repository](https://github.com/dalbrecht/dust3d-swiftui) remains
a GitHub fork and can host focused upstream contribution branches.

Initial upstream revision: `3fbcaa7a3e8792d1c2ca31ca2ade442dcd21b222`.
`upstream-sync` is untouched upstream history. `master` contains maintained core
additions. Original source paths, copyright, MIT license and third-party license
files are retained. The Qt tree is reference-only and never compiled by CMake.

After cloning:

```bash
git remote add upstream https://github.com/huxingyi/dust3d.git
git fetch upstream master
git switch upstream-sync
git merge --ff-only upstream/master
git push origin upstream-sync
git switch master
git pull --ff-only origin master
git switch -c codex/sync-upstream-<short-revision>
git merge upstream-sync
```

Open a PR, resolve conflicts, and run the full core contract/sanitizer/compatibility
suite before merging. Review serialization, geometry, dependency and license
changes explicitly. Do not force-push master or automatically reset it to upstream.
No automatic sync, scheduled task or messages to upstream maintainers are enabled.

Keep generic engine fixes separate from adapter features so they can be offered
upstream as small PRs when authorized. New project additions remain MIT; bundled
dependencies keep their own existing license terms and notices.

The Apple viewer initially still compiles its in-tree core, allowing its PR to
land independently. Switch to pinned core consumption in a dedicated follow-up
PR after document/API contract gates pass. This setup is not an ABI-stable binary
release. Avoid maintaining two independently modified adapters during migration.
