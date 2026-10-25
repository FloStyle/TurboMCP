# Widget variable workflow review

WF-UI-09-WIDGET-VARIABLE, 2026-10-25. Focus: EditTurboMCPWidgetsAsVars. Existing compile/connect/remove tools are supporting operations.

## Result

Native build/workflow 3faaddbb451a4f9d820afc0aa6d4d6dc passed on candidate 99fad26. Scope gate passed. Two disposable text widgets start with Is Variable disabled. A case-duplicated request exposes exactly two widgets. Compile produces the widget member; compiled graph logic changes the live label text through that member. Repeated enable calls are unchanged. Empty and missing-name requests preserve flags and dirty state.

Disabling a batch containing a referenced widget returns unchanged before either flag changes and reports the exact getter node ID. Explicit GraphRemove removes the test callback/body/getter; disabling then succeeds and removes the generated member after a clean compile. Both designer widget objects and original texts remain. Repeated disable is unchanged.

## Fixes and quality review

The stock toggle only changes the flag and regenerates the skeleton. It does not check existing graph uses. Added whole-batch preflight for self-variable getter/setter nodes across the Blueprint's graphs. The refusal names exact references and tells the caller to repair them first. It explicitly limits the check to local self-variable nodes. External, dynamic and name-based references are not covered.

Deduplication now uses Unreal's case-insensitive names. A change is counted only after the flag reaches the requested state; an unexpected failure reports partial changes without claiming rollback. Help and results explain that Is Variable exposes an existing widget, not widget creation, visual editing or graph wiring. Reviewed planner, wrapper, engine toggle, helper declarations, comments, state reporting and public description. The initial build caught an incorrect graph-enumeration API call, corrected to UBlueprint::GetAllGraphs before native execution.

## Limits

No external dependency discovery, setter-specific native case, injected toggle failure, undo restoration or save/reload test. Designer creation and reference-node creation are fixture prerequisites, not evidence for authoring tools. Coverage is 37/125 tools in bounded workflows. No screenshot is needed for a nonvisual flag operation; runtime and designer-state assertions provide the evidence here.

Offline gate 38b7cddf872443b7af3428622e358229: 22/26 passed; the same four structure, language, client-retry and lookup-guard failures remain.
