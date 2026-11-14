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

## NOV05-16: placed widget class replacement

Source reconciliation confirms that the existing tree-sync `replace` verb is a subtree rewrite, not an in-place class conversion. Its public contract lists removed widgets and warns that its text is not a lossless backup. It cannot satisfy WT-17/FT-143 merely because it uses the word replace. The report requests preservation of the widget name, parent slot, sibling order and compatible properties; remove/add already failed those expectations in the field. This remains an open feature.

The implementation contract must include a preview of copied and incompatible properties, child-capacity compatibility, variable and bound-event references, animation bindings, and an explicit refusal list before mutation. Apply must revalidate that preview, preserve parent slot and sibling order, and provide exact undo without saving or compiling automatically. A plain Button-to-framework-button case and a ListView-to-dynamic-entry-box case require separate compatibility decisions: collection entries are not ordinary panel children. Unsupported conversions must be disclosed as unsupported, not counted as implemented capability. The next step is a bounded design/source review of Unreal's widget replacement APIs and reference repair before approving an implementation scope.

### Engine API review

The local 5.8.4 source exposes `FWidgetBlueprintOperationUtils::ReplaceWidgetWithTemplate`. Its preflight checks widget-tree identity, circular references and the parent BindWidget property's class requirement. It then calls `ReplaceWidgetsWithTemplateClass`, followed by `FixupWidgetBlueprintReferences` for a variable widget. The inner helper exports/imports properties, replaces the parent child or root, transfers panel children, renames the old object into the transient package, deletes it, marks the Blueprint structurally modified and replaces variable references.

This helper is not an atomic preservation contract. It creates the replacement before checking every reparenting outcome, returns false from paths that have already created objects, and explicitly permits incompatible child-slot properties to be silently dropped. The transfer loop does not establish replacement panel capacity before moving children. These findings rule out directly wrapping this API as a safe preview/apply tool. Before implementation, specify capacity and property compatibility preflight, graph/event/animation reference validation, and verified rollback for each mutation stage. Verify structural-modification side effects against the separate-compile contract. No runtime conversion was attempted and no engine source was changed.

## NOV09-05: confirmed row-union cause and repair contract

The archived TD-76 observation requests two wraps in a 15-node chain of test/optional-action repeats. Both the branch bypass and action output enter the next repeat. Current ArrangeRows excludes only wires whose source is in WrapAfterNodes. Its remaining undirected row union follows the bypass, reconnecting the action and continuation into one row. Final WrapAfter validation then refuses. This is a concrete source explanation of the reported refusal; no new editor reproduction is claimed.

This can be repaired independently of tail shifting: WrapAfter already applies only to selected movable nodes. Remove NOV09-02 as an implementation dependency for this item; retain existing fixed-node, comment and collision boundaries.

Approved contract: represent a requested wrap as an explicit set of execution edges entering one common continuation. Find the finite optional-action region containing the requested action, including its branch bypasses. Require every forward exit from that region to reach the same continuation; reject cycles, feedback, conflicting exits, ambiguous anchors or an incomplete selected region. Separate rows using the complete edge boundary, not by adding the branch to WrapAfterNodes (that would also cut its action path). Preserve the action path on the earlier row. Multiple requested wraps must compose deterministically. Keep original node-ID selection and public parameters compatible; disclose the resolved continuation and bypass boundary in preview. All affected row-ordering and row-break scoring must use the same boundary. Never rewire, remove or synthesize execution links to obtain the layout.

Focused acceptance: reproduce three optional-action repeats with two requested wraps. Assert every bypass and action link survives, next-repeat execution pins occupy the lower row, preview/edit coordinates agree and undo restores original geometry. Include conflicting-exit and partially selected-region refusals with no mutation. A plain linear wrap remains supported. Review one native screenshot of the repeated-chain result; no full cascade rerun is required for this slice.

### NOV09-05 integration evidence

Reviewed candidate `3e0c774ef8a9416b36ecccca11ff452513753d1c` against approved base `c787313e27bcf23a29d0ebf509e7aecb3084d464`, assignment `nov14-converging-wrap`. The planner now separates all incoming edges at an approved common continuation, preserves execution links, and reports resolved boundaries in preview. Existing scope/comment/collision guards remain.

Execution `3b655a5ed8104ef7994a6b56f3e95a60` passed the build and selected `test-graph-append` wrapper. Native TidyFeedback exercised three optional-action repeats with two wraps, exact link preservation, planned/applied geometry agreement, transactional undo, incomplete-selection refusal and conflicting-exit refusal. The native OptionalRepeats screenshot was inspected: three separated rows, aligned branch/action pairs, readable labels and no overlapping nodes. This is a six-node synthetic fixture, not a replay of the private 15-node graph. Plain linear wrapping was not given a new dedicated assertion in this slice.

