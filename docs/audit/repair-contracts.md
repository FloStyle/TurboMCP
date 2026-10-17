# Repair contracts from the task review

[Review register](task-review.md) | [Architecture](../architecture.md) | [Roadmap](../../ROADMAP.md)

These contracts describe confirmed boundaries at revision `6aee9dfd92a4c06a73e89c091973f982d715b7a2`. They do not authorize a worker to skip the approved queue or implement every repair. The integration owner assigns one bounded repair after its prerequisites. Preserve original finding IDs when recording implementation evidence.

For every repair: add the stated negative control, show that it fails on the reviewed behavior, implement the smallest change, then run the focused suite through the supervisor. Keep raw evidence local. Record commit, candidate digest, engine revision, loaded DLL identity, exact command and result. Restore owned fixtures. A suite that passes without exercising the defect does not satisfy acceptance. Missing evidence is not a pass.

## Reference direction

Finding: [484d5638](../../todo/484d5638.md). Owner: R09. Contract: ARC11.

**Problem and source:** `TurboMCPRefs.cpp`, `ReadTurboMCPRefs`, computes `bWantBoth` but gates the queries only on `bWantOut` and `bWantIn`. Explicit `both` takes neither branch. `Tools/test-refs.py` sends an empty direction for its combined zero-reference check, so the current passing suite misses this defect.

**Allowed change:** direction selection and its focused regression. Preserve asset resolution, filtering, registry readiness, query result semantics and public signature. Do not expand this repair into a registry cache or asynchronous scanner. If query failure can still look like zero, record that separately; do not call the whole ARC11 audit complete.

**Synthetic reproduction:** use two saved repository-owned assets, A referencing B, with independently verified package edges. Query A and B by short name, package path and object path with `in`, `out`, `both` and empty direction. Use a third asset with no content edges as the genuine-zero control. Do not depend on an optional AudioWidgets engine asset for the new regression.

**Done when:** explicit `both` and empty direction return the same incoming/outgoing endpoint sets as the two individual queries, with the same filtering and counts. A nonzero edge must be present in the comparison. Invalid direction and unavailable/incomplete registry behavior still refuse; no query changes asset state. Check endpoints, not only a `counts:` substring.

**Verification:** extend `Tools/test-refs.py` and run `python Tools/execute-tests.py --profile engine --suites test-refs --execute`. Run the offline gate before integration. Stop if fixture edge availability cannot be independently established.

## Cast lint

Finding: [7bbe9c3c](../../todo/7bbe9c3c.md). Owner: R06. Contract: ARC11 reader truth.

**Problem and source:** `TurboMCPBPLint.cpp`, `RunUnnecessaryCasts`, guesses `AsObject` and interprets a missing pin as no consumers. The pinned engine exposes `UK2Node_DynamicCast::GetCastResultPin`, `GetCastSourcePin` and `TargetType` in `Engine/Source/Editor/BlueprintGraph/Classes/K2Node_DynamicCast.h`. The enclosing Blueprint's class does not establish the type of an arbitrary input object.

**Allowed change:** this check's cast-node inspection and conservative diagnostic rules; its fixtures and assertions. No node deletion, general lint rewrite or new reachability engine. Prefer no finding with an explicit limitation to a claim that a live cast is unnecessary.

**Cases:** a typed cast output feeding a variable setter; object output unused while success/failure execution branches are used; a cast on an arbitrary object to the enclosing Blueprint type; a provably redundant cast; and an unrelated non-cast node. Read actual node/pin state independently. Missing or unresolved type evidence must not be converted to proof of redundancy. Preserve graph and dirty state throughout.

**Done when:** the used typed result and meaningful execution branch are not labelled unused; same enclosing class alone does not imply an infallible cast; a positive redundant case is still identified where supported. The report states the evidence and scope of its conclusion.

**Verification:** extend `Tools/test-bp-lint.py`; run `python Tools/execute-tests.py --profile engine --suites test-bp-lint --execute`. Its existing refusal assertion failed during this review on `Unknown tool EditTurboMCPBPLint`. Handle that as a separately reviewed test expectation: assert an error outcome, the rejected name and no executed mutation, with a negative control that rejects a success response. Do not silence it merely to make the cast regression green.

