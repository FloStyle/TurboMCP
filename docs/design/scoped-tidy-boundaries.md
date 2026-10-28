# Scoped tidy and fixed connections

OCT27-04 repair contract, approved by the integration owner on 2026-10-28.

## Source findings

TurboMCPGraphTidy.cpp initializes the unboxed group beyond every fixed node and box. This ignores connectivity and makes unrelated distant content influence a small edit. Pure-only selections have a consumer-based exception, but retain their old vertical starting position. BetterLayout compares structural measures before total wire length, so an alignment improvement can accept arbitrarily longer boundary wires. The generic rejection text also lists an order that differs from the comparator.

## Required behavior

Place each connected selected component near its fixed connections, preserving its internal execution/data ordering. Prefer incoming execution anchors, then outgoing consumers and data boundaries. Unrelated distant nodes are obstacles only when their rectangles actually intersect a candidate, never a global starting coordinate. Keep all unselected nodes and untouched boxes unchanged. Preserve existing whole-graph layout behavior.

Measure wires crossing the selected/fixed boundary individually and in aggregate, with native pin offsets. Do not allow structural scoring or Force to hide excessive growth. Allow only an explicitly documented small geometry-based clearance allowance; report the limiting wire, measured before/after length and allowance on refusal. Check both planned and actual results; rejected edits must restore positions, links, defaults, box geometry and dirty state. A refusal is a fallback, not a substitute for placing feasible local selections correctly.

Report the actual first failing quality criterion with before/after values. Distinguish safe refusal, no change, accepted layout and incomplete rollback. Keep comments and tool output clear and accurate.

## Focused evidence

Use disposable owned Blueprints with real native measurements. Cover a dense graph with unrelated distant boxes, selected distant connected components, a single inserted execution call, a seven-node function, and an obstructed selection that must refuse unchanged. Assert fixed node/box preservation and bounded boundary wires; inspect before/after editor captures for accepted layouts. Include a small whole-graph control to preserve existing behavior. Retain baseline failures and any fixture corrections. Runtime gameplay behavior is outside this layout task.

## Ownership

Assignment oct27-scoped-tidy owns GraphTidy implementation/header plus a dedicated native fixture. The integrator separately owns catalog, wrapper, help, audit and tracking. DSH remains paused; engine source remains stock. OCT27-05 append placement remains a separate dependent task.

Implemented clearance rule: each crossing wire may grow by the larger native endpoint width clamped to 64..256 graph units, with a 1-unit rounding tolerance. Aggregate growth is limited to the largest individual allowance, not the sum. Planned and actual checks use the same rule, including getter-copy endpoints. Force does not bypass it.
