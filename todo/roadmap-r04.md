# R04: Audit callable behavior and result contracts

ID: R04
Status: todo
Priority: P1
Readiness: ready
Owner: unassigned
Design: docs/design/protocol.md
Kind: investigation
Review: source-reviewed

## Current purchase workflow review (2026-10-25)

Scenario, named-local/reader repairs and getter direction/order are implemented with focused native evidence. WF-BP-01-WIRE-COMMENT remains open: the subtotal wire crosses the new annotation. Full per-tool acceptance remains open. See [the review and reproducible procedure](../docs/audit/purchase-workflow-review.md) and [per-tool status](../docs/audit/purchase-workflow-review.json). R04 is not complete; 125 inventoried tools are not 125 accepted tools.

## Approved getter-direction repair (2026-10-25)

User screenshot accepts the remaining purchase layout but rejects local getters stacked above their consumers. Preserve a forward-facing consumer for the original shared getter; place copies left of their own input. Do not change graph values or links beyond the existing safe getter split. Scope: GraphTidy and PurchaseWorkflowTest. Verify with supervised test-purchase-workflow, geometric getter direction assertions, runtime purchase results and inspection of the final PNG. Preserve preview no-mutation and comment/execution layout.

## Approved purchase append review (2026-10-25)

Review Preview/Edit GraphAppend through a minimum-quantity purchase rule. Fixture starts with an unused MinimumQuantity input because Append cannot edit an existing function signature. Discover node types/pins; preview and append comparison/AND nodes, replacing only CanAfford. Verify three outcomes and unchanged price, exact existing node/position/default/link preservation except the intended connection, preview/refusal non-mutation, clear output and actual capture. Review implementation, public descriptions and comments; repair confirmed defects within assignment scope. Run supervised test-purchase-workflow and record limitations; no global tool certification.

## Approved append follow-up (2026-10-25)

Native append run 66a3139ded6a464fb65c9d46181f1db5 passes semantic/preservation checks. Captured pure nodes are placed beyond Return because only exec attachments anchor placement. Anchor wholly pure blocks to their existing data consumer using existing dependency/collision planning; preserve all existing positions. Discovery reports AND as not pure because it reads an unconfigured class default: use associated function purity. Add consumer-side placement and discovery/pin agreement assertions to the same workflow. Run supervised purchase suite, inspect final PNG and record residual layout limitations.

## Append review outcome (2026-10-25)

Implemented label, preview endpoint, pure placement and discovery purity fixes. Native purchase extension passes with exact preservation, runtime values and geometry checks; see [review](../docs/audit/purchase-append-workflow-review.md). Broader Append regression failed due stale reader/fixture expectations and remains validation debt, not a product feature. Existing function-signature editing is an unsupported workflow prerequisite, not claimed implemented.

## Problem
For each tool record actual callable schema, side effects, supported scope, refusal behavior and test evidence. Distinguish declared, definition located, source reviewed, runtime observed and release accepted. Start with result-envelope double wrapping and invalid numeric/default parsing. Do not invent a new public result schema.

## Scope
All 101 rows of docs/audit/capabilities.json, declarations, wrapper definitions, schema/result helpers and matching tests. Review one tool family at a time.

## Dependencies

R16

## Acceptance
- Every tool has an explicit audit status and a source/test reference; none disappears from the inventory.
- Runtime calls use owned disposable fixtures; execution-capable Read/Preview tools are identified explicitly.
- Each confirmed defect maps to one bounded repair with observable acceptance, not an unbounded refactor.

## Verification
- `python Tools/audit-inventory.py --check`
- `python Tools/test-result-contract.py`
- `python Tools/test-client-retry.py`
- `python Tools/execute-tests.py --profile engine --suites test-uncovered-tools,test-batch-tools --execute`

## Stop conditions
Do not expand into another domain. If a prerequisite is absent, record blocked with the exact missing input. After two identical unexplained failures, stop repeating the command and diagnose. Do not run a full engine rebuild without evidence that an incremental build cannot answer the question. Do not change public schemas, weaken safety contracts or start deferred features.

## Deliverable
A reviewed specification and evidence record for each confirmed repair, with duplicates linked and unsupported historical claims withdrawn explicitly. This investigation does not authorize implementing every finding.

## Completion evidence

Compact panel first pass accepted at worker abb84d6143f0784289423f428d4ba4237f38204d, integration f071e94. Scope/source review and supervised pinned 5.8.3 build, test-panel-history and test-panel passed in d99d42df17824a44b9467579f9b447e4. Monitor/Tools separation, pinned inspector, actual control visibility and geometry at 400x650 and 1000x700 are tested. This is bounded functional/layout acceptance, not visual sign-off: rendered text, popup placement, keyboard navigation and scrollbar appearance remain unverified.

Widget persistence contract reviewed at ea6d995b603d22ef0f8a7918fc639199ebe99d8e with integrator clarification that invalidated weak identities remain invalid. Scope and offline structure pass; worker full offline report 5b5c5883525240f3b68007c7bd57cb54 passes. Authorizes a bounded test implementation only: no persistence runtime evidence yet. Unload/global effects, scoped failure-fixture discard, SaveOnCompile restore and conditional recovery are explicit. Native execution remains gated by integrator source review.

Widget post-write compilation accepted: worker cbede8e4087cabe8f51e55cf326d32291a5a9ac8, integration 227bcec. Source and full-base scope passed. Supervised pinned 5.8.3 build and test-widget-roundtrip passed in 8e886eb23ec04542a0216f14001857f7; final validate-repo passed after inventory refresh. Both literal and connected cases compile with zero errors and a Widget CDO, reacquire graph/node identities, and verify fresh reader evidence. Cleanup uses current Blueprint graphs, including early-return paths. Worker offline 25/26 passed; sole stale inventory was refreshed by integrator. Unsaved synthetic cases only: save/reopen durability and broader node families remain unverified.

Connected-input Widget acceptance: worker 0ae628e2b466fb1fc803b41a8af9c0b2bc4aeb36, integration 646d7d2c487ad005b4938496df8196bd8ba9b000. Independent source/scope review, final structure and wrapper self-check passed. Supervised pinned 5.8.3 build and test-widget-roundtrip passed in adc76e414e2b486a81b767e0f367ffc0 (53.515 seconds; 16 files restored). Shared Concat_StrStr source is derived from emitted text; exact new data/exec links, identity/state preservation and invalid-source Plan/Apply refusal are tested. Post-write compilation, save/reopen and other types remain outside this evidence.

Monitoring correctness accepted: worker 06bc26a, integration 5d6de07. Offline gate e942950189ea46f08af74dab7b8dfc31 passed before the include-only correction; final structure passed after it. Supervised pinned 5.8.3 build, test-panel-history and test-panel passed in 6512253ae18543459cab9c961785ea93. Earlier build run 1b7a8f99ffbe4cef8931a1452f1c8374 remains failed evidence. Capped-write and clear/refill refresh, separate unknown counts and unobserved completion are covered by isolated sequential tests. Concurrency and visual UX are not certified.

Widget targeted reader/append/read-back acceptance: worker e60a0dd94d2c7b2640ffbbde90e294b5553b80ae integrated with catalog at 649efbbe71ed5eb7ea91844d00199b15babc8e68. Independent scope passed. Full offline gate 9c472ed87a7e44bbaa294ddca28da0be passed. Supervised pinned 5.8.3 build and test-widget-roundtrip passed in 64589b84a6e84258a04bbee4e711b9dc (375.803 seconds; 16 content files restored). Test derives the append identity from production reader output, checks exact new PrintString/link and fresh form, and preserves existing node/widget/tree/slot identities and state. Refusal and malformed extraction controls pass. This certifies one synthetic in-memory case only, not all nodes, save/reopen or public release readiness.

Integrator accepted parallel-claim candidate 8856969ba9998e9c34964d0cbc25a0e3b0fa9960 after independent 14-test claim regression, worker-scope regression and full offline gate. Canonical aliases refuse before mutation; exact legacy fixture no longer depends on private Git history. Production migration and disjoint dispatch are separately applied by the integrator. This accepts coordination helpers, not engine concurrency or R04 completion.

### Integrator acceptance: Widget append and Plan isolation

Accepted repair candidate `9e6878b038ca14b64c0a399eef9edd097f9151e6`. Plan now probes on the existing transient scratch-graph path instead of modifying the target graph; no user dirty-state restoration masks mutation. Widget fixture independently verifies clean/repeated/already-dirty Plan, refused Plan/Apply, exact appended node/link, existing layout/defaults, sibling graph and widget-tree preservation.

Pinned 5.8.3 supervised run `754dc0a636da405895b7237e7f3eeb36` passed build and Widget acceptance with no errors or warnings; sixteen files restored. Append regressions passed in `afc09af42ce44ffeac3c9526784f9d14` at `1705e181baa812e22016d27b59dd5b6323fced19`; the later correction changed only Python reporting and evidence. That earlier overall run failed due to an HTTP warning and reporting bug and is not represented as wholly passing. Integration offline run `486c21d562d04231af95fb77f5ab28b5` passed after merging current main documentation and refreshing only capability plugin identity.

Fixtures are unique never-saved packages, deliberately non-transient to exercise dirty tracking; retirement is not proof of garbage collection. Full read/write round-trip, save/reopen, all spawner classes, measured visual overlap and installed-engine support remain unverified. R04 remains incomplete.

### Integrator acceptance: shared-value reader

Accepted candidate `83d3443f03eadcbf5a8353df400b47119b3672c4` from base `315a264a83bfe63d222e59e4cd149a890d7f4a13`; trusted five-file scope passed. The first complete shared pure value is rendered inline; subsequent references retain the exact output pin. Partial, unsupported and malformed forms cannot become reusable definitions.

Supervised pinned-5.8.3 engine run `dfa84d6a98d842d2b9ba55b4b9c52afa` passed build and `test-graph-text-guards`. `TurboMCP.GraphText.SharedValueRepair` returned Success with no errors or warnings. The supervisor restored sixteen files. Candidate engine tree digest: `2621352f3c3c2007b45fbbd45bdcf822555b233efdc6127f8b55ec8eabc798ba`. Offline run `e1587e485d2f4e108c68a954da1a1f5c` passed after the integrator refreshed only the generated capability plugin identity (101 tools unchanged).

Earlier failed fixture runs remain local evidence; they were corrected without weakening the definition-count or exact-output-pin assertions. This acceptance covers the tested reader cases, not a complete reader/writer round trip, macro/no-entry completeness, Widget Blueprint acceptance, saved assets or fresh reopen. R04 remains incomplete.

### Widget Blueprint append acceptance (R04-WIDGET-APPEND-01)

Task R04, Widget Blueprint append acceptance and Plan probe isolation, claim `f985a11bf00346cda05354da4d36fa4e` (owner `dsh-r04-probe-02`). Approved base `46c27ad19e46c1d92e6fdd56ac2942b0a45e72da`; branch `task/r04-widget-append-acceptance`; worktree `C:\ExampleDev\SampleRoot005\conhost-integrator`. The candidate is the commit containing this evidence; its full SHA is returned in the submission message. This slice is not accepted and does not certify persistence, save/reopen or editor-session behavior.

Changed files (the recorded five-file allowlist; nothing else):
- `Plugins/TurboMCP/Source/TurboMCP/TurboMCPWidgetAppendAcceptanceTest.cpp` (new): unique never-saved Widget Blueprint fixture with CanvasPanel root, `TitleText`/`StatusText` variables and non-default Canvas slots, a function graph with entry and two sequential PrintString nodes with forced positions/GUIDs, and an untouched sibling function graph. It snapshots target and sibling graphs, exact node/pin identity, widget child order, variable flags and slot values, and dirty state; drives the real Append Plan and Apply on the intended text and on a refused plan; checks exactly one new PrintString with `InString "appended"`, one new link and deterministic placement. Negative controls run on a separate disposable Widget fixture so comparator mutations cannot contaminate the acceptance fixture; they cover position, default, link, sibling node, slot Z-order and variable flag, with restore verification. Cleanup reports marked-garbage retirement and rooted-object limits rather than claiming collection.
- `Tools/test-widget-append-acceptance.py` (new): invokes the exact compiled automation through `AutomationTestToolset` and strictly parses one matching Success entry with no errors or warnings.
- `Tests/catalog.json`: adds only the `test-widget-append-acceptance` engine suite; all other entries preserved.
- `Plugins/TurboMCP/Source/TurboMCP/TurboMCPGraphAppend.cpp`: Plan now resolves each new-node form with `FTurboMCPNodeType::BuildProbeNode`, which invokes the same filtered action entry in a throwaway `RF_Transient` graph. The probe is never added to or removed from the real graph; it is retired with its throwaway graph. Action filtering, resolved class/pins, refusal notes, link/placement planning and Apply's real spawn/transaction path are unchanged.
- This Completion evidence only.

Checks run in this worker: `python -m py_compile Tools/test-widget-append-acceptance.py` exited 0; `python Tools/test-widget-append-acceptance.py --self-check` exited 0 while recording its expected failure; `Tests/catalog.json` parsed as JSON; `python Tools/validate.py` exited 1 with 25 of 26 checks passing and only `validate-repo` failing at `Tools/audit-inventory.py --check` with `Stale capability inventory`. The generated inventory was not regenerated because it is integrator-owned. The trusted base scope gate passed with exactly the five allowed files. Supervised compiler run `033bd4d8c7084847a5ed7e4fd64a98c4` failed before editor execution with three C2065 errors: `BS_Successful` does not exist in the pinned `EBlueprintStatus` declaration. The correction uses `UBlueprint::IsUpToDate()` (the pinned status predicate for `BS_UpToDate`/`BS_UpToDateWithWarnings`) while retaining the zero-error requirement. Supervised run `d2d9c2f57ea741d5a0753a2e4a2df669` then built but failed automation with a handled ensure in `WidgetBlueprintCompiler.cpp:781`: `StatusText` had no GUID. The owned editor log places the ensure inside `FWidgetBlueprintOperationUtils::AddWidget` (line 430), which calls `MarkBlueprintAsStructurallyModified` and can flush compilation immediately. The fixture had constructed both TextBlocks before adding the first, so the intermediate compile saw the not-yet-added StatusText. The correction constructs, adds and toggles each widget in sequence, and asserts each GUID before the next construction. Supervised run `023d46b087ca473ba4f62e7d15deee36` then built and executed with exactly one automation error: `Apply marks the fixture dirty` was false. Pinned `UObjectBaseUtility::MarkPackageDirty` returns early when any object in the outer chain is `RF_Transient`; the fixture had marked both package and Blueprint transient, suppressing the intended dirty observable. The fixture now uses unique, never-saved `/Game/Tests/TurboMCPWidgetAppend_<guid>` and `TurboMCPWidgetComparator_<guid>` packages without transient flags, so Apply marks the package dirty and cleanup only marks the owned objects garbage and clears the dirty flag without saving. Supervised run `88712b5cef5448d490069b70692edcb0` then executed with exactly three errors: the first Plan, refused Plan and refused Apply all left the fixture dirty. Source tracing establishes a production Plan defect: `FTurboMCPGraphAppend::Plan` calls `FTurboMCPNodeType::SpawnNodeOfType`, whose `UBlueprintNodeSpawner::Invoke`/`SpawnNode` calls `ParentGraph->Modify()` before `AddNode`; `UObject::Modify` calls `MarkPackageDirty`. The probe is removed afterward, but the dirty bit remains. The fixture gives success Plan, refused Plan and refused Apply independent fresh packages; refused Apply measures against the dirty state immediately before Apply. Plan now uses the existing `BuildProbeNode` path to invoke the same filtered action in a throwaway transient graph; the probe is never added to or removed from the real graph, so `ParentGraph->Modify` cannot dirty the asset. The real graph is still used for action filtering, class and pin resolution, and the captured pins are identical to the previous probe. Tests retain clean Plan/refusal assertions, refused Apply, repeated Plan, and a separate owned already-dirty fixture, plus graph/widget snapshots and Apply deltas. The success fixture is asserted clean immediately before Apply and dirty afterward; no success-fixture dirty reset or pre-Apply dirty injection is used. The probe is marked garbage only after proving its outer graph is the throwaway graph, is transient, is not the real graph, and is owned by the Blueprint; otherwise the form is refused and nothing is marked or mutated. Supervised run `afc09af42ce44ffeac3c9526784f9d14` at `1705e18` built and the Widget automation reported Success with no errors, but one warning: an HTTP request to `https://www.google.com/generate_204` timed out after 3.00 seconds. The Python wrapper then crashed with `TypeError` while concatenating that warning list into the display string, so the overall run failed and this slice remains unaccepted. The wrapper now stringifies structured detail safely and boundedly and has an offline `--self-check` that proves a nonempty list detail records a failure without crashing; warnings are neither ignored nor suppressed, and the no-warnings check remains a failure condition. The prior append regression suite passed, including placement. No build, editor, MCP, native or recovery operation was run in this correction; compiled/engine acceptance remains integration-owned. The fixture package is unique and never saved, and is deliberately non-transient so dirty tracking is observable; save-on-compile, fresh reopen and on-disk persistence are not certified.

### Shared-value reader repair (R04-SHARED-VALUE-01)

Task R04, shared-value reader, claim `21e8dac84b6843df8adb0a1466d8db80` (owner `dsh-r04-shared-value-02`). Approved base `315a264a83bfe63d222e59e4cd149a890d7f4a13`; branch `task/r04-shared-value-reader`; worktree `C:\ExampleDev\SampleRoot005\conhost-integrator`. The candidate is the commit containing this evidence; its full SHA is returned in the submission message. R04 remains incomplete and this assignment does not accept it.

Changed files (the recorded five-file allowlist; nothing else):
- `Public/TurboMCPGraphText.h`: private `EFormEvidence`, evidence out-parameters, cross-entry shared set, and corrected shared-value comments. No public tool signature change.
- `TurboMCPGraphText.cpp`: removes the undefined `$id` emission; renders the first complete shared pure value inline and later uses the existing supported `(-> node-id:output-pin)` link; adds bounded upstream pin-preserving knot resolution; propagates Complete/Partial/None from child renders; a pin cap, unresolved source, unsupported node, cycle or depth stop cannot mark a reusable form.
- `TurboMCPGraphTextSharedValueTest.cpp` (new): transient compiled synthetic automation for direct and rerouted multi-output values, cross-entry sharing, depth/cycle partials, malformed knots, a direct unsupported no-type-ID first form, an unsupported no-type-ID child under a valid shared parent, a malformed nonempty nested link under a shared parent, deterministic unique fixture GUIDs, and a source-derived exact legacy single-use literal including the `UK2Node_CallFunction` target annotation.
- `Tools/test-graph-text-guards.py`: runs the compiled test through the pinned `AutomationTestToolset` JSON report and strictly validates one Success entry with no errors.
- This Completion evidence only.

Checks run in this worker: `python Tools/validate.py` exited 1 with 25 of 26 checks passing; only `validate-repo` failed at `Tools/audit-inventory.py --check` with `Stale capability inventory`. The generated inventory was not regenerated because it is integrator-owned. `python -m py_compile Tools/test-graph-text-guards.py` exited 0; `git diff --check` exited 0. No build, editor, MCP, native or recovery operation was run; the compiled automation and engine-suite result are integration-owned.

Preserved: normal single-use pure-node rendering, full-detail `via knot` behavior, the exec-chain knot helper, cycle/depth guards, pin-cap honesty and unrelated text. A nonempty link array with a null element or null owner is diagnosed as Partial rather than falling back to default text. The exact single-use literal is derived from approved-base source and fixture metadata, but has not been executed against the approved base here. Limitations: the compiled test is not executed here; combined reader-text writer round-trip is not certified by link-syntax acceptance; the unsupported fixtures use a plain `UEdGraphNode` that has no addressable type ID and is not a production node class; macro/no-entry omissions and reader completeness remain separate debt. No writer, lexer, GraphCheck, dialect, pagination or macro change.

### Integrator acceptance: Append placement

Accepted `8ebe1a00fef4bf3b0382c35a266d262d8a8e3814` from approved base `479c9df0fbc8e1e5d31c9ac760da69bb75323b13`; trusted seven-file scope passed. Append Preview/Edit opt into a deterministic column while shared callers keep legacy placement. Positive stored dimensions are preserved independently; missing dimensions and new nodes use estimates. Overflow and malformed requested-position indexes refuse before mutation.

Supervised pinned-5.8.3 run `6dbd8f4ab6654f719602cc90e36af652` passed build and `test-graph-append` at candidate digest `5c302a10967e96da6259215e5a63525728a33dc18a119bcb8adee810f28c3a96`. Both `TurboMCP.GraphAppend.Placement` and `TurboMCP.GraphAppend.AliasCollision` returned Success with no errors or warnings. Sixteen fixture files were restored. DLL SHA-256: `C3BA623BD8D786B267774645F970755485CEA4A43CD0A04528247CCD28349DA4`, 3,853,824 bytes. Integration offline run `545ca142cb064d8eb48fd3261babfa25` passed after the generated inventory identity refresh.

This proves the tested stored/estimated placement and preservation cases, not measured Slate overlap, full Widget Blueprint coverage, compile/save/fresh-reopen or complete graph dumping. Session 1 next prepares a read-only Widget Blueprint acceptance specification; Session 2 owns a read-only shared-value reader investigation. No new mutation scope is authorized by this evidence. R04 remains incomplete.


### Integrator acceptance: Append alias refusal

Accepted candidate `fcfc8b722162c71366ca4e89ddfecae0c4969157` from approved base `6b00fa4acdaa539d504d48294c7848188395581c`. The trusted four-file scope gate passed. Ambiguous six-character existing-node aliases now refuse per reference; qualified IDs, unique aliases and label/type-ID fallback remain supported.

Independent parser checks passed 17/17. Supervised 5.8.3 build `ebfddfcbe49644eb8606712002b66997` passed at candidate digest `2f485c68e0ca9649ad0addbee0679021da87c15b21b4a91ef5c4576bc7a50bb0`. The focused engine run `861cbb06c5434fe8b136bd0490e1886c` passed `test-graph-append`, including `TurboMCP.GraphAppend.AliasCollision` with Success and no errors or warnings. The runner restored fixtures. The DLL SHA-256 was `D86A53F1F5C1DC4043BAB1A34FE9AD6A59E8759DD64286841F5AEB047AF6794C` (3,782,144 bytes).

Integration offline run `38c9535495ae4d03906678c2c160c46b` passed after refreshing only the generated inventory plugin-tree identity; all 101 tool rows were unchanged.

Evidence applies to this candidate and bounded append checks. It does not certify save/reopen, Widget Blueprint coverage, graph-dump completeness, automatic placement or all of R04. The existing suite's final disk-save assertion checks response wording rather than a fresh disk fingerprint; do not treat it as persistence evidence. Placement and reader completeness remain separate work. Worker history is preserved on its branch; integration copies the reviewed implementation and records acceptance here.

### Append placement implementation (R04-APPEND-PLACEMENT-01)

Task R04, append placement, claim `2313eed090f34a84bbcff9190715edfe` (owner `dsh-r04-placement-01`). Approved base `479c9df0fbc8e1e5d31c9ac760da69bb75323b13`; branch `task/r04-append-placement`; worktree `C:\ExampleDev\SampleRoot005\r04-vcs`. Submitted candidate `32b55a358c075e51f4e860e22ab2d603a869b62d` is preserved in history; this correction commit adds the evidence below, and its full SHA is returned in the submission message. R04 is not complete and this assignment does not accept it.

Changed files (the recorded seven-file allowlist; nothing else):
- `Public/TurboMCPGraphAppend.h`: opt-in plan fields and the `PlanPlacement` declaration; `Plan` gains a defaulted placement request.
- `TurboMCPGraphAppend.cpp`: placement plan invocation, pre-transaction validation, apply-time position assignment and placement evidence.
- `TurboMCPGraphAppendPlacement.cpp` (new): deterministic column over stored/estimated bounds with widened arithmetic.
- `TurboMCPGraphAppendTools.cpp`: Append Preview and Edit request placement; shared callers do not.
- `TurboMCPGraphAppendPlacementTest.cpp` (new): compiled synthetic automation.
- `Tools/test-graph-append.py`: runs both compiled append tests through the pinned report parser.
- This Completion evidence only.

Implementation:
- Plan placement is opt-in. `PreviewTurboMCPGraphAppend` and `EditTurboMCPGraphAppend` request it; `GraphPatch` and `GraphReplace` keep the default and the previous origin placement.
- The column starts at the rightmost stored-positive or estimated existing bound plus 160, at the topmost existing top; created forms follow in input order with a 224 vertical stride (160 estimated height plus 64 gap). An empty graph starts at (0,0); negative coordinates are preserved and comments participate.
- Each positive stored dimension is kept independently and only a missing or non-positive dimension is estimated, so a known wide bound stays authoritative when its height is missing. Created nodes use a 320x160 estimated footprint. `FPlan` records `bPlacementRequested`, `PlannedPositions` keyed by form index and `bPlacementUsedEstimatedSize` (at least one existing dimension estimated).
- Edges, extents and additions are int64 and checked before the int32 cast. Both the top-left position and the estimated 320x160 footprint must fit int32; `INT32_MIN` is accepted when the arithmetic fits, and a column, right-edge or bottom-edge overflow refuses in Plan. Apply revalidates structural completeness before the transaction, so missing, duplicated, link-only or out-of-range form indexes refuse with no graph or dirty-state mutation.
- Apply writes positions only on newly created nodes, by form index; existing nodes, links, defaults and comment bounds are untouched. The report appends created node IDs and coordinates plus the stored/estimated-geometry caveat; the first line is unchanged.

