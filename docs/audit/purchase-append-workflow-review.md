# Minimum-quantity append workflow review

Status: provisionally accepted for this scenario. This is not full Append certification.

## Expected behavior

Extend CalculatePurchase so CanAfford requires sufficient funds and Quantity >= MinimumQuantity. Keep FinalPrice unchanged. Preserve existing nodes, positions, defaults, comments and unrelated functions. Only the old CanAfford connection is replaced; its old affordability output feeds AND instead.

MinimumQuantity must already exist. Append does not change an existing function signature, and FunctionAdd creates a new function rather than updating one. The test creates this unused input as fixture setup; it does not claim a tool performed that step.

## Model procedure

1. Read the Blueprint overview and exact function. Use live IDs and inspect CanAfford's current source.
2. Discover >=, AND Boolean, Quantity and MinimumQuantity. Select the returned GreaterEqual operator by meaning, not simply the first ranked result. Inspect pins. Promotable operators get their numeric type from typed links.
3. Use nearby function-input getters for Quantity and MinimumQuantity. These read parameters, not mutable local storage. Submit four new forms and an existing Return reference with only CanAfford connected to the new AND output.
4. Preview the exact text. Check node count, both wire endpoints, placement and refusals. The preview is not a reservation; edit replans against current state.
5. Apply once. Repeating new forms creates duplicates; it is not an idempotent retry. Read back before deciding what to retry after an error.
6. Compile explicitly and execute independent cases. Inspect actual native capture, not only numeric overlap counts. Save remains a separate action.

## Evidence

Candidate 2193c8624303f32c2f0da7916c0af9b7025a4599. Supervised build and test-purchase-workflow passed in run d144f02447e34727a8f460ab1d118bfb, with no native warnings or errors. The test creates and disposes an unsaved unique Blueprint. It never modifies workplace assets.

Four independent cases cover equality with sufficient funds, quantity below minimum, insufficient funds and quantity above minimum. Price remains Quantity * 45. Preview and malformed/missing-pin refusal preserve graph text, positions, count and dirty flag. Apply creates exactly four nodes; existing nodes, defaults and positions survive, old links remain except the intended replacement, and the unrelated function is unchanged. Native measured rectangles have no new overlaps. The actual final PNG was inspected.

The earlier two-operation version also passed semantics (66a3139ded6a464fb65c9d46181f1db5), but had long entry wires. The final scenario deliberately includes two input getters for readability; this is an explicit four-node plan, not unexplained node growth.

## Confirmed fixes

- Labels after a multiline closing parenthesis were silently ignored. Append now handles them with the same label parser as other supported comment positions.
- Preview wire lines omitted the target node. They now identify both endpoints, using local labels or line/type references and existing IDs.
- Public comments incorrectly guaranteed rollback and preview/edit equivalence. They now describe current-state replanning, attempted undo, unverified restoration, and explicit compile/save.
- Pure connected additions were treated as detached and placed beyond Return. They now use the existing data consumer as their placement anchor and reuse dependency ordering and obstacle avoidance.
- Node discovery called AND not pure by inspecting an unconfigured class default. It now uses the associated function's BlueprintPure flag.

## Limits and remaining work

The existing test-graph-append suite failed in run 2233299766164635bca3a2e8abe0b90d. Its reader calls omit required revision/entryFilter arguments; several assertions expect detached execution nodes that current preflight rejects, and some refusal wording checks are outdated. Its native AliasCollision case passed; its Placement case failed. Preserve the failures and repair that harness separately; do not weaken product validation to satisfy it.

No save/reload or injected live rollback proof is included. Arbitrary mixed execution/data blocks, multiple fixed consumers and all overloads remain outside this review. Fixed original geometry can require curved connections. The earlier Subtotal wire/comment crossing (WF-BP-01-WIRE-COMMENT) is unchanged and still open.

Progress: 22 of 125 inventory tools now have these bounded workflow review records. This does not mean 22 globally certified tools or 125 completed tests.
