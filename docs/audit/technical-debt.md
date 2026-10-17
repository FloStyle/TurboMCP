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
