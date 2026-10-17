# Remaining technical debt

[Task review](task-review.md) | [Repair contracts](repair-contracts.md) | [Roadmap](../../ROADMAP.md)

This is a risk index for the completed task review, not another task queue.
The source-backed findings, original IDs and individual acceptance requirements
remain in those linked records. No item below is fixed by this document.
Owners are roadmap investigation owners, not permission to implement every finding.

## Repair order and boundaries

| Risk | Remaining work | Owner and required proof |
| --- | --- | --- |
| False success or damaged assets | Extract a transaction-free mutation core before restricted batching; validate sequential state, rollback and compile outcomes | R05; ARC03-ARC09 and B01-B05, including injected failure and unchanged-state controls |
| Misleading save or unexpected execution | Require durable save evidence; apply explicit per-call execution consent to widget preview | R04/R08; save and execution contracts in the repair specifications |
| Wrong graph edited | Reject empty writer graph names before probes or transactions; preserve explicit graph identity | R06; two distinct graphs, both unchanged on refusal |
| Incorrect read results | Explicit reference direction `both`, cast result pins, loaded-only struct readers, enum read-back, widget object references, trace offsets | R06/R08/R09; independent synthetic expected results, real nonzero controls and truthful limitations |
| Wrong domain mutation | Data-table operation order and preview refusals; override removal isolation | R08; ordered state read-back and unrelated members preserved |
| Tests that miss defects | Reference test misses explicit `both`; lint refusal assertion currently fails; material constants and window capture checks are weak | Relevant audit owner; prove new assertions fail on the faulty result before accepting a fix |
| Incomplete certification | Map and execute all 97 acceptance cases; audit behavior of all 101 declarations | R04 then R11; source presence and passing unrelated suites are insufficient |
| Maintenance and compatibility cost | Native comparison, supported registration/export boundaries, installed-engine packaging | R13 then R16; R12/R14 retain separate runtime certification requirements |
| Release process gaps | Required private CI, release failure controls, clean consumer installation, broken public metadata URLs | R15 with R16 findings; no release from a normal development push |
| Human and agent discovery | Verified tool preferences, readable examples, editor panel/icon design | R17/R18/R10; no claim of superiority based only on tool names |

## Refactoring policy

Refactor only to satisfy an approved repair contract. The known transaction-core
separation is justified by nested transaction ownership. Graph identity and result
handling may need small shared helpers after their callers and failure behavior
are audited. A general architecture rewrite, universal batch engine, custom MCP
transport or speculative engine adapter is not approved.

Each repair must state the allowed files, behavior preserved, failure control,
focused test command and rollback plan. New debt discovered during a task goes
in its review packet with evidence and a proposed owner. Stop if it prevents safe
completion; do not fix adjacent systems or bury a workaround in a passing test.

## Known evidence limits

The task review verified source findings and the bounded execution framework.
It did not certify every historical report. The focused lint run failed its
edit-shaped-name refusal assertion; the passing reference run did not exercise
the explicit `both` defect. Preserve those distinctions when writing regressions.
5.9 runtime and installed 5.8.3 compatibility remain unverified.

The [worker submission gate](../worker-boundaries.md) limits the next two
investigations. It cannot make unchecked code safe or replace human review.

## R13 integration follow-up

The source comparison is accepted; runtime preferences remain unverified. R04
must probe VCS failed refresh with a cached state: TurboMCP's non-null
`ForceUpdate` result does not prove freshness when a provider ignores the update
operation result. Specify a failure control before any production repair.
Enum partial failure/read-back remains [214d6e7a](../../todo/214d6e7a.md), and
struct graph-only reader coverage remains [7bd81b84](../../todo/7bd81b84.md).

The ownership test's locale-dependent pipe decoding was repaired separately in
`00ac2b8`. Producer and consumer now agree on UTF-8. The exact non-ASCII stdout,
stderr and false-verdict controls pass, as does the original unreadable-lease
blocking check. Full offline run `3121fd3995424c49ae8317227915c0d0` passed with
`PYTHONUTF8=0`. Earlier failures are retained; no product safety claim follows
from this harness repair. R16 still must investigate integration dependencies,
and R12/R14 must establish installed-engine and forward runtime support.

The R13 claim reported by DSH is not present in the integration host ledger.
This is unresolved coordination debt, not evidence of a particular root cause.
R16 dispatch is blocked until the exact claim path and Git common-directory
identity are reconciled from both shells. Do not bypass this by creating a new
claim or assuming a reported ID is shared ownership evidence.

## Shared claim storage correction

A matching repository probe and main commit established shared repository
visibility while the legacy per-profile claim file differed between harnesses.
Task claims now use the Git common directory. A concurrency regression with two
worktrees and different `LOCALAPPDATA` roots allows exactly one claim; incorrect
release IDs leave it intact and either worktree can release the exact ID.
The previous records are preserved locally and completed R13 is in shared
history. R16 still requires DSH to confirm the new ledger before dispatch.

The precise cause of the divergent profile views is unproven. Execution/recovery
locks also use `LOCALAPPDATA` and require a separate cross-harness contention
check or shared-lock repair before simultaneous engine work. Do not infer their
visibility from a successful task-claim test.
