# Worker boundaries and integration gate

[Agent instructions](../AGENTS.md) | [Handoff](agent-handoff.md) | [Debt index](audit/technical-debt.md)

Use one reviewed task, one isolated worktree and one durable claim per worker.
The integration owner records a full approved base commit before dispatch.
R13 and R16 are accepted. R04 checkpoints 1-5 are accepted as source evidence and checkpoint 6 as a bounded design investigation. Checkpoint 7 is accepted as a pure offline identity validator. Checkpoint 8 is accepted as a design. Checkpoint 9 is accepted as pure offline orchestration. Checkpoint 10 is accepted for offline behavior only. Checkpoint 11 is accepted as design only. Checkpoint 12 is accepted for offline containment behavior only. The active assignment is checkpoint 13: synthetic acceptance harness implementation, with offline mocks only.

## Approved scope

[worker-scopes.json](../roadmap/worker-scopes.json) contains exact writable paths
for R13, R16 and the active R04 checkpoint. Everything else is outside scope. R13 may update the six listed
comparison/evidence files. R16 may write its two dependency reports and task
evidence. R04 checkpoint 13 may write only the executor profile, new fixed synthetic acceptance script, additive offline reader tests and task Completion evidence, as listed in worker-scopes.json. In a task file, only the Completion evidence section may change.
The reviewer owns task status, review registers, generated TODO and queue updates.
Unknown tasks fail closed until the integration owner supplies a reviewed scope.

Checkpoint 13 permits the synthetic profile and harness implementation, but only offline validation. Native execution remains integration-owner work after review. No live collection or production caller is approved. No plugin edits, engine edits, baseline upgrades, existing-test changes, dependency changes,
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

Use R16 or R04 for those assignments. The commits must be available in this repository.
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

## R16 dispatch boundary

The shared Git-directory claim ledger was confirmed from both harnesses with
matching path, SHA256 and no active claim. R16 is ready for one source/offline
worker from updated main in a fresh worktree. Do not use the retired R13 helper.
The worker must not launch builds, editors, engine suites or recovery commands.
Cross-harness execution-lock visibility is still unverified; the integration
owner supplies the required supervised baseline-build evidence during review.
An absent build report remains pending, never passed. The exact report-file
allowlist and completion-review requirements remain unchanged.

## Current integrator assignment: Append alias refusal

This assignment supersedes checkpoint 13 write permissions for its worker. Implement per-reference refusal of ambiguous existing six-character aliases in GraphAppend Plan, preserving qualified keys, unique aliases and label/type-id fallback. The exact four-file allowlist is worker-scopes.json. Add compiled editor automation with transient fixtures; no binary asset, fixture generator, Build.cs, public API, placement, full-GUID export or completeness redesign. Worker runs offline checks only; build/editor acceptance is integration-owned. Task evidence changes are limited to Completion evidence. Session 2 is read-only after its accepted test repair; it has no mutation assignment while this worker holds the single durable claim. This scope does not accept prior accumulated changes or complete R04.

## Current integrator assignment: Append placement

This supersedes the alias assignment above, which passed source review, a pinned 5.8.3 build and the focused append engine suite. The seven-file scope in worker-scopes.json permits opt-in placement for Append Preview/Edit only. Preserve shared Replace behavior, existing nodes, links except requested deltas, defaults and comment bounds. Use deterministic stored/estimated geometry with checked widened arithmetic; requested but missing/invalid placement must refuse before mutation. No visual no-overlap guarantee is claimed. The worker may run offline checks only; engine validation remains integration-owned. Session 2 remains read-only. The exact base and placement contract accompany the dispatch; R04 as a whole remains incomplete.

## Current integration state

Append placement through 8ebe1a0 is accepted with pinned-engine and offline evidence in R04. Both worker follow-ups are read-only acceptance/design investigations until a new implementation scope and claim are explicitly issued. The placement allowlist is historical permission for its completed assignment, not authorization for further edits.

## Current integrator assignment: shared-value reader repair

This supersedes the preceding read-only state for Session 2 only. Its five-file scope permits replacing undefined shared-value tokens with a successfully rendered first form and subsequent source-pin references, with bounded upstream reroute resolution. Preserve cycle/depth guards, full-detail knot behavior and unrelated single-use text. Partial child rendering, unsupported forms, unresolved sources and pin caps must not certify a reusable complete form. No writer, dialect, completeness protocol, macro rendering, pagination or engine change. Session 1 remains read-only. Worker validation is offline only; build and engine acceptance remain integration-owned. The exact base and contract accompany dispatch.
