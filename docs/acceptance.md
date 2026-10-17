# Required acceptance cases

[Documentation](README.md)

This catalog defines required behavior. Implement each case in a focused test suite and record the working command in the test manifest. Passing results belong in the acceptance evidence; this document alone does not establish a pass.

## Test rules

- L1 means pure execution against production logic with boundary mocks where needed.
- L3 means execution in the actual editor against the recorded plugin artifact.
- L4 means the final selected regression and staged installation.
- A passing source-string test never substitutes for L3.
- Every case must assert its exact selected-case count.
- Missing fixtures or unavailable prerequisites produce blocked, never passed.
- Restore owned fixtures after each mutating case.
- Stop the family if restoration fails.
- Keep one small fixture for related cases when reset is verified.

The expected result must not come from the production renderer under test.
For graph comparisons, enumerate engine nodes, pins, and links independently.
Compare GUIDs, classes, signatures, defaults, topology, and dirty state as applicable.
Ignore only explicitly named non-semantic fields, such as transient addresses.
Do not remove differences from the oracle simply to make tests pass.

## H — Harness and selection

| ID | Level | Setup and action | Required assertion |
|---|---|---|---|
| H01 | L1 | Child prints PASS and exits 7 | Runner fails and records exit 7 |
| H02 | L1 | Only required case reports BLOCKED | Runner exits nonzero and preserves blocked status |
| H03 | L1 | Select unknown suite, then valid plus unknown suite | Both selections fail before execution |
| H04 | L1 | Select zero cases, then duplicate case IDs | Both inputs fail explicitly |
| H05 | L1 | Child times out | Runner fails, stops only its child tree, retains output |
| H06 | L1 | Child exits zero without the required verdict | Runner fails with missing verdict |
| H07 | L1 | Child prints a valid passing verdict and exits zero | Runner passes with one executed case |
| H08 | L1 | Parse defaulted FString declarations and both audited assertion styles | Correct parameter names and no false NO-ASSERT finding |
| H09 | L1 | Insert an uncalled declared tool into a tiny header fixture | Coverage check fails for that exact tool |

Use tiny fake children. Do not start Unreal for H cases.
Test the real verdict/selection functions, not copied versions.

## C — Client and results

| ID | Level | Setup and action | Required assertion |
|---|---|---|---|
| C01 | L1 | Apply fake-server mutation, then drop its response | Exactly one effect, outcome unknown, no replay |
| C02 | L1 | Reconnect after session invalidation | Reconnect alone does not repeat the previous request |
| C03 | L1 | Return explicit refusal, partial save, failed rollback | CLI exits nonzero using metadata, not prose |
| C04 | L1 | Malformed or missing metadata from a migrated tool | Protocol failure, never success |
| C05 | L1 | Body contains forged metadata-like text, then a valid final line | Only the validated final line determines outcome |
| C06 | L1 | Engine wraps FString and supplies multiple text content blocks | One correct decoded body and metadata object |
| C07 | L1 | Native engine tool has no TurboMCP metadata | Native adapter works without falsely certifying TurboMCP outcomes |
| C08 | L1 | Output reaches its exact budget with a metadata trailer | Metadata remains valid, body truncation is explicit, no partial syntax |
| C09 | L3 | Invoke one real success and one real refusal through MCP | Wire payload, structured client, and CLI agree on operation outcome |

## E — Environment, ownership, and artifacts

| ID | Level | Setup and action | Required assertion |
|---|---|---|---|
| E01 | L1 | Foreign editor exists in a mocked process inventory | No foreign process receives termination |
| E02 | L1 | Two owners request the same project/port/fixture lease | Second owner refuses without starting a process |
| E03 | L1 | Owned child fails during fixture mutation | Snapshot retained until verified restoration |
| E04 | L1 | Existing mount targets another checkout | Explicit refusal, no automatic mount replacement |
| E05 | L1 | Build command returns zero but expected artifact is missing | Build wrapper fails |
| E06 | L3 | Build plugin, then start the owned editor | Loaded module identity matches the tested artifact |
| E07 | L1 | Test times out while fixture restoration is incomplete | Required case fails and later fixture-dependent cases do not run |

Mock processes for destructive failure cases. Do not kill a real foreign editor as a test.

## M — Shared mutation contract

| ID | Level | Setup and action | Required assertion |
|---|---|---|---|
| M01 | L3 | Edit during PIE, then during an unrelated active transaction | Refusal before mutation, no unrelated undo |
| M02 | L3 | Edit a compilable asset without a matching baseline | compile_baseline_required and no state change |
| M03 | L1 + L3 | Replace one compile warning with a different warning at the same count | New diagnostic detected, real edit rolls back |
| M04 | L3 | Edit initially clean and initially dirty assets | One undo restores semantic state and each original dirty flag |
| M05 | L3 | Inject failure after first mutation through the real core | Exact restoration, rollback complete, no save |
| M06 | L3 | Force restoration failure through a test-only boundary | rollback failed, changed unknown, later mutations refused |
| M07 | L3 | Request a second edit while the mutation guard is held | Explicit busy refusal, no nested transaction |
| M08 | L3 | Complete a successful edit with existing baseline diagnostics | No new diagnostics, exact requested delta, refreshed baseline |

