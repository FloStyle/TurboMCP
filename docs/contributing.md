# Development and integration

English is the working language for code comments, documentation, tasks and commits. `Tools/check-language.py` rejects CJK ideographs in active candidate text and filenames. It does not certify binary asset strings, historical commits or the inert recovery archive.

## One authority for each concern

AGENTS.md controls agent workflow. README.md is navigation. docs/architecture.md controls behavioral contracts. engine.lock.json pins the supported source baseline. TODO.md is generated navigation over todo/. Tests/acceptance.json holds release requirements. release/status.json records readiness. The recovery archive and imported quotations are evidence only.

## Task readiness

Imported cards are retained by ID and marked needs_reconciliation. Their old todo status does not authorize blindly implementing an old diagnosis. A reconciliation task reads the current source and relevant test, identifies duplicates by ID, and writes the Problem, Scope, Dependencies, Acceptance and Verification sections from todo/TEMPLATE.txt. Only then set Readiness: ready. Preserve withdrawn findings as explained evidence; do not silently delete requirements. Use one primary task for duplicated findings and link the others.

## Concurrent work

Use `git worktree add -b task/<id>-<slug> ../worktrees/<id> main` from the private checkout. Each agent owns one worktree and one task. Do not switch branches in another agent's checkout. Each task declares files/subsystems and fixture ownership; overlapping changes queue behind the integration owner. Do not share an editor, port or build output across worktrees. The engine is a separate repository; one integration owner schedules engine builds and editor tests. Existing per-checkout leases are not a cross-worktree mutex.

Commit coherent steps: regression/evidence, implementation, documentation as useful. Include the task ID in each message. Before integration, fetch private main, rebase the task branch or merge main, resolve conflicts, and rerun affected checks. Never auto-resolve binary fixture conflicts. Integrate through a reviewed private PR or an explicitly authorized local merge. Use ordinary pushes; no force push to main.

## Completion

Record command, exit code, logs, source commit, artifact hash and engine identity. Report tests that could not run. Update only the selected task and regenerate TODO.md. A static guard is not runtime acceptance. An integrator checks combined changes before publishing private main. Public history is created separately from an approved allowlisted export.