Earlier run `c2ffa92ffefc41088be01c7da2e912f7` failed five undo assertions: the newly created fixture nodes and graph lacked transactional flags. The fixture was corrected without weakening the assertions. The later successful run does not erase that failed evidence. No engine changes were made.

## NOV10-11: supported multi-target self pins misclassified by the reader

Source review found a concrete independent defect: RenderPinValue in TurboMCPGraphText.cpp rejects every non-execution input with LinkedTo.Num() != 1. Stock 5.8.4 EdGraphSchema_K2::DetermineConnectionResponseOfCompatibleTypedPins explicitly permits multiple scalar self connections when the owning UK2Node allows multiple selfs and no array source is connected. Therefore a legitimate multi-target call can be labelled malformed. The one-time field report supplies neither exact endpoints nor the number of sources, so this is a plausible cause, not confirmation of that original graph.

Approved next repair contract: render all valid scalar target endpoints for engine-supported multi-self calls as clearly labelled inspection-only evidence; preserve endpoint order and exact identifiers. Validate every reciprocal wire and reject null, duplicate, same-direction, cross-graph and unsupported multi-value inputs. Do not make this observation appendable syntax or silently choose the first target. Keep existing single-target output unchanged. Add a synthetic engine-schema-created multi-target impure call fixture, verify both endpoints and no malformed diagnostic, and retain malformed ordinary-value coverage. Verify reader calls do not mutate links or dirty state. Review help/output wording and disclose that this does not establish the cause of the historical field observation. No engine edit is needed.

### Multi-target repair evidence

Candidate `4a275905` implements the supported scalar multi-self case as inspection-only text with exact endpoints in connection order. Single-source syntax is unchanged; malformed and unsupported multi-source values retain their diagnostic. No public parameters changed. The fixture creates two targets using the native schema, connects the call to its function entry, and checks endpoint reporting, unchanged links and dirty state.

Build and `test-graph-text-guards` passed in `ca280e86c5de40f88fcd2f1dd7f3fd67`, including `TurboMCP.GraphText.SharedValueRepair`. Preserve earlier failures: `4409c654c0814c2c9cf098b492a5b54c` exposed a non-static schema API call corrected before runtime; `0300dbf08f8e4f5f8e4b67ee7c40b4b7` failed the new fixture because its call was unreachable until connected to the function entry. Assertions were retained. `539004b607c84a1bbdfb15a9e657777c` passed the append wrapper but did not run this reader fixture and is not reader acceptance evidence.

The concrete reader defect is implemented and tested. NOV10-11 remains an unresolved historical investigation: the original field graph and exact endpoints are unavailable, so this does not prove its cause. No additional test coverage is claimed for every malformed target variation.


## NOV09-04: section growth source reconciliation

At revision 64de766c, ArrangeRows groups execution components and attaches upstream pure expressions, then stacks the resulting rows vertically. The existing-box loop calls it at the original comment's padded top-left. Only afterward does it enlarge PlannedWidth and PlannedHeight to fit those positions. There is no obstacle-aware choice between additional rows and additional columns. This explains why free space to the right does not currently help a vertically blocked arrangement; the private 65-node graph has not been replayed.

Scope output also incorrectly says all comment boxes remain fixed anchors. Selected complete boxes can already be resized through bWouldFit and the comment apply loop. Repair the wording to distinguish fixed top-left anchors, resized selected boxes, and untouched surrounding boxes. Do not describe a rolled-back or partially restored result as unchanged without the existing restoration checks.

Next implementation contract: retain the original top-left and exact outside-selection geometry. Plan a deterministic rightward alternative when the normal selected-section arrangement would hit fixed geometry. Treat connected execution paths and their pure inputs as coherent groups; explicit wrap boundaries must retain their requested vertical order. Check the complete grown comment rectangle against unrelated fixed sections, not only its member nodes. Nested comments and single connected branching groups must be handled explicitly; packing disconnected rows alone is only a partial implementation. Never silently move neighbors, change links, or reduce collision clearance. Compare candidate layouts before applying, expose resized bounds in preview, and preserve exact undo. If no valid alternative exists, refuse with the blocking section/node identity and explain the required additional scope.

This section-growth repair can be implemented independently of NOV09-02: it rearranges an explicitly selected complete section, whereas NOV09-02 grants controlled movement to existing append-tail nodes. Remove that dependency; it does not authorize either operation to widen the other's scope.

Focused acceptance must include a fully selected boxed section with a fixed section directly below and free rightward space, a connected branching case, explicit wraps, nested comments, and an obstacle to the right that causes a no-mutation refusal. Verify preview/apply agreement, outside-node and outside-box geometry, links, and undo. Inspect one successful native capture. Keep unsupported shapes or unproved field equivalence recorded; do not close the whole record on a disconnected-row-only fixture.
