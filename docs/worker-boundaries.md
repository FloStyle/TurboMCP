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

## Current integrator assignment: Widget Blueprint append acceptance

This supersedes the completed shared-value reader assignment. Session 2 implements only the four-file scope in worker-scopes.json. Session 1 remains idle. R04 is not complete.

Create a transient compiled Widget Blueprint automation fixture using the existing FWidgetBlueprintOperationUtils::CreateWidgetBlueprint precedent in TurboMCPKitCommandlet.cpp. Include a CanvasPanel, variable TitleText, StatusText and non-default slot properties; a function graph with entry and two sequential PrintString nodes with deterministic unique GUIDs, fixed positions/defaults; and an untouched sibling graph. Assert valid initial compilation and all fixture preconditions. Never touch user assets or persist the fixture.

Capture live before/after snapshots of graph node identity, positions, defaults, pins, links, sibling graph, widget tree and relevant slot properties. Exercise the real Append Plan/Apply path with exactly one intended new node/link delta. Assert Plan does not mutate, Apply makes only its intended delta and places the new node deterministically without moving old nodes. Add refusal cases and prove snapshot equality, including dirty state. Include negative controls that deliberately change a position, default or link in disposable fixture state and prove the comparator detects each change; restore and verify state before the real operation. Do not reset dirty flags to mask observed mutation. Assert fixture cleanup even on failed checks. Use current headers/APIs, not guessed Blueprint construction.

A new engine-suite wrapper must invoke the exact compiled automation through the existing AutomationTestToolset pattern and strictly parse one matching Success result with no errors. Register only that engine suite in Tests/catalog.json; preserve other entries. Worker may run offline validation and scope checks only. Build/editor/MCP/native validation belongs to the integrator. No production code, engine code, public API, dependency, existing test weakening, save/reopen or fixture generator changes. If an indispensable seam is absent, report the blocker rather than widen scope. Fresh reopen, save-on-compile and on-disk persistence are not certified by this slice.

Record Completion evidence only in R04, full candidate SHA, exact changes, checks and limits. Generated capability identity may be stale after adding plugin source: leave its integrator-owned file untouched and report the failure. Retain the claim and stop for review. No merge, push, claim release, completion marking or follow-up dispatch by the worker.

## Current integrator assignment: Append Plan probe isolation

Supersedes test-only Widget assignment for Session 2. Preserve its accumulated unaccepted fixture work. Scope adds only TurboMCPGraphAppend.cpp to the four Widget acceptance files. Plan currently spawns a probe in the real graph, dirtying the package through ParentGraph->Modify. Use the existing BuildProbeNode isolation path after verifying context and refusal parity; never clear/restore the user's dirty flag to mask mutation. Preserve action filtering, resolved class/pins, links, placement and actual Apply behavior. Do not remove the scratch probe through the real graph. Bound probe lifetime and report any spawner side effects that cannot be isolated within this scope. No NodeType/engine or unrelated writer changes.

Retain independent clean success/refusal fixtures and refusal Apply checks. Add already-dirty state preservation and repeated Plan checks; assert graph/widget snapshots unchanged. Tests must remain red if isolation is insufficient. Offline/scope validation only for worker; integration owner runs engine suites. Exact new base and claim accompany dispatch. No merge, push, claim release or completion marking.