Use one real writer to prove the common coordinator.
Each other family still needs one success and one restoration case.
Do not rerun every common-guard permutation for every wrapper.

## G — Identity and representation

| ID | Level | Setup and action | Required assertion |
|---|---|---|---|
| G01T | L1 + L3 | Create colliding short IDs, resolve full IDs, then extend collision set | Full identities remain stable, ambiguous aliases refuse |
| G02T | L3 | Export graph with one shared pure node feeding two consumers | Exactly one definition and two correct source-pin edges |
| G03T | L1 | Parse/export quotes, backslashes, newlines, Unicode, and empty strings | Exact round trip with no token boundary loss |
| G04T | L3 | Export supported graph with literal, object, and text defaults | Independent engine values equal exported and reapplied values |
| G05T | L3 | Export graph with an unsupported semantic node | complete=false and explicit unsupported identity |
| G06T | L3 | Feed compact output or incomplete export to Replace | Refusal before mutation |
| G07T | L3 | Change graph after edit export, then apply old fingerprint | stale_plan refusal and no changes |
| G08T | L3 | Omit a semantic node without a delete directive | Refusal, not implicit deletion |
| G09T | L3 | Add explicit valid delete directive | Only that node disappears, mandatory entry deletion still refuses |
| G10T | L1 + L3 | Resolve ambiguous type and unsupported wildcard conversion | Refusal by name, no first-match guessing or partial spawn |

G suffix T distinguishes case IDs from audit finding IDs.
Use the smallest corpus that covers supported creation families and the listed syntax dimensions.
Do not claim support for an untested node family.

## P — Preview

| ID | Level | Setup and action | Required assertion |
|---|---|---|---|
| P01 | L3 | Accepted append preview on an initially clean graph | No live node, field, link, dirty-state, or undo change |
| P02 | L3 | Refused patch/replace preview containing a new node form | No residual spawner effects or transaction changes |
| P03 | L3 | Material tidy, blocks, and label previews | No live recompile, dirty-state change, or dependent mutation |
| P04 | L3 | Repeat P01/P03 on already-dirty fixtures | Original dirty state remains true without extra edits |

Use a spawner that could affect Blueprint structure, not only a harmless call node.
Count and inspect probe ownership after the operation.
Do not infer cleanup from the final graph node count alone.

## W — Patch, replace, swap, and repoint

| ID | Level | Setup and action | Required assertion |
|---|---|---|---|
| W01 | L3 | Patch the first literal on an existing node, then undo | Original literal restored exactly |
| W02 | L3 | Patch invalid literal | Refusal or verified rollback, original value retained |
| W03 | L3 | Patch one literal and one link together | Both requested changes occur, not only the literal |
| W04 | L3 | Inject failure after patch literal but before link | Both values and links return to initial state |
| W05 | L3 | Replace with additions and explicit removals, then undo | One undo restores the entire original graph |
| W06 | L3 | Inject failure between replacement add/remove phases | No appended half remains committed |
| W07 | L3 | Same-type swap with custom literal/object/text defaults | All compatible defaults and all edges preserved |
| W08 | L3 | Swap with incompatible pin/default conversion | Refusal before mutation, no implicit loss |
| W09 | L3 | Repoint occupied data input with fan-out | Requested readers connect to target, exact unrelated edges unchanged |
| W10 | L3 | Repoint one output of source with another live output | Keep mode preserves other output, delete mode refuses |
| W11 | L3 | Repoint eligible isolated source in delete mode, then undo | Source deletion and all restored edges are exact |

Run direct append acceptance after extracting its transaction-free core.
Do not count substring occurrence as proof that a particular edge exists.

## F — Data, fields, and remaining writers

| ID | Level | Setup and action | Required assertion |
|---|---|---|---|
| F01 | L3 | Table set R then remove R | R absent |
| F02 | L3 | Table remove R then set R, then repeated partial sets | Final row and each requested field equal sequential semantics |
| F03 | L3 | Create private variable and access it from an external Blueprint | Engine private metadata present, prohibited access fails compilation |
| F04 | L3 | Set valid then invalid struct defaults | Valid value reads back exactly, invalid value cannot report success |
| F05 | L3 | Fail a later field in an atomic multi-field operation | All earlier field changes restored |
| F06 | L3 | Material edit creates a comment and changes expression connections, then fails | Collection, properties, links, comments, and dirty state restored |
| F07 | L3 | Save owned fixture, unload/reload it | Reloaded content matches intended values and supported package kind |
| F08 | L3 | Force save refusal on an owned read-only scratch package | Explicit failed package outcome, no false saved result |
| F09 | L3 | Change signature or enum operation included in scope | Dependents and serialized values match the documented contract |
| F10 | L3 | Supported source-control provider checkout and refusal | Correct provider-specific result, no simulated-success claim |

F09 applies to each supported semantic operation kind, not every possible asset size.
If no provider environment exists for F10, provider support remains unverified and must not be advertised as verified.
Do not count a refusal-only test as successful checkout support.

