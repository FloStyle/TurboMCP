# FT-166: batch reference reads

Readiness: ready
Priority: P0
Owner: Sol implementation; parent integration

User-approved repair, 2026-11-10. Implement first to unblock the field audit. Links: plan-2026-11-10.json NOV10-03/04/05; RF-10/11/12, X-134, FT-166.

Current source review: TurboMCPRefs.cpp uses registry edges, limits each direction to 60 entries and truncates at 20000 characters without a continuation cursor. Preserve incomplete-registry warnings and no-mutation behavior.

Implement ReadTurboMCPRefsBatch: assets OR folder+recursive+optional class filter; referencers/dependencies/both; depth 1 default, at most 6; include hard/soft/searchable-name/management kinds; compact mode, stable cursor and total count. Return seed and related package/class/kind, missing-on-disk information with script/virtual-package distinctions. Memoize registry adjacency/metadata in one tool invocation; avoid per-seed client calls. Bound cycles and output without silent omission. Expose partial results honestly. Optional stop-at-root/root chains must be evidence-backed; registry cannot prove all dynamic or config references. Never invent a root or claim exhaustive runtime impact.

Extend existing Refs with kinds and real pagination beyond 60 while preserving compatible existing arguments. Review tool schemas, help, English prose and comments. New API remains read-only: no save, checkout, map load, PIE or asset mutation. Do not implement arbitrary read-only Python filtering.

Focused validation: inspect pinned engine APIs; build once after coherent implementation; synthetic fixture tests for folder boundaries, both directions/kinds, >60 results, paging without duplicates/missing records, cycles, invalid cursor/depth, incomplete data and no mutation. Measure 700-seed depth-1/depth-2 duration and calls; report actual results, target depth-2 <60 seconds, do not invent a baseline. Reuse Tools/test-refs.py. Parent registers new test catalog entries if required; request that immediately while continuing independent implementation. No full suite or repeated builds absent a new failure.

User explicitly requests the existing C:/ExampleDev/TurboMCP checkout. Sole plugin implementation worker; DSH paused. Serialize engine/build access using repository lease helpers. Do not touch another project's editor. Commit scoped changes; return SHA, evidence and precise remaining limits. Parent owns tracker/catalog updates and final push/review. Stop after this P0 deliverable, not after merely writing a plan.

Previous NOV09-18 worker has ended and its scoped commit 3829a44 is retained pending final integration review. Releasing its claim transfers exclusive checkout ownership; it does not certify completion.
