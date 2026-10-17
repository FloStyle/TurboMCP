# Verify exact unconnected material constants

ID: f434ffba-f4cc-49a1-bb51-21b51941c8b7
Status: todo
Priority: P6
Readiness: needs_reconciliation
Owner: unassigned
Design: docs/design/verification.md
Review: source-reviewed

## Current review

**Decision:** partial. **Follow-up owner:** [R08](roadmap-r08.md).

ConstantForInput and unlinked-input rendering exist. The current suite accepts any numeric constant rather than the fixture's exact expected value, so it cannot prove exact constant preservation.

### Next action and acceptance

Use multiple distinct constants, zero, vectors and a documented default case. Compare each named input to independent expression properties; an arbitrary numeric token is not a pass. Keep this as the gate for b6fa0cf7 expansion.

### Current source and verification

Reviewed at private main `6aee9dfd92a4c06a73e89c091973f982d715b7a2`. These source findings do not certify runtime behavior. Source links open the current file; line numbers refer to the reviewed revision.

- [Plugins/TurboMCP/Source/TurboMCP/TurboMCPMaterial.cpp](../Plugins/TurboMCP/Source/TurboMCP/TurboMCPMaterial.cpp)
- [Tools/test-material.py](../Tools/test-material.py#L120)

Starting checks for the assigned follow-up:

- `python Tools/execute-tests.py --profile engine --suites test-material --execute`

These suites are starting checks, not proof of the new cases. Use owned synthetic fixtures and retain local evidence. Reference records do not authorize implementation.

See the [review register](../docs/audit/task-review.md) for definitions, duplicate ownership and observed test results.

<details>
<summary>Historical requirements and evidence (not execution instructions)</summary>

## Roadmap disposition

Workstream: data-widget. Reconcile this finding with the current source under the ordered roadmap before implementation. Preserve its acceptance requirements and link duplicate findings to one repair task.

## Scope and acceptance

Preserve the substantive scope below. Before editing, reconcile the finding with current source and identify the focused acceptance check. Do not execute historical board/harness instructions. Record duplicates by task ID without discarding their requirements.

## Imported evidence

Historical paths and completion claims below are not current proof. This task must be made self-contained against the current source before implementation; no private archive is a prerequisite for public contributors.

> *** SOURCE: `Feedbacks/TurboMCP_TaskList_2026-10-13.md:367`, group P6 (a table row). ***
>
>     CONTENT, IN THE LIST'S OWN WORDS
>         *** "Test: the material reader keeps constants on unconnected inputs." ***
>     PASS WHEN, IN THE LIST'S OWN WORDS
>         *** "EVERY UNCONNECTED INPUT PRINTS ITS CONSTANT VALUE, OR 'default'." ***
>
>     *** AND THIS CARD IS A GATE FOR ANOTHER: `T35` SAYS "Run test T38 first", SO THIS IS THE CHEAPEST CARD IN THE LIST
>     THAT UNBLOCKS THE MOST WORK. ***
>     *** AND THE PASS CONDITION IS ALREADY THE PROJECT'S HOUSE SHAPE, WHICH MAKES IT EASY TO CHECK RATHER THAN EASY TO
>     FAKE: `TurboMCPLayout.cpp:141-142` PRINTS "findings: none. No backward wire, no overlap, no misplaced getter, no
>     stray node and no disconnected group" - *** AN EMPTY RESULT THAT ENUMERATES WHAT WAS CHECKED, SO A ZERO IS A
>     MEASUREMENT RATHER THAN AN ABSENCE. "EVERY UNCONNECTED INPUT PRINTS ITS CONSTANT VALUE, OR 'default'" IS THE SAME
>     REQUIREMENT AT THE ROW LEVEL, AND THE WORD "default" IS THE HONEST FALLBACK THE LIST ITSELF NAMES. ***
>     *** AND ONE MEASUREMENT PRECEDES THE TEST: whether the material reader already HAS the constants. `TurboMCPMaterial
>     Instance.cpp:359-365` PRINTS A CHAIN WITH ITS DENOMINATOR, SO THE REPORTING HABIT EXISTS IN THIS FAMILY. ***

## Verification

Pending. Record command, result, commit, and any blocked acceptance here.

</details>