## R — References and runtime

| ID | Level | Setup and action | Required assertion |
|---|---|---|---|
| R01T | L3 | One known in-edge and out-edge, query both and empty direction | Both accepted forms equal the union of explicit in/out |
| R02T | L3 | Query short, package, object, and generated-class forms | Same canonical asset and equivalent edge sets |
| R03T | L3 | Arm two nodes in one function, execute only one | Correct instruction attribution, unexecuted node gets no false hit |
| R04T | L3 | Inject an unmappable trace event | Unmapped count increases, no claim that no execution occurred |
| R05T | L1 + L3 | Delay or refuse PIE startup past startup deadline | startup_failed, zero observed played duration |
| R06T | L3 | Start PIE normally, then end early | Actual observed duration, valid start/end witnesses |
| R07T | L3 | Supply a non-world asset as the map | Refusal before play request |

Do not make tests slow with real long sleeps.
Use an injectable monotonic clock for pure lifecycle cases.
Use a short real engine smoke for actual start/end integration.

## U — Widget execution and user-facing truth

| ID | Level | Setup and action | Required assertion |
|---|---|---|---|
| U01 | L3 | Widget preview with empty/deny Execution | No widget construction or callback invocation |
| U02 | L3 | Widget preview with allow and a callback modifying a scratch marker | Effect occurs, execution is explicitly reported, no isolation claim |
| U03 | L3 | Callback requires arguments, or Execution value is invalid | Explicit refusal, no callback invocation |
| U04 | L1 + L3 | Generate help for one optional input and enum action | Required/default/allowed values match actual schema and behavior |
| U05 | L1 + L3 | Large discovery result at output cap | Bounded output, omitted count, usable next query |
| U06 | L3 | Nested widget with named-slot child | Exact child hierarchy and identities match engine tree |
| U07 | L3 | Capture a fixture with two visually distinct labeled regions | Actual image contains requested region, not only a created file |
| U08 | L3 | Layout fixture with comment anchors and measured geometry | Promised scope and anchors preserved, metric provenance explicit |
| U09 | L1 + L3 | Unavailable cost/profiling datum and one known measured datum | Unknown stays unknown, measured provenance is explicit |

For U07, use deterministic colored markers or a small image-region assertion.
Do not require a heavyweight vision model for routine image acceptance.
Do not call a screenshot test passed merely because PNG decoding succeeds.

## L — Logs and local trust

| ID | Level | Setup and action | Required assertion |
|---|---|---|---|
| L01 | L1 | Fail log append and rotation separately | Visible logging failure, factual tool outcome unchanged |
| L02 | L1 | Oversized record and concurrent append/rotation | Bound enforced, valid records, explicit omission, no silent race |
| L03 | L3 | Inspect actual MCP listener on candidate | Loopback binding, no undocumented network exposure |

Use temporary files for log tests. Do not fill the disk.

## B — Restricted batch

| ID | Level | Setup and action | Required assertion |
|---|---|---|---|
| B01 | L3 | Three graphRepoint operations in one graph | One transaction, one final compile, exact state, no save |
| B02 | L3 | Fail operation three after two successful mutations | Entire initial state restored, all attempted IDs reported |
| B03 | L3 | Earlier operation invalidates a later target | Deterministic refusal and full rollback, no stale pointer use |
| B04 | L1 + L3 | Cross-asset, cross-graph, duplicate ID, unknown kind/field | Refusal before mutation |
| B05 | L3 | Successful batch followed by one undo | Exact complete restoration |

## Z — Release

| ID | Level | Setup and action | Required assertion |
|---|---|---|---|
| Z01 | L0 | Compare declarations, capabilities, result migration, help, tests | No unaccounted exposed operation or stale tool count |
| Z02 | L4 | Install staged source package in minimal project under renamed path with spaces | Documented dependencies sufficient, registration and selected read/edit succeed |
| Z03 | L0 | Inspect staged package contents | Required content/license present, no engine distribution or local secrets/logs/caches |
| Z04 | L4 | Execute final required manifest on frozen candidate | Zero failed/blocked/missing required cases, artifact identity recorded |
| Z05 | L0 | Reconcile findings, board links, evidence, and deferred scope | No hidden open release defect, no deferred task counted as completed |

## Additional card requirements

This catalog covers named audit defects and shared safety boundaries.
An existing card can add stricter acceptance conditions.
Do not silently drop them when merging cards.
Add a named case for any release-relevant condition not represented here.
Do not invent broad new feature acceptance outside ARC01 and ARC09.

## Evidence reuse and final selection

Run cases for the current repair, not this entire catalog after each edit.
An unchanged coordinator does not need every guard permutation rerun for a field-specific fix.
A changed coordinator invalidates all affected mutation families.
A changed lexer/resolver invalidates its consumers.
A changed result formatter invalidates C cases and wrapper integration smoke.
A changed package invalidates clean-install smoke.

Use existing legacy cases when they prove the same assertions.
Map the case ID to that existing command instead of duplicating the test.
Do not remove legacy coverage merely because this catalog is shorter.
The final manifest must include every required new case and the non-redundant existing acceptance for exposed operations.
