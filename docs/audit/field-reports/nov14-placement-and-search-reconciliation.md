# November 14 placement and search reconciliation

This is an integrator source review, not a new implementation or runtime result. The four November 9 placement reports and the November 13 bulk-search report remain open.

## NOV09-02 and NOV09-03: append placement

`PlanPlacement` in `TurboMCPGraphAppendPlacement.cpp` chooses the first fixed execution attachment for an appended block. It has a special in-gap case for **one** new execution node. The general planner lays all new forms out as one dependency block, then searches for a collision-free translation near fixed connections. It does not split independent groups by anchor or shift an existing downstream tail. The current local mode can refuse when that whole block has no local space.

The reported four-group case explicitly requested `outside`. Current tool help and the native placement test define that mode as placing new nodes beyond existing graph bounds while preserving existing nodes; long boundary wires are disclosed. The reported far-right result is consistent with that contract. Per-anchor placement is a requested feature, not evidence that `outside` silently broke its documented contract. Changing `outside` into an inline mode would invalidate the existing safety and test contract.

**Execution order:** First specify NOV09-02's tail-movement boundary: which fixed nodes may move, the complete preview of those movements, refusal when the boundary is incomplete, and exact undo. Its acceptance fixture must cover a boxed section and unrelated lanes. Only then specify NOV09-03's per-anchor grouping and local collision policy. A group with conflicting fixed anchors must refuse or request explicit positions; it must not silently use the first anchor. Pure inputs remain beside their consumer. Preview and edit must agree on every new coordinate. No production assignment is approved by this note.

## NOV13-03: safe bulk-search coverage

The existing NodeFind and WidgetInventory dependency preflight refuses an unloaded Blueprint when its hard-dependency walk reaches a map, redirector, missing registry metadata, or the traversal budget. This preserves the no-map-load boundary. NodeFind's integrated loaded-object path bypasses that preflight only for a valid Blueprint already resident in the editor; its focused inventory test passed. Unloaded coverage remains incomplete and explicitly counted.

Stock Unreal 5.8.4 exposes Find-in-Blueprints indexed search data through `FFindInBlueprintSearchManager::GetSearchDataForAssetPath` and `ProcessEncodedValueForUnloadedBlueprint`. The record can be absent, unindexed, or stale, and the imaginary-data API has a `GetBlueprint()` path that force-loads the asset. Indexed search is therefore a possible **separate, labelled evidence source**, not an exact replacement for a live node/pin read or proof of exhaustive results. A safe investigation must establish index freshness and extract graph/node identifiers without calling force-load paths. Otherwise the response must remain `complete=false` with skipped assets and reasons. Do not weaken the dependency preflight to increase counts.

The field assets and exact original revision are unavailable. The prior full append wrapper failure on its literal-count assertion is retained; the focused inventory pass does not certify that wrapper.

## NOV10-02: one-client policy refusal

The November 10 report says every read call failed in one of six concurrent clients, while the other clients and editor kept working. It does not retain an exact failing tool name, error payload, client toolset list, or transport log. Current `TurboMCPToolPolicy::IsAllowed` has distinct text for an unknown policy entry, a disabled preference, and a recovery latch; none uses the reported phrase "tool policy unavailable." `TurboMCPCallRecorder.cpp` checks the wrapped tool and policy before executing it, but this source review cannot establish whether that client's error came from the plugin, a stale client registration, or the external MCP layer. The earlier NOV09-18 discovery fix only covers known unknown-name and disabled-tool paths. Do not infer a concurrency defect or close NOV10-02 from those tests.

At the next occurrence, preserve one exact request and response (toolset, bare tool name, parameters with project data removed), a contemporaneous `list_tools` result for the failing and a working client, plugin status and tool-policy output, and the matching editor/MCP transport log interval. Compare the same read call in both clients without restarting. This evidence determines which layer can own a bounded repair. No code assignment is approved by this review.
