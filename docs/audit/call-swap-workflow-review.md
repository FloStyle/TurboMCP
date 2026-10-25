# WF-BP-04: Replace one calculation call

Status: implementation and bounded native scenario passed; user visual acceptance pending.
Candidate: 38ef353. Native build/run: 0cc5d7d515d3425bbee0d9cf300820ed.

## Scenario and tool roles

A disposable, never-saved Actor Blueprint contains Purchase and Refund event flows plus a separate Audit graph. Purchase incorrectly adds a connected value of 42 to a literal 2. The repair replaces only that call with Multiply, then explicitly tidies its two-node expression.

BlueprintLoad selects the graph; GraphLoad, NodeFind and NodeContext identify the precise target among similar calls. NodeType discovers the replacement and NodePins describes it. SwapCall preview explains migration; SwapCall edit uses KeepOld=false to remove the old call. Scoped GraphTidy cleans the affected expression; Compile and Capture verify behavior and appearance. Usage across workflows is recorded in workflow-tool-coverage.json.

## Fixes found by this scenario

- Discovered promotable operators begin with wildcard pins. SwapCall now resolves an engine overload from mapped old pin types before validating defaults and connections. It refuses implicit conversions.
- The hidden, unconnected receiver of a static function is implementation context, not a user default that must transfer to the operator.
- Help and preview distinguish retained disconnected nodes from removal, explain current-state replanning, and state that compile/save are separate operations.
- Failure output no longer calls an undo attempt complete restoration. It reports unverified restoration and asks the caller to inspect before retrying or saving.

## Evidence

The native fixture asserts non-mutating inspection and previews; eight refusal calls preserve graph state and dirty flag. The successful swap preserves the connected input, literal 2, output consumer, positions and every other captured link/default/node. Old identity is removed without a leftover node. The snapshot comparison normalizes identity before sorting.

Scoped tidy preserves node count, links, defaults and all unselected positions. The source precedes Multiply and Multiply precedes its consumer. Compiled execution produces Purchase=84, Refund=30 and Audit=300. The final native capture was inspected: readable glyphs, forward data flow, no visible overlap or leftover old call.

The implementation, public descriptions, helper logic, comments and recorded tool responses were reviewed. Result text reports the old/new IDs, migrated-link count, preserved defaults and compile/save state. The wrapper requires a successful native result, no warnings and exactly one fresh transcript.

## Limits

This is a limited scenario, not exhaustive tool certification. KeepOld=true was previewed but its edit path was not exercised. Positive pin_map/new_inputs variants, split pins, latent/impure calls, mixed types, injected rollback and persistence remain unverified. Fixture construction uses Unreal APIs and is not counted as tool coverage. Swap preserves layout; tidy is a separate, explicit workflow step. The earlier purchase comment-crossing wire remains separately open.

The first native run exposed the operator defect; a subsequent snapshot failure was ordering-only and was repaired without relaxing preservation assertions. The final run, including tidy, passed.

Offline gate 13b682a48d97402ab8e0a2f6dfa7243d: 22/26 passed. Existing validate-repo, check-language, test-client-retry and test-lookup-guards failures remain; this is not a clean repository-wide gate.