## Empty graph names

Finding: [c1e04796](../../todo/c1e04796.md). Owner: R06. Decision: D-008.

`TurboMCPGraphPatch.cpp` and `TurboMCPGraphReplace.cpp` each have `FindGraphLocal` returning the first candidate for an empty name. The shared `TurboMCPNodeId` resolver has the stricter policy. Before implementation, inventory all writer and reader callers, including preview paths, because a helper-only change may otherwise produce a generic or late refusal.

Use two distinct graphs whose contents make accidental selection obvious. Empty writer names must produce `graph_name_required` before any transaction or probe changes live state; both graphs and dirty flags remain identical. Named graph behavior remains unchanged. Readers that support an empty name explicitly enumerate graphs, label each result and mark incompleteness; other readers refuse. No helper may return the first graph as an empty-name convenience.

Update the named comment/check/dead-node tests deliberately. Run `python Tools/test-lookup-guards.py` and the focused engine suites listed on the task. Do not make compact graph text a lossless edit format as part of this repair.

## Restricted batch

Findings: [b230e508](../../todo/b230e508.md), then [5b6a8e45](../../todo/5b6a8e45.md). Owner: R05. Contracts: ARC03-ARC09, cases B01-B05.

Split this into reviewed commits with these boundaries:

1. **Transaction ownership:** inspect `FTurboMCPGraphRepoint::ApplyCore`, which currently calls `ApplyTransacted`. Extract an internal operation that cannot start/end/undo a transaction, compile or save. Public repoint keeps its owned wrapper and accepted result contract. Batch owns the only transaction. Prove the public single-operation behavior before integrating the batch caller. Swap and other operation kinds are out of scope.
2. **Input and planning:** keep only `graphRepoint`, one canonical Blueprint and one graph. Validate the root and each operation's allowed fields and types, nonempty unique IDs and exact scopes before mutation. Preserve raw validated operation data so each operation can re-resolve against predecessor state. Do not store a plan of raw UObject pointers across calls. Preview cannot mutate live assets to simulate a sequence; refuse a sequence it cannot honestly plan or use an explicitly designed transient plan state.
3. **Apply and restoration:** revalidate each operation in order under one owner, compile once at the batch boundary against the valid baseline, and never save. Include the failing operation if it partially changed state. Verify the whole original state and dirty flag after restoration. On failed rollback report `failed`, `rollback=failed`, `changed=null`, then block further mutations according to ARC05. Never print `unchanged` merely because Undo was requested.

Cases must cover duplicate IDs, unknown fields/kinds, cross-asset and cross-graph rejection before mutation, a second operation depending on the first, stale targets, an injected failure after a partial edit, failed restoration, new diagnostics with equal diagnostic counts, successful one-undo behavior and unchanged disk bytes. Use test-only failure injection through the real core; do not expose failpoints through public MCP input.

`Tools/test-batch-edit.py` explicitly contains baseline assertions. Replace the affected baseline expectations with the B01-B05 contract as the approved repair lands; preserve the old run as failing-baseline evidence. Run `python Tools/execute-tests.py --profile engine --suites test-graph-repoint,test-batch-edit --execute`. Do not mark the broad [bbb63d1e](../../todo/bbb63d1e.md) multi-asset proposal complete.

## Domain read-back

Owner: R08. These are separate repairs, not one multi-domain change.

