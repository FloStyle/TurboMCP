# Worker boundaries and integration gate

[Agent instructions](../AGENTS.md) | [Handoff](agent-handoff.md) | [Debt index](audit/technical-debt.md)

Use one reviewed task, one isolated worktree and one durable claim per worker.
The integration owner records a full approved base commit before dispatch.
R13 source comparison is reviewed and accepted. R16 is the next assignment.

## Approved scope

[worker-scopes.json](../roadmap/worker-scopes.json) contains exact writable paths
for R13 and R16. Everything else is outside scope. R13 may update the six listed
comparison/evidence files. R16 may write its two dependency reports and task
evidence. In a task file, only the Completion evidence section may change.
The reviewer owns task status, review registers, generated TODO and queue updates.
Unknown tasks fail closed until the integration owner supplies a reviewed scope.

No plugin edits, engine edits, baseline upgrades, test changes, dependency changes,
release changes or broad refactors are authorized by these investigation scopes.
Use the existing supervisor for the exact checks specified in the task. Keep raw
logs and licensed or workplace source outside Git. Do not copy engine bodies into
reports. R13 must preserve every routing row and unverified preference status.

## Review a submission

The worker commits only its deliverables and returns the full base and candidate
SHAs, claim ID, commands, results, evidence locations and unresolved debt. It must
stop without merging, releasing its claim, changing readiness or starting another
task. It must report any uncommitted or untracked work; those files are not part
of a submitted commit.

From a trusted integration checkout, run this command with the recorded full SHAs:

```powershell
python Tools/check-worker-scope.py --task R13 --base <approved-base-sha> --candidate <submitted-sha>
```

Use R16 for that assignment. The commits must be available in this repository.
The command reads policy from the approved base, not from the worker submission.
Do not accept a base chosen retrospectively by the worker. Do not run a worker's
modified validator as review authority. Inspect changes before executing any
candidate code, including tests. Re-run against the final submitted commit after
corrections. Changes to the integration base require a fresh reviewed scope check.

The gate rejects paths outside the allowlist, deletions, executable files,
symlinks/submodules, task status/scope edits, missing or duplicate routing rows
and premature routing preferences. It checks the final tree difference, not
every intermediate commit. Review intermediate commits separately before merging
their history; secrets must never enter any commit.

Passing means only that the submission fits its file and structural boundaries.
Review prose and JSON for truthful evidence, licensed material, unsupported claims
and hidden scope changes. Run the offline gate, inspect the final diff and verify
task-specific evidence. Only the integration owner then updates completion state,
review records and TODO, releases the claim, and integrates into private main.

## Security limits

This is a fail-closed integration check, not an OS sandbox, branch protection or a
guarantee against a worker with unrestricted shell access. Give workers write access
only to their worktree and required local output directories. Keep the engine
read-only except where an owned build requires generated outputs. Avoid exposing
publication credentials. The current process uses explicit reviewer checks;
required server-side CI remains R15 debt. A worker unable to honor these boundaries
must stop and return its blocker rather than bypass a check.

## Dispatch hold after R13

R16 is the next planned task, but dispatch is blocked until DSH and the
integration host reconcile the reported R13 claim. The integration ledger has
no active claim. Do not create another claim or start R16 yet; follow the
dispatch prerequisite in [R16](../todo/roadmap-r16.md). No claim was released by the reviewer.
