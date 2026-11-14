# Read Designer navigation and layout context

ID: NOV09-17
Status: done
Priority: P1
Readiness: ready
Owner: codex-integrator
Design: docs/design/protocol.md
Review: source-reviewed
Validation: focused_native_passed; workplace_field_validation_pending

## Current review

The November 9 field observations WT-19, WT-20, WT-21 and FT-164 all point to the same read gap. `ReadTurboMCPWidgetTree` prints the hierarchy and a few values, while `ReadTurboMCPWidgetProperty` limits output to editable scalar/struct values. Neither reports the `UWidget::Navigation` object. Stock 5.8.4 exposes six `FWidgetNavigationData` directions, each with a rule and a name used for an explicit target or custom function. `UWidget` also exposes clipping and render transform getters. `UButton` and `UUserWidget` expose focusability getters. These are Designer facts, not live focus results.

Finding: The reviewed implementation now exposes the stored Designer directions and selected non-default context without changing edit eligibility; the focused fixture passed, but runtime focus remains unobserved.

Action: Keep the read-only contract and compact output; use a separate runtime or workplace check before claiming observed navigation behavior.

## Problem

An agent investigating gamepad navigation must inspect each widget by hand to see custom directions and cannot read the named custom function or explicit destination through TurboMCP. The compact tree also omits focusability, clipping and render-transform context.

## Scope

Assignment `nov09-widget-navigation-read` may change only `TurboMCPWidgetTreeSync.cpp`, `TurboMCPWidgetTreeTools.cpp`, `Public/TurboMCPWidgetTreeSync.h`, `Public/Tools/TurboMCPToolsets.h` and one new focused test `TurboMCPWidgetNavigationReadTest.cpp`. Keep the existing widget-tree text grammar unchanged; add clearly labeled read-only context after the round-trip tree. Do not change edit/preview property eligibility, load policy, asset state, navigation rules or runtime focus. No engine edits.

## Dependencies

None beyond the current stock 5.8.4 baseline.

## Acceptance

- A widget with Designer rules reports each of Up, Down, Left, Right, Next and Previous, including the exact rule and the explicit target or custom function name when present. Absent navigation is distinguishable from six default Escape rules.
- `ReadTurboMCPWidgetProperty` with `Property=Navigation` returns this read-only Designer data even though it is not editable. An empty property query points to this special read and does not imply edit support. Preview/Edit of Navigation continue to refuse before mutation.
- The tree adds compact focusability and navigation summaries plus clipping and render-transform facts where useful. It labels unknown focusability as unknown rather than false. Stored Designer data is not presented as observed runtime focus.
- Read calls leave node/slot structure, navigation values and package dirty state unchanged. Missing widget/asset and unsupported property remain clear refusals.

## Verification

Use `python Tools/validate.py`, one supervised stock-5.8.4 build and a focused native `TurboMCP.Widget.NavigationRead` fixture. The fixture creates synthetic widgets with explicit and custom rules, reads both tools, checks exact direction/name output and unchanged package state. No full-suite rerun is required for this bounded read change. Record run IDs, exit codes and any blocked prerequisites.

## Stop conditions

If the engine's reflected navigation name semantics differ from the header, stop and report before shipping misleading output. Do not run a workplace project or use runtime focus as a proxy for stored Designer rules.

## Deliverable

Exact scoped source and fixture commit, with the focused result and limitations for integrator review.

## Completion evidence

Candidate `3dce216854d03a815093d0662456045e6c539bc7` passed the schema-2 scope gate at approved base `f200b0b8a68219a729361af3a96c2f8886cd6062`. Integrator review reduced default tree noise without changing the six-direction property read. Supervised stock-5.8.4 build `b15bfbadbe9940888192a7d91eae93dd` and focused native fixture `1b901109746b4e0798912237f922b277` passed. The fixture restored the test project. A prior attempt with the local plugin preference disabled was interrupted and recovered, not counted as a pass. Runtime focus and workplace field behavior are unobserved. See [the integration record](../docs/audit/field-reports/nov14-widget-navigation-integration.md).
