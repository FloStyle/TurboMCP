# Verified execution and agent handoff

[Documentation](../README.md) | [Verification design](../design/verification.md)

The local Windows framework is ready for one claimed roadmap task at a time.
This is not certification of every TurboMCP feature, a public release or an
installed-engine package. The active R04 assignment is defined in todo/roadmap-r04.md and the worker scope policy.
Imported reports still need reconciliation before product implementation.

## Commands

Run these from the intended checkout after configuring the engine in
workspace.local.json. Planning is the default; `--execute` starts a bounded run.

| Purpose | Command |
| --- | --- |
| Structural feedback | `python Tools/execute-tests.py --profile fast --execute --max-seconds 300` |
| Offline regression gate | `python Tools/execute-tests.py --profile offline --execute --max-seconds 600` |
| Incremental plugin build | `python Tools/execute-tests.py --profile build --execute --max-seconds 900` |
| Focused engine test | `python Tools/execute-tests.py --profile engine --suites test-agent-help --execute --max-seconds 900` |
| Regenerate fixture, test twice | `python Tools/execute-tests.py --profile fixture --suites test-graph-swap-call,test-graph-repoint --execute --max-seconds 900` |
| Inspect interrupted recovery | `python Tools/recover-tests.py <run-id>` |
| Recover an interrupted run | `python Tools/recover-tests.py <run-id> --execute` |

Build/editor/fixture scripts are internal entry points. Direct engine operations
require a live supervisor ancestor and matching checkout/engine context. Do not
manufacture environment variables to bypass that check. A refused operation is
a stop condition, not an invitation to kill a process or delete a lease.

## Ownership and restoration

The supervisor holds kernel locks for the canonical checkout, engine and MCP
port. Existing editors/builds, occupied ports and unknown leases are refused.
Children enter a Windows Job Object before they can launch further processes.
Timeout or owner death terminates only that owned job. These are coordination
controls for trusted code under one Windows account, not a hostile-code sandbox
or a cross-machine scheduler.

The entire TestProject/Content boundary is snapshotted before engine work.
Recovery verifies all backup hashes before writing, restores deleted/changed
files, removes newly generated files within that boundary and verifies the
result. Linked boundaries are refused. A pending/corrupt journal blocks another
engine run. Recovery refuses a live owner, foreign lease, changed backup or a
previously completed recovery; it cannot overwrite later work using an old run.

An interrupted test stays interrupted after successful recovery. No mutation is
automatically replayed. The fixture profile runs a second trial only after the
first passes and stores each trial's detailed logs in a distinct directory.

## Evidence from 17 October 2026

Baseline: Unreal 5.8.3, revision 396c9f059903aed5fec78ecd3d437a40c6415368.
The verified plugin binary SHA-256 was
6a20846e6adcd22bb879a1557d93a5bae6a278a60c9a16ab7174ddc2469902c9.
Each local report also records its candidate digest and commands. Run identifiers
below refer to ignored `.local/executions/` evidence on the validation machine.

| Experiment | Observed outcome | Run |
| --- | --- | --- |
| Original incremental build | Failed on unity helper-name collisions; 16 content files restored | a6f03b86c2cb462d95be2f8426db99ad |
| Current DLL, build and tool-help suite | Passed after process-module path/hash verification | 7ca82e7cab3e42fb83bbdfbff61300ae |
| Current DLL, regeneration and first/second graph trials | Both suites passed on both trials; separate logs and original bytes restored | 2a3cbfe7fe264839909336c163509bee |
| Real 15-second engine deadline | Timed out, evidence retained, 16 files restored in about 15.8 seconds overall | c5cba87440ca4f94920f89f047d7c588 |
| Competing engine request | Refused without starting a command or stopping a process | c51ccfaebb634dcdbe9632514fc534b3 |
| Forced supervisor death after editor launch | Recovery CLI restored 16 files, released the owned lease, kept interrupted status | 735dec26c6bc499ab48be1eedb75f0f0 |

The final discovery review found a legacy engine junction loading the old
pre-migration plugin. It was archived locally without deleting its target.
Earlier editor-suite passes, including 802e63ff2caf45c38a76a7a03ca1c466,
85dad6cf87484b6b9ae27f473b01be46 and 9cc5e19d1d33446d8badca17ce5218f6,
are invalid for current-plugin acceptance. They are retained only as historical
harness evidence. The current-plugin rows above supersede them. Timeout and
owner-death experiments establish process/recovery mechanics, not plugin behavior.

Doctor now refuses duplicate TurboMCP installations, including renamed plugins
declaring the same module. Before accepting a healthy test editor, the probe
inspects the owned process's loaded modules and verifies the DLL path and SHA-256
against build provenance. A matching tool name or a healthy MCP response is not
sufficient evidence of the tested code.

Offline controls additionally cover owner death after a synthetic mutation,
unrelated-process survival, corrupt backups/journals, context rejection and
simultaneous task claims from two real disposable Git worktrees. A passing
source check is never counted as a runtime plugin acceptance case.

## Crash feedback and privacy

Failure logs, available crash artifacts, commandlet logs and build provenance
remain local and ignored by Git. Raw artifacts may contain proprietary data.
Collection is capped at 512 MiB per attempt; omissions are recorded and original
files are retained. Artifacts may predate the run, so their presence alone does
not establish its crash cause. No crash is inferred from a nonzero exit.

The separate ASCII feedback draft contains controlled status fields only. It
does not copy raw paths, exception text, arguments or logs. A maintainer must
add synthetic reproduction steps and review it before sharing. Last tool calls
must be established from the retained log; unavailable calls remain unknown.
Existing drafts are never overwritten. No artifact or report is uploaded.

Common credential environment variables are removed, but OS credential access
and outbound networking are not isolated. Use synthetic repository fixtures;
workplace projects must not be introduced into automated testing.

## Agent workflow

1. Start in a clean, isolated worktree and read AGENTS.md.
2. Run `python Tools/next-task.py --claim deepseek-01` with a distinct owner name.
3. If refused, stop. Do not skip a blocker, delete a claim or take another task.
4. Implement only the selected task. Use the smallest applicable test profile.
5. Return commits, changed files, commands, exits, evidence and limitations.
   Stop for review; do not merge, publish or automatically start another task.

Claims persist after worker exit and are shared through the repository's Git
common directory identity. The integration owner reviews abandoned work before
releasing a claim with `python Tools/task_claim.py --release <claim-id>`.
The default queue remains serial. Starting multiple independent workstreams or
using another account/machine needs a separate coordination decision.

## Remaining boundaries

R13 compares Epic interfaces before expansion; R16 reviews installed-engine
integration. Full tool behavior, rendering/capture, UI and release acceptance
remain separate work. Headless tests do not certify rendered output. GitHub CI
is still not active; local results are not remote branch enforcement. No agent
dispatch here starts a paid model session or automatically publishes anything.

## R16 dispatch boundary

The shared Git-directory claim ledger was confirmed from both harnesses with
matching path, SHA256 and no active claim. R16 is ready for one source/offline
worker from updated main in a fresh worktree. Do not use the retired R13 helper.
The worker must not launch builds, editors, engine suites or recovery commands.
Cross-harness execution-lock visibility is still unverified; the integration
owner supplies the required supervised baseline-build evidence during review.
An absent build report remains pending, never passed. The exact report-file
allowlist and completion-review requirements remain unchanged.
