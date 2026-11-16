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

## Plan 1.0.0

Use the **1.0.0** milestone and the **1.0.0 - planned stable release** draft in [Releases](https://github.com/FloStyle/TurboMCP/releases). The draft is a work plan, not a release announcement or promise of support. Do not create `v1.0.0` or `stable` until acceptance passes.

See the [1.0.0 checklist](release-1.0.0.md) for open issues, exact-target tests and package requirements. Stable release approval remains manual.

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
