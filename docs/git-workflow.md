# Git workflow

[Contributor workflow](contributing.md) · [External agents](external-agent-workflow.md) · [Release manager](release-manager.md)

| Ref | Purpose | Update rule |
| --- | --- | --- |
| main | Development integration | Integrate reviewed changes and rerun affected checks. |
| stable | Latest released source | The release owner advances it to the exact accepted source commit as part of each approved release. It starts with the first approved release. |
| vX.Y.Z | Immutable stable release tag | The release owner creates the tag at the accepted source commit after reviewing its exact source and artifact identity. Keep each published tag fixed. |
| vX.Y.Z-dev.YYYYMMDD | Optional immutable development prerelease tag | The owner can authorize a source-only prerelease at the exact reviewed public commit after the source and hosting gates. |
| codex/ue59-preparation | Unreal 5.9 compatibility candidate | Use a separately approved assignment, worktree and actual 5.9 engine checkout. Review 5.9 support independently from the baseline. |

Branches organize work. Anyone who can read a public repository can read its published branch history. Keep credentials, private evidence, proprietary projects and recovery material outside Git. Review the complete retained history and every ref intended for publication.

## Development and compatibility

Unreal 5.9 implementation is deferred until the repository is public. The reserved candidate branch is dormant; its existence does not dispatch work or establish compatibility.

The integrator dispatches exact file scope and an approved full base SHA. Create or provide one clean isolated worktree at that SHA. A new compatibility worktree can use the following command when the branch and destination are unused.

~~~powershell
git worktree add -b codex/ue59-preparation '<isolated-worktree-path>' '<approved-full-base-sha>'
~~~

Use the already designated worktree if the branch exists. Claim each reviewed task through the [contributor CLI](external-agent-workflow.md#claim-a-reviewed-assignment). Keep Unreal in a separate checkout, configure the actual engine paths for the assignment, and run builds or editor suites through the supervisor. The candidate does not change the supported engine baseline by itself. Review and integrate any change to the pin explicitly. The stable branch advances only through release acceptance.

## Integration host and release

Before cutover, integrate in private main. After the [approved source cutover](public-preparation.md), use FloStyle/TurboMCP as the sole writable development repository and keep the original host private and read-only.

For a stable release, the release owner confirms that the target commit and plugin tree match the accepted evidence, checks the engine and package hashes, and verifies licensing and privacy. Follow the [release manager](release-manager.md) and record separate release approval.

At release time, check that stable and the version tag resolve to the accepted source commit. Protect main and stable against ordinary direct pushes and force pushes. Restrict release tag creation to the release role and keep published tags immutable. Verify which controls the actual hosting plan enforces. Run pull request checks without secrets, and keep untrusted changes away from privileged engine runners.