Tests written but NOT compiled or run in this assignment:
- `TurboMCP.GraphAppend.Placement` builds transient in-memory Blueprints with forced positions and stored sizes. It covers an empty graph, a populated graph whose comment is the rightmost bound, negative coordinates, a non-positive stored size, mixed width/height bounds (a large known width with no height, and a known height with no width), two identical plans, column/right-edge/bottom-edge overflow, the valid `INT32_MIN` boundary and a large valid upper bound, a malformed requested-position count, duplicate, out-of-range and link-only form indexes (each refusing with the graph and dirty state unchanged), the legacy no-placement path, preview and refusal rendering, a snapshot-negative position, default and link change, and a successful Apply.
- The Apply case seeds a known link and defaults and asserts: every existing node keeps its GUID, position, comment bounds, pins, defaults and graph order; only the planned nodes are created at their planned positions; existing links survive; new links are exactly the planned deltas; and the report names each created node. The fixture is transient and is retired by scoped cleanup on every return.
- `Tools/test-graph-append.py` runs both `TurboMCP.GraphAppend.AliasCollision` and `TurboMCP.GraphAppend.Placement` through the pinned `RunTests` JSON parser, preserving the alias checks.

Validation (offline):
- `python Tools/check-language.py` exit 0.
- `python Tools/test-graph-append.py --parser-only` exit 0; 17 parser checks with no editor, socket or fixture.
- `python Tools/validate-repo.py`: exit 0 on the placement working tree before its first commit. On the correction tree the plugin source is already committed, so only `audit-inventory --check` fails because the generated `docs/audit/capabilities.json` embeds `plugin_tree`; all 101 inventory rows are identical. That generated file is outside the seven-file scope, was not written, and remains integrator-owned. A candidate-bound inventory-only failure is reported as such, not bypassed.
- Candidate-bound offline evidence and the full-base scope gate result are obtained after this commit and returned in the submission.

Not proven here (integration-owned):
- The new C++ was not compiled and neither automation test was run. The placement arithmetic, transient fixtures, seeded link, Apply under a transaction and the read-back are asserted by the tests, not measured here.
- The geometry is stored/estimated; visual non-overlap is neither claimed nor tested. `Plan` still builds and removes probe nodes on the actual graph, so Preview has transient engine side effects.
- The MCP tool surface, compile, save and editor reopen are not exercised.

Rollback: revert the implementation commit. Restoring the five-argument `Plan` call in the Append wrappers and removing the placement files restores the previous origin placement; Replace and Patch need no change because they already use the default. No persisted state, asset or schema needs migration.


### Integrator acceptance: synthetic containment harness

The synthetic harness through de899f52 is accepted for its bounded test-infrastructure purpose. Native execution f70d31cd23c04736b7586c76ddb29df5 passed six cases on the content committed as 8b33ac0; its recorded base was 5546bd6 with a dirty tree. The integrator independently reproduced tree digest 123119bc8b6f2ddd43b088c56b3c2ad0e144ca98e5d298ede882955b39723286. The subsequent test-isolation correction passed 103 independently rerun focused tests and candidate-bound offline execution 69e7451916474651ac5fc741daf06032 (all 26 checks). No production reader or engine runtime is certified: all native cases have reader_path_evidence=false. Earlier unaccepted/failed evidence below remains historical, not current admission status. R04 remains incomplete.

Remaining harness debt: actual_worker_path is a static field even on blocked attempts; consumers must use case/attempt status. Non-proof is conservatively blocked, including failed negative attempts; per-case exit codes are partial. Context admission is accidental-execution protection, not an OS security boundary. Four cases are synthetic analogues; preassignment and sentinel cases are not marked analogues. No additional native run was needed for the test-only correction.

Accepted Blueprint audit corrections through 907c802 are source/planning evidence only. The Append alias repair was subsequently accepted as recorded above. The next separately owned implementation is Append placement; its exact base and scope are recorded at dispatch.
Task R04, first dispatch checkpoint (`call-log-contracts`), claim `ab2cf6535c584bc6a9419c79e85d0b04` (owner `dsh-r04-01`). Approved base `1180c6d43655195dc2772b6d1f0b80588e4b466a`; branch `task/r04-call-log-audit`; worktree `C:\ExampleDev\SampleRoot005\r04`. The candidate is the correction commit containing this evidence; its full SHA is returned in the submission message. R04 is not complete.

Source identity:
- Engine 5.8.3 at `396c9f059903aed5fec78ecd3d437a40c6415368` (`engine.lock.json`).
- Plugin tree at the base: `590661ec45c829264ea1743fccea994edb748092`.
- Claim store: `c:\ExampleDev\turbomcp\.git\turbomcp\task-claims.json`, SHA-256 `308037d40eec30e2e8342409642786cae795bd5914dc5081fcb358812cd92758`; active R04 claim inspected in this worktree.

Scope and inventory:
- `docs/audit/callable-contracts.json` contains schema 1, the approved base and plugin/engine identities, and exactly 101 unique tool IDs from `docs/audit/capabilities.json`.
- Four rows are `reviewed_source`: `ReadTurboMCPCallCost`, `ReadTurboMCPCallLog`, `EditTurboMCPCallLog`, `EditTurboMCPCallLogClear`. The other 97 are explicitly `not_reviewed` and retain their capability source/test references.
- The audit traces declarations, the `FRecordingToolset`/`FScopedCall` wrapper, implementations, the text/result client path, defaults, numeric parsing, effects, success/refusal and truncation.
- Schema omitted-versus-empty is source-traced, not runtime-tested: the four parameters have no defaults; `FJsonSchemaGenerator` marks them required, and `ObjectFunctionToolCall` rejects missing fields before invocation. An explicit empty string reaches the body and selects the in-code default/refusal.

Findings recorded (source review only; no runtime claim):
- `R04-CALL-01`: `IsNumeric` + `Atoi` accepts fractions and does not check overflow; `1.5` becomes 1. `+1` is a valid positive-integer spelling and remains a compatibility choice, not inherently malformed.
- `R04-CALL-02`: `ReadTurboMCPCallCost` returns the empty-log report before validating `Cap`; with no entries, `banana` or `0` is accepted while `ReadTurboMCPCallLog` refuses an invalid `Limit`.
- `R04-CALL-03`: the log's truncation sentence says `since=<newest shown>` pages forward, but selection is newest-first and `since` excludes earlier records, so it cannot retrieve the omitted older rows.
- `R04-CALL-04`: the four tools return plain `FString` without an ARC02 result line; `Tools/tmcp.py` treats every `TurboMCP.*` toolset as migrated, so `call_structured` without `requires_result=False` would raise. Whether to migrate tools or narrow the client is unresolved.
- `R04-CALL-05`: switching the log OFF from ON writes a start and suppresses its end, leaving a start-only record rendered as running/missing-end.
- `R04-CALL-06`: ordinary domain refusals (`error:` strings returned by the body) are reflected as successful values, so `bError=false`; they are absent from ERROR rows, failed counts and `filter=error`. Genuine unknown-tool, pre-invocation parameter, conversion and script errors are `HasError` and record `bError=true`. No prose parser is proposed.
Each finding has a bounded proposal in the JSON/Markdown. Proposal IDs are `R04-REPAIR-01` through `06`; they are not implementation permission. `R04-REPAIR-06` is a probe-only deferred item, and the tool-migration alternative in `R04-REPAIR-05` awaits integrator specification.

Observability and persistence corrections:
- `FScopedCall::Finish` stores the first 4000 result characters plus a truncation suffix, not at most 4000 total; `ResultChars` retains the full measured length.
- `ReadTurboMCPCallLog` reads in-memory `Entries`; `GetLogFilePath` only constructs a display path and the tool does not read on-disk history.
- Clear's body does not alter the file, but the wrapper writes start/end records and normal rotation can run when logging is ON. The existing unchanged-file assertion is valid only in its OFF-mode setup.
- Self-recording and self-filter matching occur only when logging is enabled and the recorder is installed. An enabled normal MCP read is preceded by its own start, so ON-and-empty is a direct/synthetic state, not an ordinary read sequence.
- `SaveConfig` is a persistence attempt; successful disk writing and persistence across restart are not verified.

Validation:
- `python Tools/audit-inventory.py --check` exited 0: 101 definitions; 95 `parsed_call`, 6 `name_reference_only`.
- `python Tools/test-result-contract.py` exited 0: PASS, C03-C08; it does not exercise these four tools.
- `python Tools/test-client-retry.py` exited 0: PASS, C01/C02; it does not exercise these four tools.
- `python Tools/execute-tests.py --profile offline --execute` exited 0 on the corrected tree (request commit `5067616`): report `.local/executions/80e0be2ee0be46869c49d72d72483d5d/report.json`, tree digest `70db220b04b4d42f02162e00c0e43a4d3312aba0ea229f19836d6c6e50748f3c`; all offline suites passed.
- No engine suite, editor, build, direct MCP call, packaging or recovery command was run. `test-call-log.py` and `test-call-cost.py` remain existing engine-suite assertions, not results for this checkpoint.
- Candidate-bound offline evidence is obtained after this commit and returned separately.

Changed files: `docs/audit/callable-contracts.md`, `docs/audit/callable-contracts.json`, and this Completion evidence only. No plugin, engine, test, helper, dependency, pin, descriptor, status/readiness, TODO, review-register, routing or release control was changed.

Unresolved: the four tools' runtime behavior; fraction/overflow and empty-log invalid-cap cases; 50/500 and 4000-plus-suffix boundaries; truncated paging; ON->OFF, OFF->ON and clear-with-log-on record shapes; omitted-vs-empty live schema behavior; refusal-versus-invocation bError; `call_structured` migration; and any `FScopedCall` state/header change required by `R04-REPAIR-04`. The 97 unreviewed rows remain unreviewed. Stopping for integration review; no merge, push, completion or claim release.

Integration review accepted candidate `12d515ac46b58e42e83fffb9b577181fb4e68b55` as the first source-only checkpoint. The scope gate and candidate-bound offline run `ba930a8` passed; the shared claim matched. The integration owner qualified error-filter and pre-wrapper/transport behavior. Four of 101 tools are source-reviewed; R04 remains todo and every repair proposal remains unapproved. No next-family dispatch is authorized by this acceptance.

### Checkpoint 2: status, help and schema
Task R04 checkpoint 2, claim `350b288bbc2d4f5ab0e408d760bf2dfb` (owner `dsh-r04-02`). Approved base `b353fdd8b44f405c84ef9a67fde9db679f8c4804`; branch `task/r04-discovery-audit`; worktree `C:\ExampleDev\SampleRoot005\r04-discovery`. The candidate is the correction/checkpoint commit containing this evidence; its full SHA is returned in the submission message. R04 remains incomplete.

Source identity:
- Engine 5.8.3 at `396c9f059903aed5fec78ecd3d437a40c6415368` (`engine.lock.json`).
- Plugin tree: `590661ec45c829264ea1743fccea994edb748092`.
- Claim store: `c:\ExampleDev\turbomcp\.git\turbomcp\task-claims.json`, SHA-256 `402acaf349aefb5613fbb10fe5bf93c05e5ba4a184598b94ad379eb5be8ce649`; active R04 claim inspected in this worktree.

Scope and inventory:
- `docs/audit/callable-contracts.json` retains all 101 IDs and the four accepted call-log rows, findings `R04-CALL-01` through `06` and their repair proposals. Three rows are added as `reviewed_source`: `ReadTurboMCPStatus`, `ReadTurboMCPToolHelp`, `ReadTurboMCPToolSchema`. Current totals are 7 reviewed and 94 `not_reviewed`; checkpoint 1 remains recorded as 4/97.
- Checkpoint-specific base/claim/validation identities are stored under `checkpoint_2`; checkpoint 1's base, claim, validation and acceptance text are preserved.
- The audit traces declarations, implementation, registry availability/registration, enabled/schema-listed/discoverable layers, help/schema/traps, recorder side effects and the client path.
- Parameter behavior is source-traced: all three have no C++ defaults. Status has no parameters. Help and schema parameters are required; omitted fields are rejected before the body, while explicit empty strings reach the body. Help's empty values mean summary/search-all/refusal depending on the pair; schema's empty toolset is refused and empty tool lists the toolset.
- Status count is the static declared-class count, not a registry count. A registered-but-disabled toolset can be absent from help/schema because `GetJsonSchema` returns empty for disabled toolsets. Status does not distinguish registry-unavailable from MISSING. No client-discovery or runtime claim is made.

New findings (`R04-DISC-01` through `06`):
- Help omits `REQUIRED`/`default=` though the schema reader prints them.
- Help's single-tool path warns over its 2000-character promise without cutting; the summary path returns before that check and caps rows, not characters. `TurboMCPToolsets.h:102-103` is the help promise; `:481` belongs to the schema tool's separate cap.
- Status's “costs nothing and changes nothing” is false when logging is ON because the recorder writes/rotates.
- Status conflates declared, registered, enabled and discoverable layers; it reports settings/registration, not discovery.
- Help preserves joined `Toolset.Tool` in `ToolsetName` and exact dotted toolset names, but does not accept the schema's `Read` alias, normalization variants or a qualified name in `ToolName`; the report's eight-row matrix distinguishes these modes.
- Help does not distinguish a missing inputSchema/properties object from a zero-parameter tool.
Each finding has an unapproved bounded proposal `R04-REPAIR-07` through `12` with exact repository-relative files, preserved behavior, synthetic probe, expected result, negative control, focused checks and rollback. Required/default, long-help and missing-schema probes are marked as awaiting fixture/test-seam scope rather than claiming current `test-agent-help.py` supplies them. Status enablement, cap policy and name normalization remain unresolved decisions.

Validation:
- `python Tools/audit-inventory.py --check` exited 0: 101 definitions; 95 `parsed_call`, 6 `name_reference_only`.
- `python Tools/test-result-contract.py` exited 0: PASS, C03-C08.
- `python Tools/test-client-retry.py` exited 0: PASS, C01/C02.
- `python Tools/execute-tests.py --profile offline --execute` exited 0 after corrections (request commit `4a79243`): report `.local/executions/536baf104ac5401fa25e907fe5f48c29/report.json`, tree digest `c603009e509b32398f892e4f26bec4704babb2ab9940f429ae569b73043eee4c`; all offline suites passed.
- No engine suite, editor, build, direct MCP call, packaging or recovery command was run. `test-agent-help.py`, `test-batch-tools.py` and the status uses in other suites are existing assertions only, not results here.
- Candidate-bound offline evidence is obtained after this commit and returned separately.

Changed files: `docs/audit/callable-contracts.md`, `docs/audit/callable-contracts.json`, and this Completion evidence only. No plugin, engine, test, helper, dependency, pin, routing preference, task status/readiness, TODO, review register or release control was changed.

Unresolved: status/help/schema runtime behavior; the single-tool cap versus summary row cap; fixture/test-seam scope for required/default, long-help and missing-schema probes; disabled/ambiguous name behavior; declared/enabled/discoverable status contract and enablement API; all three tools' lack of ARC02 result lines and `call_structured` implications; `R04-CALL-06` refusal recording; the 94 unreviewed rows. R04 remains incomplete; no merge, push, completion or claim release.

Checkpoint 2 integration review accepted candidate `f10dc74ca4569ca449c0e598fe76abe042a0e7b7` as source-only evidence for status, help and schema. Scope and checkpoint-1 preservation passed; candidate offline run `8018fadb647741b6a07bd7cd7c80668c` passed. R04 remains todo, seven tools source-reviewed and 94 not reviewed. Repair proposals and next-family dispatch remain unapproved.

### Checkpoint 3: source-control state and checkout
Task R04 checkpoint 3, claim `c44cc6b13e824f5682e53c43bee3cd5d` (owner `dsh-r04-vcs-01`). Approved base `fc46e01845c4fbd6814c07f5f228d946aeaef6b9`; branch `task/r04-vcs-audit`; worktree `C:\ExampleDev\SampleRoot005\r04-vcs`. The candidate is the checkpoint commit containing this evidence; its full SHA is returned in the submission message. R04 remains incomplete.

Source identity:
- Engine 5.8.3 at `396c9f059903aed5fec78ecd3d437a40c6415368`; local engine HEAD verified equal to the pin.
- Plugin tree: `590661ec45c829264ea1743fccea994edb748092`.
- Claim store: `c:\ExampleDev\turbomcp\.git\turbomcp\task-claims.json`, SHA-256 `6a003399621601864306562b3d74418f328098574bd1edfdb53cc16cfc2f64e5`; active R04 claim inspected in this worktree.

Scope and inventory:
- `docs/audit/callable-contracts.json` retains all 101 IDs and prior checkpoint rows/findings/proposals. Three rows are added as `reviewed_source`: `ReadTurboMCPVcsState`, `PreviewTurboMCPVcsCheckout`, `EditTurboMCPVcsCheckout`. Current totals are 10 reviewed and 91 `not_reviewed`; prior checkpoints remain 4/97 and 7/94.
- Checkpoint-specific base/claim/validation identities are stored under `checkpoint_3`; earlier base, claim, validation and acceptance text are preserved.
- The audit traces declarations, `Plan`/`Render`/`Checkout`, `TurboMCPAssetResolver`, provider gating and `GetState`, the recorder, and the text/result client path. No runtime behavior is claimed.

Contract notes:
- `Assets` is required; omitted fields are rejected before the body. Explicit empty means provider/module facts with no asset rows. Comma/pipe separators are trimmed and empty parts culled; order is preserved and duplicates are not deduplicated.
- One row per nonempty input. Resolution failures and missing verified package files produce per-row refusals. Loaded-object and sole-package fallback routes do not universally reject ambiguous names or absent object members; multi-asset refusal belongs to the package fallback. An unsaved package may resolve but has no verified file, so it is refused before provider state. Duplicate inputs remain duplicate rows.
- Disabled, unavailable and module-missing runs refuse every resolved asset with the engine string/module reason and do not call per-asset `GetState`.
- Null provider state is reported as UNKNOWN. A non-null `IsUnknown` state is not checked; Perforce `DontCare` defaults can print as source-controlled true with provider text `Unknown`.
- `ForceUpdate` does not prove refresh success: the single-file helper returns a state only when the array call returns `Succeeded` and is nonempty (otherwise null) and does not expose the result; pinned Perforce ignores `FUpdateStatus` failure and returns cached state with `Succeeded`.
- Resolution route precedence is not `/Game`-only: a slash-free input first tries a loaded `FindObject` (no mount filter/uniqueness); `/Game` belongs to the later registry fallback. `TryDirectForms` can fall to a sole-package fallback without matching the requested object member; the `_C` condition at `Len-3` does not recognize normal `Package.Object_C`. Multi-asset refusal is package-fallback-only.
- `Read`/`Preview` issue per-asset `GetState(ForceUpdate)` and wrapper logging but no `FCheckOut`; Preview can still execute a refresh operation. `Edit` issues per-asset `FCheckOut`; it does not revalidate the plan or read state back, does not populate the result fields, and has no aggregate/partial/rollback outcome. `UsesCheckout() == false` is reported as a no-op expectation but the operation is still issued.
- Preview/Edit branch order differs: `AddAssetLines` checks `!UsesCheckout` before `!bStateRead`; `Checkout` checks `!bStateRead` first. With no-checkout provider and null state, Preview says the provider is still asked while Edit skips. Already checked-out/added skip applies only when `UsesCheckout()` is true; readable no-checkout rows still issue.

New findings (`R04-VCS-01` through `07`):
- `R04-VCS-01`: pinned ForceUpdate/cached-state freshness limitation; not stale-proof.
- `R04-VCS-02`: non-null `IsUnknown` state is treated as measured; Perforce `DontCare` can print source-controlled true.
- `R04-VCS-03`: duplicate inputs produce duplicate rows/state queries and potentially duplicate checkouts.
- `R04-VCS-04`: checkout has no revalidation/post-read-back/aggregate result; stale plan and partial multi-asset outcomes.
- `R04-VCS-05`: checked-out-other, non-null unknown and read-only distinctions are not structured.
- `R04-VCS-06`: Preview/Edit branch mismatch for UsesCheckout=false + null state; checked-out/added skip is UsesCheckout=true-only.
- `R04-VCS-07`: resolver route precedence, `/Game` restriction, generated-class condition and package-fallback member matching were overclaimed.
Each has an unapproved proposal `R04-REPAIR-13` through `19` with exact files, preserved behavior, synthetic provider/state probe or explicit fixture gap, expected result, negative control, validation and rollback. Duplicate policy, branch policy, post-state verification, freshness representation and provider/resolver fixtures remain unresolved.

Validation:
- `python Tools/audit-inventory.py --check` exited 0: 101 definitions; 95 `parsed_call`, 6 `name_reference_only`.
- `python Tools/test-result-contract.py` exited 0: PASS, C03-C08.
- `python Tools/test-client-retry.py` exited 0: PASS, C01/C02.
- `python Tools/execute-tests.py --profile offline --execute` exited 0 after consistency corrections (request commit `aa4cf84`): report `.local/executions/5c23c907de3b4e08995eb1b890ff84cb/report.json`, tree digest `969a2a37287f63d5e73d3f898ee7fa560606a1770c6ee1af5149f2abb821f90d`; all offline suites passed.
- No engine suite, editor, build, direct MCP call, source-control checkout, packaging or recovery command was run. `test-vcs.py` and `test-vcs-write.py` are existing engine-suite assertions only, not results here.
- Candidate-bound offline evidence is obtained after this commit and returned separately.

Changed files: `docs/audit/callable-contracts.md`, `docs/audit/callable-contracts.json`, and this Completion evidence only. No plugin, engine, test, helper, dependency, pin, routing preference, task status/readiness, TODO, review register or release control was changed.

Unresolved: live-depot checkout and post-state verification; failed-refresh/cached, non-null unknown, checked-out-other and read-only provider states; duplicate policy; Preview/Edit null-state branch policy; aggregate/partial outcome and rollback policy; resolver route precedence/generated-class/member-matching policy; how to represent freshness without a new public schema; all three tools' lack of ARC02 lines and `call_structured` implications; `R04-CALL-06` refusal recording; the 91 unreviewed rows. R04 remains incomplete; no merge, push, completion or claim release.

### Checkpoint 4: save contracts

Task R04 checkpoint 4, claim `613fbd8768e04378859aad03a5a26bb6` (owner `dsh-r04-save-01`). Approved base `15ab395ac88eaa52ea31d7628d032446c90fddb6`; branch `task/r04-save-audit`; worktree `C:\ExampleDev\SampleRoot005\r04-vcs`. The candidate is the checkpoint commit containing this evidence; its full SHA is returned in the submission message. R04 remains incomplete.

Source identity:
- Engine 5.8.3 at `396c9f059903aed5fec78ecd3d437a40c6415368`; local engine HEAD verified equal to the pin.
- Plugin tree: `590661ec45c829264ea1743fccea994edb748092`.
- Claim store: `c:\ExampleDev\turbomcp\.git\turbomcp\task-claims.json`, SHA-256 `1e4930ce16f30f4623d50de4721fbb36e7b82ca7e230c9d71dd30ee0e9343473`; active R04 claim inspected in this worktree.

Scope and inventory:
- `docs/audit/callable-contracts.json` retains all 101 IDs and prior checkpoint rows/findings/proposals. Two rows are added as `reviewed_source`: `EditTurboMCPSave` and `EditTurboMCPSaveWithoutThumbnail`. Current totals are 12 reviewed and 89 `not_reviewed`; prior checkpoints remain 4/97, 7/94 and 10/91.
- Checkpoint-specific base/claim/validation identities are stored under `checkpoint_4`; earlier base, claim, validation and acceptance text are preserved.
- The audit traces declarations, `SaveTurboMCPAssets`, `UEditorAssetLibrary::SaveLoadedAsset`, `UPackage::SavePackage`, the source-control preflight, resolver, engine save/thumbnail callees, recorder and text/result client path. No runtime behavior is claimed.

Contract notes:
- `Assets` is required; omitted fields are rejected before the body. Explicit empty or separator-only input is the whole-call no-asset error. Comma/pipe separators are trimmed and empty parts culled; order is preserved and duplicates are not deduplicated. More than 25 parts is refused before any provider call.
- E-SAVE goes through `SaveLoadedAsset` -> `SavePackages` -> `InternalCheckoutAndSavePackages` -> the editor `OBJ SAVEPACKAGE` path. It can save external packages, issues another engine checkout pass when source control is enabled, and generates thumbnails before the write. The no-thumbnail tool calls `UPackage::SavePackage` directly; it skips the thumbnail renderer, external-package save, the registered-asset check and mark-for-add, but still runs `PreSave`/`PostSave` and serialization.
- The source-control preflight runs before the loop's dirty, package-load and `/Game/` eligibility decisions. A clean asset, a refused package, an unloadable object or a duplicate can receive `GetState(ForceUpdate)` and `FCheckOut`; the preflight remains report-only and never gates the save.
- Success is the package clean dirty flag after one retry, not a disk readback. No compile is performed. `saved` is an engine success signal, not verified durable persistence or multi-file atomicity.
- Neither tool has an ARC02 result line. Body `error:`/refusal text is a successful value to the recorder; `call_structured` would require metadata unless `requires_result=False`. `Edit*` is a client mutation, so a lost response is unknown and is not replayed.

