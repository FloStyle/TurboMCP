# Migration provenance and recovery

Date: 2026-10-17. This is repository restructuring, not completion of the plugin release.

## Preserved work

The original local checkout was saved and pushed to the private branch backup/pre-migration-20261017 at eca2771a0084b33126de1efee0575f221b2e3b94. It includes previously uncommitted source, test fixtures, release planning and exported tasks. Ignored caches, binaries, credentials and scratch data remain local and were not pushed.

Before migration, release-repository main 4560ccf6c35fda041a69f2999af6fdfe730e9054 was an ancestor of private main f045c1313f1db04ddcac269d6910be9ad6befb11. Private main was 13 commits ahead, with no deleted files between those tips. The private v1.0.0 tag was also an ancestor. This proves commit retention, not behavioral correctness or recovery of unreachable/deleted remote commits.

The new checkout uses migration/clean-layout and only the private repository as origin. Source/Content/Resources and the descriptor moved under Plugins/TurboMCP. TestProject remains separate. Existing Tools command names were retained to minimize disruption.

## Documents and tasks

The old root handovers/plans/status files, Spec working notes, Release audit/session scripts and external-board exporter are not active documents in this tree. Recover them from the backup commit when historical evidence is needed. docs/architecture.md preserves approved product contracts and supersedes the old board workflow. docs/acceptance.md and Tests/acceptance.json preserve required acceptance cases, not claims that they passed.

All 85 open board tasks from revision 1803 were imported: 65 todo, 20 backlog. Original IDs and substantive descriptions were preserved; per-run prompts and execution sessions were not imported. docs/task-import.json maps the import. Descriptions remain historical evidence requiring reconciliation, and no item was marked done by migration. TODO.md is generated from todo files. The external board is no longer an authority; its data has not been deleted.

Additional migration/5.9 tasks are separate from the 85 imported IDs. No historical architecture text authorizing a previous run overrides the owner's current instructions.

## Privacy and credentials

A targeted screen of 708 pre-backup reachable commits and 2,105 blobs found personal-path and identity metadata, but no matches for the tested GitHub-token/private-key patterns in TurboMCP objects. It is not an exhaustive secret or personal-data audit. Private history stays private; public exports start with clean history.

The embedded token was removed from the Unreal remote URL. Standard Git Credential Manager replaced TurboMCP's custom helper, FloStyle was selected locally, and noninteractive remote reads succeeded for plugin and engine. The exposed token still needs revocation/rotation by its owner. The old release remote's push URL was disabled locally; that is mistake prevention, not credential isolation.

No GitHub repository was deleted, renamed or made public. The original engine and Desktop checkout remain available. The old TurboMCP repository was reported private by GitHub during inspection, despite its intended public-release role.

Backup: [private snapshot](https://example.invalid/repository).

Validation: [migration checks](migration-validation.md).
