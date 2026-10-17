# R02: Make shared engine ownership atomic

ID: R02
Status: todo
Priority: P1
Readiness: ready
Owner: unassigned
Design: docs/design/verification.md
Kind: implementation

## Problem
Current lease acquisition is check-then-write in a per-checkout .agent path. Two worktrees can race for one engine or port. Implement shared atomic ownership keyed by canonical engine and port, with explicit owner identity and recovery. Preserve foreign-process refusals and same-owner nested operations.

## Scope
Tools/ownership.ps1 and its build/editor/runner callers; focused process-concurrency tests. No C++ changes.

## Dependencies
R16

## Acceptance
- Two actual concurrent processes in separate temporary worktrees cannot both acquire the same resource.
- Different engines/ports can proceed independently where their fixtures and outputs are separate.
- Unreadable or stale state has an explicit recovery path; no foreign editor is killed or live lease silently removed.
- Run-start and cleanup preserve a pre-existing unrelated lease.

## Verification
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