New findings (`R04-SAVE-01` through `08`):
- `R04-SAVE-01`: source-control preflight before per-asset write eligibility; clean/refused/unloadable/duplicate inputs can receive state/checkout work.
- `R04-SAVE-02`: E-SAVE reaches a second engine `CheckoutPackages` pass that the appended block does not report.
- `R04-SAVE-03`: no-thumbnail direct write does not preserve E-SAVE package semantics for map extension, external packages, registered assets or mark-for-add.
- `R04-SAVE-04`: direct `SavePackage` still executes `PreSave`/`PostSave` code; "cannot run the asset's own code" is too broad.
- `R04-SAVE-05`: per-input counters are not per-package outcomes for multi-asset packages, duplicates or external packages; partial success is not explicit.
- `R04-SAVE-06`: failure/draw/error-attribution text is not conditioned on outcome or provenance; a failed no-thumbnail widget save can say the package was written, E-SAVE says thumbnail work ran without observing a draw, and the `:604-611` warning attributes any captured error in the per-asset save interval to thumbnail/design-time work without renderer provenance; failure probes for both tools, fresh/cached-thumbnail state and controlled error probes are absent.
- `R04-SAVE-07`: no plugin-side read-only preflight or stable read-only failure cause; a dirty eligible package that remains dirty after the first attempt can receive a speculative "may be read only" message. A clean package is skipped, checkout may make the file writable, and retry depends on `Package->IsDirty()`; a live read-only failure and the post-checkout attribute/provider outcome are unverified.
- `R04-SAVE-08`: "saved" is the dirty flag, not verified persistence; F07 reload verification is unmapped.
Each has an unapproved proposal `R04-REPAIR-20` through `27` with exact files, preserved behavior, synthetic probe, expected result, negative control, fixture gap, validation and rollback. No repair is implemented or approved.

Correction after review: this commit preserves candidate `ab7b10333b4cf13a4720c82be00536b82a6ea426` and addresses request `68812c12-7fb0-4870-9709-23f83a1eb8d1` (verified report SHA-256 `f253abf38b222bf4de9cd63ba36ea9c09778dc6f2a6dccb2c4f2cdec657c9d5b`) with documentation-only corrections: two R04-REPAIR-24 controls (clean package -> no save/no changes; dirty single-asset successful save -> `1 saved`), qualified R04-SAVE-07/read-only wording, and R04-SAVE-06 error-attribution coverage with a proposed controlled non-thumbnail/thumbnail/zero-error probe. All proposals remain unapproved.

Proposal correction after review: this commit preserves candidate `489268b522073ef2101f8ff376881e3ef6138e63` and corrects R04-REPAIR-25 without narrowing R04-SAVE-06. It restores failed-save probes for both save tools on owned `WBP_NodeTest`, restores the fresh/cached-thumbnail E-SAVE case, retains the non-thumbnail-error/thumbnail-error/zero-error cases, and replaces blanket preservation of existing success text with save behavior, retry/count behavior and truthful success semantics. Correcting misleading draw/provenance wording is explicitly permitted. All proposed seams remain absent, and all repairs remain unapproved.

Validation:
- `python Tools/audit-inventory.py --check` exited 0: 101 definitions; 95 `parsed_call`, 6 `name_reference_only`.
- `python Tools/test-result-contract.py` exited 0: PASS, C03-C08.
- `python Tools/test-client-retry.py` exited 0: PASS, C01/C02.
- `python Tools/execute-tests.py --profile offline --execute` exited 0 on the pre-commit correction tree (request commit `489268b`): report `.local/executions/2517d7617b934fc69e7038b279ccf17e/report.json`, tree digest `b3964e03f585d43aa3371b4525d05b821be1c7f56cd8af7a7c3171242f9ade13`; `Tools/validate.py` passed.
- No engine suite, editor, build, direct MCP call, packaging or recovery command was run. `test-save-thumbnail-option.py`, `test-vcs-write.py`, `test-struct.py` and the acceptance catalog are existing assertions only, not results here.
- Candidate-bound offline evidence is obtained after this commit and returned separately.

Changed files: `docs/audit/callable-contracts.md`, `docs/audit/callable-contracts.json`, and this Completion evidence only. No plugin, engine, test, helper, dependency, pin, routing preference, task status/readiness, TODO, review register or release control was changed.

Unresolved: preflight placement/policy; E-SAVE engine checkout reporting or suppression; map/external-package scope; honest project-code wording; package-level outcomes without a new public schema; read-only preflight versus post-checkout failure, with no live read-only failure or final attribute/provider outcome established; durable persistence verification without automatic reload; warning provenance for `:604-611`; `R04-CALL-04`/`R04-CALL-06`; resolver route caveats and the absence of a top-level asset check; the 89 unreviewed rows. R04 remains incomplete; no merge, push, completion marking or claim release.

### Checkpoint 5: compile contract for Monday widgets

Task R04 checkpoint 5, claim `88f668a66d8740acae6454d27e162c28` (owner `dsh-r04-compile-01`). Approved base `a92cae75abd5c4a8e65179fa8b1ad32f5aef436f`; branch `task/r04-compile-audit`; worktree `C:\ExampleDev\SampleRoot005\r04-vcs`. The candidate is the checkpoint commit containing this evidence; its full SHA is returned in the submission message. R04 remains incomplete.

Source identity:
- Engine 5.8.3 at `396c9f059903aed5fec78ecd3d437a40c6415368`; local engine HEAD verified equal to the pin.
- Plugin tree: `590661ec45c829264ea1743fccea994edb748092`.
- Claim store: `c:\ExampleDev\turbomcp\.git\turbomcp\task-claims.json`, SHA-256 `929824eba2c75c020eb20d3ca00fabe2d27efd27cbfc7a27f287aaaa2f34acd3`; active R04 claim inspected in this worktree.

Scope and inventory:
- `docs/audit/callable-contracts.json` retains all 101 IDs and prior checkpoint rows/findings/proposals. One row is added as `reviewed_source`: `EditTurboMCPCompile`. Current totals are 13 reviewed and 88 `not_reviewed`; prior checkpoints remain 4/97, 7/94, 10/91 and 12/89.
- Checkpoint-specific base/claim/validation identities are stored under `checkpoint_5`; earlier base, claim, validation and acceptance text are preserved.
- The audit traces declaration, `EditTurboMCPCompile`, resolver, refuses, recorder, client and pinned 5.8.3 compile/widget compiler callees. No runtime behavior is claimed.

Contract notes:
- `Assets` is required; omitted fields are rejected before the body. Explicit empty/separator-only is the whole-call no-asset error. Comma/pipe separators are trimmed and empty parts culled; order is preserved and duplicates are not deduplicated. More than 25 parts is refused before compiling.
- The tool accepts any resolved `UBlueprint`; other asset classes are refused per row. It does not read `Blueprint->Status`; errors are based on `CompileLog.NumErrors`, while captured engine errors are counted in the warning branch.
- `CompileBlueprint` can rewrite Widget Blueprint generated classes, widget trees, animations, named slots and runtime bindings; it can run project `UWidgetCompilerRule`/extension code and reinstance classes/instances. No transaction/undo wrapper is created.
- Compile may auto-save/checkout when `UBlueprintEditorSettings::SaveOnCompile` is enabled; the tool passes no `SkipSave`. The manager restores the pre-compile package dirty flag after compiler-side source work, so `saved` is not proof of persistence. The effective project setting is unverified.
- Compile success, package write and durable persistence are separate; no reload/readback or generated-class/binding reader exists. Common UI/Input support is not established from `UBlueprint`/`UWidgetBlueprint` inheritance.
- No ARC02 line. Body `error:`/refusal is a value to the recorder; `Edit*` is a client mutation, so a lost response is unknown and is not replayed.

New findings (`R04-COMPILE-01` through `07`):
- `R04-COMPILE-01`: conditional auto-save/source-control write through `SaveOnCompile`; not distinguished from compile-only.
- `R04-COMPILE-02`: compiler-side source binding/graph mutation with the prior dirty flag restored; no transaction.
- `R04-COMPILE-03`: compiler failures and engine diagnostics are not separately named; `Blueprint->Status` is unread. Engine-only errors remain `not clean` but are not declared compiler failures; the count units need explicit naming, and treating every engine Error as a compile failure requires an owner decision.
- `R04-COMPILE-04`: Widget Blueprint generated-class/binding/reinstancing/project-code effects are real but unreported and untested.
- `R04-COMPILE-05`: Common UI/Input support is unverified; class inheritance is not evidence.
- `R04-COMPILE-06`: partial/all-error batches still say `changed: compiled`; no aggregate outcome.
- `R04-COMPILE-07`: compile success, package write and durable persistence are not separately established.
Each has an unapproved proposal `R04-REPAIR-28` through `34` with exact files, preserved behavior, synthetic probe, expected result, negative control, fixture gap, validation and rollback. No repair is implemented or approved.

Proposed Widget Blueprint acceptance plan:
- Existing owned fixtures: `/Game/Tests/WBP_NodeTest` (plain UMG; `TitleText` widget; used by `test-save-thumbnail-option.py`, `test-widget-tree.py` and `test-widgets-as-vars.py`) and `/Game/Tests/BP_NodeTest` (plain Blueprint control; used by `test-compile-ensure.py` and `test-uncovered-tools.py`).
- Trial A: record baseline; set `TitleText` variable with existing `EditTurboMCPWidgetsAsVars`; read a `(widget TitleText ...)` line carrying `:variable` and the `ReadTurboMCPBlueprintLoad` flag; compile; save explicitly; supervisor-owned restart; re-read and require the `TitleText` variable flag, loaded/on disk/no unsaved.
- Trial B (memory-only control): restore baseline; same edit and compile with `SaveOnCompile` observed as Never; no save; same supervisor restart; require `TitleText` not marked. If the setting is not demonstrably Never, mark the control blocked. File hashes are supplementary; a dirty package need not change hash.
- The existing `tmcp_editor.stop_editor`/`ensure_editor` pair is not a restart under the suite runner: `run-suites.ps1:643` sets `TURBOMCP_RUNNER_OWNS_FIXTURES=1`, `tmcp_editor.py:81-82` returns without stopping, `:203-217` reuses an answering server, and `start-editor.ps1:130-131` refuses a direct start. The proposed supervisor-owned restart boundary is an implementation prerequisite (owner integration-owner/R02; files `Tools/run-suites.ps1`, `Tools/tmcp_editor.py`, `Tools/start-editor.ps1`, `Tools/check-mcp-server.py`).
- The restart must prove old owned PID exited, new owned PID started, and `check-mcp-server.py --editor-pid` passes with project/engine identity. Manual fallback must remain supervised; no direct `start-editor`, no clearing `TURBOMCP_RUNNER_OWNS_FIXTURES`, no foreign editor stop, no unrelated unsaved-state loss. If the supervisor cannot restart, persistence is blocked/unverified.
- `Tools/test-widget-compile-save-reopen.py` is proposed, not present. Existing tests are assertions only; no widget compile/restart test exists. Candidate-bound engine evidence is required before this plan is called passed.

Validation:
- `python Tools/audit-inventory.py --check` exited 0: 101 definitions; 95 `parsed_call`, 6 `name_reference_only`.
- `python Tools/test-result-contract.py` exited 0: PASS, C03-C08.
- `python Tools/test-client-retry.py` exited 0: PASS, C01/C02.
- `python Tools/execute-tests.py --profile offline --execute` exited 0 on the pre-commit correction tree (request commit `5da62367`): report `.local/executions/4c41e7fec10b4c6393c7d65037eb2ad3/report.json`, tree digest `3cf0270b5baaefaee93cc38fa4e251872265ec1f966c409cdc9d4fdfd8a26b83`; `Tools/validate.py` passed.
- No engine suite, editor, build, direct MCP call, packaging or recovery command was run. `test-compile-ensure.py`, `test-uncovered-tools.py`, `test-graph-knots.py`, `test-save-thumbnail-option.py` and `test-widget-tree.py` are existing assertions only.

Changed files: `docs/audit/callable-contracts.md`, `docs/audit/callable-contracts.json`, and this Completion evidence only. No plugin, engine, test, helper, dependency, pin, routing preference, task status/readiness, TODO, review register or release control was changed.

Unresolved: save-on-compile policy; compile-side source mutation/transaction policy; compiler-error versus engine-diagnostic count units; Widget Blueprint generated-class/binding/reinstancing visibility; Common UI/Input support; partial/all-error outcome; compile-vs-save-vs-persistence contract; supervisor-owned restart and observed SaveOnCompile seams; the 88 unreviewed rows. R04 remains incomplete; no merge, push, completion marking or claim release.

Correction after review: this commit preserves candidate `5da62367fc473d0edce262a08b66c6da9a7372b9` and addresses request `7074def1-119b-4017-a8aa-163e8ed71ce6` (verified report SHA-256 `44b2d56a520cca97167122a8e29bae722e0e18fb6641a6cface7d1fd9e152502`). The reopen plan now requires a supervisor-owned restart with old-PID-exit/new-PID/server-owner proof; `tmcp_editor.stop_editor`/`ensure_editor` is ineffective under the runner. The persistence plan now uses a reversible `TitleText` `IsVariable` edit plus an otherwise identical no-save control with `SaveOnCompile` observed as Never; file hashes are supplementary. Compiler-error assets, compiler-warning assets and engine-diagnostic counts are now named separately; treating every engine Error as compile failure remains an owner decision. All repairs remain unapproved.

### Checkpoint 6: persistence prerequisite contract

Task R04 checkpoint 6, claim `66394016d7b64f77a5106c8b316f43bd` (owner `dsh-r04-persistence-01`). Approved base `633c86cd6871f69bd997eefe6bae2176306b13f1`; branch `task/r04-persistence-design`; worktree `C:\ExampleDev\SampleRoot005\r04-vcs`. The candidate is the checkpoint commit containing this evidence; its full SHA is returned in the submission message. R04 remains incomplete.

Source identity:
- Engine 5.8.3 at `396c9f059903aed5fec78ecd3d437a40c6415368`; local engine HEAD verified equal to the pin.
- Claim store: `c:\ExampleDev\turbomcp\.git\turbomcp\task-claims.json`, SHA-256 `f83d96d0b67c3e34e22265ed4bdb19a9f247d46a35011a111717f1f81cbdd67a`; active R04 claim inspected in this worktree.
- Checkpoint 5 accepted candidate: `1c0cb15f737d619bd9777136366d141d75da8cd9`; all its findings and repairs are unchanged.

Design decision:
- Delivered one design in `docs/audit/widget-persistence-prerequisites.md`: a supervisor-owned **two-pass persistence transaction** plus a test-project `UBlueprintEditorSettings::SaveOnCompile` observation seam.
- Phase A observes effective `Never`, makes the reversible `TitleText` `IsVariable` edit, compiles, saves or deliberately omits the save, writes a strict state file and exits `75`. The supervisor stops only its owned editor, proves old PID/start-time exit, starts a fresh owned editor, proves new PID/start-time and **loopback listener ownership** (not merely a same-named process), then runs phase B as a new client. The saved trial keeps bytes until readback; the no-save control must lose the edit.
- The current `tmcp_editor.stop_editor`/`ensure_editor` pair is explicitly rejected under the runner: `run-suites.ps1:643` sets `TURBOMCP_RUNNER_OWNS_FIXTURES=1`, `tmcp_editor.py:81-82` no-ops stop, `:203-217` reuses an answering server, and `start-editor.ps1:130-131` refuses a direct start. No ownership flag is cleared and no direct helper is substituted.
- `SaveOnCompile` is the config property at `BlueprintEditorSettings.h:242`; the manager reads it at `BlueprintCompilationManager.cpp:458-490`. No existing Python/MCP API exposes it. The design adds a test-project fixture/automation readback that sets the effective value in memory and logs it with the current trial token/PID; it restores the prior in-memory value on every same-process path. `GetDefaultConfigFilename()` is only the source default path; the observed settings set is `GEditorPerProjectIni` plus the source/project/global user paths, and it is never written. A fresh `effective=Never` observation is required before mutation; stale logs are rejected and one default-file hash is not treated as proof of all user configuration.
- **Cleanup blocker:** `execute-tests.py`/`execution_runtime.run_owned` uses `KILL_ON_JOB_CLOSE`. A refusal due to unrelated unsaved assets cannot preserve them by skipping the supervisor's `finally`; the outer job still kills the editor. The design is not admitted for live acceptance until exclusive disposable-editor isolation is proved or the outer lifecycle gets a non-destructive detach/recovery path. Hard supervisor death retains a journal for explicit recovery; a failed readback is reported **persistence unverified**, not "not saved".

Proposed subtasks (all `proposed_unapproved`):
- P1: test-project `TurboMCPTestPersistenceSettings` fixture and automation tests; `TurboMCPTest.Build.cs` adds `BlueprintGraph`.
- P2: process identity/start-time and loopback listener-owner proof in `Tools/ownership.ps1` and `Tools/check-mcp-server.py`.
- P3: `Tools/persistence_supervisor.ps1` pure state/identity module, two-pass mode in `Tools/run-suites.ps1`, cleanup eligibility and recovery-journal changes in the outer lifecycle, and `Tools/test-persistence-supervisor.py`.
- P4: `persistence` profile in `Tools/execute-tests.py` and catalog entry.
- P5: proposed `Tools/test-widget-compile-save-reopen.py`; phase A/B state protocol and exact assertions.
- P6: supervisor-only live acceptance; no source. Monday recommendation is **no-go for automated acceptance now**, conditional go after P1-P6 and exact supervisor evidence; manual supervised fallback is not an automated gate.

Validation:
- `python Tools/audit-inventory.py --check` exited 0: 101 definitions; 95 `parsed_call`, 6 `name_reference_only`.
- `python Tools/test-result-contract.py` exited 0: PASS, C03-C08.
- `python Tools/test-client-retry.py` exited 0: PASS, C01/C02.
- `python Tools/execute-tests.py --profile offline --execute` exited 0 on the pre-commit correction tree: report `.local/executions/4d216704f1a6496db3e9f50ed2513acb/report.json`, request commit `229fbfcf33f2120e09a61ed8a06f033ed77e538d`, tree digest `51e48692a08da193b289d7eb4087055f0d49508f6823fc603afb6c4e20836fd4`.
- No engine/editor/build/MCP/recovery command was run. The proposed live acceptance is unexecuted.

Changed files: `docs/audit/widget-persistence-prerequisites.md` (new) and this Completion evidence only. No code, test, fixture, config, task status/readiness, TODO, routing, release or scope-policy change.

Unresolved: whether the `BlueprintGraph` test dependency and automation seam can be built; whether the runner profile can own the two-pass restart without broadening normal suites; whether process start-time plus loopback socket-owner identity/foreign-listener proof is accepted (the existing `--editor-pid` is insufficient); whether `GEditorPerProjectIni` plus source/project/global settings files can be observed without writing and restored/verified; whether the current KILL_ON_JOB_CLOSE outer job can preserve unrelated unsaved assets on refusal, or an outer lifecycle detach/recovery change is required; whether the no-save control can be kept isolated. If any is absent, the plan is blocked, not passed. R04 remains incomplete; no merge, push, completion marking or claim release.

Correction after review: this commit preserves candidate `f9aa39fff11368622b72fde9a4a6a2218aae1b0c` and addresses report SHA-256 `66b4b2ec3637b04233a653fd38ac26ed92c9c4d23c8ee0d71634c699e0fbea55`. The probe now binds the loopback socket owner to the expected PID/start time/canonical project and engine, with foreign/ambiguous/permission/replacement negative controls. Config provenance now distinguishes `GetDefaultConfigFilename()` from `GEditorPerProjectIni`/the loaded hierarchy, never writes settings files, restores in-memory state on every same-process path and requires a fresh token/PID `effective=Never` result. Cleanup eligibility is explicit; the current KILL_ON_JOB_CLOSE job is a blocking lifecycle gap for refusal with unrelated unsaved assets, hard death uses the journal/recovery path, and failed readback is reported **persistence unverified**. All subtasks remain `proposed_unapproved`.

### Checkpoint 7: offline identity validator

Task R04 checkpoint 7 correction, claim `b226ff78f8b44fa9ab49e54b6f763e9f` (owner `dsh-r04-identity-02`). Approved base `ad1a19cce23a19092e68b08f94d15b188dfb59f1`; branch `task/r04-identity-corrections`; worktree `C:\ExampleDev\SampleRoot005\r04-vcs`. The original `e8adf04` remains preserved on `task/r04-identity-validator`; its implementation was replayed as `0773959` on this corrected base. The candidate is the commit containing this evidence; its full SHA is returned in the submission message. R04 remains incomplete.

Implementation:
- `Tools/persistence_identity.py` adds the pure `validate_evidence(payload)` API and schema `turbomcp.persistence-identity/1`. It validates expected PID/start time/project/engine, literal `127.0.0.1` or `::1` endpoints, two bounded-time process/TCP snapshots and exact listener ownership. It returns explicit pass/refusal reason codes and never raises on invalid input.
- Creation times now may equal their observation's collection time but may not be later by even one 100 ns tick. This is checked for the process and independently for every listener row.
- Win32-forbidden path characters (`< > " | ? *`), NUL/control characters (`U+0000`-`U+001F`, `U+007F`) and non-string dictionary keys are refused at every schema level. Existing colon/device/dot/UNC rules remain; valid Unicode path names remain accepted; no filesystem access was added.
- Path identity comparisons now lowercase ASCII A-Z only and preserve every non-ASCII code point exactly. This keeps existing ASCII-only case-insensitive behavior while refusing distinct supplied names such as sharp-s versus `ss`; it is not Unicode normalization or filesystem identity. Some equivalent Unicode casing can therefore be refused deliberately.
- `Tools/test-persistence-identity.py` contains 69 deterministic injected-evidence tests. It covers valid IPv4/IPv6 exact and wildcard cases, explicit same-owner dual-stack rows, multiple rows for one owner, foreign listener with the intended editor alive, PID reuse, changed project/engine, failed collection, missing listener, hostname/remote endpoints, multiple owners, ambiguous other-family wildcard coverage, stale/reversed/gapped observations, changed snapshots, malformed/unknown/missing fields, booleans-as-integers, future start times (including the equality boundary and one-tick/one-listener regressions), project/engine path characters and controls at each identity level, sharp-s-versus-ss project/engine collisions (including listener-only mismatches), identical Unicode paths, existing ASCII-only case variations, mixed dictionary keys and relative/UNC/device/dot/forward-slash/wrong-extension paths.
- `Tests/catalog.json` gains exactly one row: `test-persistence-identity`, path `Tools/test-persistence-identity.py`, tier `offline`, timeout 30 seconds, acceptance `not_mapped`. All other rows are preserved.
- No caller, helper, fixture, config, lease, process/network/filesystem collector, editor or engine path is wired. Only Python's standard library is used.

Validation:
- `python Tools/test-persistence-identity.py` — exit 0; `Ran 69 tests ... OK`.
- `python Tools/execute-tests.py --profile offline --execute` — exit 0; `.local/executions/59e97bd502bd4320b2df4f20882cf6dd/report.json`; aggregate log records `PASS: test-persistence-identity` and every other offline check passed.
- Candidate-bound focused/offline runs and the full-base R04 scope gate are recorded in the submission after the commit. No engine/editor/build/MCP/recovery command was run.

Limitations: passing proves only that the supplied evidence is internally consistent. It does not prove live ownership, does not secure a future connection, and is subject to a residual race between the final snapshot and any later connect. Path identity uses ASCII-only case folding and preserves non-ASCII code points exactly; it is not Unicode normalization or filesystem identity, so some Unicode casing variants can be refused. A refusal is preferable to accepting distinct supplied identities. A trusted collector must supply canonical Windows paths and truthful, bounded process/TCP records; the module neither resolves symlinks nor queries the OS. All P1-P6 persistence work remains `proposed_unapproved`.

Rollback: remove `Tools/persistence_identity.py`, `Tools/test-persistence-identity.py`, the one catalog row, and this evidence. No existing caller or production file is affected.

### Checkpoint 8: trusted identity collection contract

Task R04 checkpoint 8, claim `8dfd55914dce466ea7d0a6de64edc3a6` (owner `dsh-r04-collector-01`). Approved base `577fdaafed023485adb4e1d3f65c4d6218c5eef3`; branch `task/r04-collector-contract`; worktree `C:\ExampleDev\SampleRoot005\r04-vcs`. The candidate is the commit containing this evidence; its full SHA is returned in the submission message. R04 remains incomplete.

Deliverable: `docs/audit/persistence-collector-contract.md` specifies a read-only Windows collector for the accepted validator. The first slice is pure orchestration with an injected typed backend and no Windows adapter: process, listener, path and command-line reads return `ReadResult` values; orchestration owns deadlines, identity matching, snapshot assembly and validator invocation. A future adapter is separately described with `GetProcessTimes`, `Win32_Process`, `CommandLineToArgvW`, `Get-NetTCPConnection`, `GetFileInformationByHandle`, path-component checks and bounded child-process calls.

