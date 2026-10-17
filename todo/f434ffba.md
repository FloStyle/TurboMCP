# T38 (P6) — Test: the material reader keeps constants on unconnected inputs

ID: f434ffba-f4cc-49a1-bb51-21b51941c8b7
Status: todo
Priority: P6
Readiness: needs_reconciliation
Owner: unassigned
Design: docs/design/verification.md

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
