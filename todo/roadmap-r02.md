# R02: Make shared engine ownership atomic

ID: R02
Status: in_progress
Priority: P1
Readiness: ready
Owner: integration-owner
Design: docs/design/verification.md
Kind: implementation

## Problem
Current lease acquisition is check-then-write in a per-checkout .agent path. Two worktrees can race for one engine or port. Implement shared atomic ownership keyed by canonical engine and port, with explicit owner identity and recovery. Preserve foreign-process refusals and same-owner nested operations.

## Scope
Tools/ownership.ps1, execution_runtime.py, execute-tests.py, build.ps1, start-editor.ps1, run-suites.ps1 and all four make-*.ps1 fixture callers. Include focused process-concurrency tests. No C++ changes.

## Dependencies

R01

## Source review

The existing lease is per checkout and uses check-then-write. The new executor
has tested kernel locks and bounded child containment, but legacy entry points
do not yet require its context. Engine execution therefore remains disabled.
Global editor/trace/Live Coding stops have been removed from recovery paths.
Do not treat those repairs as completion of cross-worktree ownership.

## Acceptance
- Every engine entry point validates a live supervisor, canonical checkout and engine identity before mutation. An environment variable alone is insufficient.
- Reject an existing editor/build, occupied port, foreign lease or unreadable lease without stopping or erasing it.
- Own only descendants created by the run. Keep ordinary nonzero exit codes distinct from supervisor timeouts.
- Preserve run identity and crash evidence for R03. R02 may complete with engine dispatch disabled; fixture recovery and engine acceptance belong to R03.
- Two actual concurrent processes in separate temporary worktrees cannot both acquire the same resource.
- Different engines/ports can proceed independently where their fixtures and outputs are separate.
- Unreadable or stale state has an explicit recovery path; no foreign editor is killed or live lease silently removed.
- Run-start and cleanup preserve a pre-existing unrelated lease.

## Verification
- python Tools/test-execution-runtime.py
- python Tools/test-process-cleanup.py
- `python Tools/test-ownership.py`
- `python Tools/test-runner-verdicts.py`
- `python Tools/validate.py`

## Stop conditions
Do not expand into another domain. If a prerequisite is absent, record blocked with the exact missing input. After two identical unexplained failures, stop repeating the command and diagnose. Do not run a full engine rebuild without evidence that an incremental build cannot answer the question. Do not change public schemas, weaken safety contracts or start deferred features.

## Deliverable
The bounded change plus regression evidence, updated affected specifications and a concise handoff.

## Completion evidence
Pending. Record source/engine/artifact identity, command, exit, log and remaining limitations. A reviewer must check acceptance before marking done.

## Related findings
- [foundation-shared-lease](foundation-shared-lease.md)