Key limits recorded:
- Expected identity comes from the trusted launch record, never from observations. The proposed record binds creation time to the `Start-Process -PassThru` process handle, verifies exit/PID reuse and captures file IDs before admission. Existing leases lack these fields; reused editors with missing provenance refuse rather than being upgraded from observations.
- Deadline enforcement belongs to the adapter: each blocking read runs in its own child process with the aggregate remaining budget; timeout/unknown completion is refusal and only the collection child may be terminated, never the editor. Child PowerShell uses `-ErrorAction Stop`/`$ErrorActionPreference='Stop'`, inspects `$Error`, and writes structured output only on clean completion. A clean empty listener result, provider failure and incomplete/unknown output are distinct.
- Clock domains are separate: integer monotonic nanoseconds with an arbitrary epoch for deadlines/durations, canonical UTC for calendar evidence, never cross-compared. Policy defaults/bounds are explicit; deadline equality refuses, equal UTC snapshot timestamps are allowed, clock rollback refuses, and there is no implicit sleep.
- The public contract fixes expected-record/file-ID keys, endpoint duplication, policy types/defaults and separate success/refusal results. Backend `ReadResult` stays internal; unknown fields, bool-as-int, malformed results and endpoint mismatch are structured refusals with no raw dumps.
- Rechecks compare listener rows as a normalized multiset preserving duplicates, and process recheck includes parsed project/path/file IDs, not only PID. Foreign owner rows are refused immediately; missing/zero link counts or unreadable flags refuse.
- Path checks walk every component for reparse points, reject 8.3/short-name substitution and refuse `nNumberOfLinks > 1`; file-ID equality does not reject hardlinks. Aliases are rejected, not resolved, and the open-by-path engine file is not claimed to be the loaded image. FILETIME's 100 ns representation is not claimed as physical clock resolution.
- Schema v1 cannot carry auditable file IDs. A future `turbomcp.persistence-identity/2` with `project_file_id`/`engine_file_id` volume/file-index fields is the exact proposed adjustment if file identity must be auditable.
- The collector refuses unreadable/partial process or TCP enumeration, PID reuse, process exit, listener replacement, unsupported addresses, owner 0/4 and more than 32 rows. CIM/TCP completeness is not atomic and is not overclaimed.
- A validation pass is evidence consistency only; it never authorizes connection, mutation, stop, restart or a persistence claim. The residual race remains.
- The proposed first slice is `Tools/persistence_collector.py`, `Tools/test-persistence-collector.py` and one `Tests/catalog.json` offline row, with fake-backend tests only. No caller, launch-record, lifecycle, settings or persistence work is included.

Validation:
- Source/docs review only; no live process, socket, command line, editor, build, MCP or recovery operation was run.
- `python Tools/execute-tests.py --profile offline --execute` — exit 0; `.local/executions/b0b4d32b64b247bebf05de12e97b55e6/report.json`; all offline checks passed.
- Candidate-bound offline evidence and the full-base scope gate are recorded in the submission after the commit. All collector/launch-record steps remain `proposed_unapproved`.

Rollback: remove the new report and this evidence only. No existing file or caller is changed.

### Checkpoint 9: offline collector orchestration

Task R04 checkpoint 9, claim `23c37940f4314c4cbd8a14d451b09773` (owner `dsh-r04-collector-02`). Approved base `f89114d15f63cda336cf7b86260533c217cb2c88`; branch `task/r04-collector-offline`; worktree `C:\ExampleDev\SampleRoot005\r04-vcs`. The candidate is the commit containing this evidence; its full SHA is returned in the submission message. R04 and Monday persistence acceptance remain incomplete.

Implementation:
- `Tools/persistence_collector.py` adds the pure `collect_pair(expected_record, endpoint, backend, policy)` orchestration and a named `default_policy()` factory. It validates the exact expected/policy records, enforces separate monotonic-ns deadlines and canonical UTC age/gap checks, reads process/project/path/listener evidence only through the injected backend, performs independent final rechecks, compares listener multisets, builds two v1 observations and calls `persistence_identity.validate_evidence`.
- File IDs are exact unsigned 32-bit components including zero; `link_count` must be exactly one. Foreign owner PIDs refuse immediately without reading unrelated command lines. Missing/zero link counts, reparse/short flags, path/file-ID changes and malformed backend records refuse.
- `Tools/test-persistence-collector.py` contains 59 deterministic fake-backend tests covering valid zero/uint32 file IDs, sub-second and non-multiple-second policies, policy-derived bounds, both-snapshot ages, age/gap equality and one-tick boundaries, FILETIME epoch conversion, malformed records at each layer and discriminant shape, backend exceptions, no further reads after refusal, foreign-listener behavior, reordered/duplicated listener rows, all-sampled UTC rollback, final process/path changes, endpoint duplication, policy/schema strictness, input immutability and output aliasing.
- `Tests/catalog.json` adds exactly one offline row: `test-persistence-collector`, 30-second timeout, `acceptance=not_mapped`.

Limits:
- This slice is pure orchestration only. No Windows adapter, launch seam, subprocess, process/socket/filesystem collection, sleep, launch/restart/kill, caller, lease, config or engine change was implemented.
- The backend must enforce the absolute deadline it receives; this pure module cannot interrupt a backend that never returns.
- Passing proves supplied evidence consistency only, not live ownership, future connection safety or persistence.
- All implementation remains `proposed_unapproved`; the Windows adapter and launch record are still blockers.

Validation:
- `python Tools/test-persistence-collector.py` — exit 0; `Ran 59 tests ... OK`.
- `python Tools/execute-tests.py --profile offline --execute` — exit 0; `.local/executions/5cc045305a7e48fca0457f3feef82cb8/report.json`; aggregate log records `PASS: test-persistence-collector` and all other offline checks passed.
- Candidate-bound focused/offline evidence and the full-base scope gate are recorded in the submission after the commit. No live engine/process/network action was run.

Rollback: remove `Tools/persistence_collector.py`, `Tools/test-persistence-collector.py`, the one catalog row and this evidence. Existing `Tools/persistence_identity.py` and all callers are unchanged.

### Checkpoint 10: bounded Windows process reader

Task R04 checkpoint 10, claim `62c5510de89244cc947e188b9a4237fc` (owner `dsh-r04-process-01`). Approved base `509e9537566d9f425c480fa8884adb8ec96d8758`; branch `task/r04-process-reader`; worktree `C:\ExampleDev\SampleRoot005\r04-vcs`. The candidate is the commit containing this evidence; its full SHA is returned in the submission message. R04 remains incomplete.

Implementation:
- `Tools/persistence_process_reader.py` adds `read_process(pid, deadline_ns, *, runner=None, clock=None, platform=None)` and `ProcessReader`. It validates positive uint32 PID and integer deadline, refuses non-Windows and expired/equal deadlines without spawning, rechecks the tracked monotonic clock after executable/script resolution and immediately before `Popen`, rejects backward/malformed clocks, invokes only the fixed checked-in PowerShell script through the resolved Windows installation path with an exact argv, uses hidden/no-window startup, bounds child stdout/stderr while reading (64 KiB each), enforces/rechecks the monotonic deadline, rejects partial/extra/malformed/invalid-UTF-8 output, and reaps only its collection child.
- The runner now places every post-spawn operation under guaranteed cleanup, including first/second thread-start failures, clock/read exceptions, overflow, timeout and early exit; it closes pipes and reaps the child on all recoverable paths and reports cleanup failure honestly.
- Wire parsing requires integer schema exactly 1, rejects duplicate keys and nonfinite constants, handles excessive nesting/decoder failures as unknown, and rejects extra output. The fixed script selects BOM-free UTF-8 via a StreamWriter over standard output.
- `Tools/read-persistence-process.ps1` accepts only `ProcessId`, uses strict terminating errors, queries one PID through CIM, opens a same-process `PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE` handle, reads creation FILETIME before and after CIM, checks zero-time `WaitForSingleObject` liveness before and after CIM, requires well-typed PID/executable/command-line data, closes the handle on every path, and emits one versioned JSON result. `WAIT_TIMEOUT` means running; `WAIT_OBJECT_0` means exited (including exit code 259); `WAIT_FAILED`/other refuses.
- The earlier zero-exit-FILETIME liveness rule was an **integrator correction**, not a worker defect. Microsoft documents `lpExitTime` as undefined while the process is active; that predicate and its comment were removed. Liveness now uses the official `WaitForSingleObject` semantics and the same retained handle.
- `Tools/test-persistence-process-reader.py` contains 37 offline tests with injected runner/clock plus mocked Popen, streams and threads. It executes the actual runner control flow for thread-start failures, clock/read exceptions, overflow, timeout, early exit, wait/kill failures, stuck drains and pipe closure. It also verifies exact resolved command construction, pre-spawn deadline/setup and clock rollback/malformed checks, no spawn on invalid/non-Windows/expired requests, deadline equality/after-return refusal, EOF/nonzero/invalid UTF-8/duplicate keys/schema 1.0/nonfinite/nesting/shape/overflow/provider taxonomy, escaped Unicode decoding, early exit, output aliasing, and fixed script/runner source constraints.
- `Tests/catalog.json` adds one offline row: `test-persistence-process-reader`, 30-second timeout, `acceptance=not_mapped`.

Limits:
- No live process, CIM, PowerShell, editor, build, socket or engine execution was authorized or performed.
- Offline tests prove request construction, mocked runner lifecycle, deadline/wire handling and source constraints; they cannot prove native API behavior, actual CIM results, race reduction, FILETIME values on a real process, hidden startup/reaping behavior, UTF-8/script execution on Windows, or script syntax.
- The script contains no `Start-Process`, but Windows PowerShell, Add-Type, CIM/WMI or other internals may create helper processes. Offline source inspection cannot establish that they do not; this remains an unresolved lifecycle blocker and is not claimed as child-free.
- `read_process` returns only the existing `ProcessRead` shape. It does not infer ownership, upgrade a lease, or certify identity of future reads. Retained-handle checks reduce but do not eliminate races.
- The listener/path/parser adapter, launch record and caller integration remain unimplemented.

Validation:
- `python Tools/test-persistence-process-reader.py` — exit 0; `Ran 37 tests ... OK`.
- `python Tools/execute-tests.py --profile offline --execute` — exit 0; `.local/executions/975c9df86efd49ff984efaeee9cd8d3b/report.json`; aggregate log records `PASS: test-persistence-process-reader` and all other offline checks passed.
- Candidate-bound evidence and full-base scope gate are recorded in the submission.

Rollback: remove the two new tools, the one catalog row and this evidence. Existing collector/validator and production callers remain unchanged.

### Checkpoint 11: process-reader lifecycle admission

Task R04 checkpoint 11, claim `b3223a4a39b04b599d4ab4bf14029a78` (owner `dsh-r04-lifecycle-01`). Approved base `2d120a9e282e149b7e0f2368a4acf44f5035fd03`; branch `task/r04-process-lifecycle`; worktree `C:\ExampleDev\SampleRoot005\r04-vcs`. The candidate is the commit containing this evidence; its full SHA is returned in the submission message. R04 and live process-reader use remain incomplete.

Deliverable: `docs/audit/process-reader-lifecycle.md` records the bounded source investigation. Windows PowerShell 5.1 Add-Type exposes CodeDom/compiler parameters; the .NET Framework C# CodeDom compiler names `csc.exe` and its Executor uses Process/CreateProcess, so Add-Type can create a compiler child. Whether this host actually spawns it and whether it inherits the PowerShell result pipes remain unverified. The current runner cleans up only its direct Popen child, so compiler-descendant survival on timeout/parent death is not excluded. CIM/WMI is a shared external service, not an owned child.

Decision: select Job Object containment of the dedicated collection process and its descendants for this scope. This is a scoped choice, not a claim that every narrower compiler-free alternative was disproved. Use a new fixed `execution_runtime.py --persistence-worker` (not generic `--worker`) that accepts one complete bounded `{"schema":1,"pid":<uint32>}` message, requires parent EOF and rejects trailing bytes/second requests before launch, derives the fixed PowerShell executable/script locally, and waits for assignment before authorization. Add checked Job methods (`terminate_checked`, `active_processes`, `close_checked`, `is_process_in_job`) without changing `run_owned`; one shared cleanup deadline bounds polling, worker wait and drains. Assignment/nested-job failure, nonzero active count, query/close/wait failure or unproven compiler containment refuses. Generic `--worker` and shared CIM/WMI/unrelated processes are untouched.

Compiler observation: polling can miss a short-lived `csc.exe`; absence of an observed child is not proof. The supervisor-only synthetic suite must use Job completion-port new-process notifications while compiling a harmless synthetic Add-Type class. If no compiler child or its job membership can be established, admission remains blocked, not passed. The current direct-child cleanup is offline-tested, with native/pipe limitations.

Mock fidelity: the current fake `Thread.join` treats unstarted joins as a no-op; real Python `Thread.join` raises before start. This affects cleanup_failed classification in thread-start-failure mocks and is not proof of a child leak. The proposed test-only repair is recorded. The fixed PowerShell script's real parsing/API/UTF-8/handle behavior remains unverified.

Test placement: offline mocks stay in the existing `tier=offline` `Tools/test-persistence-process-reader.py`. Native admission uses a new `persistence-native` profile: `python Tools/execute-tests.py --profile persistence-native --execute`, mapping only to new `Tools/persistence-job-acceptance.py` with no engine, build, editor, `run-suites.ps1` or acceptance asset. The suite is not named `test-*.py`; no `Tests/catalog.json` change is proposed. Offline mocks never certify native containment; missing/failed native harness is a blocker, not implementation acceptance.

All implementation remains `proposed_unapproved`; no code, test, catalog, lease, status, routing, scope or release change was made. No live process, PowerShell, CIM, native, editor, build or recovery operation was run.

Validation:
- Source/docs review only; no live processes, PowerShell, native calls, CIM, editor, build or recovery.
- `python Tools/execute-tests.py --profile offline --execute` — exit 0; `.local/executions/cd845db0fef041aaa97385d6f75992fc/report.json`; all offline checks passed.
- Candidate-bound evidence and full-base scope gate are recorded in the submission.

Rollback: remove the new report and this evidence only.

### Checkpoint 12: offline job containment

Task R04 checkpoint 12, claim `6da779ef16204e8a82c3a64a8aa9cb56` (owner `dsh-r04-containment-01`). Approved base `d2759a8d0341e2f44c9af6cfdfc20487eaa13261`; branch `task/r04-job-containment`; worktree `C:\ExampleDev\SampleRoot005\r04-vcs`. The candidate is the commit containing this evidence; its full SHA is returned in the submission message. Live admission and native harness execution remain blocked.

Implementation:
- `Tools/execution_runtime.py` adds `BASIC_ACCOUNTING`, exact checked Win32 bindings for `QueryInformationJobObject`/`IsProcessInJob`, and checked `terminate_checked`, `active_processes`, `close_checked` and `is_process_in_job` methods. Existing `WindowsJob.close`, `run_owned` and generic `--worker` semantics are preserved.
- The new `--persistence-worker` accepts one complete UTF-8 request of at most 512 bytes, requires parent EOF, and accepts exactly `{"schema":1,"pid":<positive uint32>}`. It rejects empty EOF, additional bytes/objects, duplicate keys, nonfinite constants, malformed fields, oversized input and non-integer/bool schema or PID before launching. It derives the fixed PowerShell executable and checked-in script locally; no argv/executable/script request input is accepted and it does not invoke the public reader.
- `Tools/persistence_process_reader.py` creates a fresh non-inheritable `KILL_ON_JOB_CLOSE` job, launches the fixed worker hidden, assigns and verifies membership before a bounded authorization thread writes and closes the sole request, waits for drain EOF before closing streams on the normal path, and uses one shared five-second cleanup deadline for checked termination, active-process polling, worker wait and drain joins. It queries while the job handle is open, then closes it; assignment/membership/authorization/wait/query/close failures refuse. Direct child termination is used only when assignment/membership failed and the worker is not contained. No process tree or shared service is enumerated.
- `Tools/test-persistence-process-reader.py` has 58 offline tests with fake job/API/process/streams/clock. It covers assignment-before-send, failed assignment/membership, complete/short/blocked/failing authorization, deferred stdout/stderr EOF, expired/short/crossing/shared-budget waits, blocked no-drain close, expiry before/after spawn/authentication and at the loop, constructor/thread-start/drain failures, active-count transitions/query failure, terminate/close/wait/kill failures, shared cleanup deadline, overflow, exact job/request/pipe cleanup, strict worker request parsing/EOF, and legacy `close`/`run_owned`/`--worker` compatibility. `FakeThread.join` now raises for unstarted threads, and thread-start cleanup classification is asserted honestly.

Limits:
- No live process, PowerShell, native, CIM, editor, build, recovery or subprocess-native test was run. All containment evidence is mocked.
- The checked methods are offline-tested only; actual Win32 behavior, nested-job assignment, job accounting, hidden startup, inherited-pipe behavior, parent-death cleanup and Add-Type compiler containment remain unverified. Bounded drain/close helpers are daemon threads; mocks cannot prove native cancellation or that an OS-blocked helper has exited, so unconfirmed cleanup refuses success.
- The new worker and cleanup are not wired into production callers beyond the existing process-reader path. The native profile/suite and supervised acceptance remain deferred.
- No live containment claim is made.

Validation:
- `python Tools/test-persistence-process-reader.py` — exit 0; `Ran 58 tests ... OK`.
- `python Tools/execute-tests.py --profile offline --execute` — exit 0; `.local/executions/e5068ab1d664421abbe3b309f0cf2ba6/report.json`; all offline checks passed.
- Candidate-bound evidence and full-base scope gate are recorded in the submission.

Rollback: remove the containment methods, worker mode and orchestration changes from the three allowed tools, restore the previous runner semantics, and remove this evidence. No catalog, script, production caller or native harness is added.

Integration review accepted checkpoint 12 candidate `6677350a67b63a0c6e857821c357146d3d5904e3` for offline containment behavior. Full-base scope gate passed; independent offline execution `bd8793a3694e402998756c1481f9a88b` passed. The report hash and retained claim matched. Deadline-capped waits and bounded no-drain closes resolve the requested corrections. No native containment, compiler observation, parent-death or live reader admission is certified. R04 remains incomplete; checkpoint 13 authorizes harness implementation and offline tests only.


### Checkpoint 13: synthetic containment acceptance harness

Task R04 checkpoint 13, claim `231a91e08a4042099baca8161417f0c4` (owner `dsh-r04-native-harness-01`). Approved base `9fe2615bb974b3a06b637cb0bfd2ab606ace3e96`; branch `task/r04-native-harness`; worktree `C:\ExampleDev\SampleRoot005\r04-vcs`. The candidate is the commit containing this evidence; its full SHA is returned in the submission message. Native execution and live admission remain incomplete and were not performed by this worker.

Implementation:
- `Tools/execute-tests.py` adds only the fixed `persistence-native` profile. It maps to exactly `Tools/persistence-job-acceptance.py`, rejects suite selection, leaves `engine=''`, and bypasses engine resolution, preflight, build, fixture snapshot/recovery, `run-suites.ps1`, editor startup and port resources. Existing profiles are unchanged.
- `Tools/persistence-job-acceptance.py` is a new synthetic supervisor-only harness. Import is process/native-free. `main` requires the execute-tests supervisor context, including matching repo, empty engine, live-ancestor PID and token; malformed or unsupervised invocation refuses before live work. The fixed helper modes cover an actual bounded timeout with owned descendant identity, ordered inherited-pipe handshake plus bounded drain/EOF, supervisor death with retained creation identity and handle, pre-assignment stdin EOF through the real fixed persistence worker, harmless Add-Type observation with immediate per-NEW_PROCESS handle/membership/image/creation evidence and separate compile result, and an owner-controlled unrelated sentinel with unique per-run scratch. All inner jobs are fresh `KILL_ON_JOB_CLOSE` jobs; cleanup requires explicit observed active-process zero while the job handle is open and reports exhausted budget, query/close/wait, close-helper and drain failures. Cleanup runs in every case finally, including assignment, handshake, parse and early-return failure paths; cleanup failures change the final case status. Bounded close/read/join helpers share the cleanup deadline. Sentinel teardown enters ownership cleanup immediately after Popen and uses one deadline for stdin completion, wait/kill fallback, both output closes and helper joins. Each independent step is guarded so a raised stdin/close/helper exception does not skip remaining cleanup, and every teardown error forces suite failure. _write_stdin_bounded and _close_bounded use monotonic seconds consistently (no nanosecond conversion), matching Thread.join; joins are capped by the exact remaining seconds. _write_stdin_bounded refuses expired admission before starting a writer, requires a real non-bool integer equal to the byte count, refuses None/short/negative/other values without flushing or authorizing, rejects completion after the deadline, and closes owned stdin through the bounded path. All helper entry modes validate supervisor admission before file/process actions. Minimal injection seams were added to the timeout case and completion drain for deterministic seconds-based tests. The new close-failure test exposed that process_evidence returned an identity alongside a failed CloseHandle; it now returns no identity and an error, so close failure cannot pass. Parent-death cleanup authorizes termination only after positive creation-identity verification on the retained handle; mismatched/missing identities close without termination, while verified fallback uses that same handle. One cleanup deadline covers child fallback, supervisor wait, pipes and helper joins, and all cleanup failures are retained. No global process search, name-based kill or real command-line inspection is used.
- `Tools/test-persistence-process-reader.py` adds additive offline mocks for profile command selection/no engine path, suite-selection refusal, malformed/unsupervised admission, aggregation of missing/blocked evidence, unsupervised main refusal, cleanup truthfulness/query failure, per-mode helper admission, ObservedJob acquisition/close failures, full Add-Type and pipe orchestration success/failure, terminal authorization timeout/short-write failure, strict None/zero/partial/bool/exception/exact write results, ordered handshake/partial output, exception cleanup propagation, preassignment end-snapshot blocking, query access mask/ABI checks, sentinel readiness/teardown exceptions with one shared budget, injected helper/close/join exceptions, expired write admission, second-based join caps (~0.01 s remaining and 0.05 s cap), completion-after-deadline refusal for write/close, and shared close budget, and full parent-death identity mismatch/missing, success, fallback, cleanup failure and shared-deadline cases, full timeout actual/early-exit/membership/active/authorization/cleanup failure paths, process-evidence open/membership/image/creation/close failures, drain callback ordering/timeout/API errors, and Add-Type absent/missing/nonzero compiler-result paths. Existing 58 assertions are preserved; the file now runs 91 offline tests.

Limitations:
- No native acceptance execution, engine, editor, build, PowerShell, CIM, subprocess-native or live collection was performed. All evidence for checkpoint 13 is offline.
- Every acceptance case is a **synthetic analogue**, not production-reader-path evidence. `reader_path_evidence` is false in the harness results.
- Add-Type compiler observation depends on job completion notifications and retained executable identity. A missing/short-lived compiler or unavailable identity is BLOCKED, never passed. Job NEW_PROCESS alone is not executable identity.
- Supervisor death uses a retained child handle; marker absence is not used as proof. PID reuse and parent-death race windows remain native limitations.
- The `ObservedJob` completion-port code is a synthetic harness seam, not a modification or proof of the production `WindowsJob`.
- Native harness execution, parent-death/Add-Type results and live admission remain integration-owner work.
- Remaining debt: preassignment wiring is offline-only and the native suspended-launch/accounting/positive-control run remains pending; no native acceptance is claimed. Native harness/teardown and native compiler observation remain unexecuted. Timeout end-to-end and compiler observation now have injected offline coverage.

Integrator-owned native evidence (not worker execution or acceptance):
- Exact candidate `29560ec3abf1b41389bf04b1fe27130d689eadb7`, tree `c9fe57732716710d852a4de4d403796341180ef8330fe6dad51a360b64405358`.
- Integrator ran exactly `python Tools/execute-tests.py --profile persistence-native --max-seconds 180 --execute`. Evidence: `.local/executions/e1be5b01bb3248fbb78427d0dba4ce19/report.json` and `00.log`; duration 32.373s, child exit code 2, no timeout, `engine` empty.
- Supervisor status was `failed` because the harness status was `blocked`; this is not a native acceptance pass. Passed cases were timeout_descendant (active count two), inherited_pipe (stdout and cleanup), parent_death (verified retained child signaled), add_type_compiler (csc.exe with membership/creation/image plus cvtres.exe and compile result zero), and unrelated_sentinel.
- `preassignment_eof` remained BLOCKED: the real worker refused empty EOF, membership was assigned, and the final active snapshot was zero. The preassignment interval was not observed. No production-reader path, Unreal, editor or build was certified.

### Preassignment observation design (proposal only; no implementation)

Correction status: the earlier NEW_PROCESS-only absence argument is superseded. Microsoft states that job completion-port messages, except notification-limit messages, are not guaranteed to arrive; an empty queue cannot prove that no event occurred. Notifications remain useful positive evidence only. The primary absence proof below is fresh observer-job lifetime accounting.

Required property:
- Within a fresh, controlled, no-breakaway observer job, prove that the fixed suspended worker was the only process associated for the job's lifetime. This is bounded to process association with that job, not a general statement about every process on the machine.
- Distinguish this from “no surviving owned descendant.” A short-lived child can terminate before an ActiveProcesses sample; ActiveProcesses alone cannot prove absence of a launch. TotalProcesses counts processes associated during the job lifetime, including terminated processes and failed associations due to limits.

Chosen smallest viable design: suspended worker plus an owner-created fresh observation/containment job, using job lifetime accounting as the primary proof and completion messages only as positive corroboration.
1. The harness creates a fresh anonymous `KILL_ON_JOB_CLOSE` job before the worker exists. It sets neither `JOB_OBJECT_LIMIT_BREAKAWAY_OK` nor `JOB_OBJECT_LIMIT_SILENT_BREAKAWAY_OK`. It associates the completion port while the job is inactive. Initial lifetime totals must be zero: `TotalProcesses == 0`, `ActiveProcesses == 0`.
2. Launch exactly the fixed worker command (`sys.executable`, the repository `execution_runtime.py`, and the literal `--persistence-worker`) suspended with `CREATE_SUSPENDED | CREATE_NO_WINDOW`. Use explicit inheritance, not arbitrary command execution: an inheritable-handle list limited to the worker's stdin read end and, if needed, stdout/stderr write ends; `STARTF_USESTDHANDLES`; `bInheritHandles=TRUE`; no shell, no caller-supplied argv/env, no breakaway flag. The harness owns the process and thread handles, both pipe ends and the observer job/port.
3. Assign the suspended worker to the observer job and verify membership. Query the job while the handle is open; require `TotalProcesses == 1`, `ActiveProcesses == 1` as the baseline. Do not resume while the baseline is inconsistent. The worker is the sole expected association.
4. Resume the worker. Release stdin EOF only after assignment, membership, and the baseline accounting query are complete. Do not use fixed sleep as readiness.
5. Drain `JOB_OBJECT_MSG_NEW_PROCESS` messages throughout the interval as positive evidence. Any non-worker PID is a failed observation. Absence of a message is not absence evidence because delivery is not guaranteed.
6. Observe worker exit and record its exit code with the retained process handle. Close the exact owned worker process handle (and any remaining thread handle) to release process references, then query the same open job handle before close: require `ActiveProcesses == 0` and `TotalProcesses == 1`. The lifetime total, not the empty queue and not the final active count alone, is the primary no-additional-associated-process proof. The process handle must be closed before the final query because `ActiveProcesses` is decremented only when the terminated process exits and all references to it are released; the job handle remains open for the query.
7. Close the job only after the final query. `KILL_ON_JOB_CLOSE` contains any survivor; close is cleanup, not proof.

