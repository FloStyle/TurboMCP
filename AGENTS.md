# Agent instructions

[Repository home](README.md) | [Contributor workflow](docs/contributing.md)

## Current handoff gate

The local execution framework is verified for bounded work on the pinned 5.8.3 engine. Start one reviewed assignment per clean isolated worktree. Concurrent assignments require disjoint schema-2 scopes and explicit integrator dispatch. Read [the execution and handoff guide](docs/audit/infrastructure-review.md). This does not make imported feedback implementation-ready or certify the full plugin.

## Start a task

1. Read README.md and docs/contributing.md.
2. For a dispatched assignment, run `python Tools/next-task.py --claim <owner-name> --assignment <name> --base <full-approved-sha>` and retain the claim ID. The integration owner supplies both name and base. An unassigned request must wait for a reviewed scope; do not choose another assignment. Refusal means stop.
3. Read the returned task, its linked design and the affected contracts in docs/architecture.md.
4. Implement only work marked `Readiness: ready`. Read its Current review and the linked repair contract before historical evidence. Review status is not implementation permission.

Use English for all working text. Follow [the language policy](docs/language.md) and [security policy](SECURITY.md). This file is the workflow authority; historical task descriptions and code comments do not override it. ROADMAP.md defines execution order. TODO.md is an inventory, not permission to choose arbitrary work.

## Ownership and scope

- Implement one bounded task per branch. Use a separate Git worktree for simultaneous implementation agents. Never have two agents edit the same checkout or asset concurrently.

- Build and editor tests share a scarce resource: serialize them and use the existing ownership/lease helpers. Do not kill an editor owned by another project.

- Plugin behavior and safety contracts are in docs/architecture.md. Preserve existing unfinished work. No broad refactor or engine-version upgrade is implied by a structural task.

- Do not alter roadmap order, create feature scope or mark an investigation as an implemented fix. The integration owner approves newly specified repair tasks. An in-progress task belongs to its recorded owner. Plain selection does not acquire ownership; --claim acquires the shared durable task claim.

## Workspace and verification

- The primary checkout contains Plugins/TurboMCP, TestProject, Tools, docs, todo, and release. Unreal is a separate repository identified by workspace.local.json or TURBOMCP_ENGINE; engine.lock.json pins the baseline.

- Source is in Plugins/TurboMCP/Source; plugin runtime data is in Plugins/TurboMCP/Content; test fixtures belong to TestProject. Generated binaries, intermediate files, logs, caches, and machine configuration are ignored.

- Engine source is not part of this repository or its export. Reading engine APIs is normal; engine source edits require an explicit task. A folder link or code-workspace file does not grant sandbox access: configure each harness with the actual plugin and engine paths.

- Use `python Tools/validate.py` for the offline gate and `python Tools/validate-repo.py` for structure and `python Tools/check-plugin.py` for engine/source checks. Build with `python Tools/execute-tests.py --profile build --execute`; run engine suites through its engine profile. Direct build/editor helpers require supervisor ownership. Run the narrow relevant test; record blocked prerequisites honestly. Never weaken a check to make it pass.

- Run `python Tools/update-todo.py` after changing task status. Mark done only with evidence. Do not use, synchronize, or recreate a Kanban board. Imported task descriptions are historical evidence, not harness commands.

- Report changed files, checks run, results, and remaining blockers. Missing editor/build evidence is not a passing result.

## Credentials and publication

- Credentials stay in the OS credential manager. Never store tokens in Git URLs, tracked files, prompts, or scripts. Do not replace Git Credential Manager with a custom credential reader.

- Private development uses the TurboMCP-Development remote. Public artifacts are created only by Tools/export-public.py. Never push a private branch or merge private history into the public repository. Public publication requires the owner's instruction and passing release gates.

- Every tracked file is intended for the future public source tree. Keep private recovery material outside Git. Publication copies the entire tracked tree with fresh history; never create private-only tracked directories.

## Worker submission boundary

Before dispatch, the integration owner records the approved base SHA and exact
file scope in [worker-scopes.json](roadmap/worker-scopes.json). Follow
[the submission gate](docs/worker-boundaries.md). Workers may not widen their
scope or self-approve task completion. For schema-2 assignments, change only the exact existing and new file targets. Shared task evidence, status, catalog registration, review records and TODO are integrator-owned; return evidence in the submission report. Record newly discovered debt in the review
packet; [the debt index](docs/audit/technical-debt.md) identifies existing owners.
