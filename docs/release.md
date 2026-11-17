# Release process

[1.0.0 plan](release-1.0.0.md) | [Release manager](release-manager.md) | [Git workflow](git-workflow.md)

## Daily source snapshots

The owner authorizes automatic daily source prereleases in FloStyle/TurboMCP.

1. A push to `main` runs **Repository validation**.
2. **Daily source release** runs only after that push succeeds. Failed, cancelled, PR and other-branch runs do not publish.
3. The publisher checks that the validated commit is still `main`. A stale run is skipped.
4. It creates or updates `daily-YYYY-MM-DD` using the Europe/Paris date. An unchanged commit is skipped.
5. The GitHub Release is a prerelease, includes its exact commit and CI link, and offers source downloads. It is never marked as the latest stable release.

Several successful pushes on one day update the same daily tag and page. A day without a validated change creates no release. These tags are mutable; immutable `v*` tags are never updated by this workflow. Native and installation acceptance are not implied. No binary package is produced.

The repository and its published source releases are public. The workflow does not change repository visibility.

## Released: 1.0.0

`v1.0.0` is released at commit `ef1d1e1f`, built and tested against the pinned Unreal Engine 5.8.4 source build (`f45729c0`). The package is `TurboMCP-1.0.0-Win64-UE5.8.4-ef1d1e1fbd15-04d2c06d468a4e249f41cea3b567f5cf.zip`, SHA-256 `59b28032ae6d8f72fc079e601f937efb89ed8e9b5870452f99a42e59db6c02fe`. Both clean consumers passed on that exact archive (run `b8dd28b1`). The [`stable`](https://github.com/FloStyle/TurboMCP/tree/stable) branch starts at that commit.

Launcher builds and Unreal 5.9 are not certified yet. See the [1.0.0 checklist](release-1.0.0.md) for the accepted scope and the deferred work.

## Source and installation packages

The source release contains the complete reviewed tracked tree. A plugin installation package is a separate artifact, tested for an exact engine distribution.

Preview the source export in a new output directory:

```powershell
python Tools/export-public.py --preview --output .local/source-preview
```

The exporter verifies tracked files and refuses unsafe content. Preview does not grant stable release approval. Follow the [release manager](release-manager.md) for accepted evidence, package hashes, complete export and final owner approval.

## GitHub Pages

The **Documentation site** workflow uses the same successful-main validation gate. It uploads only the static `site` content. Set the Pages source to **GitHub Actions** and repository variable `PAGES_ENABLED=true` after Pages is available and configured.

Pages is enabled at https://flostyle.github.io/TurboMCP/. Deployment follows successful main validation. The workflow does not change repository visibility.
