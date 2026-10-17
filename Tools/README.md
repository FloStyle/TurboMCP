# Development commands

Run commands from the intended checkout. Configure the pinned engine through
workspace.local.json. Start with the [execution and recovery guide](../docs/audit/infrastructure-review.md).

| Command | Purpose |
| --- | --- |
| `python Tools/next-task.py --claim <owner>` | Atomically claim the next ready task from a clean worktree |
| `python Tools/execute-tests.py --profile fast --execute` | Structure and English encoding checks |
| `python Tools/execute-tests.py --profile offline --execute` | Complete editor-free regression tier |
| `python Tools/execute-tests.py --profile build --execute` | Owned incremental build and artifact verification |
| `python Tools/execute-tests.py --profile engine --suites <name> --execute` | Build and run explicit catalogued engine suites |
| `python Tools/execute-tests.py --profile fixture --suites <name> --execute` | Regenerate the acceptance fixture and retain first/second trials |
| `python Tools/recover-tests.py <run-id> --execute` | Recover one interrupted run after ownership checks |
| `python Tools/doctor.py` | Verify engine revision and project discovery |
| `python Tools/update-todo.py` | Regenerate the task index |
| `python Tools/export-public.py --preview --output .local/public-preview` | Prepare a complete source export for review |

Omit `--execute` to inspect a test request. Set `--max-seconds` for an explicit
total deadline. Logs, snapshots and feedback remain under ignored .local paths.
PowerShell build/editor/fixture helpers require the supervisor's live context;
they are not independent agent entry points.

No automatic publication or paid agent launch is performed. GitHub CI remains
inactive; its template is Tools/ci/validate.yml. Local validation is not remote
branch protection. See [release management](../docs/release-manager.md).

Task review integrity is checked by `python Tools/check-task-review.py` as part of repository validation. It checks complete ID coverage, source references, follow-up owners and catalogued test commands. It does not decide whether a defect is fixed or replace semantic review.
