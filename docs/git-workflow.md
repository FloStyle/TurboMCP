# Git workflow

[Contributing](../CONTRIBUTING.md) | [Release process](release.md)

## Three long-lived branches

| Name | Purpose | Update rule |
| --- | --- | --- |
| `main` | Development | Review focused changes and require passing validation. Daily source snapshots come from this branch. |
| `stable` | Accepted stable releases | Create it at the first approved stable release. Advance it only to later accepted version tags. No daily snapshot or unaccepted preview belongs here. |
| `codex/ue59-preparation` | Unreal 5.9 candidate | Keep a separate worktree, engine checkout and evidence. Do not change the supported baseline or claim 5.9 support from this branch alone. |

Short-lived `codex/*` task branches are not extra release channels. Remove them after integration.

## Tags

- `vX.Y.Z`: accepted stable release. Keep the tag fixed.
- `vX.Y.Z-dev.YYYYMMDD`: manually approved development preview. Keep the tag fixed.
- `daily-YYYY-MM-DD`: rolling development snapshot for that Europe/Paris date. Later validated pushes on the same day may move this tag. Always record the exact commit when reporting a daily build.

Daily publication is owner-authorized for this repository. It follows successful `main` push validation and never publishes `stable`, a `v*` tag or installation binaries. See [release process](release.md).

## Work and integration

Use FloStyle/TurboMCP for new cleaned-history work. Never merge or push unsanitized ancestry from the old recovery repository. The owner controls the public visibility change; retire the recovery host after verified cutover.

Each implementation assignment needs an approved base, exact file scope and task claim. Keep engine runs serialized through the existing supervisor. Follow [AGENTS.md](../AGENTS.md) and the [contributor workflow](contributing.md).

Unreal 5.9 implementation remains deferred until public cutover and an approved assignment. The branch may exist before work starts.

## Protection

Require PR validation on `main`. Protect `stable` and immutable version tags before using them. Restrict publication credentials and do not run untrusted PR code on privileged engine runners. The hosting plan may require public visibility before these controls are available; verify the actual settings.
