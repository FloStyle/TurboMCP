# November 12 node-search integration review

Implemented NOV10-07, NOV09-08 and NOV09-19 in commits d0a1ffc6, 4ba80407 and 25ba1591. Reviewed exact scope from approved base dc47d7552adc8914ee90ad0a855f2f57d5ac6d8b; three allowed files changed. No engine modifications.

NodeFind supports asset, folder and project scope, deterministic registry candidate ordering, any/all matching, optional unlinked input literals, exact class filtering, count-only and continuation. Each call scans at most 16 candidate assets and 10000 nodes, returning at most 40 rows. Project scope includes /Game and enabled project content plugins. Counts are cumulative scan counts, final only when scan_finished is true. Skipped loading/ambiguous assets prevent an exhaustive claim. Continue without changing assets; cursors bind the query and candidate paths, not a frozen graph snapshot. Loading preflights hard dependencies and disables compile-on-load; arbitrary native PostLoad hooks are not sandboxed.

Stored CreateDelegate and multicast delegate member identities are searched. NodeContext accepts complete reader IDs without redundant selectors, refuses conflicting asset/graph selectors and labels direct input source titles. No runtime dispatch is inferred.

## Evidence and failure history

Initial execution 7b44f22e5cc04f1fa7e7084f5ce4d8b7 built successfully but failed the first MCP call: Unreal exposed empty-string defaults as required fields. Commit 25ba1591 uses explicit any/first defaults, preserving existing three-argument calls. The failure is retained, not replaced by the passing rerun.

Execution e55f4f5772e14b89bf39825cd100c708: stock Unreal 5.8.4 build passed; test-node-reader-output passed (1.176 seconds suite process, 25.727 seconds runner). DLL SHA prefix 74FEFF7DC185128B. Focused checks cover existing calls, any/all, invalid mode, class mismatch, one-row pagination, duplicate-free IDs, count-only totals, stale-query cursor refusal, folder response, full-ID context inference and conflicting graph refusal. Sixteen tracked fixture files and the temporary editor preference were restored. No full regression run.

## Remaining validation flags

Dedicated delegate bind/unbind, literal-value and one-level source-title fixtures remain untested. Large-project and project-plugin enumeration, scan-budget continuation beyond 16 assets/10000 nodes, load-refusal cases and real-project performance remain untested. Folder smoke checks do not certify all these paths. These are validation flags on implemented tasks, not claims of field acceptance. Native implementations, runtime delegate targets and inherited assets outside the query scope remain outside the search contract.
