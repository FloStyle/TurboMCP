# Fix Oct23 S1 plugin compile errors

ID: FIELD-X97
Status: done
Priority: P0
Readiness: done
Category: Bug
Owner: Codex
Review: source-reviewed
Validation: build_passed_runtime_untested

## Problem

Report 2026-10-23_S1, X-97: clean plugin installation at 5eaf00b failed on UE 5.8.3 / MSVC 14.50 with C4456 in BlueprintRead and C2039/C3861 in GraphRefactorTools. No editor session, asset edits or crash occurred. Later compile failures may have been masked.

## Completion evidence

The component and actor bound-event branches now use distinct local names. BindNewEvent calls UBlueprint::GetAllGraphs instead of the nonexistent FBlueprintEditorUtils method. Stock engine Blueprint.h declares the const member used. The exact source diff was reviewed; no build/tests/editor runs were performed.

Done means the two reported source defects are repaired, not that the complete plugin build passed. Full compile evidence is pending. Preserve recurrence links X-70, X-75, X-77 and FT-82 without counting them as new instances of this same repair.

## Local build evidence, 2026-10-23

Supervised build b8fae2fc15934c388f429bd659ca998d passed at source commit 168fbaceb25fe807f86dd2c74e1091e019493b21 against pinned local stock UE 5.8.3. Doctor and build returned 0; 120 actions completed and UnrealEditor-TurboMCP.dll linked. Runtime tests were not run. Later tracking commits do not change plugin source. The previous unbuilt statement describes the pre-build state; compile is now passed, runtime remains untested.
