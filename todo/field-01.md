# Reproduce and contain native DataTable JSON-write crashes

ID: FIELD-01
Status: in_progress
Priority: P0
Readiness: ready
Owner: Codex
Design: docs/design/mutations.md
Kind: investigation
Review: source-reviewed

## Problem
Two crashes are reported, including a direct one-row call after restart. Native add/set/rename share a whole-table JSON rewrite; the exact assertion cause is unverified.

## Scope
Bounded investigation of the affected subsystem. Report-qualified observations: DT-07.
See [field intake](../docs/audit/field-testing-2026-10-20.json) for every imported observation and its owner.
No engine modifications, production reproduction, release claim or automatic worker dispatch is authorized by this record.

## Dependencies
Review and integrate the weekly prototype tests first. Exact repair scope and synthetic fixture are required before implementation.

## Acceptance
Obtain exact build and crash artifacts; reproduce on a disposable table with a sanitized reference-bearing row struct. Avoid the affected native JSON-write path pending diagnosis. Preserve existing rows and references through save/read-back.
Distinguish field reports, source corroboration, reproduced behavior and verified fixes. Retain negative controls.

## Verification
`python Tools/validate.py`
Runtime verification must use the existing supervisor and disposable fixtures after the focused reproduction is specified.

## Current review
Decision: investigate

Two crashes are reported, including a direct one-row call after restart. Native add/set/rename share a whole-table JSON rewrite; the exact assertion cause is unverified.

Obtain exact build and crash artifacts; reproduce on a disposable table with a sanitized reference-bearing row struct. Avoid the affected native JSON-write path pending diagnosis. Preserve existing rows and references through save/read-back.

## Handoff
Open; no fix or new runtime proof. Codex owns urgent follow-up. Auto-dispatch remains paused.

## Approved synthetic reproduction

Use a disposable transient DataTable with FTableRowBase to isolate object lifetime from private row fields. Exercise FillDataTableFromJSONString first without and then with a TStrongObjectPtr. Record a crash as diagnostic failure, not acceptance. No engine changes or production assets in this assignment. The reference-bearing row and open/closed editor variants remain follow-ups. Private crash artifacts are unavailable; the owner authorizes synthetic reproduction without work code.

Exact assignment: field-datatable-repro. Supervisor command: `python Tools/execute-tests.py --profile engine --suites test-datatable-import-repro --execute --max-seconds 600`.

## Reproduced lifetime failure and approved engine repair

Execution 5dc032a4dbd7443796e5f51dcc6b4466 failed with the reported UObjectArray.cpp:405 refcount assertion on a transient FTableRowBase table. The deliberately held object isolates lifetime; it does not establish which reference holder existed at work.

Integrator scope: at pinned engine base 396c9f059903aed5fec78ecd3d437a40c6415368, change only Engine/Source/Editor/UnrealEd/Private/Factories/CSVImportFactory.cpp in the separate engine checkout. Reuse ExistingTable instead of reconstructing it with NewObject; preserve the existing row-import, options and delegate flow. No CurveTable changes. Retain the engine patch outside the plugin export and report that plugin-only deployment does not include this fix. Rerun only the synthetic import diagnostic before broader deferred regression. This explicit repair scope supersedes the earlier no-engine-change restriction for this file only.

## Focused repair result

Native execution 9f90e8fe6b46468d9d6a45f74f4a49d8 passed after the one-file engine patch (6 insertions, 2 removals), including the held import that previously asserted. Engine HEAD stayed at the pinned base with that explicit working-tree modification. This proves the synthetic lifetime path only; private reference-bearing rows, editor-open variant and save/reload remain untested.

## Engine delivery

Separate engine branch fix/field-emergency-2026-10-20 contains verified lifetime repair commit 1ab08f9b290 and untested pin/transaction batch 0144a1931f3b8034d6fe953b676d0edaf4f4ef6f. These engine commits are local only and are not in the TurboMCP plugin. engine.lock.json intentionally remains at the supported upstream baseline; the doctor gate will reject the local custom engine HEAD until integration explicitly reconciles the tested engine revision. Do not bypass that gate or overwrite the modified Python dependency files with GitDependencies --force.

## Additional untested native DataTable source batch

Approved engine scope also includes Engine/Plugins/Experimental/Toolsets/EditorToolset/Content/Python/editor_toolset/toolsets/data_table.py: reject empty/None row names and case-colliding requests before mutation; resolve requested row names consistently with case-insensitive existence checks in get/set/rename; reject duplicate property aliases; refuse schema/export mismatches before native write. No test run during this pass.

Native DataTable input follow-up committed as cc5b062305f3201d4f27ee8497bd54e9671efd1e (untested). The engine repair branch now includes the Python dependency files explicitly, because they were previously ignored by Content/. GitDependencies must not overwrite these local changes. Engine delivery and regression remain integration work, not completed release evidence.

## Superseding delivery requirement: stock engine, plugin only

Owner explicitly rejects custom-engine deployment. Engine source and three Python dependencies restored to pinned stock revision, dependency SHA-1 verified. Previous engine commits retained as diagnostic evidence only. Source batch continues in TurboMCP with no test runs: in-place DataTable prevalidation, validated pin-default tool with dirty/undo handling, transaction/save safeguards and bounded batch outcomes. Registry filters disable only named unsafe native alternatives and direct users to plugin tools; general script batching has no equivalent and remains unavailable through the protected registry. Direct Python/editor code is outside this protection. Engine binaries must be rebuilt after stock source restoration; no engine patch is a deployment prerequisite. Exact assignment field-plugin-only owns the implementation files.

### Plugin-only source batch (2026-10-20)

Native DataTable add/set/rename MCP entry points are filtered while TurboMCP is loaded. TurboMCP row edits preserve the existing table object, validate on initialized scratch rows, compare canonical field values, reject conflicting/case-aliased operations and attempt transactional rollback on failure. The intentional native import crash diagnostic was retired from source/catalog; its reproduction and engine patch remain in Git history, not a deployment dependency. Source batch is untested; stock binaries still require rebuilding.

## Plugin-only batch evidence

DataTable text splitting now respects escaped quotes, keeping embedded commas/parentheses inside strings. Stock-engine build e09c5e1cbcd34d899ec8342d81db6c4a passed at 85d2f11; this final parser adjustment awaits rebuild. Runtime regression deferred.
