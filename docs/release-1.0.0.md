# 1.0.0 release plan

Status: planned. No date or stable support claim is promised.

[Milestone](https://github.com/FloStyle/TurboMCP/milestone/1) | [Release drafts](https://github.com/FloStyle/TurboMCP/releases) | [Open issue map](github-issues.md)

## Target

Windows and the exact reviewed stock Unreal 5.8.4 source build. Check [engine.lock.json](../engine.lock.json) when selecting the final candidate. Installed distributions need their own package evidence. Unreal 5.9 stays on its separate candidate branch.

## Required work

- [ ] [#14 Review open findings and agree the 1.0.0 release scope](https://github.com/FloStyle/TurboMCP/issues/14)
- [ ] [#13 Resolve unloaded exact object paths in multi-asset packages](https://github.com/FloStyle/TurboMCP/issues/13)
- [ ] [#18 Make fresh-checkout task claim setup and output reliable](https://github.com/FloStyle/TurboMCP/issues/18)
- [ ] [#15 Complete exact-candidate 1.0.0 acceptance](https://github.com/FloStyle/TurboMCP/issues/15)
- [ ] [#16 Verify clean installation and versioned release packages](https://github.com/FloStyle/TurboMCP/issues/16)
- [ ] [#17 Finish repository protections and release publication controls](https://github.com/FloStyle/TurboMCP/issues/17)

## Acceptance

- Review each open finding and record whether it blocks 1.0.0. A new feature or an unknown cause must not silently become a completed fix.
- Map all 97 required acceptance cases to exact tests and evidence. Retain failed, blocked and missing results.
- Verify tool behavior, installed package identity and clean-consumer checks for each supported distribution.
- Review source, documentation, licenses and repository protections at the final commit.
- Obtain the owner's stable-release approval. Then create `stable` and `v1.0.0` at the accepted commit and publish the tested artifacts with hashes.

The draft in Releases is planning material. It must remain a draft until these gates pass. Daily source snapshots are a separate prerelease channel and cannot satisfy stable acceptance by themselves.

## Current evidence

The cleaned source preview at `c89b95a` preserved 3,606 commits. Its 27 offline checks and hosted validation passed; all 1,040 source files matched the export. Native fixture coverage was 13/15, with unresolved exact-object resolution in multi-asset packages. These are bounded preview results, not 1.0.0 acceptance.

See the [release manager](release-manager.md) for the exact evidence format and final release gate.
