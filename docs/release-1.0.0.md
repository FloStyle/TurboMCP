# 1.0.0 release plan

Status: planned. No date or stable support claim is promised.

[Milestone](https://github.com/FloStyle/TurboMCP/milestone/1) | [Release drafts](https://github.com/FloStyle/TurboMCP/releases) | [Open issue map](github-issues.md)

## Target

Windows and the exact reviewed stock Unreal 5.8.4 source build. Check [engine.lock.json](../engine.lock.json) when selecting the final candidate. Installed distributions need their own package evidence. Unreal 5.9 stays on its separate candidate branch.

## Required work

- [ ] [#14 Review open findings and agree the 1.0.0 release scope](https://github.com/FloStyle/TurboMCP/issues/14)
- [x] [#13 Resolve unloaded exact object paths in multi-asset packages](https://github.com/FloStyle/TurboMCP/issues/13)
- [x] [#18 Make fresh-checkout task claim setup and output reliable](https://github.com/FloStyle/TurboMCP/issues/18)
- [ ] [#15 Complete exact-candidate 1.0.0 acceptance](https://github.com/FloStyle/TurboMCP/issues/15)
- [ ] [#16 Verify clean installation and versioned release packages](https://github.com/FloStyle/TurboMCP/issues/16)
- [x] [#17 Finish repository protections and release publication controls](https://github.com/FloStyle/TurboMCP/issues/17)

## Acceptance

- Review each open finding and record whether it blocks 1.0.0. A new feature or an unknown cause must not silently become a completed fix.
- Map all 97 required acceptance cases to exact tests and evidence. Retain failed, blocked and missing results.
- Verify tool behavior, installed package identity and clean-consumer checks for each supported distribution.
- Review source, documentation, licenses and repository protections at the final commit.
- Obtain the owner's stable-release approval. Then create `stable` and `v1.0.0` at the accepted commit and publish the tested artifacts with hashes.

The draft in Releases is planning material. It must remain a draft until these gates pass. Daily source snapshots are a separate prerelease channel and cannot satisfy stable acceptance by themselves.

## Current evidence

As of 17 November, issues #13 (exact object resolution), #17 (repository controls)
and #18 (fresh-checkout claims) are integrated and closed. Their completion does
not close the remaining scope, acceptance or clean-install gates.

The supervised Win64 BuildPlugin run `802cee64cd5640019a27c45c9b81ad4d`
passed at source `9c97d01af2c094fb1f022fccf903f377a2f78fdc` on the pinned
5.8.4 engine. Its 371-file ZIP and module build ID were verified. Fresh BP-only
and C++ consumer installation, load/discovery/read/edit/refusal checks and
Launcher compatibility remain unverified. The daily prerelease is source only.

The original source-preview fixture collection was 13/15. A later focused native
metadata check verified the exact-object repair; the full original fixture audit
has not been rerun. All 97 required acceptance specifications still need reviewed
mapping and exact-candidate evidence. Known append-suite failures remain open.

A source review confirmed that graph replacement still deletes omitted eligible
nodes, contrary to ARC07's explicit-delete requirement. The approved bounded
repair includes a complete edit export, fresh fingerprint, refusal cases and
caller/help migration. It must be reviewed and tested before release acceptance.


See the [release manager](release-manager.md) for the exact evidence format and final release gate.