| Finding | Scope | Required independent check |
| --- | --- | --- |
| [214d6e7a](../../todo/214d6e7a.md) | Enum names and display-name result counts | Compare actual qualified enumerator names and display names with the plan. Requested array length is not a measurement. Invalid/duplicate names cannot be reported as verified success. |
| [ba6df3c8](../../todo/ba6df3c8.md) | DataTable preview and ordered application | Use set/remove, remove/set and repeated partial sets on one row; independently read exact final fields. Invalid lines are not accepted plan entries. Restore every field on atomic failure. |
| [1256778b](../../todo/1256778b.md) | Widget object-reference overrides | Compare an editable object reference to the correct template/default, including explicit null. Exclude structural slot noise with an explicit rule, not a blanket object-property exclusion. Do not construct a live widget to inspect an asset template. |
| [7bd81b84](../../todo/7bd81b84.md) | Struct make/break usage | Count loaded affected assets using a documented unit, including graph-only make/break use; use the same set in force validation. State that unloaded assets remain outside the scan. |
| [c4485ba2](../../todo/c4485ba2.md) | Override removal | Compare remaining override identities and unrelated graph contents, then prove one undo restores the full state. A removed-target boolean or total count is insufficient. |
| [f434ffba](../../todo/f434ffba.md) | Material constant assertions | Compare each named unconnected input with its exact independent value. The current any-numeric-token assertion cannot prove this. |

Use the commands on each task. Establish synthetic fixtures and ARC03-ARC05 mutation requirements first; no broad shared-parser or authoring expansion is included.

## Save and preview safety

Owner: R05. Source: `TurboMCPCompile.cpp`, `SaveTurboMCPAssets`; `TurboMCPWidgetPreview.cpp`, `UTurboMCPLookToolset::LookTurboMCPWidgetPreview`.

The current save-success branch checks `!bStillDirty` without requiring the engine write result. A clean package flag alone is not durable-write proof. Specify success from engine write outcome plus target-specific verification, and use a clean-fixture reload test. Test a failed write, an unchanged package, a partial multi-package save and a successful write with separate thumbnail/script warnings. Do not claim disk rollback from memory Undo. Preserve both existing explicit save tools; removal of an old optional argument did not remove the separate thumbnail-free tool.

Widget preview constructs a generated widget and may execute a named callback. Apply the existing ARC10 decision: explicit per-call `Execution` denial before construction or callback, and truthful script-capable effects when allowed. Test deny with a synthetic construction counter, allow with the same counter, invalid consent and invalid callback parameters. Do not add persistent hidden consent, claim external effects are reversible or use workplace widgets. Coordinate the signature, live schema, client arguments and tests in one approved change.

## Compile-fixture verdict

Finding: [7de8d185](../../todo/7de8d185.md). Owner: R04/R05.

The native fixture is registered in `TestProject/Source/TurboMCPTest/Private/TurboMCPTestCompileEnsureTest.cpp`. It deliberately queues compilation during another compile and produces ensure evidence. The Python suite tries to distinguish those injected errors from fixture assertion failures. A raw native failure is therefore neither an automatic plugin regression nor an accepted success.

Run the isolated `test-compile-ensure` suite through the supervisor. Verify discovery, both fixture assets, hook execution and actual ensure identities. Retain the complete local test result and expected/unexpected error lists. Test classification with an additional unexpected assertion/error as a negative control. Expected-error treatment must not suppress the log signal being tested. Do not delete the fixture, blanket-ignore failed native tests or announce full native acceptance from a passing Python wrapper.

Transport-envelope investigation is separate: add synthetic JSON-object/string-layer controls to `test-result-contract.py`, then inspect the actual raw baseline payload locally if the historical shape remains unresolved. Malformed metadata and mixed media must not be accepted as a successful plain body. Preserve the no-replay rule for uncertain mutations.

## Trace mapping

Finding: [02674939](../../todo/02674939.md). Owner: R09. Contract: ARC11.

`TurboMCPTrace.cpp` passes zero as the debug bytecode offset. Before implementation, verify the exact pinned FFrame/debug mapping API, offset convention and validity/lifetime; do not guess a subtraction or copy a forward-version snippet. Record source identifiers without copying licensed engine implementation into the repo.

Use two distinct armed nodes executed different numbers of times, an unmappable event and an idle/no-event interval. Separate observed raw events, mapped hits, unmapped events and unavailable instrumentation. Verify arm state across start/end/restart PIE and explicit clear. A zero mapped-hit count with unmapped events must not say the node never ran. Run `python Tools/execute-tests.py --profile engine --suites test-trace,test-play-check --execute` with the new assertions. No recorder redesign or runtime input injection is included.
