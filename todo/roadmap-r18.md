# R18: Specify a clearer editor panel and icon system

ID: R18
Status: todo
Priority: P1
Readiness: ready
Owner: unassigned
Design: docs/design/editor-experience.md
Kind: investigation

## Problem
The editor panel needs a clearer layout and easier status/result interpretation. Produce a concrete design that preserves behavior and can be handed to a separate implementation task.

## Scope
STurboMCPPanel.cpp, Public/Panel/STurboMCPPanel.h, TurboMCPPanelToolRunner.cpp, module tab registration and Resources/Icon128.png. Design only: no Slate rewrite, icon replacement or engine edits in this task.

## Dependencies
R17

## Acceptance
- Capture current interactions on the owned 5.8.3 fixture; record observed friction without private project data.
- Deliver annotated connected/unavailable/empty/busy/refused/failed/partial states, information hierarchy and exact labels.
- Define docked narrow/wide, high-DPI, keyboard/focus and non-color status requirements.
- Specify tool effect labels, safe copy/export behavior, actionable errors and honest outcome display. No hidden execution from status controls.
- Propose an icon direction and reproducible asset format without adding an unlicensed asset.
- Produce bounded implementation tasks with file ownership, acceptance checks and review gates. Owner approves the design before implementation.

## Verification
- Inspect the current panel and existing Tools/test-panel.py and panel probe entry points.
- `pwsh -File Tools/run-suites.ps1 -Suites test-panel` against the pinned editor under exclusive ownership; record failures as baseline evidence.
- Walk through the design states with the owner; record approved and unresolved decisions.
- `python Tools/validate-repo.py`

## Stop conditions
Do not patch engine source, change the pinned baseline, add a competing server, weaken safety contracts or change another task's scope. Missing prerequisites must be recorded. Do not rebuild the entire engine without evidence that an incremental build is insufficient. Stop after two identical unexplained failures and diagnose. Findings are not permission to implement unrelated fixes.

## Deliverable
A reviewable editor UX specification, annotated layouts and bounded implementation proposals. A design artifact does not certify an implemented UI.

## Completion evidence
Pending. Record exact source and engine identities, commands, exits, evidence locations, tested binary hashes where applicable, changed files and unresolved limits. Integration review is required before marking done.
