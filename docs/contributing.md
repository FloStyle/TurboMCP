# Contributor workflow

[Documentation](README.md) · [Contributing](../CONTRIBUTING.md)

## Find the authoritative document

| Concern | Source |
| --- | --- |
| Agent workflow | [AGENTS.md](../AGENTS.md) |
| Behavioral requirements | [Architecture](architecture.md) |
| Engine baseline | [engine.lock.json](../engine.lock.json) |
| Execution order | [Roadmap](../ROADMAP.md) |
| Task inventory | [TODO.md](../TODO.md), generated from `todo/` |
| Release acceptance | [Tests/acceptance.json](../Tests/acceptance.json) |
| Release readiness | [release/status.json](../release/status.json) |

Use English for comments, documentation, tasks and commits. `Tools/check-language.py` checks candidate text, filenames and encoding. See the [language policy](language.md) for binary, history and semantic review limits.

## Prepare a task

Imported findings retain their original IDs and start as `needs_reconciliation`. Check each finding against current source and tests before treating its diagnosis as correct.

Use [the task template](../todo/TEMPLATE.txt) to record the problem, scope, dependencies, acceptance criteria and verification. A ready task must include:

- Current source references and the relevant engine APIs.
- A reproducible input or a concrete investigation question.
- Expected success, failure and refusal behavior.
- Ownership of affected files, subsystems and fixtures.
- Exact verification commands and an observable check for each acceptance criterion.

Resolve conflicts with the architecture and record the decision. Link duplicate findings to one primary task. Preserve withdrawn findings with an explanation. Include all facts needed to execute the task; it must not depend on unavailable private notes.

Only set `Readiness: ready` after this review. The integration owner approves new repair tasks and changes to execution order.

## Work independently

Give each implementation agent one task and one worktree:

```powershell
git worktree add -b task/<id>-<slug> ../worktrees/<id> main
```

Do not switch another agent's branch or edit its checkout. Queue overlapping file or fixture changes through the integration owner. Binary fixture conflicts require manual review.

Engine builds and editor tests need a shared schedule. Do not share an editor, port or build output between workers. Existing per-checkout leases do not provide a cross-worktree mutex. One integration owner schedules access to the engine and test editor.

## Verify and integrate

Commit coherent changes and include the task ID in commit messages. Before integration, fetch private main, rebase the task branch or merge main, resolve conflicts, and rerun affected checks. Never weaken a failing check to obtain a pass.

Record commands, exit codes, logs, source commit, artifact hash and engine identity as applicable. Identify tests that could not run. Static checks do not replace runtime acceptance.

Update the selected task and regenerate TODO.md. An integrator reviews the combined changes before they enter private main, through a reviewed private PR or an explicitly authorized local merge. Use ordinary pushes; never force-push main.

Public publication is a separate, approved export with fresh history. Follow the [release guide](release.md).
