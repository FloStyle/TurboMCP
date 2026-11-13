# Node search structured output and scope handoff

Readiness: ready. Integrator-approved bounded repair for NOV13-08 and NOV13-09.

## Source findings
At 82e9ec52, TurboMCPNodeResolve.cpp returns text only from ReadTurboMCPNodeFind. Its cursor tracks asset/node position and cumulative matches/skips, bound to query and candidate paths, not a frozen graph snapshot. Existing coverage and load guards must remain unchanged. The field report establishes parsing friction, not a failure in the matching algorithm. Mapping-context and other Data Asset properties are outside graph search.

## Required behavior
Add trailing Format=text|json, default text for compatibility. JSON mode must return exactly one valid JSON document, including input/refusal errors. Construct records directly from nodes, never parse formatted text. Include schema version, status, scope, query filters, hits (asset, graph, exact node ID, title, type ID, class), skipped records (asset and reason), next_cursor (null at end), eligible asset count, nodes searched this page, matches this page/so far, skipped assets so far, scan_finished and complete. Counts must clearly distinguish this page from cumulative values. A final match total is known only after scan_finished; complete requires no skipped assets. CountOnly omits hit rows without suppressing match counts. Preserve existing bounded scan/page budgets and oversized-label behavior; explicitly flag omitted display fields rather than dropping identity.

Both modes must explain that a graph search does not inspect Data Asset properties or prove an asset unused. Recommend ReadTurboMCPRefsBatch followed by ReadTurboMCPDataAssetProperty for those references. Keep native/runtime and inherited-asset limitations clear. Pagination is not a snapshot; changing assets requires restarting.

## Scope and checks
Exact files: TurboMCPNodeResolve.cpp, Public/Tools/TurboMCPToolsets.h, TurboMCPAgentHelp.cpp and TurboMCPGraphInventoryTest.cpp under Plugins/TurboMCP/Source/TurboMCP. No registry-load guard removal, no file export, no changed node matching, no engine changes. NOV13-04 full-result export remains a separate open requirement.

Extend the existing synthetic inventory fixture: parse JSON, compare matching identity/counts with text, follow a small page size to exhaustion, exercise CountOnly, no-hit and invalid-input JSON, and verify scope warning. Compile once after the related changes, then run the focused fixture. Do not infer project-wide completeness from fixture success. Catalog/tracker/inventory updates are integrator-owned.

## Focused runner approval
Integrator approves a separate nov13-node-json-runner assignment for Tools/test-node-search-json.py only, using the existing strict RunTests report parser to run TurboMCP.GraphAppend.Inventory and reject unexpected warnings. This avoids invoking the full append suite. Register its catalog entry separately during integration.

## Nonvisual acceptance isolation
Approve nov13-node-json-acceptance on the existing inventory fixture source and focused wrapper only. Add a distinct native NodeSearchJson test using the same synthetic event setup without invoking position measurement. Retain the original inventory test and its failure evidence. Verify every returned identity exactly once, final null cursor, text parity, counts, refusals and dirty state. This isolates an unrelated rendering prerequisite; it does not repair or certify rendering.
