# Infrastructure review and handoff gate

[Documentation](../README.md) | [Verification design](../design/verification.md)

This review covers test orchestration source. It does not certify the plugin or
claim that all historical feedback has been reproduced. Product implementation
is paused while the infrastructure gate is open.

## Verified foundation

- Static audits distinguish parsed Python call syntax, literal name references
  and missing references. None proves execution. The 101 declared tools currently
  have 95 parsed-call references and six name-only references.
- The Windows execution supervisor uses kernel locks and a Job Object. Focused
  tests cover contention, owner death, nonzero exits, timeout cleanup and an
  unrelated process surviving cleanup.
- Fast and offline profiles run locally with a total deadline and unique logs.
  Requests bind to the commit and candidate-file digest. Unknown fields and
  stale requests are refused. There is no arbitrary agent-command field.
- Runner captures now use unique filenames. Recovery and fixture generation no
  longer stop Unreal or trace processes by name. Failed cleanup stops a build
  or fixture operation. Unreadable and differently owned leases are preserved.

## Run now

`python Tools/execute-tests.py --profile fast --execute --max-seconds 300`

`python Tools/execute-tests.py --profile offline --execute --max-seconds 600`

Omit `--execute` to print a request without running checks. Evidence stays under
`.local/executions/`. Fast means structural feedback; it does not test plugin
behavior. Offline runs the catalogued editor-free regression tier.

## Remaining engine gate

Engine plans can be inspected, but execution is deliberately refused. R02 and
R03 must establish the following before it can be enabled:

1. Route every build, launch, fixture commandlet and restart through shared
   ownership. Reject copied contexts, unknown processes and occupied ports.
   Existing per-checkout lease files are not cross-worktree locks.
2. Snapshot owned fixture bytes before mutation. Restore after success, suite
   failure and timeout after all owned processes exit. Keep a durable recovery
   manifest for supervisor death. Refuse another run until recovery is complete.
3. Prove two-worktree contention and independent-resource operation with real
   processes. Test stale/corrupt bookkeeping without deleting unrelated state.
4. Run the pinned 5.8.3 incremental build, a focused first run, then a second run.
   Retain the first failure. Record engine revision, candidate digest, binary
   hash, fixture hashes, exit codes, durations and full local logs.
5. Keep rendering tests separate from NullRHI tests. A headless result cannot
   certify capture, layout appearance or editor UI.

Do not launch implementation agents while this gate is open. Task selection is
not task claiming. A future dispatcher must atomically claim one reviewed task,
allocate a distinct worktree, enforce a deadline and iteration budget, and stop
at a reviewable commit. It must not merge, publish, add scope or retry mutations
whose outcome is unknown.

## Source-backed task corrections

| Task | Source observation | Required decision before implementation |
| --- | --- | --- |
| R01 | Regex-based references and assertion detection were overstated | Retain parser negative controls; static evidence stays separate from execution |
| R02 / foundation-shared-lease | ownership.ps1 writes a per-checkout lease after checking it | One implementation owner; shared locks must cover all entry points, not only the new executor |
| R03 / 9935c0ea | run-suites.ps1 snapshots TestProject/Content/Tests; direct generation also changes fixtures | Do not untrack an asset until clean-clone regeneration and semantic content comparison justify it |
| 048107d9 | Material test assertions and transaction-cancel paths are different evidence | Recount the current failure paths and design fault injection; the historical count of 28 is not current proof |
| R15 | A CI template is not an active required workflow | Record hosting/token constraints separately; do not claim remote enforcement from local validation |

The remaining imported reports retain `needs_reconciliation`. Each needs current
source anchors, a minimal synthetic reproduction, expected behavior, an explicit
non-goal, a failure-path test and a completion check before `ready`. Historic
line numbers, severity and proposed fixes must be rechecked. This table is a
focused review, not a completed semantic audit of every report.

## Privacy and authority

Use repository-owned synthetic fixtures only. Local logs are ignored by Git,
but may contain paths or tool output and require review before sharing. Common
credential environment variables are removed from child environments. This is
not credential isolation: same-user programs still have the user's filesystem
and OS credential access. No network sandbox or absence of engine telemetry has
been proved. Untrusted code requires a separately restricted account or machine.

No automatic upload, public push, paid agent dispatch or release publication is
part of the executor. Engine source and workplace assets are never test inputs
to publish. Human approval of a sanitized report remains separate from testing.
