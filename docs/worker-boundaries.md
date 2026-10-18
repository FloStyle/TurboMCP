# Worker boundaries and integration gate

[Agent instructions](../AGENTS.md) | [Handoff](agent-handoff.md) | [Debt index](audit/technical-debt.md)

## Current assignments

The integrator dispatches one named assignment, full approved base SHA and isolated registered worktree per worker. The schema-2 manifest in roadmap/worker-scopes.json at that base is authoritative. Historical scopes in task evidence do not grant current permission. Multiple assignments may share R04 only when their exact paths are disjoint. R04 remains incomplete.

- monitor-correctness: implement the Queued monitoring-correctness slice in docs/design/editor-experience.md. Preserve JSON/filter compatibility. Add isolated compiled regression coverage and its wrapper. No general layout, server controls, settings migration or changed-asset telemetry.
- widget-roundtrip-acceptance: add a bounded synthetic compiled regression and wrapper for the current production Widget Blueprint graph dump and targeted append workflow. Use the accepted Widget append fixture as precedent, without editing it. Test the actual supported reader/writer contract; do not assume an entire dump is append input. If that contract cannot express a useful targeted round trip, report the exact source-backed blocker before writing a substitute test. Include clean preview, exact intended node/link delta, preserved existing positions/defaults/tree/sibling graph, fresh read-back and refusal without mutation. No save, disk fixtures, production code repair or persistence certification.

The exact files and new_files are in the immutable base manifest. Workers cannot edit shared task documents, catalog, capabilities, queue, scope policy or release state. Return proposed catalog registration and evidence in the report for integrator application. Both workers run offline checks only; the integrator reviews and schedules native build/editor tests serially through the supervisor. No direct engine, MCP, recovery or ownership bypass.

## Claim and submit

Run `python Tools/next-task.py --claim <owner> --assignment <name> --base <full-approved-sha>`. Refusal means stop. Do not migrate, steal or release claims. One worktree owns one claim. A claim is coordination, not a security sandbox or permission to widen scope.

Commit only scoped files. Return full candidate/base SHA, claim ID, commands and results, untracked state, limitations and evidence locations. Retain the claim and stop. No merge, push, readiness changes or follow-up task selection.

## Integration review

From the trusted integration checkout, run `python Tools/check-worker-scope.py --task R04 --assignment <name> --base <approved-sha> --candidate <candidate-sha>`. Policy comes from the approved base, never from the submission. Inspect source before executing candidate tests. Passing scope is not semantic acceptance. Run the offline gate and the task-specific evidence checks; missing native evidence remains pending.

Only the integration owner records acceptance, updates shared catalog/task evidence, integrates private main and releases the exact claim. Publication remains a separately approved clean-history export. No automatic stale-claim recovery exists. Migration requires an empty validated legacy store and exact backup; rollback is forbidden after any schema-2 mutation.

## Execution and privacy

Engine builds and editor tests remain serialized using existing shared kernel locks and supervisor ownership. Never kill another owner's process or touch workplace assets. Keep raw private logs and engine source outside Git. Do not weaken tests to pass. Required server-side CI remains R15 debt; scope checks are not an OS sandbox.

## Next Widget acceptance case

widget-connected-input supersedes the completed roundtrip assignment. Extend only its compiled fixture and wrapper with one supported connected-data-input case through the real reader and targeted Append path. Derive the data source reference from emitted output, assert the exact intended consumer/data link and read-back, preserve unrelated graph/widget identities and state, and prove invalid source refusal without mutation. Use existing supported node/pin syntax confirmed in source; report a missing contract before inventing one. Do not change production, save assets or widen to all node types. The monitoring assignment remains unchanged. Offline checks only; native acceptance is integrator-owned.

## Compact panel layout assignment

panel-layout supersedes completed monitor-correctness. Implement compact native Monitor/Tools views. Default to Monitor; retain connection/recording status, bounded filters/history and one selected-call inspector for existing request/result data. Tools retains discovery, help, arguments and results. Rename Sample to Fill example arguments with a schema-scaffolding tooltip. Use neutral selection; reserve outcome colors for known states. Preserve runner behavior, revision refresh and isolated tests. View switching and history selection must not execute tools or change settings. No server lifecycle, menu/icon, changed-asset telemetry, fake turns or duplicate-call fix. Update allowed tests without weakening checks. Offline validation only for worker; integrator owns native and narrow/wide visual acceptance.
