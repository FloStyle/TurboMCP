# Implementation handoff and review

[Documentation](README.md) · [Roadmap](../ROADMAP.md)

## Next assignment: R16

Give the agent this instruction:

> Read AGENTS.md. Create or use a clean isolated worktree, then run `python Tools/next-task.py --claim deepseek-01`. Work only on the returned task. Stop on a refusal. For R16, map production and fixture dependencies, public headers/exports and build boundaries. Use only the approved report paths and task Completion evidence. Run the bounded baseline build through the supervisor; do not edit plugin or engine source, change the baseline pin, remove dependencies or claim untested installed-engine support. Run the offline gate and return commits, evidence and unresolved questions. Do not merge, publish, release your claim or start another task. Wait for review.

Use a different owner identifier for each worker. The [execution guide](audit/infrastructure-review.md)
contains commands, recovery steps and measured limits. This readiness applies to
the approved queue; imported reports remain blocked until reconciled.

## Review packet for every agent

- Task ID, base commit and submitted commits; one bounded change.
- Problem, observable before/after behavior and affected contracts.
- Commands, exits, focused regression evidence and offline results.
- For engine work: engine revision, plugin binary hash, loaded plugin identity, fixture/reset evidence and editor ownership.
- Changes to schemas, dependencies, tool preference, UI or packaging called out explicitly.
- Unrun checks and unresolved risks; no claim that source inspection is runtime evidence.
- Suggested task status. The reviewer checks acceptance and approves completion and integration.

The reviewer inspects the diff, verifies evidence, checks scope and regressions, and either accepts or returns a bounded correction request. Only reviewed work enters private main. Concurrent workers need explicitly independent scopes and worktrees; the default selector is serial. Do not launch multiple workers against the same engine outputs or test editor.

## Sequence and priorities

R01 repairs evidence classification. R02 and R03 establish ownership and fixture recovery before any handoff. R13 then completes the source comparison and R16 establishes integration and packaging constraints. R04-R09 provide behavioral audit evidence. R17 makes verified preferences discoverable; R18 produces the UI design; R10 reconciles help and documentation. R11 is baseline acceptance and R15 release automation. Confirmed fixes discovered by investigations require owner-approved bounded repair tasks before their acceptance gate.

R12 separately validates installed-engine packages. R14 handles the 5.9 adapter. Neither optional target inherits baseline certification. Prefer useful native functionality over duplication; preserve TurboMCP workflows where evidence demonstrates a worthwhile improvement.

Review continues when the owner provides a commit, branch or review packet. This document does not install a background monitor or automatically start agents.

## Task review boundary

All 110 records have a [current source/specification review](audit/task-review.md). Read the assigned task's Current review first. Historical prose is retained in a collapsed section and does not override the current architecture. Confirmed defects have [repair contracts](audit/repair-contracts.md); the integration owner assigns each repair after its prerequisites. R13 source comparison is accepted; R16 is the next task. R14 is unready until the exact compatibility adapter is specified. No review status certifies all runtime acceptance or authorizes implementing the backlog.

## Required submission gate

The integration owner supplies the approved base SHA and applies
[the worker boundaries](worker-boundaries.md) before accepting a submission.
Workers must not change task status or regenerate TODO for R13/R16; submit
Completion evidence only. This narrower rule overrides the general status-update
workflow. Read [the debt index](audit/technical-debt.md) to avoid adjacent refactors.

## Dispatch hold after R13

R16 remains blocked pending a second-harness check of the repaired claim store.
Claims now live at `turbomcp/task-claims.json` inside the shared Git common
directory, not under `LOCALAPPDATA`. Run `python Tools/task_claim.py --inspect`
from updated main to report the actual path, digest and active claim. Never use
the old worker checkout's helper after this storage cutover. The integrator
preserved the conflicting legacy observations and recorded completed R13 in the
new history; it has no active claim. Compare that same record from DSH before
reopening R16. The exact cause of the old profile-view difference remains unknown.

The claim fix does not certify shared engine/port execution-lock visibility.
Those locks still use per-profile storage; keep build/editor work coordinated
by the integration owner until cross-harness contention is verified or repaired.
