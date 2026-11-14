# Loaded Blueprint search review

Partial implementation of NOV13-03 in 307de74c840247006fbeb7b02915fcebd49f72b8. Exact two-file scope passed against 244ea9deaf53200d6b83aca2cc6967df8e30e9eb.

NodeFind now unions valid loaded Blueprint assets with registry candidates, filters by exact asset/folder/project scope, deduplicates before cursor binding, and reads loaded objects without running dependency-load preflight. Objects requiring Load/PostLoad and map packages are excluded from the loaded-object route. The existing preflight still protects every unloaded candidate. WidgetInventory already uses loaded assets; no equivalent change was needed there.

Supervised execution 4cb0955a1271404a8e75d7d1f86e1135: stock 5.8.4 build passed. GraphAppend.Inventory passed, including existing empty-event search, unsaved loaded folder discovery, exclusion of a different folder and dirty-state preservation. This resolves the retained inventory-suite failure. Full test-graph-append still FAILED (1): legacy pin-count wording assertion in Tools/test-graph-append.py. Do not call the suite passing. Runner completed fixture restoration and stopped its editor. Suite 181.186 seconds; runner 206.814 seconds.

NOV13-03 remains open/in progress. Unloaded map/redirector-dependent assets remain skipped. The field assets and exact revision are unavailable; two successful standalone reads do not prove absence of map-loading side effects. Do not remove the guard or treat deferred dependency flags as full graph-load safety. Large-project performance and formerly skipped unloaded assets are not validated.

Dashboard HTTP root and /api/state both returned 200 through direct loopback access after the previous request timeout.

## November 14 assertion reconciliation

The historical full-wrapper failure above is retained. Its literal-count assertion was subsequently repaired in `f0eb3b8855d5191a19f610c77007d4c64741a955`, which is already on main. Current source checks accepted assignments equal changed plus already-matching defaults, requires at least one changed default for the unique marker, and independently reads that marker back through the graph reader. Its expected wording matches `TurboMCPGraphAppend.cpp`. Do not schedule another implementation of this same assertion repair. This source comparison is not a passing full-wrapper rerun; full-suite acceptance remains unverified until a scoped execution supplies that evidence.

Subsequent supervised execution `810ccf6e47f9459a9d9b7dc106d218fc` on `f2f2e57e3fc76db8f9adb8171d6361cf51e0aea3` passed the stock 5.8.4 build and the full `test-graph-append` wrapper (one selected suite). Build: 13.828 seconds; suite process: 245.888 seconds; supervisor: 313.023 seconds. All 16 fixture files were restored and the owned editor was stopped; the checkout was clean. This supplies the previously missing wrapper rerun evidence. Preserve the earlier failures and their repairs. Unloaded bulk-search coverage, unrelated suites and public release acceptance remain outside this result.
