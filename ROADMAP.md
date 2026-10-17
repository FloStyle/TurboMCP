# Roadmap to a dependable v1

The product is an experimental editor extension, not 101 certified features. The [capability inventory](docs/audit/capabilities.md) accounts for all 101 declarations and definitions: 95 have parsed static call references and six have name-only test references. Runtime acceptance remains separate. The [task dispositions](roadmap/task-dispositions.json) account for all 92 pre-roadmap tasks, including duplicate/process evidence and deliberately deferred work.

## Give an agent one instruction

> Read AGENTS.md. Run `python Tools/next-task.py`. Work only on the returned task, follow its scope and stop conditions, and record evidence. If it says STOP, explain the blocker; do not skip ahead or invent work.

The selector reads the approved sequence and refuses blocked, unready, already-owned/in-progress or incomplete-prerequisite work. It does not execute commands, grant a lease, assign credentials or publish. A coordinator must claim the task before launching an agent. Concurrent agents require distinct worktrees and explicitly assigned independent tasks; the default queue is intentionally serial.

## Investment decisions

Fix false success, data-loss risk, misleading observations and unusable tests before adding features. Keep 5.8.3 pinned. Compare 5.9 native tools before expanding enum/struct/source-control/widget-animation or building a proxy. Do not remove existing useful workflows merely because native names overlap. General multi-asset batching and broad new authoring stay deferred until the native comparison and a concrete missing workflow justify their maintenance cost.

## Baseline execution order

| Stage | Work | Exit condition |
| --- | --- | --- |
| R01 | Make test-audit classifications truthful | Parser regressions distinguish evidence from references and false positives |
| R13 | Compare pinned 5.9 native interfaces early | Adopt/adapter/retain/defer decisions; unknown runtime parity stays explicit |
| R02 | Shared engine/editor/port ownership | Concurrent-process contention and cleanup tests pass |
| R03 | Fixture and binary reproducibility | First-run failures and fixture drift have reproducible root-cause evidence |
| R04 | Audit every callable and result contract | Every tool has schema/effect/refusal/evidence status and bounded repair specifications |
| R05 | Mutation, rollback and restricted batch | Exact lifecycle gaps and required repairs are specified against ARC03-ARC09 |
| R06 | Graph identity and representation | Findings resolved or specified with concrete negative controls |
| R07 | Geometry, layout and capture | Measured geometry/framing requirements and fixes |
| R08 | Data/material/widget correctness | Domain read-back and restoration requirements resolved |
| R09 | References and runtime observation | Unknown/unmatched data cannot masquerade as measured absence |
| R10 | Human docs, help and discovery | Descriptions and examples agree with observed behavior |
| R11 | Complete 5.8.3 acceptance | All 97 required cases mapped and passing on the candidate |
| R15 | Release automation | Required CI and release dry-run failure controls pass |

Investigations produce reviewed repair specifications, not unlimited implementation permission. Confirmed defects must be assigned bounded repair tasks by the integration owner and inserted before their acceptance gate; the executor cannot silently close a finding or expand its own scope. This queue is the audit and foundation plan, not a claim that all 5.8.3 product repair tasks are already specified.

## Optional compatibility track

R12 proves installed-engine packaging. R14 implements and verifies the 5.9 adapter. Run `python Tools/next-task.py --track compatibility` when the baseline prerequisites and target engine are available. Missing optional engine distributions do not block baseline safety fixes and do not become support claims.

## Cost and drift controls

Read the selected task, relevant design and exact source/test files; do not reload all historical audits each turn. Reuse existing fixtures and focused suites. Do not run a clean engine build to answer a source-only question. After two identical unexplained failures, diagnose instead of repeating. Record invalidated assumptions and stop on missing prerequisites. Changes to release scope, public schema or approved architecture require an explicit design decision. No automatic publication from a development push.

For each completed task, retain the observable outcome, source/engine/binary identity, commands, exits and logs. A statement of confidence or a generated document is not acceptance evidence. See [audit findings](docs/audit/findings.md), [technical decisions](docs/audit/technical-decisions.md) and [native overlap](docs/audit/native-overlap.md).
