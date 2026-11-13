# Loaded Blueprint search review

Partial implementation of NOV13-03 in 307de74c840247006fbeb7b02915fcebd49f72b8. Exact two-file scope passed against 244ea9deaf53200d6b83aca2cc6967df8e30e9eb.

NodeFind now unions valid loaded Blueprint assets with registry candidates, filters by exact asset/folder/project scope, deduplicates before cursor binding, and reads loaded objects without running dependency-load preflight. Objects requiring Load/PostLoad and map packages are excluded from the loaded-object route. The existing preflight still protects every unloaded candidate. WidgetInventory already uses loaded assets; no equivalent change was needed there.

Supervised execution 4cb0955a1271404a8e75d7d1f86e1135: stock 5.8.4 build passed. GraphAppend.Inventory passed, including existing empty-event search, unsaved loaded folder discovery, exclusion of a different folder and dirty-state preservation. This resolves the retained inventory-suite failure. Full test-graph-append still FAILED (1): legacy pin-count wording assertion in Tools/test-graph-append.py. Do not call the suite passing. Runner completed fixture restoration and stopped its editor. Suite 181.186 seconds; runner 206.814 seconds.

NOV13-03 remains open/in progress. Unloaded map/redirector-dependent assets remain skipped. The field assets and exact revision are unavailable; two successful standalone reads do not prove absence of map-loading side effects. Do not remove the guard or treat deferred dependency flags as full graph-load safety. Large-project performance and formerly skipped unloaded assets are not validated.

Dashboard HTTP root and /api/state both returned 200 through direct loopback access after the previous request timeout.