Primary references:
- [Job Objects](https://learn.microsoft.com/en-us/windows/win32/procthread/job-objects): child association, no-breakaway behavior, lifetime accounting including terminated processes, and the explicit notification-delivery limitation.
- [JOBOBJECT_BASIC_ACCOUNTING_INFORMATION](https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-jobobject_basic_accounting_information): `TotalProcesses` counts all processes associated during the job lifetime, including terminated ones and failed associations due to limits; `ActiveProcesses` is current state only.

Exact accounting and evidence:
- Use the existing harness-local `BASIC_ACCOUNTING` structure, which already contains both `TotalProcesses` and `ActiveProcesses`. Add one harness-local `accounting()` query path using `QueryInformationJobObject(handle, 1, byref(info), sizeof(info), None)` and return both fields from the same snapshot. No production reader/runtime edit is needed.
- Evidence fields: `worker_suspended`, `observer_job_created`, `initial_total == 0`, `initial_active == 0`, `assigned_before_resume`, `membership_verified`, `baseline_total == 1`, `baseline_active == 1`, `resumed`, `eof_after_assignment`, `worker_refused` (exit code 2), `worker_handle_closed_before_final_query`, `final_total == 1`, `final_active == 0`, `query_before_close`, `new_process_messages`, `unexpected_notification_count`, `port_idle`, `port_api_error`, `observer_cleanup`.
- Pass only when ordering flags hold, the worker refused empty EOF, initial total/active is 0/0, baseline is 1/1 and final is 1/0 as above, the worker process handle was closed before the final query, both accounting queries succeeded before job close, and cleanup succeeds. Notifications are recorded but are not required for the absence proof.
- Extra association: `final_total > baseline_total` (normally 2 or more) is a failed observation even if every notification was dropped and `final_active == 0`.
- Blocked: initial/baseline/final accounting query failure, initial total not zero, baseline total not one, baseline active not one, final total less than baseline, final active not zero, query after close, membership/assignment failure, or inconsistent counters. A final total of one with active zero plus a clean query is the only bounded negative.

Port behavior and limits:
- `GetQueuedCompletionStatus` returning zero with `ERROR_TIMEOUT` (258) is ordinary idle drain and is not itself proof of absence or a failure of the accounting proof.
- Any other completion-port API error is recorded as `port_api_error`; it is not treated as an idle proof. Because accounting does not depend on notification delivery, a port API error does not by itself replace a sound accounting result, but it is reported and prevents any claim that notification corroboration was complete.
- A non-worker `NEW_PROCESS` is always a failed observation, whether or not accounting later agrees.
- Counter width: the Windows fields are `DWORD` (32-bit). In this bounded fresh-job test the expected total/active pairs are 0/0 initially, 1/1 after assignment and 1/0 after exit; the design does not claim universal arithmetic proof, long-running rollover behavior, or a general guarantee for external brokers, arbitrary process creation, or jobs that permit breakaway. A value outside the small expected set, or any apparent inconsistency, is blocked rather than normalized.
- The lifetime proof covers processes associated with this fresh controlled job. It does not prove absence of a process deliberately created outside the job through breakaway/explicit job lists, a process created by another process on the worker's behalf, or any process not associated with the observer job. The fixed worker does not use those mechanisms; this remains an explicit limit.

Ownership and setup-failure cleanup:
- Harness owns the observer job, port, suspended worker process handle, primary thread handle, worker stdin read/write handles and stdout/stderr pipe handles.
- One setup deadline covers job/port creation, suspended launch, assignment, membership and baseline accounting. One observation deadline covers resume, EOF release, worker exit, release of the owned worker process/thread handles, and the final accounting query while the job handle remains open. One cleanup deadline covers wait/kill, pipe closes, port drain/close, job close and thread join.
- On any setup failure before a validated resume, terminate the exact owned suspended process with `TerminateProcess` on its retained handle; do not resume unvalidated code and do not use `ResumeThread` as a cleanup fallback. Then close thread, process, pipe, port and job handles with the cleanup deadline. A termination/close failure is reported, and job close still runs as containment fallback.
- On failure after resume, terminate the exact owned worker handle, wait bounded, close the worker process/thread handles, then close pipes, port and job. No global search, name match, bare-PID kill or later PID lookup is used.

Expected controls:
- Real-worker negative: baseline total one, final total one, active zero, worker exit 2 after empty EOF, clean queries and cleanup. This is the only path that may become a reviewed pass.
- Short-lived positive helper: a helper that creates and reaps a child before reading EOF must raise lifetime `TotalProcesses` to two even when `ActiveProcesses` returns to zero; the case fails. Notifications may be present or absent and are not the proof.
- Dropped-notification offline case: final total two with no `NEW_PROCESS` message must refuse.
- Port idle: zero with `ERROR_TIMEOUT` is normal idle; no absence claim from it.
- Port API error: recorded; accounting result still governs, with notification corroboration explicitly incomplete.
- Additional failures: suspended creation, handle-inheritance, assignment, membership, baseline query, resume, EOF release, final query, counter inconsistency and cleanup failure.

Alternatives and tradeoffs:
- NEW_PROCESS-only drain: rejected; delivery is not guaranteed, so an empty queue is not an exhaustive negative proof.
- Final ActiveProcesses only: rejected; it cannot distinguish “never associated” from “associated, terminated, and reaped.”
- Observer job without suspension: rejected; the worker could launch before assignment.
- ETW/general process monitor: broader permissions, loss and lifetime surface; rejected as disproportionate.
- Instrumenting the worker: tests a reporting path rather than independent job accounting and would touch production code; rejected.

Proposed files (not permission to edit in this dispatch):
- `Tools/persistence-job-acceptance.py` — suspended launch with explicit handle inheritance, observer job/port setup, accounting snapshot, EOF ordering, cleanup and preassignment evidence.
- `Tools/test-persistence-process-reader.py` — deterministic fake Win32/job/accounting/port/process/pipe/clock tests.
- `todo/roadmap-r04.md` — Completion evidence only.
- `Tools/execute-tests.py` — no change expected.
- No production reader/runtime, PowerShell, catalog, engine or profile change is proposed.

Allowlist check:
- The four-file allowlist is sufficient: the accounting query and suspended observer belong in the existing harness and its tests; `BASIC_ACCOUNTING` already exposes both fields.
- If implementation were restricted to `Popen`-only without suspended launch, explicit inherited-handle control or both accounting fields, the exact missing seam would be a harness-local `accounting()` query returning `TotalProcesses` and `ActiveProcesses`, plus suspended-launch/resume and handle-list control. Without those, the interval cannot be proven and preassignment must remain blocked.

Implementation slice A (reviewer-approved; accounting and decision only):
- `ObservedJob.accounting()` performs one checked `QueryInformationJobObject` class-1 call on the open job handle and returns the same snapshot's binary `TotalProcesses` and `ActiveProcesses`. Existing `active_processes()` callers and behavior are unchanged.
- `preassignment_accounting_decision()` is a pure harness-local decision helper. It requires validated ordering, initial `0/0`, suspended baseline `1/1`, final `1/0`, worker refusal and successful cleanup. It rejects malformed, missing, inconsistent and query-failed inputs; an extra lifetime total fails even with no notifications and cannot be overridden by active zero. Notifications are supporting positive evidence only. A completion-port API error is retained as non-clean diagnostic evidence but does not replace the narrow accounting result.
- Deterministic offline tests cover exact accounting fields, query failure, initial/baseline mismatch, final extra count, dropped-notification total two, final active nonzero, missing flags/refusal/cleanup, unexpected notification, API-error diagnostics and the expected sequence. The decision helper treats the DWORD fields as bounded 0..0xFFFFFFFF values and does not claim rollover or universal arithmetic proof.
- `_case_preassignment_eof` was intentionally left unwired in slice A. Slice B now wires the helper to the suspended observer and keeps the native case unaccepted until the supervisor run is independently reviewed.
- Deferred to slice B at the time: suspended launch, explicit handle inheritance, assign-before-resume, accounting snapshots around the live worker, setup-failure termination without resume, and native positive/negative controls. Slice B now implements the harness-local pieces; native controls remain pending.
- Integrator evidence remains tied to exact candidate `29560ec…` and its five passed cases plus blocked `preassignment_eof`; none of this new code is certified by that older run.

Implementation slice B (reviewer-approved; offline validation only):
- `launch_suspended()` is a harness-local fixed-mode launcher. It builds only the real `execution_runtime.py --persistence-worker` command or the fixed `preassignment-parent` control command; callers cannot pass arbitrary commands. It uses direct `CreateProcessW` with `CREATE_SUSPENDED | CREATE_NO_WINDOW | EXTENDED_STARTUPINFO_PRESENT`, a `STARTUPINFOEX` handle list, and explicit Win32 prototypes.
- It creates the anonymous no-breakaway observer job and completion port before launch, sets an inheritable-handle list containing only the three child-side standard handles, makes the parent ends non-inheritable, and closes the child-side ends after creation so the stdin EOF is real. It retains exact process and primary-thread handles; no PID lookup or global search is used.
- `preassignment_attempt()` records initial accounting, suspended launch, assign, membership, baseline, resume, EOF, wait, process/thread release, final accounting and drain ordering. Any setup failure closes through the owned launch and terminates the exact suspended process without resuming it.
- The case now requires both the real-worker negative (exit 2, baseline 1/1, final 1/0, clean accounting/ordering/cleanup) and a fixed short-lived-child control whose lifetime total reaches at least two with active zero. The control succeeds only when the detector refuses it; its expected refusal is not an unconditional suite failure. Missing positive evidence blocks the case.
- Accounting remains primary; notifications are positive corroboration. A port/API error is retained and downgrades an otherwise proved accounting result to a non-clean blocked result rather than claiming an entirely clean pass. The five previously passing cases are unchanged.
- Offline tests use fake APIs, handles, processes and clocks: exact create→assign→baseline→resume→EOF→wait→release→query order; restricted handle list; setup failures without resume; resume/EOF/wait/query/close failure cleanup; extra total two with notifications omitted; true negative; positive control; missing control; suspended launch command/flags and no native operation at import.
- Slice B correction: raw launcher acquisition now uses an explicit ledger. Handles are removed/transferred only after confirmed close or ownership transfer, failed closes and termination/wait errors are retained and reported, the original Win32 error is preserved, and one launcher-failure cleanup deadline terminates and bounded-waits the exact suspended process without resuming it. `DeleteProcThreadAttributeList` is called only after successful attribute-list initialization; sizing requires the expected insufficient-buffer result and a bounded nonzero size. Offline failures cover all three pipe acquisitions, each parent inheritance end, sizing/init/update/create, process/thread non-inheritance, child close, terminate return/exception, wait timeout/exception and close-result preservation.
- Slice B correction: initial accounting is rejected before launch unless `0/0`; setup/observation deadlines use remaining budgets; `ResumeThread` requires the expected previous suspend count of one; and control success is a dedicated predicate requiring validated ordering, exit zero, final total at least two with active zero, clean required queries/cleanup and no API/operation error. Detector refusal remains separately visible. Negative acceptance now fails closed on operation/deadline errors and `clean_run` cannot remain true after an operation failure; a demonstrated extra-association failure stays a failure rather than being converted to a pass. Late wait/final-query/drain stages are checked against the same observation deadline, and cleanup still runs once under its shared budget.
- Native diagnostic evidence at candidate `30d6cda…`: supervised `persistence-native` run `.local/executions/1836722ccd00410da0191a3a4de81e32` exited 2, not timed out, engine empty. Five cases passed. `preassignment_eof` remained blocked: negative initial `0/0`, baseline `1/1`, final total/active `2/1`, exit 2, one unexpected notification; control initial `0/0`, baseline `1/1`, final `3/1`, exit 0, two unexpected notifications. Ordering and operation/port/cleanup checks were clean. The extra process identities were not captured by the prior code; do not attribute them to conhost, interpreter startup or the production worker without identity evidence.
- Diagnostic correction: bounded job-scoped observation now starts before resume and calls `process_evidence` on each unexpected `NEW_PROCESS` notification as it arrives, recording exact handle membership/image/creation evidence or an explicit unavailable/exited error. The observer uses duplicated job/port handles, so the thread owns and closes only its duplicates; a main-handle close cannot invalidate a callback. Duplicate acquisition and thread-start failures raise a typed ownership error carrying the unresolved view and retained handle names; the original error plus every confirmed close failure is preserved, and no uncertain close is retried. Late duplicate-close errors appended while joining after an earlier operation failure are reconciled before cleanup, force unresolved state, and trigger checked termination of the owned job before main handles close. The observer uses the existing completion port, a stop event, 50 ms bounded polls and the observation deadline; stop, liveness and join failures are independently aggregated, and launch/process cleanup still runs under the same cleanup deadline. A timed-out or close-failed observer is explicitly unresolved: the owned job is terminated through the checked job operation before main handles close, process termination runs independently, and no success is claimed. Pre-cleanup accounting remains the only acceptance evidence. Initial, baseline and pre-cleanup final snapshots are retained separately; no post-cleanup zero is used as the negative proof. An extra association remains a refusal; short-lived identity remains unknown. Offline tests cover start failure without stale registration, stop/is-alive/join failure, callback exception, stoppable shutdown, duplicate-handle closure, identity available/unavailable, observer API error, extra association refusal and cleanup.
Observer lifecycle evidence was accepted for the native diagnostic run at candidate `5546bd6…`. That run (`.local/executions/863b6ab94db3445e808b7ca42e0ee0fc`) kept five cases passing and preassignment blocked. Negative accounting was initial `0/0`, baseline `1/1`, final total/active `2/1`, exit 2; the extra associated process has job membership, image `C:\Windows\System32\conhost.exe`, and creation identity. Control was initial `0/0`, baseline `1/1`, final `3/1`, exit 0; it contained the same verified conhost image plus the expected Python child. No observer, operation, port or cleanup error was reported.

### Console-host diagnosis and test-only repair proposal

Confirmed source facts:
- `launch_suspended()` currently uses `CREATE_SUSPENDED | CREATE_NO_WINDOW | EXTENDED_STARTUPINFO_PRESENT` with `STARTUPINFOEX`, `STARTF_USESTDHANDLES`, explicit pipe handles and a restricted handle list.
- `execution_runtime.persistence_worker()` reads the complete request plus EOF; on empty input it returns 2 before any child launch. The fixed positive control intentionally launches one child.
- The native negative result has no worker-launched application child in source. Notification evidence verifies a conhost association; the final accounting sample was total 2 / active 1, but no liveness observation tied that conhost identity to the final sample. No forced cleanup occurred before that query.
- The production `_PopenRunner` also uses `CREATE_NO_WINDOW`; the production path has not been re-run and this evidence does not certify it.

Confirmed versus unproven:
- Confirmed: conhost is associated with the newly owned test job and has verified image/creation identity at notification time. The final sample was total 2 / active 1; the identified conhost is not independently proven to be that final active process.
- Unproven: that `CREATE_NO_WINDOW` alone, Python startup, or pipe inheritance is the sole cause. No controlled launch comparison was run. Microsoft [Process Creation Flags](https://learn.microsoft.com/en-us/windows/win32/procthread/process-creation-flags) documents `CREATE_NO_WINDOW` as a console application without a console window and `DETACHED_PROCESS` as giving a console process no inherited console; [Creation of a Console](https://learn.microsoft.com/en-us/windows/console/creation-of-a-console) documents console allocation for console processes and the detached exception. The observed conhost is consistent with console-subsystem allocation, but causation is not established by this run.

Minimal test-only correction:
- Change only the harness preassignment launcher from `CREATE_NO_WINDOW` to `DETACHED_PROCESS` (value `0x00000008`), retaining `CREATE_SUSPENDED`, `EXTENDED_STARTUPINFO_PRESENT`, `STARTF_USESTDHANDLES`, explicit pipe handles, restricted handle list and all ownership/deadline logic. Do not use `CREATE_NEW_CONSOLE`.
- This is intended to avoid console allocation instead of whitelisting `conhost.exe` or changing expected counts. It is an unexecuted experiment; the worker still receives an EOF pipe and the positive control still launches its fixed child.
- Expected negative: initial `0/0`, baseline `1/1`, final `1/0`, exit 2, no unexpected process.
- Expected control: initial `0/0`, baseline `1/1`, final `2/0`, exit 0, one expected Python child, detector refuses it.
- If conhost persists or total is not 1/2, the case remains blocked; no name filter or count rewrite is permitted.
- Production equivalence is not claimed: production still uses `CREATE_NO_WINDOW`. A detached test launch differs in console context and cannot by itself certify production behavior; a production change would need separate scope and review.

Proposed files (no implementation in this dispatch):
- `Tools/persistence-job-acceptance.py` — add the detached flag constant and use it only for the fixed preassignment launcher.
- `Tools/test-persistence-process-reader.py` — assert detached flags, no `CREATE_NO_WINDOW`, retained `CREATE_SUSPENDED`/extended flags, exact handle list, expected negative/control totals, no conhost whitelist and refusal on extra totals.
- `todo/roadmap-r04.md` — this evidence only.
- `Tools/execute-tests.py`, production reader/runtime, PowerShell and profile remain unchanged.

Required offline checks and one native experiment:
- Run focused offline tests and the existing offline profile.
- Then one supervised native run: `python Tools/execute-tests.py --profile persistence-native --max-seconds 180 --execute`.
- Expected: five existing cases pass; negative and control complete with the counts above; no observer/operation/port/cleanup errors; no production-reader certification.

Rollback: restore `CREATE_NO_WINDOW` in the harness launcher and remove the detached-flag tests/evidence; no production or profile rollback required.

### Monday Blueprint and Widget round-trip decision packet (source-only)

Status: audit and decision only. No editor, engine suite, live MCP, build or fixture mutation was run. The capability index marks these tools `parsed_call` / `not certified`; tool routing records every Blueprint/Widget route as `unverified`. The test scripts listed below are editor-dependent suites, not executed evidence. No native equivalent is certified.

Exact agent route for Monday, using TurboMCP tools only:
1. `ReadTurboMCPBlueprintLoad` for the asset manifest and graph names. It lists graphs in sorted order and caps the list at 60.
2. `ReadTurboMCPGraphLoad(asset, graphName, format="read", detail="full")` for one graph at a time. A graph name is mandatory; empty names refuse. `detail=full` carries a long asset path, class, pin names and per-direction pin indexes; the node key remains the shortest unique prefix, normally 6 hex, not a guaranteed full GUID.
3. `ReadTurboMCPWidgetTree(asset)` for a Widget Blueprint hierarchy. It is read-only; no widget-tree writer is registered.
4. For a small ordinary-Blueprint change, use `PreviewTurboMCPGraphPatch` then `EditTurboMCPGraphPatch`. For additions, use `PreviewTurboMCPGraphAppend` then `EditTurboMCPGraphAppend`. Exclude `EditTurboMCPGraphReplace` from targeted edits; its omission-based redesign is not part of this Monday workflow.
5. Compile with `EditTurboMCPCompile`; save ordinary Blueprints with `EditTurboMCPSave`, and Widget Blueprints with `EditTurboMCPSaveWithoutThumbnail` to avoid the editor thumbnail path that instantiates design-time widget code.
6. Re-read the graph/tree and use `LookTurboMCPLayoutReport`; a fresh-editor reopen is required for persistence evidence and is not automated by these tools.

The omission-based `EditTurboMCPGraphReplace` redesign is outside the Monday targeted workflow and is not a prerequisite. Do not use it for small edits.

| Workflow step | Exact tool | Actual source behavior | Existing coverage | Gap/risk | Monday priority |
|---|---|---|---|---|---|
| Ordinary graph dump | `ReadTurboMCPGraphLoad` (`TurboMCPGraphLoad.cpp:64`) | One named `UBlueprint` graph; format `read` only; detail manifest/signatures/bodies/full. Node GUID rides in a comment as the shortest unique prefix (normally 6 hex); `full` changes the asset to a long path but does not force 32 digits. `full` adds class and pin indexes, not a full node key. Comments are notes, not semantic nodes; knots are skipped at bodies. Unsupported nodes emit `; error` plus a note. Output is capped; dropped characters are reported but there is no machine-readable `complete` flag. Positions are not emitted. | `Tools/test-graph-roundtrip.py` checks a connected pin, reader/dead-check agreement and GraphCheck acceptance; `Tools/test-graph-patch.py` and `test-graph-append.py` use editor read-back. Runtime not certified. | Empty graph name refuses, so there is no asset-wide graph read; D-008 expects a reader to name every graph. No all-graphs dump, no positions/layout, no pin GUIDs, no structured completeness, no edit-export header. Truncation can hide nodes even though the text says it was capped. | Mandatory read per graph at `full`; reject any response with errors/notes before editing. Manual fallback: read manifest then graphs one at a time; never infer missing nodes. |
| Standalone widget-tree dump | `ReadTurboMCPWidgetTree` (`TurboMCPWidgetTreeTools.cpp:25` → `WidgetTreeSync::Read/Walk` → `WidgetTreeText::Render`) | Typed `(widget-tree ...)` dialect with path, name, class, variable flag, non-default visibility and rounded non-zero slot padding. It does not perform the general property or delegate-binding walk. The standalone text renderer has no 300-item cap; it preserves unknown foreign forms verbatim. Paths are names, not GUIDs. | `Tools/test-widget-tree-sync.py` checks slot order and comparison with a second reader; `test-widget-tree-nested.py`/`test-widget-tree.py` cover variants. Runtime not certified. | Read-only; no public widget-tree writer/creator. No general bindings/non-default property enumeration in this dialect; no widget GUID. | Mandatory read; no Monday widget-hierarchy authoring. Manual fallback: use editor for hierarchy changes. |
| `BlueprintLoad` widget section | `ReadTurboMCPBlueprintLoad` (`TurboMCPBlueprintLoad.cpp:194-217` → `FTurboMCPWidgetTree::Render`) | Additive section in `BlueprintLoad` at signatures and above. It uses the richer general renderer: bound delegates, non-default instance properties, visibility/padding and hierarchy. This renderer has the `MaxWidgets = 300` cap and prints the omitted count. It is not the standalone `WidgetTreeText` dialect. | `Tools/test-widget-tree.py` covers this older section; shared widget tests. Runtime not certified. | Capped at 300; no graph/animation semantics; output shape differs from standalone `ReadTurboMCPWidgetTree`. Do not mix the two texts. | Mandatory read for a quick WBP manifest; use standalone tree for the typed hierarchy. |
| Widget graph dump | `ReadTurboMCPGraphLoad` | Loads a `UBlueprint`; `FindGraphByName` searches `UbergraphPages`, `FunctionGraphs`, `MacroGraphs` and `EventGraphs`. A `UWidgetBlueprint` is a `UBlueprint`, so its ordinary EventGraph/function graphs can be read. Widget animation graphs are not in those arrays and no widget reader exposes them. | Shared graph tests, not a Widget-specific graph suite. Runtime not certified. | Inherited/Widget graph coverage is not proven; animation graphs are a source-level gap. | Mandatory for EventGraph/function graphs only. Treat animation graph coverage as unsupported until a runtime probe. |
| Target identity | `ReadTurboMCPNodeFind`, `ReadTurboMCPNodeResolve`, `PreviewTurboMCPNodePins`; node IDs from `TurboMCPNodeId` | `MakeForNode` uses asset + graph name + `NodeGuid`; the key is the shortest unique lowercase prefix, normally 6 hex and grown on collision. `bLongForm` changes the asset spelling, not the key length. There is no emitted full-GUID mode. `FindGraphByName` returns the first case-insensitive match and exposes `OutMatchCount`; readers and several writers call the two-argument form without checking the count. `GraphTidy` has its own resolver that also takes the first match and, for an empty graph name, picks `EventGraph` or the first graph. `GraphAppend` separately builds a bare-alias map with `NodeGuid.ToString(Digits).Left(6)` (`GraphAppend.cpp:325`); two nodes sharing that fixed six-character alias overwrite one another in the map, so the last node wins. `MakeForNode` itself grows its qualified prefix to uniqueness; the defect is the separate bare-alias map, not `MakeForNode`. | Node-ID tests exist (`TurboMCPNodeIdTest.cpp`, `Tools/test-graph-*`); no ambiguity test was found. Runtime not certified. | Stale/ambiguous targeting: duplicate graph names can select the wrong graph silently; short IDs are convenient aliases, not collision-proof; Tidy’s empty-name fallback conflicts with architecture D-008. | Mandatory: always pass explicit graph names and a mutation key verified unique in that graph; do not use Tidy with an empty graph name. Task 1 below must make ambiguity refusal real. |
| Targeted edit | `EditTurboMCPGraphPatch`, `EditTurboMCPGraphAppend`, `EditTurboMCPGraphReplace` | Patch changes only explicitly identified nodes, refuses forms without IDs, runs one transaction, compiles and re-reads the exact changed set. Append resolves factory types, validates pins, creates/links in one transaction, compiles and checks created nodes. Replace runs Append’s plan, computes removals from nodes the text does not describe, keeps entry/result nodes and comments, and has no edit header, base fingerprint or per-node delete directives in source. | `test-graph-patch.py` asserts exactly one changed node and reader-visible value; `test-graph-append.py` asserts created node/wire read-back and no save; `test-graph-replace.py` exists. Runtime not certified. | Replace’s omission-as-removal is a data-loss risk, especially if the source text was truncated or incomplete. Short IDs and ambiguous graph names can mis-target. No layout/position preservation check. | Mandatory: use Patch/Append for small edits; exclude Replace from Monday targeting. Its omission-based redesign is not a Monday prerequisite. |
| Layout | `LookTurboMCPLayoutReport`, `EditTurboMCPGraphTidy`, `TurboMCPLayoutReport.cpp`, `TurboMCPGraphTidy.cpp` | Layout report measures node/wire/comment counts, extent, wire length, overlaps and findings; it writes nothing. Tidy plans a column/row layout and moves nodes, requires Slate-measured outlines and an on-screen graph editor, preserves comment-box top-left and grows boxes, and anchors unselected nodes when scoped. It is **not position-only**: plan step 7 (`GraphTidy.cpp:1090`) splits a selected variable getter that feeds several consumers, creates copies in Apply (`:1380`), and rewires consumer pins (`:1428`); rollback restores links, but the operation changes node/link identity when it succeeds. Append calls `SpawnNodeOfType(..., FVector2D::ZeroVector)` and has no placement pass. | `Tools/test-layout.py`, `test-graph-tidy.py`, `test-graph-tidy-scope.py`; editor-dependent. Runtime not certified. | Appending multiple nodes places them at the origin and can overlap. Tidy is a semantic graph transform, not a mandatory identity-preserving layout fallback. Widget canvas slot layout is not authored by any tool. | Layout report mandatory; Tidy optional only after Preview and accepting getter splits/rewiring. Task 2 below is the append-placement gap. |
| Widget edits | `PreviewTurboMCPWidgetRetarget`, `EditTurboMCPWidgetRetarget`, `EditTurboMCPWidgetsAsVars`, `EditTurboMCPWidgetBindAdd` | Retarget lists bindings whose delegate no longer exists on the current widget class, removes broken bindings, and re-reads binding counts; it does not compile. WidgetsAsVars toggles `bIsVariable` and re-reads each flag; it does not compile. BindAdd replaces an existing widget/property binding, defaults the callback name to `<Widget>_On<Property>`, adds it, and re-reads presence; it does not prove the callback function exists or compile the Blueprint. None save. | `test-widget-retarget.py`, `test-widgets-as-vars.py`; editor-dependent. No dedicated bind-add suite was found. Runtime not certified. | False success after widget edits: no compile/read-back of the generated class. Retarget removes bindings rather than repairing them. No widget tree creation/removal or layout. | Mandatory: compile and re-read after any widget edit; prefer explicit callback names and verify the function graph. Use `SaveWithoutThumbnail` for WBP. |
| Compile/save/reopen | `EditTurboMCPCompile`, `EditTurboMCPSave`, `EditTurboMCPSaveWithoutThumbnail` (`TurboMCPCompile.cpp`) | Compile verdict comes from compiler log plus captured engine log; save verdict comes from the package’s unsaved flag, with one retry. Normal save uses the editor path, which for a Widget Blueprint runs the thumbnail renderer and can instantiate design-time code; SaveWithoutThumbnail writes the package directly for `/Game` assets. No tool restarts the editor or reopens the package as a new session. | `test-save-thumbnail-option.py`, `test-widget-retarget.py`, `test-widgets-as-vars.py`; runtime not certified. `docs/audit/widget-persistence-prerequisites.md` records no-go for automated restart/reopen acceptance. | A successful save/compile does not prove reopen persistence. Normal WBP save has a crash/design-code risk. No automated fresh-editor read-back. | Mandatory: compile, save with the WBP-safe tool, then reopen in a fresh editor for acceptance. Manual fallback: supervisor-owned restart; no invented runtime value. |

What is implemented versus delegated versus missing:
- Graph dump and single-graph reader are implemented in TurboMCP; they are not native engine calls. The agent must call the TurboMCP toolset names above, not an engine-native graph API.
- Layout is implemented by `EditTurboMCPGraphTidy`, but it depends on the editor/Slate measured outlines; it is not a headless deterministic service. `LookTurboMCPLayoutReport` only measures.
- Appended-node placement is missing; it is not delegated to a deterministic layout pass.
- Widget hierarchy authoring is missing. Widget graph reading is limited to the ordinary Blueprint graph arrays; animation graphs are not covered.
- Compile/save are implemented; fresh-editor reopen and persistence acceptance are not automated.

Bounded tasks, ranked by crash/data-loss, then fidelity/targeting, then layout/latency. These are proposals; no runtime safety follows from source inspection.

#### Task 1 (identity fixes dispatchable; completeness provenance unresolved): unambiguous graph and node identity
- Files: `TurboMCPNodeId.cpp/.h`, `TurboMCPGraphLoad.cpp`, `TurboMCPGraphText.cpp`, `TurboMCPGraphAppend.cpp`, `TurboMCPGraphPatch.cpp`, `TurboMCPGraphTidy.cpp`; tests `Tools/test-graph-targeting.py` (new), `test-graph-roundtrip.py`, `test-graph-patch.py`.
- Preserve: existing `read` format, short-key display, Patch/Append edit behavior, graph-name case-insensitivity, and all current refusals.
- Implemented-behavior target: readers expose the graph match count and a reader-owned `complete` flag; mutators refuse `OutMatchCount > 1`; Tidy refuses an empty graph name and does not select `EventGraph`/first; add an additive full-GUID target emission and prove the mutation resolver accepts exactly that emitted value; remove or refuse the separate fixed-six bare-alias map collision in `GraphAppend.cpp:325` when a prefix is not unique.
- Open design decision: a reader’s `complete` flag is not, by itself, verifiable by a separate edit endpoint. Unless the request carries verifiable provenance or the orchestrator enforces the read prerequisite, a writer cannot know that a submitted body came from a complete read. Do not require a generic full-read text to round-trip through a different edit dialect without a defined conversion contract. This provenance decision is unresolved, so the completeness half of this task is **not dispatch-ready**; the identity/ambiguity half is bounded.
- Acceptance: duplicate graph names refuse at read and write; an emitted full-GUID target edits exactly one node; a colliding bare six-character alias refuses without mutation; truncated reads report `complete=false`; unsupported nodes remain explicitly reported; unrelated graph IDs and links unchanged. A writer-side truncated-text refusal is required only when the chosen provenance contract gives the writer enough information to decide it.
- Negative controls: duplicate graph name, two node GUIDs sharing six hex digits, truncated read, unknown node key, complete flag absent.
- Runtime evidence: live duplicate-name and short-prefix fixtures; reader complete/omitted fields; Preview refusal; no mutation on refusal; compile/read-back. Rollback: remove additive full-GUID/complete fields and restore current resolution; no asset-format change.

#### Task 2 (bounded, placement): deterministic estimated append placement
- Files: `TurboMCPGraphAppend.cpp`, `TurboMCPNodeType.cpp`; tests `Tools/test-graph-placement.py` (new), `test-graph-append.py`, `test-layout.py`.
- Preserve: existing node positions, comment boxes, bindings, links and GUIDs; existing Patch behavior and unrelated Replace behavior (Replace is not invoked); Tidy as an optional separate edit.
- Input/output: each appended node receives a deterministic **estimated** position from the existing graph extents and available/fallback sizes; no existing node moves. This is not a promise of measured visual non-overlap. When reliable Slate bounds are unavailable, a documented grid fallback may be used, but the result must be reported as “overlap unverified,” not as collision-free.
- Determinism contract: identical starting graph plus identical append input yields identical coordinates and node order. A repeated append into an already changed graph is a separate case: it must use fresh free space and need not reproduce the earlier coordinates.
- Acceptance: two runs from the same starting graph/input produce identical placements; existing positions/comments/links are unchanged; a measured-bounds case passes the layout report with no overlap; a fallback-bounds case reports overlap unverified and never claims measured safety; compile and read-back pass. Tidy is not invoked and no getter is cloned.
- Negative controls: occupied origin, unavailable measured sizes, repeated append into changed graph, multi-node block. Runtime evidence: before/after layout report plus the instrumented exact position snapshot from Task 3; aggregate layout report alone is insufficient when bounds are estimated. Rollback: restore ZeroVector placement and remove placement tests.

#### Task 3 (qualification, not production): exact read-edit-readback-compile-save-fresh-reopen
- Files: `Tools/test-blueprint-roundtrip.py` (new), `Tools/test-widget-roundtrip.py` (new); existing `Tools/test-graph-roundtrip.py`, `Tools/test-widget-tree-sync.py`; test-only instrumentation `TestProject/Source/TurboMCPTest/Private/TurboMCPRoundTripStateCommandlet.cpp` (new) and `TestProject/Source/TurboMCPTest/TurboMCPTest.Build.cs`; fixtures `BP_NodeTest` and `WBP_NodeTest`. No production plugin change in this task.
- Instrumentation: current public graph/tree readers do not expose node X/Y, comment bounds, every pin default, or complete link endpoint sets. The controlled test-only commandlet will emit a JSON snapshot keyed by asset path containing per-node GUID, NodePosX/NodePosY, comment bounds, pin default values, outgoing/incoming link endpoint IDs, and widget path/variable/visibility records. It is not a production tool and must not become one.
- Preserve: production tools and formats; this is runtime qualification only.
- Input/output: ordinary Blueprint and Widget Blueprint each perform read (manifest, per-graph full, WBP tree), one small existing-node edit, Preview/Edit, exact instrumented read-back, compile, save (WBP with `SaveWithoutThumbnail`), fresh-editor reopen and exact instrumented read-back. Emit exact tool calls, bytes and latency for that run.
- Acceptance: diff the before/after snapshots per node. Excluding only the intended node/pin or widget flag, every node GUID, X/Y position, comment bound, pin default and link endpoint set must be identical; WBP widget paths/classes/variables/visibility must be identical outside the edit. An unchanged overlap count or aggregate layout report is insufficient. Compile passes, save leaves no unsaved flag, and the reopened asset preserves the exact unchanged snapshot. The fresh-editor persistence gate from `docs/audit/widget-persistence-prerequisites.md` remains required; no runnable restart path is invented here.
- Negative controls: stale/missing node key, ambiguous graph, truncated read, compile error, failed save, no-edit control, and a deliberately changed unrelated position to prove the diff catches it. Runtime evidence is mandatory; source inspection alone is not a pass. Rollback: remove the test commandlet, two test scripts and temporary fixtures; no production rollback.

Contract status separation:
- Implemented behavior reviewed here: one-graph read, short-prefix IDs, ambiguous first-match resolution, fixed-six Append aliases, Tidy getter splitting/rewiring, Append at `(0,0)`, widget edit read-back without compile, and separate widget renderers.
- Required acceptance: the three task contracts above. They are proposals only and are not authorization to implement.
- Open design decisions: writer enforcement of read completeness/provenance (Task 1) is unresolved and not dispatch-ready; reliable measured bounds for append placement (Task 2) remain unavailable and fallback safety is unverified; fresh-editor restart for Task 3 remains a documented prerequisite with no invented runnable path.

Manual fallback and optional work:
- Mandatory manual fallback until Task 1/2 land: no Replace, no empty graph names, no unverified short aliases in mutations. Use Patch/Append, compile and read-back. Tidy is optional only after Preview; it can split getters and rewire consumers, so it is not an identity-preserving fallback.
- Mandatory manual fallback for Monday persistence: use `EditTurboMCPSaveWithoutThumbnail` for Widget Blueprints and a supervisor-owned fresh-editor reopen; do not claim persisted state from an in-memory read.
- Optional, not Monday blockers: widget tree writer/creator, animation graph reader, widget canvas-slot layout, native-equivalence routing. No broad CommonUI authoring, C++ save system, 5.9 or release work.

Acceptance scenarios for later runtime execution (no values invented here):
- Ordinary Blueprint: read manifest, read `EventGraph` at `full`, patch one existing pin/default, read back, compile, save, reopen in a fresh editor and read the same node GUIDs (once Task 1 exposes them) and links/comments; run layout report before/after and require no new overlap. Unrelated graphs must be byte/semantic-identical by reader/ID sets.
- Widget Blueprint: read `WBP_NodeTest` tree, read its EventGraph at `full`, toggle one existing widget variable or patch one existing pin, compile, save with `SaveWithoutThumbnail`, reopen in a fresh editor and read the tree/graph. Require the edited state to persist, unrelated widget paths/graphs unchanged, and no new layout overlap. This scenario is not executable until the fresh-editor restart prerequisite in `docs/audit/widget-persistence-prerequisites.md` is met.
- Measure tool calls, response bytes and latency only in that runtime run; no invented runtime numbers are recorded here.

- This slice is offline-only. Integrator evidence remains tied to exact candidates `29560ec…`, `30d6cda…` and `5546bd6…` as described above; none certifies this documentation-only candidate or the deferred detached-launch experiment. The later supervisor-only command remains `python Tools/execute-tests.py --profile persistence-native --max-seconds 180 --execute`, after independent review.

Offline tests and later supervisor-only command:
- Deterministic offline tests must inject fake suspended launch/assign/resume, job/accounting/port/process/pipe and seconds clocks; assert create→assign→baseline→resume→EOF ordering; initial 0/0; baseline 1/1; final 1/0; extra total two with no notifications refuses; short-lived child total two fails; query failure/inconsistent counters block; port `ERROR_TIMEOUT` is idle while other port errors are recorded; setup failure terminates without resume; every owned handle closes.
- After review and implementation, the later supervisor-only command is exactly `python Tools/execute-tests.py --profile persistence-native --max-seconds 180 --execute`. A pass of `preassignment_eof` would remain synthetic supervisor evidence, not production-reader certification; the harness overall remains unaccepted until separately reviewed.

Validation:
- `python Tools/test-persistence-process-reader.py` — exit 0; `Ran 103 tests ... OK`.
- `python Tools/validate-repo.py` — exit 0.
- `python Tools/check-language.py` — exit 0.
- `python Tools/execute-tests.py --profile offline --execute` — exit 0; the candidate-bound report path and its request commit are returned in the submission after the final commit; all offline checks passed.
- Candidate-bound evidence and full-base scope gate are recorded in the submission.

Rollback: remove the `persistence-native` profile branch and `Tools/persistence-job-acceptance.py`, remove the additive harness tests and this evidence. No catalog, production reader/runtime, PowerShell, engine or production caller changes are included.

### Parallel assignment claims implementation

Implementation commit `6b3b9e74dde0c284a0a32f50ced5ec8b6c27abd7` (corrections to `309b5f2ccb0940add462cc20ab9f7c9774599f8f` and `bfba1316de281e0cde20e2d7710b70d929115540`) from approved base `682ebe1f13ce0256003bce74c5ac8e5d33af13fa`, five Python files only: `Tools/task_claim.py`, `Tools/next-task.py`, `Tools/check-worker-scope.py`, `Tools/test-task-claims.py`, `Tools/test-worker-scope.py`.

Schema 2 stores an active list of per-assignment records with a distinct random `claim_id`, explicit assignment name, full approved base commit, canonical registered worktree/common-dir identity, exact validated files and declared absent-at-base `new_files`. Claims load the assignment manifest from that immutable base; `next-task.py` exposes the approved disjoint same-task path through `--assignment/--base/--claim` and checks readiness and dependencies. Claims and existing active scopes reject case/slash aliases, traversal, absolute/glob/directory entries, symlink/junction paths, canonical aliases across existing and new targets, prefix overlaps, case-variant protected files, Windows ADS/trailing-dot/space aliases and a second claim in the same worktree. The scope gate accepts `A` only for declared new paths and refuses undeclared additions or modification of a declared new path. Claim/release/migrate/rollback use the existing bounded kernel lock and atomic JSON write; the lock wrapper retries acquisition only. Store and backup records are fully validated before any mutation; malformed stores/backups refuse byte-identically, and rollback refuses when active claims exist or after any schema-2 mutation. Schema 1 remains readable and the legacy path remains operable. Migration refuses an active legacy claim with the store unchanged and requires schema-1 `active` explicitly null. No heartbeat, stale recovery, engine/port lock, policy, registry, catalog or real-ledger change was made. The retained live claim remains schema 1 and was not migrated or released.

Validation (offline, disposable repositories/stores):
- `python Tools/test-task-claims.py` — exit 0; 14 tests, including two same-task `next-task.py --assignment` CLI claims in separate worktrees, blocked/dependency refusal, concurrent disjoint claims, concurrent overlap (one winner), wrong-ID no-op, absent-at-base new files, canonical alias refusal before mutation, malformed stores/backups, protected/path aliases, accepted-claim readability/release, legacy-active migration refusal, rollback-after-release refusal, body-exception no-retry, and the exact approved-base legacy client (embedded digest `6b7f7eb3…`, provenance `682ebe1…`) failing closed on schema 2 with empty and active lists.
- The same suite was copied to a disposable directory with no Git history and passed 14 tests; it has no runtime dependency on the private-history commit.
- `python Tools/test-worker-scope.py` — exit 0; retains the schema-1 scope controls and adds schema-2 assignment/base-manifest controls, including declared new-file `A` acceptance and undeclared/modified refusals.
- `python Tools/validate.py` — exit 0; all offline checks passed, including `test-task-claims` and `test-worker-scope`.
- `git diff --check` — exit 0.

Limits: no real ledger migration or candidate CLI use on the live store; no native/build/editor/MCP execution; production schema-2 assignment manifests and protected policy installation remain integrator-owned. The CLI same-task path requires separate registered worktrees; an active legacy claim still blocks new assignment claims because its scope is unrecorded. Full-base scope-gate evidence is returned with the final candidate SHA.

## Related findings
- [019cf7af-dfe5-4cda-91b9-1ca706ace4e5](019cf7af.md)
- [7de8d185-a025-4485-837e-2d03975691b0](7de8d185.md)
- [abdd2c08-6544-4730-b183-e4d856aad868](abdd2c08.md)

## Native parity probes
For overlapping workflows in docs/audit/tool-routing.json, compare native and TurboMCP behavior on the same owned 5.8.3 fixture. Record exact schemas and inputs locally, effects, refusal cases, output completeness, call count and measured timing/output size. Report a candidate advantage only with evidence. Unavailable native tools are not equivalent to absent source functionality. These results feed R17; the audit itself does not change search ranking or descriptions.

## Current review

**Decision:** plan. **Follow-up owner:** [R04](roadmap-r04.md).

The 101-row inventory is structural; it does not certify all callables. Current review supplies concrete starting points for numeric parsing, native fixture verdict and weak assertions.

### Next action and acceptance

Work one family per evidence checkpoint. Produce rows for declared schema, effective defaults, effects, refusal/result shape and exact assertions. Keep unknowns explicit and author bounded repair specs; do not modify public behavior during investigation.

### Current source and verification

Reviewed at private main `6aee9dfd92a4c06a73e89c091973f982d715b7a2`. These source findings do not certify runtime behavior. Source links open the current file; line numbers refer to the reviewed revision.

- [docs/audit/capabilities.json](../docs/audit/capabilities.json)
- [Tools/tmcp.py](../Tools/tmcp.py#L139)
- [Plugins/TurboMCP/Source/TurboMCP/TurboMCPToolSchema.cpp](../Plugins/TurboMCP/Source/TurboMCP/TurboMCPToolSchema.cpp)

Starting checks for the assigned follow-up:

- `python Tools/test-result-contract.py`
- `python Tools/test-client-retry.py`

These suites are starting checks, not proof of the new cases. Use owned synthetic fixtures and retain local evidence. Reference records do not authorize implementation.

See the [review register](../docs/audit/task-review.md) for definitions, duplicate ownership and observed test results.

## First dispatch checkpoint: call-log contracts

This checkpoint is source/offline investigation only. It does not complete R04.
It overrides the general engine verification commands for this worker dispatch.
The integration owner runs any engine probes while cross-harness execution-lock
visibility remains unverified. Do not run builds, editors, engine suites,
packaging, recovery commands or direct MCP calls.

Create docs/audit/callable-contracts.json with schema 1, the approved base and
plugin/engine identities, and a tools array containing exactly the 101 unique
IDs from capabilities.json. Keep unaudited rows explicitly not_reviewed. Audit
only ReadTurboMCPCallCost, ReadTurboMCPCallLog, EditTurboMCPCallLog and
EditTurboMCPCallLogClear in this checkpoint. Write a concise human summary in
docs/audit/callable-contracts.md. Do not edit the generated capability inventory.

For these four rows, trace declaration, wrapper, implementation and relevant
client/result decoding. Record parameter/default parsing, supported scope,
side effects, success/refusal/unknown outcomes, and exact source references.
Separate source findings, existing test assertions, actual offline results and
unrun runtime probes. Inspect shared result helpers only as needed for these
four callers; do not broaden the audit to other families.

Start with the current review in 019cf7af.md and the result-decoding portion of
7de8d185.md. The compile-ensure engine fixture remains outside this checkpoint.
Cover empty/populated logs, omitted/zero/negative/fractional/overflow/malformed
numeric inputs and truncation boundaries where relevant. Trace result envelopes
and retry behavior without assuming a passing client test proves tool behavior.
Do not invent a cause or claim a source-only finding was reproduced at runtime.

For each confirmed gap, give a proposed repair with one existing owner, exact
allowed files, preserved behavior, synthetic reproduction/probe, expected result,
failure control, focused validation and rollback plan. Proposals are not repair
permission. Keep raw logs and proprietary or licensed source outside Git; use
source paths and short factual descriptions rather than copied engine bodies.

Only the three R04 paths in roadmap/worker-scopes.json may change. In this task,
only Completion evidence is worker-editable. No plugin/engine/test/helper,
status/readiness, TODO, review-register, dependency, routing or release changes.
Use the existing offline profile, audit-inventory --check, test-result-contract
and test-client-retry. Record the assertions they actually cover and remaining
probe gaps. Commit the checkpoint, obtain candidate-bound offline evidence and
run the full-base-to-candidate scope check. Retain the claim and stop for review;
do not start the next family, mark R04 done, merge, push or release the claim.

## Accepted dispatch checkpoint 2: status, help and schema

Checkpoint 1 is accepted source evidence, not an active assignment. This section
supersedes its tool selection and report-creation instructions for the next worker.
The same source/offline restrictions and three-file allowlist remain in force.

Audit only ReadTurboMCPStatus, ReadTurboMCPToolHelp and ReadTurboMCPToolSchema.
Extend the existing callable-contracts reports rather than replacing them.
Preserve all 101 IDs, the four accepted call-log rows, R04-CALL-01 through 06,
repair proposals and prior evidence identities. On completion, exactly seven
rows should be reviewed_source and 94 not_reviewed. If accepted evidence appears
wrong, record the discrepancy separately for review; do not silently rewrite it.
Record checkpoint-specific base and validation identities so the first checkpoint
is not relabeled with the new base. Use new R04-DISC-xx finding identifiers.

Read the reflected declarations and actual implementations, plus shared schema,
registration, help/trap, recorder and client paths only where needed by these
three tools. Check parameter requiredness, omitted/empty/unknown input, exact
name and case matching, normalization, defaults, truncation and error handling.
Distinguish declarations from registered/enabled/discoverable tools and runtime
availability. Identify stale or unsupported capability/version/support claims.
Check whether help and schema describe actual parameter behavior and refusal
boundaries. Explain what status can prove and what it merely reports from settings
or registration. Trace side effects from wrappers even for Read tools. Do not
assume prose refusals become invocation errors or that transport success proves
semantic success. Read the current 0e4658ad.md review as relevant context.

Native overlap and routing reports are read-only context. Do not promote any
unverified routing preference, change descriptions/ranking or implement R17.
Use pinned source only; no engine upgrade, external feature expansion or broad
rewrite. Every cited local path must exist and every claimed declaration or API
must be read. Do not copy engine source bodies into the reports.

For each confirmed gap, propose one owner, exact repair files, preserved behavior,
a reproducible synthetic probe, expected result, negative control, focused checks
and rollback. Mark proposals unapproved. If more than one design is possible,
state the unresolved decision and separate the alternatives' scopes. R04 remains
incomplete and no production repairs are authorized.

Run audit-inventory --check, test-result-contract, test-client-retry and the
existing offline profile. Inspect relevant engine suites as source only, recording
which assertions exist and which probes remain unrun. Do not run builds, editors,
engine suites, direct MCP calls, packaging or recovery. Commit, obtain candidate-
bound offline evidence and run the full approved-base-to-candidate scope check.
Retain the new checkpoint claim and stop for review; no merge, push, completion,
claim release or next-family work.


## Completed dispatch checkpoint 4: save contracts

Checkpoints 1-3 are accepted source evidence. Checkpoint 3 candidate:
39c229306f1e5d56cf156be9de3b7891af7382e8. Independent offline run:
b95fc842c1f543f1a8cffcb220780597. R04 remains incomplete.

Audit only EditTurboMCPSave and EditTurboMCPSaveWithoutThumbnail. Preserve all
101 IDs, 10 reviewed rows, prior findings, proposals and evidence. Expected result:
12 reviewed_source and 89 not_reviewed. Use R04-SAVE-xx finding IDs.
Trace Public/Tools/TurboMCPToolsets.h and TurboMCPCompile.cpp save implementations,
shared resolver, write/VCS guards, recorder and client decoding. Read
Tools/test-save-thumbnail-option.py and Tools/test-vcs-write.py as assertions only.
Cover omitted/empty/list/duplicate inputs, loaded/unsaved assets, package selection,
dirty/compile conditions, source-control/read-only refusals, thumbnail work,
save failure propagation, partial success and side effects. Distinguish checkout,
package write, compile and thumbnail work. Do not infer atomicity, rollback or
persistence from a success string. Inspect exact pinned engine callees as needed;
record source references and failure semantics without copying engine bodies.

Only the three R04 worker-scope files may change; task edits are Completion
evidence only. No plugin/engine/test/helper/routing/pin/status/TODO/release edits.
No engine/editor/build/MCP/packaging/recovery execution. Source/offline audit only.
Each repair proposal needs one owner, exact files, preserved behavior, synthetic
probe, expected result, negative control, fixture gaps, validation and rollback.
Proposals remain unapproved. Run audit-inventory --check, test-result-contract,
test-client-retry and the offline profile. Commit, obtain candidate-bound offline
evidence and run the approved-base-to-candidate scope gate. Retain claim and stop
for review. No merge, push, completion marking, claim release or next-family work.


## Completed dispatch checkpoint 5: compile contract for Monday widgets

Checkpoint 4 is accepted as source evidence at 8bd3a449, with independent offline
run 96579e7ca2514b09818f78ce413a27a6. R04 remains incomplete; all repairs unapproved.

Audit only EditTurboMCPCompile. Preserve all 101 tool IDs, 12 reviewed rows and
27 findings/repairs. Expected inventory: 13 reviewed_source, 88 not_reviewed.
Use R04-COMPILE-xx finding IDs and continue repair IDs without collisions.
Trace declaration, implementation, resolver, recorder, client and pinned engine
calls. Cover valid/invalid inputs, dirty state, compile errors versus warnings,
partial batches, widget-generated classes, bindings, reinstancing, execution
side effects, refusal guards and truthful results. Existing tests are assertions,
not runtime evidence. Do not claim Common UI/Input support from inheritance alone.

In the same reports, specify the smallest synthetic Widget Blueprint compile ->
save -> reopen acceptance sequence. Identify existing fixtures and missing seams,
exact candidate-bound evidence needed and manual fallbacks. This is a proposed
acceptance plan, not new feature scope or authorization to run an editor.

Only callable-contracts.md/json and this task's Completion evidence may change.
No implementation, engine/editor/build/MCP/packaging/recovery commands. Repairs
need exact files, preserved behavior, expected outcomes, negative controls,
fixture gaps, validation and rollback. No broad tool audit beyond compile.
Run inventory, result-contract, client-retry and offline checks; commit and retain
candidate-bound offline evidence and full-base scope result. Stop for review.


## Completed dispatch checkpoint 6: persistence prerequisite contract

Checkpoint 5 is accepted as source evidence at 1c0cb15; independent offline run
cab5c12cb1ce47c7bf702748c146b172 passed. R04 remains incomplete.

Write docs/audit/widget-persistence-prerequisites.md: the smallest implementable
supervisor-owned restart and SaveOnCompile observation contract needed by the
accepted Widget Blueprint persistence plan. This is a design task, not code.
Inspect run-suites, tmcp_editor, start-editor, check-mcp-server, execution locks,
leases, timeouts, recovery and fixture restoration. Choose one minimal approach;
do not produce a menu of speculative alternatives. Identify exact files and
functions, IPC/request shape if needed, ownership validation, bounded deadlines,
process identity including PID reuse, old-process exit and new-session proof,
foreign-listener refusal, save versus discard semantics for the owned fixture,
crash/interruption recovery, cleanup and lock retention. Never restore fixture
bytes between saved-trial write and readback; restore between isolated trials.

Trace actual setting ownership and a minimal readback mechanism for effective
SaveOnCompile=Never before any mutation in each trial. Separate configuration
intent from observed effective value. Preserve user config; list missing seams.
No project-only setting assumptions or unsupported Python APIs.

Produce implementation-ready subtasks in dependency order, with exact allowlists,
preserved behavior, offline fault-injection tests, supervisor-only live acceptance,
negative controls and rollback. Every status remains proposed_unapproved. Include
a concise go/no-go assessment for Monday and existing supervised fallback if
provable. No plugin feature expansion, engine patch, general runner rewrite or
new cloud service. Do not revisit the full tool audit.

Only the new prerequisite report and this task's Completion evidence may change.
No code/test/config/fixture/status/TODO/routing/release edits or engine/editor/build/
MCP/recovery execution. Run offline validation and full-base scope gate; commit,
retain candidate-bound evidence and stop for integration review.


## Completed dispatch checkpoint 7: offline identity validator implementation

Checkpoint 6 candidate ec370326 is accepted as a design investigation only.
The complete lifecycle remains blocked; P1-P6 are not collectively approved.
Integration owner now authorizes this narrow implementation prerequisite.

Add Tools/persistence_identity.py and Tools/test-persistence-identity.py,
one offline catalog entry in Tests/catalog.json, plus Completion evidence here. No existing production caller or helper changes.
The module validates supplied evidence; it must perform no process/network/file
collection, launch, termination, config writes, lease changes or engine calls.
Use standard-library Python and explicit structured pass/refusal reason codes.

Define a small strict versioned schema for expected identity and two collected
snapshots: PID (positive integer, not bool), exact normalized UTC creation time,
canonical absolute Windows project and engine-executable paths, configured
literal loopback endpoint/port, process observation, TCP listener records and
collection-success status. Validate lengths/types/unknown fields; refuse missing
or unreadable evidence. Windows path comparison must not depend on the OS where
offline tests execute. Reject relative/device/UNC paths and dot-segment ambiguity;
do not resolve symlinks or claim string normalization proves filesystem identity.
Inputs must be described as already canonicalized by a future trusted collector.

Only literal 127.0.0.1 and ::1 loopback endpoints are supported initially; reject
hostnames and remote hosts instead of doing DNS. Define TCP listener matching
by address family: exact loopback or same-family wildcard. Reject competing
owners on relevant matching listeners; allow dual-stack rows for one verified
owner without calling multiple rows multiple owners. Do not infer IPv6 wildcard
dual-stack behavior without evidence; refuse ambiguous family coverage.

Both observations must bind the matching listener PID to the expected process
PID/start time/project/engine path, with bounded collection timing supplied by
the caller. Reject stale/out-of-order observations and any identity/ownership
change between them. Include a documented residual race: validating snapshots
cannot guarantee ownership after return or secure a future connection. Passing
means supplied evidence agrees, not live ownership certification or permission.

Tests must cover intended editor alive but foreign listener, PID reuse, changed
project/engine, failed collection, missing listener, remote/hostname endpoints,
IPv4/IPv6/wildcard and same-owner dual-stack rows, multiple owners, stale/reversed
observations, malformed schema, booleans-as-integers, and valid cases. Inject data
only; never enumerate actual processes or bind sockets. Keep prior APIs unchanged.

Run python Tools/test-persistence-identity.py and the existing offline profile;
commit, retain candidate-bound runs and full-base R04 scope gate. Register only this test in Tests/catalog.json with tier offline, a bounded
timeout of 30 seconds and acceptance not_mapped. Preserve all existing rows and
validator behavior. Run it explicitly and through the offline profile. No new dependency.
Rollback removes only the two new files. Stop for review; no merge/push/claim release.


### Checkpoint 7 correction decision

The original scope omitted a mandatory test catalog row. The integration owner
now authorizes only one new offline row for test-persistence-identity; do not label
it engine or weaken validate-repo. Candidate e8adf04 remains unaccepted.
Fix future process/listener start times relative to observation time, reject
invalid Win32 path characters/control codes, and refuse non-string dictionary
keys without raising. Add adversarial regression tests and preserve pure behavior.


## Completed dispatch checkpoint 8: trusted identity collection contract

Checkpoint 7 candidate 4fa6f36fcbcf52e6b622acca87874158de79f5c9 is accepted
as a pure supplied-evidence validator only. R04 remains incomplete. No live
ownership, persistence or Monday readiness is certified.

Write docs/audit/persistence-collector-contract.md. This is one bounded source
investigation and implementation specification, not collector implementation.
Read persistence_identity.py, existing process/listener checks, ownership/leases,
runner and Windows process helpers. Specify the smallest read-only collector
that can supply the validator honestly on Windows. Choose concrete existing
APIs and trace available repository code. Do not invent guarantees.

Define sources for PID, creation time with its actual precision, executable path,
project identity and complete relevant TCP rows. Expected identity must originate
from the trusted supervisor launch record, not be copied from the observations.
Explain command-line parsing, quoted paths, canonicalization, aliases/junctions,
case-sensitive directories, and why string comparison alone cannot prove file
identity. Refuse unsupported ambiguity. Do not fabricate 100 ns precision from
a coarser source; identify a schema adaptation if needed, without implementing it.

Bound each collection attempt and the entire pair. Specify time measurement,
partial enumeration/permission failures, process exit/PID reuse between reads,
listener replacement, wildcard families and overflow. Map fields to schema v1;
if v1 cannot represent truthful evidence, record an exact proposed adjustment.
No validation pass grants permission to connect, mutate, stop or restart. State
the residual race and what a future caller must recheck.

Give one minimal proposed implementation slice with exact file allowlist,
injected-backend offline cases, success and negative acceptance, privacy/log
redaction, and rollback. Keep live acceptance supervisor-owned and explicitly
unexecuted. Do not implement lifecycle/restart/settings work or propose a general
framework rewrite. Keep the report concise and implementable. All implementation
steps remain proposed_unapproved until review.

Only this new report and roadmap-r04 Completion evidence may change. No code,
tests, catalog, engine/config, status, TODO, scope or release edits. Do not inspect
live process command lines, enumerate sockets, launch an editor or run builds.
Read source and run the existing offline profile and full-base scope gate. Commit
and retain candidate-bound evidence. Stop for review without merge/push/release.


## Completed dispatch checkpoint 9: offline collector orchestration

Implement only the pure first slice of docs/audit/persistence-collector-contract.md.
The contract is accepted as a design; no live Windows adapter or launch seam is
approved. R04 and Monday persistence acceptance remain incomplete.

Allowed files: Tools/persistence_collector.py, Tools/test-persistence-collector.py,
Tests/catalog.json (one new offline test row only), and this task's Completion
evidence. Existing persistence_identity.py is read-only. No production caller,
Windows adapter, subprocess, process/socket/filesystem collection, sleeps,
launch/restart/kill, config, lease, dependency or engine changes.

Use standard-library Python with injected backend and clocks, exact record
validation, deterministic structured refusals, and no raw diagnostic leakage.
Implement the contract's identity/path gates, independent process rechecks,
normalized listener multisets, aggregate/per-call deadlines and final v1
validation. Backend deadline enforcement is a precondition; this pure module
cannot interrupt a backend that never returns. Tests prove orchestration only.

Integration clarifications take precedence over earlier prose:
- File-ID volume_serial/index_high/index_low are unsigned 32-bit integers; zero
  components are valid. Compare all components exactly. Missing, negative,
  out-of-range and bool values refuse. link_count must be exactly 1.
- Compare UTC differences in integer 100 ns ticks converted to nanoseconds
  (multiply by 100), never against FILETIME or monotonic epochs directly.
  Enforce age_ns on BOTH snapshots, gap_ns on their difference, and nonnegative
  ages before v1 validation. Convert v1 max_age_seconds to max(1, ceil(age_ns/1e9))
  and max_snapshot_gap_seconds to ceil(snapshot_gap_ns/1e9). Exact ns enforcement
  happens first; rounding must never weaken it. No floats or timestamp truncation.
- Canonicalize FILETIME-to-UTC using integer arithmetic; reject dates outside
  validator's representable range. Bound launch_id to 128 printable ASCII chars,
  paths to the existing validator limits, command_line to 32768 characters.
- policy is a complete dict with all documented keys. Provide a named default
  policy constant or factory; reject partial dicts and unknown fields.
- Foreign owner PID refuses immediately; do not inspect unrelated command lines.
  For matching owners, verify identity and path IDs again on final recheck.
- Treat malformed backend values as backend_malformed. Catch ordinary exceptions
  at backend invocation boundaries as backend_unknown without exception text;
  do not hide internal implementation defects with a function-wide catch-all.
- Do not mutate caller inputs or return backend-owned mutable records by alias.

Tests must exercise all contract cases and zero file-ID components, uint32 limits,
sub-second and non-multiple-second policies, both-snapshot age, equality/one-tick
boundaries, FILETIME epoch conversion, malformed records at each layer, backend
exceptions, no further calls after refusal, input immutability and output aliasing.
Use a scripted fake backend; no live probes. Keep tests small and behavior-driven.
Register exactly one offline catalog entry with 30-second timeout and
acceptance not_mapped. Run focused test, offline profile and full-base scope gate;
commit, retain candidate-bound evidence, and stop for review. No merge, push,
claim release, task completion or additional task.


## Completed dispatch checkpoint 10: bounded Windows process reader

Checkpoint 9 is accepted as pure offline orchestration. Implement only the
read_process portion of the future Windows backend. This is implementation
permission for the files below, not permission to execute live collection.

Allowed: Tools/persistence_process_reader.py, Tools/read-persistence-process.ps1,
Tools/test-persistence-process-reader.py, Tests/catalog.json (one offline row),
and this task's Completion evidence. Existing collector/validator and all
production callers remain read-only. No listener/path/parser adapter, launch
record, runner integration, settings, engine or lifecycle changes.

Provide read_process(pid, deadline_ns) -> ReadResult[ProcessRead] compatible with
the collector contract. Use standard library, injectable runner and monotonic
clock; no side effects at import. Validate positive uint32 PID (not bool) and
integer deadline before spawning. Refuse expired budgets without a child.
Invoke only the fixed checked-in PowerShell script using an argument list,
-NoProfile -NonInteractive, no shell=True or interpolated command strings.
Use hidden child startup on Windows. No arbitrary command or script parameter.

The fixed script accepts only PID, uses strict terminating error handling, emits
one small versioned JSON result and never raw provider errors or command lines
outside its result channel. Command line is required data in ProcessRead; Python
must not log it or include it in a refusal. Query only that PID through CIM.
Open a native process handle with limited-query rights; get creation FILETIME
and exit state before and after the CIM read using that same handle. Confirm
PID/executable/command line data is present and well typed; process exit,
ambiguous/multiple/missing CIM rows or any error refuses. Native handles must
close on every path. Do not infer ownership from this read or upgrade a lease.
Retained-handle checks reduce races but do not certify identity of future reads.

The Python runner owns only its collection child, enforces remaining monotonic
deadline, rechecks after return, and always reaps its child on timeout/error.
No editor termination and no process-tree enumeration. The fixed script must
not launch children. Bound captured stdout/stderr (suggest 64 KiB each) during
collection, not just after unlimited buffering; overflow refuses and cleans up
only its child. Specify/test EOF, nonzero exit, invalid UTF-8, extra JSON/output,
malformed schema, size overflow, deadline equality, early process exit and
cleanup failures. Never accept partial output. Preserve integer FILETIME ticks;
no floating point/CIM timestamp conversion. Returned fields are exactly the
existing ProcessRead shape. Non-Windows execution returns unsupported/unknown
refusal without spawning. Do not add dependencies or change the ReadResult error
taxonomy; map errors to the existing categories.

Offline tests use injected runner/clock and script inspection where appropriate;
never spawn PowerShell, enumerate processes, query CIM or read actual command
lines. Tests must verify the precise fixed command construction and narrow
cleanup target, not merely mock successful results. Document what offline tests
cannot prove. No live validation is authorized for the worker. Any unavailable
native/API guarantee must be reported explicitly, not invented.

Register one offline test row with timeout_seconds 30 and acceptance not_mapped.
Run focused test, offline profile and full-base scope gate. Commit, retain
candidate-bound evidence, report full SHA/limits and stop. No merge/push, claim
release, readiness/task completion or next task.


## Completed dispatch checkpoint 11: process-reader lifecycle admission

Checkpoint 10 is accepted only as an offline-tested process-reader implementation.
Live use remains blocked. Resolve the helper-process lifecycle question before
expanding the Windows backend. This is a bounded investigation, not code work.

Allowed: docs/audit/process-reader-lifecycle.md and this task's Completion evidence.
Read the fixed PowerShell script, Python runner and available Windows PowerShell/
.NET source or authoritative documentation. Establish whether Add-Type compilation
can start a compiler child, whether that child inherits the result pipes, and
what the existing direct-child cleanup can and cannot guarantee on timeout or
parent death. Distinguish owned compiler descendants from external shared CIM/WMI
services; do not propose killing shared services or unrelated processes.

Choose the smallest viable admission strategy. Compare only (a) containment of
the dedicated collection process and its own descendants using an appropriate
Windows Job Object established before code runs, or (b) avoiding the compiler
child through a narrowly justified alternative. Do not implement either. Check
repository job helpers for reuse; explain nested-job/assignment failure, suspended
launch/resume/termination, handle inheritance, bounded pipe reads, cleanup grace
versus request deadline, orphan detection and abnormal supervisor exit. No broad
runner rewrite, package dependency, persistent service or production integration.

Deliver a concise decision (target <=200 lines) with actual source/API references,
known versus unverified facts, exact proposed implementation files, ownership and
failure invariants, rollback, and a supervisor-only synthetic acceptance recipe.
The recipe must use only newly created harmless test children and synthetic
strings, no editor or existing process command-line collection, and include
forced timeout, inherited-pipe child, parent death and a negative control proving
an unrelated process survives. It is a proposal, not worker execution permission.

Explicitly record that current fake Thread.join treats unstarted joins as no-op,
whereas real Python Thread.join raises before start. This affects cleanup_failed
classification in thread-start-failure tests; it is not proof of a child leak.
Include that small mock-fidelity repair in the proposed implementation scope.
Also record real Windows script parsing/API/UTF-8/handle behavior as unverified.

No code/test/catalog/scope/status/TODO/lease/config changes, no live processes,
PowerShell/native calls, sockets, editor, builds or recovery. Existing offline
profile and full-base scope gate only. Commit and retain candidate-bound evidence;
report full SHA and blockers, keep claim, stop for review. No merge/push/release.


## Previous dispatch checkpoint 12: offline job containment

Implement only the offline containment phase of process-reader-lifecycle.md.
Allowed files: Tools/persistence_process_reader.py, Tools/execution_runtime.py,
Tools/test-persistence-process-reader.py, and this task's Completion evidence.
No native profile/suite, catalog, PowerShell script, collector/validator, engine,
lease or production caller changes. Live admission remains blocked.

Add checked job termination/accounting/membership/close methods while preserving
existing WindowsJob.close, run_owned and generic --worker semantics. Use exact
Win32 prototypes, structure layouts and checked return values. Expose injection
seams for fake API/job/clock tests without executing native operations on import.
Only new --persistence-worker accepts a bounded complete UTF-8 request (<=512
bytes) with exact integer schema=1 and positive uint32 pid, then EOF. Empty EOF,
extra objects/bytes, duplicate keys, nonfinite constants and malformed fields
refuse before launching anything. Parent assigns and verifies membership before
writing; parent closes the sole writer. No arbitrary argv or executable input.

Worker derives fixed PowerShell/script paths locally. Avoid recursive invocation
of the public reader from the worker. Preserve strict output/encoding/error
contract and hidden startup. Parent owns all deadlines and contained descendants;
worker accepts one request only. A stalled request writer is bounded by the
parent deadline/job termination. No global replay-protection claim.

Contain the collection bootstrap and descendants in a fresh non-inheritable
KILL_ON_JOB_CLOSE job with no breakaway. Refuse assignment/verification failure
without authorization. Track exact owned handles; never enumerate/kill unrelated
processes or shared CIM/WMI services. Preserve bounded capture during reads.
Use one monotonic cleanup deadline, maximum five seconds, shared by termination,
active-process polling, worker wait and drains. Query while the job handle is
open, then close it. Every failure refuses; abnormal death cannot report success.
No live-verified containment claim from mocks.

Test actual orchestration under fake process/job/API/streams/clock: assignment
before send, failed assignment, request completion, expiry at each boundary,
constructor/thread-start/drain failures, active-count changes/query failure,
terminate/close failure, single shared grace deadline, output overflow and exact
handle cleanup. Repair FakeThread.join to match unstarted-thread RuntimeError;
verify cleanup classification honestly. Keep legacy run_owned/--worker behavior
unchanged; add focused compatibility assertions in the allowed test file.
Do not weaken tests or use broad catch-all exceptions to hide defects.

Run focused test, existing offline profile and full-base scope gate. Commit and
retain candidate-bound evidence, report full SHA and blockers. No live native
calls/subprocess tests, editor/build/recovery, merge/push/claim release or task
completion. Native harness implementation and supervised execution are deferred.


## Active dispatch checkpoint 13: synthetic containment acceptance harness

Implement the synthetic admission phase of docs/audit/process-reader-lifecycle.md;
this dispatch permits implementation and offline mocks only. The integration
owner will review and run native acceptance separately. R04 stays incomplete.

Exact writable files: Tools/execute-tests.py, Tools/persistence-job-acceptance.py
(new), Tools/test-persistence-process-reader.py (additive offline mocks only),
and this task's Completion evidence. All other files are read-only, including
execution_runtime.py, the production reader, PowerShell, catalog and engine.
If a production seam is indispensable, return a precise blocker, do not patch it.

Add the fixed persistence-native profile described in the lifecycle design.
It must select only the fixed acceptance script, reject suite selection, and
never resolve an engine, build, preflight an editor, snapshot/recover fixtures,
call run-suites or open ports. Keep existing profiles unchanged. Reuse the
supervisor's run_owned, ResourceLocks, deadline and local evidence directory.
The acceptance script must require valid supervisor context before live actions;
imports and offline tests must never create processes or call native APIs.

Implement owned synthetic cases: timeout with descendant, inherited stdout/stderr
pipe holder, supervisor death after assignment, pre-assignment stdin EOF, and
harmless Add-Type compiler observation via job completion notifications. Keep an
unrelated but test-owned sentinel outside the inner job as a negative control.
Use exact owned process handles and creation identity, never global process
search or name-based killing. No real user process/command-line inspection.
Use fixed synthetic helpers/modes only, no arbitrary command execution API.
No change to production admission semantics or arbitrary worker argv seam.
Clearly distinguish testing the real production path from a synthetic analogue;
a substitute job fixture is not proof of the reader's actual orchestration.

Every test has a deadline and guaranteed best-effort owned-resource cleanup on
exception. Missing compiler observation or unverifiable membership is BLOCKED,
never a pass; job NEW_PROCESS notification alone is not executable identity.
Account for short-lived compiler processes, Windows PID reuse and parent-death
race windows honestly. Report each case's observed facts, pass/fail/blocked,
owned process identities, cleanup proof and limitations. Overall missing required
evidence is nonzero. Do not infer cleanup solely from absent marker files or
use fixed sleeps as proof of readiness; use bounded explicit handshakes.
Keep raw evidence local and synthetic. Sentinel is terminated only by its owner
at test teardown, after verifying survival of the tested inner-job cleanup.

Add offline injected tests to the existing reader test file for profile command
selection, no engine path, malformed/unsupervised admission, deadline/refusal and
result aggregation; preserve all existing assertions. Do not add native tests
to offline execution or modify Tests/catalog.json. No native run in this worker
assignment, including the new profile or direct acceptance script.

Run focused offline tests, full offline profile and full-base scope gate. Commit
and retain candidate-bound evidence; report SHA, claim, exact changed files and
unresolved native prerequisites. Stop with claim retained. No merge, push,
release, task completion, engine/build/editor/recovery or live native action.

## Deferred review tasks (owner-requested integration)

These candidates are merged at the owner's request, not accepted as runtime-certified.

- [ ] R04-FOLLOWUP-PERSISTENCE: Review the disabled Widget persistence candidate 8b4fe3cdf2cdb40e8075f87e7d3ed353ba4c4af6 before enabling it. Keep the unconditional admission refusal. Repair per-generation cleanup without clearing dirty flags or marking objects garbage to force success; use exact acquired fixture ownership instead of a directory-prefix exemption; prove exclusive directory acquisition and ancestor/reparse confinement; correct retirement after pointers are cleared; track partial reload generations. Review supervisor admission, then build and run the bounded persistence contract with success, refusal, failure and cleanup evidence. Scope: TurboMCPWidgetPersistenceTest.cpp and test-widget-persistence.py; admission changes need a separately reviewed scope. This is not Monday's passing MCP package-reload smoke test.
- [ ] R04-FOLLOWUP-MENUS: Review menu candidate b2e535f27b6176dd176c4c6ed6f936fb288e5702 in the pinned editor. Verify Window icon, dedicated TurboMCP Tools section, both open actions, single entry after re-registration, and safe shutdown/reload removal. Check narrow-window rendering and keyboard navigation. Scope: STurboMCPPanel.cpp menu registration only. Source review does not establish visual/lifecycle acceptance.

Verification: supervised build first; menu follow-up uses the owned editor and focused panel suites. The catalog lists the merged wrapper for inventory completeness; it remains disabled and is expected to report BLOCKED. Do not execute the persistence suite until its blockers above are resolved. Neither checkbox is completed by merging source.

## Weekly candidate queue — 18 October 2026

Planning only: these are not dispatched assignments or changes to task readiness. Before implementation, the integrator must approve an exact base, file allowlist and claim. Automatic dispatch is paused. Workers must not merge or push main, force-push, release claims, weaken tests or alter release/readiness controls. Engine/editor tests remain supervisor-owned and serialized. Main branch protection is recommended, not verified here.

- [ ] WEEK-01 / P1 / Widget layout: one simple connected Widget logic edit; compare before/after node positions, links, defaults and widget hierarchy, inspect new-node overlap, then compile/save/read back. Extend focused acceptance coverage only; production fixes require a reproduced defect and bounded scope. This is the next field-test priority.
- [ ] WEEK-02 / P1 / Reader and refusal: focused regression coverage for truncated input, missing target and ambiguous identity, with a valid control and unchanged-state assertions on refusal. Source/offline work may run alongside WEEK-01 in a disjoint worktree.
- [ ] WEEK-03 / P2 / Menu UX: complete R04-FOLLOWUP-MENUS visual and lifecycle review; serialize editor access after the Widget test.
- [ ] WEEK-04 / P2 / Field-test guide: short human-readable read/preview/edit/compile/save checklist using verified schemas, known limits and refusal/partial-failure recovery. Documentation-only scope can run independently.
- [ ] WEEK-05 / P3 / Persistence harness: complete R04-FOLLOWUP-PERSISTENCE cleanup/ownership review while retaining the disabled entry. Not a prerequisite for the passing MCP package-reload smoke test.

Current integrated field-test baseline: e1c71699d8fcba6719050f5e331647ddddf15445. Final build passed in 17dd2e2b537a4e1e8a7390ea095488ed. Basic Blueprint/Widget MCP disk loop passed in 0163b414b2c046d59ae20641f4e9ab77, with all 16 fixture files restored. This is not a bug-free or complete workflow certificate. Restart is an optional durability check, not required after every normal edit. Menu visual checks and disabled persistence review remain pending despite source integration.

## Field feedback intake: 20 October 2026

Urgent follow-up is owned by Codex; auto-dispatch remains paused. FIELD-01 and FIELD-02 are P0 investigations; FIELD-03 through FIELD-07 cover persistence, batch outcomes, identity, layout and semantic verification. FIELD-08 through FIELD-10 retain diagnostics, smaller gaps and the separate GPU investigation. See [complete intake](../docs/audit/field-testing-2026-10-20.json). These records do not claim fixes or grant unbounded implementation scope.

Weekly prototype review: the three test improvements were accepted for integration after combined native run `875a4c319c2c494eb206114ce0d91648` passed and serial offline validation passed. The refusal fingerprint now includes SHA-256. Visual geometry and MCP ambiguous-identity coverage remain unverified. This acceptance applies to the additional tests, not to the field defects or release readiness.

### 2026-10-24 Replace linked-node deletion repair

Implemented explicit endpoint retention (e92fc24), with supervised native planner and live Apply evidence 447be52387e64809ac83908daf368d21. Requested wires no longer lose their explicitly referenced existing endpoints merely because those nodes lack separate forms. See the tool-quality audit for reproduction and validation limits. R04 remains open; this does not certify full rollback, persistence or all 125 tools.

### 2026-10-24 Batch graph identity repair

Fixed af034a7: staged plans preserve the live graph name rather than the scratch duplicate name. Native public Preview/Edit batch calls pass sequential existing-target edits and later-operation refusal without live mutation (03f81ea36fa5434ab4a55189eaf5b2a4). Batch rollback after live mutation and broader operation coverage remain open; see the quality audit.

### 2026-10-24 Batch failure after live mutation

Native run a426f0d227bc46d7bea1a3e98440d3aa passes a forced second-operation failure after both live rewires, with original graph links/default/position restored and both attempted operations reported. Existing sequential success/refusal suite also passed. Whole-state rollback remains unverified; see the tool-quality audit for exact limits.

### 2026-10-24 local output identity repair

Implementation 679cb88 counts consumers per distinct output pin, retains full node GUID and output name, and avoids merging separate calculated values into one candidate. Duplicate link entries do not count as independent consumers. Removed the disabled getter-only rewrite that destroyed the original calculation without assigning its value.

Supervised run 0c3822c6bad54ac9988dd3b94c6eac0a passed build and TurboMCP.LocalVars.Ancestry with no warnings. Added controls cover two singly used outputs, duplicate links and the identity of a repeatedly consumed output. Scope gate passed against 8f1eacae0754aca08e7e92d452a9e9279cf471e0. These checks prove candidate selection, not successful promotion.

P0 67f00521 remains OPEN. Explicit assignment, execution placement, transactional failure handling and an actual value-preservation fixture remain required. Promotion is still refused before mutation. All-125 behavioral acceptance and vision acceptance remain unfinished. Prior unrelated offline gate failures remain recorded; the full offline gate was not rerun for this slice.

### 2026-10-24 calculated local assignment implemented

Implementation be03a0e, 5ecd441 and ccd9e48 replaces the refused promotion path with bounded scalar arithmetic capture. It keeps the original calculations, adds local declarations, assigns in dependency order at function entry, rewires exact output consumers to getters, compiles, and checks reciprocal assignment/consumer/execution links after compilation. Existing node positions are retained. No disk save is requested.

Supported calculation families are stock native int/int64/double addition, subtraction and multiplication, with literals or stable by-value function inputs. Mutable variable reads, reference inputs, inputs assigned or passed by reference, unreviewed pure functions, ambiguous identities and unsupported entry execution shapes refuse before mutation. The whole edit refuses if any candidate is excluded. The Blueprint must already have a successful warning-free compile baseline. The edit replans against live state rather than applying stale candidate pointers.

One owned transaction covers declarations and graph changes. Failure ends it once and attempts only its exact undo record. Output explicitly says restoration is unverified and changes may remain; Undo success is not a full restoration claim. Scalar declarations are registered together, avoiding AddLocalVariable's nested transaction per declaration.

Supervised evidence: de7991f2e6d74699be9b7f5cb3fcc7e7 passed initial value/failure checks; 086c6c1263274e5fb2f679eff26d8be6 passed dependent calculations and changing inputs; final ceab56f4a0e24e3e9cf10029952128aa passed build plus test-localvars-promotion and test-localvars-analysis with no native warnings. The final fixture executes two dependent repeated calculations before/after editing with inputs 2 and 7 and verifies three returned values. It also checks retained calculation identity/position, exact new-node count, repeated-call no-op, reference-input and disconnected-entry refusal, and a deliberate post-rewire failure restoring node count, locals, consumers and executable values.

Scope gate passed be03a0e against approved base 039d0159a18e0f0238b63fc87c05ee6ab448cbfb; subsequent implementation paths were checked against that same manifest. Tests/catalog.json is a separately reviewed integrator-owned suite registration.

Implementation status: bounded repair complete. Validation: focused native evidence passed, broader field validation pending. Untested: all int64/double arithmetic variants, rendered placement, disk persistence and arbitrary callback/property restoration. Unsupported cases remain explicit refusals, not silently skipped values. No full 125-tool acceptance claim. Prior unrelated offline gate failures remain; full offline gate not rerun for this slice.

### 2026-10-24 replacement protection and post-removal recovery

65dd983 repairs a confirmed protection gap: Replace previously bypassed CanUserDeleteNode for nodes other than its hardcoded entry/result/event/comment classes. Planning now reports a refusal for an unmentioned engine-protected node, and Apply independently checks deletion permission before Append mutates the graph. It also checks the complete expected remaining GUID set/count after removal and modification callbacks, rather than checking only that removed IDs disappeared.

Replacement transaction finalization now follows the exact owned record. Scope cleanup does not end a record already ended by Append failure; failure recovery only undoes the matching finalized record. Failure output clears any success report and states restoration remains unverified. Header documentation now accurately states that Replace does not compile or save.

Supervised 79e4a84b75ae44dcbad6494420c80373 passed build and test-replace-plan without native warnings. New evidence: forced failure after real append/rewiring/removal restored all four original node identities, node count, reciprocal original edge, removed-node default and position, with no active transaction. Existing successful replacement checks still pass. A protected macro tunnel is refused in both planning and an independently checked stale/corrupt accepted plan, without creation or deletion. Scope gate passed 65dd983 against 66422d41fa9bc0f146a2e6f176dc34f3aafe7b5e.

Implemented and focused-native-tested; broader field validation remains pending. These checks do not certify all node classes, editor callbacks, breakpoint/watch restoration, disk persistence or complete rollback of arbitrary Blueprint properties. No full offline or 125-tool acceptance claim; previously recorded offline failures remain.

### Approved metadata repair — 2026-10-24

AssetDescribe misreports unloaded WidgetBlueprint generated class and loads Blueprint parents after taking its loaded-state snapshot. Use live objects and registry metadata without loading; distinguish unavailable class data from absence. Verify repeated descriptions retain unloaded state and truthful generated-class output on Blueprint/Widget fixtures.

## AssetDescribe metadata repair — 2026-10-24

c089599 uses existing Blueprint objects or GeneratedClass/ParentClass registry
tags instead of requiring a separate generated-class asset entry. Parent lookup
no longer loads an asset after the loaded-state snapshot. Missing metadata is
unknown, not proof that the asset kind cannot generate a class.

Supervised d8e2e06bb12e4c4b9db8072eb630b06a passed build and
test-asset-describe-metadata: Blueprint and WidgetBlueprint returned real _C paths
and parent metadata, remained unloaded across repeated descriptions, and produced
identical results. Subsequent 7a9039a only removes an unjustified unloaded-state
phrase from the generic missing-metadata fallback; that text-only change has no
separate runtime evidence.

Follow-up: test-asset-tools still expects generated class none for a registry-only
enum. Update this stale absence assertion to check truthful metadata availability;
the complete asset suite is not claimed passed. Loaded unsaved asset disk-state
reporting also needs review; this repair does not certify that separate logic.

### Approved asset disk-state correction — 2026-10-24

AssetDescribe currently derives disk presence from dirty state, falsely reporting clean unsaved assets as on disk. Query package existence independently; retain unknown dirty state when unavailable. Verify fresh unsaved enum dirty and clean states using the disposable native fixture. Update the stale enum generated-class absence assertion.

## Asset disk state corrected — 2026-10-24

3297fde measures package existence independently of dirty state. AssetDescribe
no longer claims a clean unsaved asset is on disk, or substitutes a dirty-state
description for package existence. Unknown package state remains explicit.

Supervised b0816ff1b2a94e489cf28f3387d66154 passed build, unloaded Blueprint
metadata checks and native enum creation checks including dirty-unsaved and
clean-unsaved descriptions (both correctly on disk: no). The asset suite only
failed two stale DirtyList wording expectations. 4f4c0f5 preserves refusal and
distinct-reason requirements while checking current precise wording.
Supervised 8ef41a5112d548dc8811420d3e71f50c then passed build and test-asset-tools.

The stale generated-class absence assertion is now corrected. These focused
cases pass; broad 125-tool acceptance and live vision investigation remain open.

## Asset-editor capture gamma correction — 2026-10-24

Implemented in 31c0c75 and 5a15c43. Asset-editor capture previously disabled
Slate display gamma and produced nearly black UI. Enabling it alone produced
washed-out UI; the final repair uses a linear BGRA target so display gamma is
applied once, matching the existing graph and screen capture paths.

Supervised 7bf71ab60a614a79b7b5ea65a116ee61 at 5a15c43 passed the build.
Codex inspected the Widget designer, Material and Blueprint PNGs: their dark-theme
UI is readable, unlike the baseline images. Before/after images are retained in
the local handoff output vision-gamma-evidence directory. This is visual evidence
for these three captures, not pixel-perfect fidelity or full capture acceptance.

The test-capture-asset suite FAILED: Pillow is unavailable, stale catalogue
expectations remain, and comparing missing colour counts raises TypeError. Do
not count this run as a suite pass. DataTable and AnimBlueprint positive capture
coverage is still missing. Repair the harness dependency/error handling and
stale assertions without weakening its image checks. CP-14 live graph contrast
remains open; this correction concerns saved capture images only.

### Approved capture harness repair — 2026-10-24
Repair missing Pillow diagnostics and missing colour-count crashes in test-capture-asset.py; align related-tool assertion with current catalogue semantics. Preserve image thresholds, named refusal coverage and explicit fixture gaps. Run with an available Pillow environment through the supervisor. No plugin behavior changes in this assignment.

## Asset capture harness repaired — 2026-10-25

72b0b8f fails clearly before MCP calls when Pillow is absent, excludes unreadable
images from colour comparisons, removes an unused comparison that crashed on
missing values, and checks current related-tool names plus execution consent.
Image thresholds remain unchanged. The module documentation no longer claims
that opening editor tabs is inherently free of side effects.

Supervised 750064b7eb414ce193c46265f35f75bb passed build and test-capture-asset.
The run used bundled Python 3.12.14 with Pillow 12.3.0 by prepending its directory
to PATH before invoking Tools/execute-tests.py. No packages were installed.
Seven kinds produced PNGs with matching reported dimensions and non-flat pixels:
Material, Material Instance, Material Function, Widget Designer, Anim Sequence,
Niagara and Blueprint. DataTable returned a named refusal because its fixture
was absent. AnimBlueprint has catalogue coverage only. Neither is a positive
capture pass. Full colour fidelity, framing and live CP-14 remain separate work.
Pillow reports getdata deprecation; this does not invalidate the current result.
The 125-tool review remains unfinished.

### Approved capture artifact retention — 2026-10-25
Asset-editor and screen captures reuse deterministic names and overwrite earlier evidence. Give each result a unique absolute PNG path. Verify repeated same-asset and same-size screen captures preserve prior bytes. Graph debug modes are outside this bounded repair; review them separately.

## Asset and screen captures retain earlier images — 2026-10-25

63fd067 replaces deterministic asset-editor and screen output names with unique
GUID-suffixed absolute paths. Repeated captures no longer overwrite the image
referenced by an earlier tool response. Includes are explicit and the obsolete
comment requiring asset-only filenames was removed.

Supervised 9118fbfa52674ed881b88a21c444d75e passed build and test-capture-asset.
Repeated asset-editor and screen calls returned distinct absolute paths, created
the second PNG and preserved the earlier bytes. Existing pixel and dimension
checks passed for seven asset kinds. DataTable remained a named missing-fixture
refusal; AnimBlueprint still lacks positive capture coverage. Graph debug-mode
artifact naming remains a separate follow-up; no full-vision acceptance claim.


### Approved purchase result clarity repair - 2026-10-25
Integrator scope: workflow-purchase-results. Readiness: ready. Echo the actual verified pin identity and normalized value on changed/unchanged PinDefault results without changing mutation or rollback behavior. State CommentList's point-based membership approximation accurately in code, help and output; escape multiline titles. Extend the disposable purchase scenario with exact-result and refusal checks. No new engine requirement or broad readiness claim.


## Purchase workflow visual and result repairs - 2026-10-25
Implemented 9e1cc0a and dd8f58f; both bounded scope gates passed. Build and expanded disposable scenario pass in c8735a8f289248269dad165ce361fd5d. Exact per-tool evidence and remaining limits are in docs/audit/purchase-workflow-review.json. SVG glyphs, function title, redundant getter placement, wrapped comment title clearance, normalized pin result identity and comment membership wording repaired. User visual acceptance remains pending; 125-tool certification is not complete. Older tidy/comment regression fixtures are missing required nodes; offline gate retains four separately documented failures. DSH stays paused.


## Purchase append scoped layout correction - 2026-10-25
Implemented: 276a1b6. User screenshot exposed backward affordability-to-AND wiring. Scoped pure-expression Tidy now places selected dependency trees before their fixed consumer. Workflow explicitly selects both old/new rule branches after Append. Build and native workflow pass: 72bcaba97a134fe5baac805860214cda; forward wires, seven-node overlap, exact links/defaults/count and unrelated positions checked; capture inspected. Scope gate passed, claim released. Offline 21/26 remains separate validation debt. Existing comment-wire issue remains open; no global tool readiness change.


## Multi-event connection workflow - 2026-10-25
Implemented 0850faa; scope gate passed. WF-BP-03 checks a single Purchase data-input repair alongside Refund and separate Audit graph. Build/native 3281be0d2c084b2f853c05d18e08e205 passed. Exact context/IDs, refusal, no-op, independent values and unchanged other flows checked. Output endpoints/displacement and preview guidance clarified. See docs/audit/event-connect-workflow-review.md; docs/audit/workflow-tool-coverage.json records usage across three limited scenarios, 24 unique tools, no global certification.


## WF-BP-03 orphan cleanup correction - 2026-10-25
Implemented ad7d71c. GraphConnect exposes displaced pure sources with no remaining consumers; workflow explicitly previews/removes the obsolete constant calculation. Build/native 47c9c30568a54267b542da4ad53bcbef passed: shared-source negative control, exact one-node removal, preserved remaining state and runtime results. Actual capture inspected, orphan absent. Scope gate passed, claim released. Cross-workflow ledger now covers 26 unique tools with bounded evidence; deletion/rollback generality not certified.

## Call replacement workflow - 2026-10-25
Implemented 38ef353; bounded scope gate passed. WF-BP-04 fixes discovered wildcard operator replacement and hidden static receiver default handling. Help/results/comments reviewed, including honest rollback limits. Build/native 0cc5d7d515d3425bbee0d9cf300820ed passed: connected input/literal preservation, old-node removal, independent event results, refusals, scoped tidy and native capture. See docs/audit/call-swap-workflow-review.md. Coverage is 28/125 unique tools in limited scenarios; user visual acceptance and documented wider paths remain open. DSH remains paused.

## Common UI button workflow planned - 2026-10-25
WF-UI-05-COMMON-BUTTON is planned, not executed. Track hierarchy/style context, event discovery, targeted edits, interaction and graph presentation. Four candidate new tools, with delegate-binding pair conditional. Full per-tool quality criteria and support gaps are in docs/audit/common-button-workflow-review.md and workflow-tool-coverage.json. Coverage remains 28/125; no new readiness claim.


## Approved Common Button inspection/preview review
Readiness: ready. Assignment workflow-common-button. Review and repair WidgetTree context/help and WidgetPreview lifecycle/output using a disposable Common UI fixture. Enable CommonUI only in TestProject; no new plugin dependency. Capture actual normal/callback states, refusals and preservation. Record unsupported style-edit and interaction paths without substituting fixture setup for tool evidence.

## Common Button reader/preview evidence - 2026-10-25
Implemented bec049e; scope gate passed. Final build and Common Button plus WidgetPreview regressions passed f7ee45f24a69440fb5ee5dcc3c3654a6. Fixed callback construction order, added style/event context, clarified help/results/helper comments. Coverage 30/125. Full WF-UI-05 remains incomplete: style-value editing, tool-driven inherited-event edits and actual pointer/focus/pressed/disabled behavior still need evidence. BindNewEvent was not used or certified. See docs/audit/common-button-workflow-review.md.


## Approved focused parent delegate binding workflow
Readiness: ready. Assignment workflow-bind-event. Focus only Preview/Edit BindNewEvent: parent Widget Blueprint binds Common Button OnButtonBaseClicked through an existing Bind delegate node. Verify typed signature, exact graph delta, preview/refusal preservation, duplicate names, displaced sources, output/help and compiled callback behavior. Support tools are reused, not recertified. Fixture setup is not tool evidence.

## Focused parent delegate binding review - 2026-10-25
Implemented 88e57fd; scope passed. WF-UI-06 focuses Preview/Edit BindNewEvent only. Native 3a38a896a17b48b1aecb7db7b399af75 passed: typed handler, exact node delta, old-state preservation, refusal/duplicate handling and compiled Common Button sender dispatch. Results/help now explain exact targets, parameters, replaced sources, empty body and runtime registration. Coverage 32/125; wider signature/live-replacement/persistence paths remain unverified. See docs/audit/parent-binding-workflow-review.md. Support tools reused without another full review.


## Approved parent-binding layout correction
Readiness: done. Assignment workflow-bind-layout. Apply explicit scoped Tidy in WF-UI-06 and verify nearby button getter, aligned handler/Set execution pins, unchanged links/defaults/count and compiled dispatch. Repair Tidy only if the native scenario demonstrates a defect; no manual fixture-coordinate workaround.

## Binding layout correction - 2026-10-25

Implemented 694a70d: the workflow now calls Preview/Edit GraphTidy on its five affected nodes after wiring the body. No fixture coordinates were manually corrected and no production Tidy change was required. The getter sits near the Bind target; HandlePurchase and Set execution pins align, as do their data pins. Preview preserves the graph; edit preserves node count, links and defaults. Native build/workflow passed 9fa27a0144b542d7a2a69409989a210e, including compile and sender dispatch. Updated capture inspected; human acceptance remains pending.

Initial assertions incorrectly measured getter distance from the Slate pin widget origin, including its label width. The final check measures visible node-edge clearance (at most 160 units), plus pin-row alignment (at most 2 units). Offline gate d20bd814286945678b180f291d0a1ee5 remains 22/26 with the same four baseline failures. Coverage remains 32/125; Tidy is reused support.

## Approved focused inherited override workflow
Readiness: done. Assignment workflow-override. Review ReadOverrides and Preview/Edit OverrideAdd in a disposable Common Button hover/unhover scenario. Verify discovery/filter truth, preview preservation, invalid-name refusals, duplicate no-op, exact created identities, compiled callback label changes and scoped tidy/capture. Review helper code, comments, descriptions and result English; fix demonstrated defects within these tools. Fixture creation and supporting tools are not new coverage. No pointer-input, persistence or all-signature certification. Run supervised test-override-workflow and the offline gate.

## Inherited override review evidence
Implemented 8faffff; native 63b610bfbc52406390897bd5ec46273d passed. Three focused tools bring bounded coverage to 35/125. See docs/audit/override-workflow-review.md for output fixes, checks and untested paths.

## Approved focused override removal workflow
Readiness: done. Assignment workflow-override-remove. Review EditOverrideRemove using a disposable Common Button with hover/unhover bodies. Verify missing/empty/repeated removal, exact removed identity and detached endpoints, retained body and unrelated unhover preservation, then explicit GraphRemove cleanup, compile, callback behavior and capture. Improve removal help/results/comments as needed. No automatic body deletion; no function graph, undo restoration, save/reload or physical-input certification. Run supervised test-override-remove-workflow and offline gate. Existing tools are support only.

## Override removal evidence
Implemented 66d3ce4; native cf71a88b34d248adb5743c36de988a0d passed. Exact retained-body reporting, explicit cleanup and preserved unhover behavior checked. Coverage 36/125. See docs/audit/override-remove-workflow-review.md for limits.

## Approved widget variable workflow
Readiness: done. Assignment workflow-widget-variable. Focus EditWidgetsAsVars: expose named text widgets, compile and access them in live graph logic, repeated/case-duplicate requests, missing-name batch preservation, removal refusal with existing self getter/setter references, successful disable after explicit cleanup. Review helper, flags, result accuracy, English and help. Refuse the whole disable request before mutation when referenced. Preserve hierarchy/defaults; no broad reference discovery, persistence or undo certification. Run supervised test-widget-variable-workflow and offline gate.

## Widget variable workflow evidence
Implemented 99fad26; native 3faaddbb451a4f9d820afc0aa6d4d6dc passed. Referenced-disable preflight, case-insensitive deduplication and result checks added. Coverage 37/125. See docs/audit/widget-variable-workflow-review.md for limits.

## Approved widget property binding workflow
Readiness: done. Assignment workflow-widget-binding. Focus EditWidgetBindAdd using a disposable text label and pure text getter functions. Validate single-cast delegate, reflected function signature and property purity before mutation. Missing/incompatible targets must not replace a valid binding. Normalize property/Delegate aliases and widget case, implement exact no-op, report function and replacement clearly. Verify initial binding and replacement through compile and displayed/runtime text, invalid/duplicate preservation. Reject multicast binding through this metadata path with clear routing advice. No persistence, dynamic-path or all-delegate certification. Run supervised test-widget-binding-workflow and offline gate.

Binding workflow implemented in 4f28cec; native build/workflow c24c254d03aa4196a02263d7e83e1df6 passed. Scope gate passed. Offline 7050368c75c24a07aa062eaf75436486 remains 22/26 with four known baseline failures. Review: docs/audit/widget-binding-workflow-review.md. Supporting preview concern subsequently withdrawn after original pixel inspection; see the repeated-preview review.

## Repeated widget preview preservation
Readiness: done. Assignment workflow-preview-repeat. Investigate WF-UI-10-PREVIEW-REPEAT: a blank button disappears on the second preview. Reproduce consecutive captures independently from fixture rendering; inspect widget/slot state and capture pixels. Fix the confirmed cause in preview or fixture, preserving explicit execution consent and truthful output. Add a regression assertion for the unchanged button across binding replacement. Use supervised test-widget-binding-workflow and offline gate. No engine changes or unrelated tool expansion.

Repeated-preview investigation: original button disappearance diagnosis withdrawn. All original PNG button regions contain 1600/1600 visible pixels. Added native repeated-capture pixel regression; 38bdf1a0d7e441ca9066455e39122b23 passes four captures. No production fix required.

## Vision workflow stress fixtures
Readiness: ready. Assignment workflow-vision-stress. Build disposable dense Blueprint (12 event flows across graphs, sequences, branches, delegates and arithmetic function), widget and material fixtures. Exercise existing graph selectors and resolutions, widget preview, material/asset editor and editor screen capture. Use supervised test-vision-workflow; wrapper may start owned bounded PlayCheck and capture screen during it, then poll completion. Record missing dedicated viewport capability as a gap, not a passed viewport test. Assert image dimensions/content and graph preservation; inspect images visually. Review actual English/helper output, describe limited coverage. No production edits, existing asset mutation or save.
