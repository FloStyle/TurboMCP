# R02: Make shared engine ownership atomic

ID: R02
Status: done
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

Shared kernel locks now supervise build, editor, runner and fixture entry points.
Per-checkout leases remain bookkeeping, not the concurrency guarantee. Direct
entry points require the live execution context. Unknown owners are preserved.

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

Reviewed against the pinned 5.8.3 engine and the candidate described in
[the infrastructure evidence](../docs/audit/infrastructure-review.md).
Focused ownership, crash/recovery and task-claim regressions pass. Real build,
tool-help and two graph suites pass. First/second trials after fixture generation
pass with separate logs. A forced real-engine timeout and supervisor death both
preserved evidence and restored 16 content files. The failed initial unity build
is retained; this module now compiles without unity grouping. No full engine
rebuild or product feature change was required.

This closes the bounded ownership/reproducibility foundation, not all historical
fixture claims, every plugin tool or release acceptance. Imported defects still
need current source and runtime reconciliation before implementation.

## Related findings
- [foundation-shared-lease](foundation-shared-lease.md)

## Operating limit

The current launcher uses port 8000, so engine sessions intentionally serialize
even for different engines. Distinct resource keys are independent at the lock
primitive; multiple live editor ports are not claimed by this handoff. Local
task claims support cooperating worktrees under one Windows account.
