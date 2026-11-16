# Binding architecture for the first release

[Documentation](README.md)

Revision 3. These contracts define required behavior for the first release. They do not certify the current implementation. [AGENTS.md](../AGENTS.md) defines repository workflow; the [design index](design/README.md) explains implementation responsibilities.

## Contract index

- [ARC01 — Release boundary](#arc01--release-boundary)
- [ARC02 — One result contract without replacing every public signature](#arc02--one-result-contract-without-replacing-every-public-signature)
- [ARC03 — Mutation coordination](#arc03--mutation-coordination)
- [ARC04 — Compile baseline and diagnostics](#arc04--compile-baseline-and-diagnostics)
- [ARC05 — Rollback and persistence are separate](#arc05--rollback-and-persistence-are-separate)
- [ARC06 — Planning and stale-state protection](#arc06--planning-and-stale-state-protection)
- [ARC07 — Graph text and destructive replacement](#arc07--graph-text-and-destructive-replacement)
- [ARC08 — Exact edit semantics](#arc08--exact-edit-semantics)
- [ARC09 — Batch release scope](#arc09--batch-release-scope)
- [ARC10 — Preview and project-code execution](#arc10--preview-and-project-code-execution)
- [ARC11 — Runtime observations and reader truth](#arc11--runtime-observations-and-reader-truth)
- [ARC12 — Testing architecture](#arc12--testing-architecture)
- [ARC13 — Build, install, and packaging](#arc13--build-install-and-packaging)
- [ARC14 — Local trust and logging](#arc14--local-trust-and-logging)
- [ARC15 — Repository workflow and release authority](#arc15--repository-workflow-and-release-authority)
- [ARC16 — Decisions taken after the first run](#arc16--decisions-taken-after-the-first-run)

## Authority and scope

Follow this document before the roadmap or archived audit recommendations.
Do not replace these decisions with a new architecture.
Use existing engine APIs and existing plugin helpers where they meet these contracts.
Extract small shared helpers only when multiple repaired callers need them.
Do not introduce another service, database, framework or general workflow engine.
Changes to module boundaries require a bounded design and packaging evidence; keep existing interfaces stable.

If engine behavior contradicts a decision, record the contradiction with evidence.
Continue unrelated safe work. Ask the owner only for a genuine architectural exception.
Do not ask the owner to select routine implementation details.

## ARC01 — Release boundary

Prepare an experimental source release for Windows and the tested Unreal 5.8.3 source revision.
Do not advertise Launcher, Linux, macOS, or adjacent engine-version support.
Keep existing declared capabilities where their supported behavior can meet this contract.
Do not remove a tool merely because its regression fails.

Fix defects in implemented capabilities before adding roadmap features.
Do not implement unbuilt curve editing, general asset CRUD, widget animation authoring, general material graph authoring, or unbounded input injection.
Keep those requests visible in backlog with disposition `deferred_by_scope`.
Do not mark a deferred feature done.

Preserve the existing batch implementation. Its release boundary is ARC09, not general multi-asset atomic editing.
A narrower supported domain must produce explicit refusal outside that domain.
A narrow domain does not permit false success within it.

Stable 1.0 guarantees remain unavailable until their acceptance gates pass.
The executor must not weaken a gate to meet a date.

### Bounded runtime input exception

The approved play-input foundation permits `EditTurboMCPPlayInput` to route bounded keyboard/gamepad events through Slate in one in-process PIE world with one local player. It executes game/UI code and is not an asset edit: asset transactions, compile, save and undo do not apply. Asset-edit PIE refusal remains unchanged. Holds expire after at most five wall-clock seconds, require normal editor ticks and release on PIE end/module shutdown. Delivery is not evidence that an action fired. Direct Enhanced Input action injection, OS input and indefinite holds remain outside this slice. The approved NOV05-15 extension adds explicit session-bound in-process local-player targets; see docs/audit/field-reports/nov14-pie-client-target-contract.md. The input slice has focused two-world keyboard isolation evidence; see the contract for remaining validation limits. Cross-window forwarding must refuse before input, without changing editor preferences. External-process input and console targeting are not provided by this extension.

## ARC02 — One result contract without replacing every public signature

Keep current FString tool return types for this release.
Keep the existing human-readable body where possible.
Append exactly one final result line from a shared formatter:

```text
; tmcp-result-v1: {"schema":1,"outcome":"changed","code":"ok","changed":true,"assets":["/Game/Example"],"compile":"passed","rollback":"not_needed","save":"not_requested","complete":true,"operationId":"opaque-id"}
```

The object contains machine-readable metadata, not unescaped embedded JSON.
The final line is authoritative. Text inside asset content is not metadata.
Only the outer tool boundary appends this line.
Inner helpers return typed internal results, not formatted public strings.
Do not infer internal outcomes by searching old result text.

Required keys:

| Field | Values or meaning |
|---|---|
| schema | Integer 1 |
| outcome | `read`, `planned`, `unchanged`, `changed`, `refused`, `failed`, `partial`, `outcome_unknown` |
| code | Stable snake_case diagnostic code, `ok` on success |
| changed | true, false, or null when unknown |
| assets | Canonical paths of affected assets, empty for no asset effect |
| compile | `passed`, `failed`, `not_applicable`, `not_run` |
| rollback | `complete`, `failed`, `not_needed`, `not_supported` |
| save | `saved`, `failed`, `partial`, `not_requested`, `not_applicable` |
| complete | false when the response omits requested data or cannot establish the full outcome |
| operationId | Unique identifier for this invocation, not a deduplication guarantee |

Add a diagnostics array when individual failures need stable codes and target paths.
Add `effects` for UI changes, script execution, files, and known external effects.
Do not represent unobserved effects as false. Use unknown where necessary.

Reserve space for metadata before applying an output budget.
Do not truncate metadata or a syntactic graph form.
An omitted list must include its omitted count and a continuation or refinement instruction.

The Python client gets a structured result method and retains its existing text method.
The structured method validates the final line and extracts all MCP text blocks in order.
The compatibility text method returns the body without the metadata line.
Expected-refusal tests use the structured method when they assert an outcome.
The CLI uses the structured outcome for its exit status.
Invalid or missing metadata from a migrated TurboMCP tool is a protocol failure, not success.
Engine-native tools retain their separate adapter. Do not require TurboMCP metadata from them.

The engine may still report MCP transport success for a plugin refusal.
Document this limitation. Do not patch engine MCP dispatch in this release.
Clients must check the TurboMCP operation outcome.

CLI exit codes: 0 for completed success, 1 for refusal/failure/partial, 2 for unknown outcome or protocol failure.
Do not add automatic replay to recover any of those outcomes.

Migrate shared formatting early. Migrate affected wrappers with each family repair.
Complete migration and consistency tests for every exposed TurboMCP tool before release acceptance.

## ARC03 — Mutation coordination

Use one plugin-local coordinator for asset edits.
The coordinator runs engine mutations on the game thread.
It owns readiness checks, transaction lifetime, compile verification, rollback, and result construction.
Family-specific cores own validation and the actual object changes.

Use this sequence:

1. Acquire the plugin mutation guard without blocking another request indefinitely.
2. Resolve every target unambiguously.
3. Reject PIE, simulation, shutdown, conflicting compilation, or an unrelated active transaction.
4. Validate the complete request without changing the live asset.
5. Capture the current fingerprint and initial dirty state.
6. Require an appropriate compile baseline for compilable assets.
7. Start one owned editor transaction.
8. Call Modify on every participating object before its first mutation.
9. Apply the family core.
10. Compile once per affected compilable asset.
11. Compare requested and actual state.
12. Commit success or restore the owned transaction.
13. Verify restoration before reporting rollback complete.
14. Release the guard.

Do not acquire several independent transactions inside one public edit.
Do not call a public writer from another writer.
Extract and call a transaction-free core instead.

Refuse commandlet edits that require editor undo when that undo facility is unavailable.
Do not use global Undo to recover an unrelated transaction.
Track transaction ownership explicitly through the available engine API.
End a transaction exactly once. A scoped destructor must not end it a second time.
Canceling a transaction record is not restoration.

## ARC04 — Compile baseline and diagnostics

Current restricted batch workflow (2026-10-30): ARC09 uses explicit compilation after the edit transaction, following the owner-requested single robust current workflow. The compile-coupled requirements below do not make BatchEdit compile implicitly. A later compile failure does not roll back a prior batch.

Do not compile the real asset in Read or non-executing Preview.
Use an explicit Compile operation to establish a baseline when no valid baseline exists.
Extend that operation to existing supported compilable asset families when necessary.
Do not create hidden compile work inside a preview to obtain the baseline.
Cache that baseline by asset and semantic fingerprint within the editor session.
A changed fingerprint invalidates the baseline.
A successful verified edit refreshes its baseline.

The baseline contains normalized diagnostic identity and multiplicity.
Use severity, message, and stable object/node identity where available.
Do not compare only the number of errors or warnings.
Pre-existing diagnostics may remain. Any new error or warning causes rollback.
A changed diagnostic with the same count is still new.

A writer without a valid baseline refuses with `compile_baseline_required` before mutation.
The client instruction must name the explicit Compile tool and asset.
Test fixtures establish a baseline once before focused edits.
Do not compile twice for every mutation when a valid baseline already exists.

Some asset types cannot compile. Their manifest must specify `not_applicable` and their exact structural checks.
Do not call `MarkBlueprintAsStructurallyModified` a verified compile.

## ARC05 — Rollback and persistence are separate

Mutation tools modify editor memory unless their public contract explicitly requests save.
Do not add automatic saving to graph, material, or field edits.
One undo restores the entire successful memory edit.
It must preserve node GUIDs, links, values, names, positions, comments, and initial dirty state as applicable.

On failure, verify structural restoration and dirty-state restoration.
Recompile restored assets when needed to synchronize generated state.
Compare restored diagnostics against the original baseline.
Restore the initial dirty flag only after structural restoration succeeds.
Restore or invalidate the compile cache to match the actual restored fingerprint.
Do not claim exact restoration from a successful Undo return alone.
If restoration fails, report `failed` with `rollback=failed` and `changed=null`.
Do not report unchanged. Do not save. Refuse subsequent plugin mutations until the session recovers.
Read and diagnostic tools remain available.

Recovery instructions must preserve the user's unsaved work.
Do not automatically reload a package over unsaved changes.
The owner must approve destructive recovery.

Save is an explicit operation with per-package outcomes.
A successful save requires a successful engine write result and durable verification appropriate to the path.
The clean-fixture acceptance test reloads the package and compares its contents.
Do not claim generic multi-file disk atomicity.
A partial save must name saved, failed, and unattempted packages.
Do not undo memory and then pretend that prior disk writes disappeared.

## ARC06 — Planning and stale-state protection

Plans contain stable asset, graph, node, and pin identifiers.
A planned result does not certify compilation or runtime semantics.
Report compile=not_run when planning did not compile a transient candidate.
Do not retain raw UObject pointers across independent tool calls.
Include a semantic fingerprint in sensitive edit exports and previews.
Re-resolve targets before application.
Refuse a stale fingerprint before the first mutation.

Use a deterministic SHA-256 digest over canonical semantic data.
Sort unordered sets and maps before hashing.
Include class, graph/node GUIDs, pin identity/direction/types, links, defaults, and relevant signatures.
Do not include transient pointers, timestamps, UI selection, or log contents.
Include layout fields when the requested operation depends on layout.

Calls are serialized within the coordinator.
Serialization does not replace stale-state validation against human editor changes.

## ARC07 — Graph text and destructive replacement

Keep compact/read/full formats as inspection formats.
Do not claim they are lossless serialization formats.
Add an explicit `edit` value to the existing graph reader's format argument.
Use the existing graph syntax with a versioned header and strict validation.
Do not invent a second general graph language.

The edit export begins with:

```text
; tmcp-edit-v1: {"asset":"/Game/Example.Example","graph":"EventGraph","graphGuid":"32-lowercase-hex","baseFingerprint":"sha256-value","complete":true}
```

The exporter must emit every supported semantic node and every pin-to-pin link.
Emit shared pure-node definitions once. Reference them by stable identity.
Use full node GUIDs plus canonical asset/graph ownership at mutation boundaries.
In edit exports, node IDs are 32 lowercase hexadecimal digits without braces or hyphens.
The header supplies their owning asset and graph GUID.
Reject an ID absent from that exact owning graph.
The graph name is descriptive. The graph GUID is authoritative.
Short display IDs remain convenience aliases only when unambiguous.
Keep alias resolution in one helper. Never choose the first ambiguous match.

Preserve source pin identity on every edge.
Use JSON-compatible escaping for quoted string tokens.
Preserve literal, object, and localized text defaults distinctly.
Do not coerce them all through DefaultValue text.

If export encounters an unsupported semantic node, set complete=false and list the unsupported identities.
Replacement refuses incomplete exports.
Do not silently omit unsupported nodes and then delete them.
Comments and layout-only objects are preserved unless an explicitly supported edit names them.

Append may accept validated manual snippets without the edit header.
Patch may accept validated updates to explicitly named existing nodes.
Both must reject unsupported syntax, ambiguous IDs, and unsupported pin conversions before mutation.

Replacement requires a complete edit header with a matching base fingerprint.
Omission alone does not authorize deletion.
Require one explicit directive per removed node:

```text
; tmcp-delete-v1: {"nodeId":"32-lowercase-hex"}
```

The replacement plan lists every addition, update, and deletion.
Refuse omitted existing semantic nodes without explicit deletion directives.
Refuse duplicate/conflicting directives and mandatory graph-entry deletion.
This is an intentional safety change for the first release.
Update callers, tests, help, and examples together.

The supported node-family list comes from actual factory support and passing corpus tests.
Do not add new families just to complete the roadmap.
Unknown, ambiguous, or unproved creation families refuse by name.

## ARC08 — Exact edit semantics

Patch applies literals and links together or restores the original state.
Connection replacement must follow schema responses, including occupied inputs.
Do not accept an invalid raw literal before the schema validates it.

Swap preserves all compatible unlinked defaults and exact link endpoints.
A lossy default conversion requires refusal, not a guessed conversion.
A same-type swap must preserve semantics.

Repoint changes only the named output's readers.
Delete the source only if no unrelated behavior or connection would disappear.
Otherwise refuse source deletion before mutation.

Data-table operations follow caller order.
For one row, set then remove leaves no row. Remove then set leaves the final requested row.
Repeated partial sets retain the last requested value per field in sequence.
Apply the final planned state under one owned transaction.

Private Blueprint variables use the engine's access metadata, not editor read-only flags.
Struct/default setters must report failure and verify the resulting values.
Multi-field mutation must restore all fields if the chosen atomic operation fails.

## ARC09 — Batch release scope

Support only the existing graphRepoint operation kind in the first release batch.
Require one Blueprint asset and one graph per batch.
Refuse cross-asset or cross-graph batches before mutation.
Refuse unknown operation kinds and unknown fields.
Require unique operation IDs.

The batch owns one edit transaction. Compilation and saving are separate explicit operations.
Use preview, edit, source read-back (and visual review where needed), explicit Compile, then explicit Save.
Compilation may reinstance objects and is not covered by the earlier edit undo transaction.
A successful edit reports compile=not_run and save=not_requested; a later compile failure never implies automatic rollback.
Each operation core must not start, end, undo, compile, or save independently.
Re-resolve and revalidate each operation against the state produced by preceding operations.
Abort and attempt to restore the entire batch when any operation fails.
Verify captured source state and the previous dirty flag after owned undo. Unreal may invalidate transient compile status; source restoration does not certify compiled behavior.
If restoration cannot be verified, report partial and block subsequent TurboMCP Edit dispatch for the editor session. Keep enabled read/status tools available for inspection. Recover the asset before restarting; never save unverified changes.
A result records every attempted operation, including a partially applied failing operation.
Do not save from the batch.

General multi-asset editing and cross-file atomic persistence remain backlog work.
Do not mark the original broad T12 card complete when only this subset works.
Create a clearly named subset card and link it to T12.

## ARC10 — Preview and project-code execution

Graph/material planning uses isolated probes or existing metadata.
It must not spawn into live graphs or recompile live materials.
Destroy transient probe ownership deterministically after use.
Do not use forced global garbage collection as the ordinary cleanup mechanism.

Widget construction can execute project code even without a named callback.
Add an explicit `Execution` string argument to the widget preview tool.
Accepted values are empty/deny and allow. Empty means deny.
Validate this argument before constructing a widget or invoking a callback.

When execution is denied, return a clear refusal if rendering requires project-code execution.
Do not silently run construction because FunctionName is empty.
When execution is allowed, mark the result and help as script-capable.
State that external effects are not sandboxed or reversible.
Keep the named callback's parameter validation.
Do not claim an effectful preview is a structurally read-only operation.

Update schema users and fixtures with the argument in the same change.
Do not introduce a hidden persistent execution-consent checkbox.
Consent is explicit per call.

## ARC11 — Runtime observations and reader truth

Reference direction both is the union of the explicit in/out operations.
Apply the same resolution and readiness rules to every accepted asset form.
Unknown registry readiness or query failure is not zero references.

Trace uses the actual executing bytecode offset.
Report unmapped events separately from zero events.
A zero mapped-hit count does not prove the target never executed.

Play-check uses explicit states: requested, started, ending, ended, startup_failed, end_failed.
Start the measured play-duration clock only after the start witness.
Use a separate startup deadline.
Never label requested elapsed time as played time.
Validate that the target package contains the required world before requesting play.

Cost reports label provenance: measured, engine estimate, heuristic, unavailable.
Do not convert unavailable data to zero.
Capture reports distinguish requested frame, actual captured region, and geometry estimates.
A saved image path alone is not proof of correct framing.

## ARC12 — Testing architecture

Use the existing Python suite entry points and runner.
Add stable case selectors where a large suite prevents a small test.
Keep all-cases compatibility.
Do not install a new test framework solely for this work.

Build pure tests around real production parser/state/result helpers.
Do not test a reimplementation of the production algorithm.
Mock transport or lifecycle boundaries, not the behavior under test.
If a C++ helper requires Unreal, test it through focused native automation after the plugin build.
Do not reproduce its algorithm in Python merely to label the test pure.
Reuse the same engine session for those native cases.

Use one small scratch Blueprint fixture for graph safety cases when reset is reliable.
Use separate tiny fixtures for material, data, widget, and runtime cases.
Snapshot only owned fixtures. Restore them after each mutating case.
Verify restoration before the next case.

The independent graph oracle reads engine nodes, pins, defaults, and links directly.
It must not invoke the production serializer to produce expected data.
The material oracle reads expression properties and connections directly.
The data oracle reads final rows/fields and their exact values.

Use injected failures through the actual shared core when necessary.
Keep failpoints test-only, default-off, and unreachable through public MCP arguments.
A failpoint requires one documented location and one deterministic effect.
Do not create production-only success bypasses.

Use the explicit acceptance catalog. Do not invent weaker substitutes.

## ARC13 — Build, install, and packaging

Use the current engine revision until a documented incompatibility proves a change necessary.
Declare all required plugin dependencies or move fixture-only dependencies out of the shipping module.
Prefer declaring existing required dependencies over a module split during this release.

Discover the plugin through TestProject/AdditionalPluginDirectories and validate the exact checkout path.
Do not modify engine-wide plugin mounts.
Fail missing artifacts and stale loaded-module identity.
Use incremental plugin builds, not repeated clean engine builds.

The public source release preserves the complete tracked repository, including contributor tools, test fixtures, tasks and specifications.
The installable plugin package is a separate artifact containing the plugin and validated target-specific binaries; development fixtures and tooling do not belong in that install package.
Neither artifact includes engine source, credentials, local evidence logs, caches or private recovery archives.
Document prerequisites honestly.
Perform the minimal clean-install test on the actual staged package.

## ARC14 — Local trust and logging

Support trusted local clients only.
Verify the actual listener binds loopback.
Do not add remote exposure, authentication infrastructure, or a hosted service in this release.

Logging remains opt-in.
Document that arguments and results may contain project information.
Check append and rotation failures. Surface them without changing an edit's factual outcome.
Serialize rotation and append together.
Bound oversized records and state what was omitted.
Do not claim logs prove an edit's final state.

## ARC15 — Repository workflow and release authority

AGENTS.md defines the workspace workflow. TODO.md links to repository-owned tasks.
No external task board is required. Evidence establishes correctness; status text does not.
Private commits and migration pushes are authorized for this restructuring. Public publication is a separate operation.
Verify the complete tracked tree with the exporter. An owner-approved [retained-history cutover](public-preparation.md) may publish only the reviewed rewritten refs in the independent destination. Keep original unsanitized ancestry private.

## ARC16 — Decisions taken after the first run

The first run was reviewed. These decisions close questions it left open, and one rule it broke.

### D-008: an empty graph name

An empty graph name never means "take the first graph".

| Caller | Behavior |
|---|---|
| Mutating tool | Refuse with `graph_name_required`, naming the asset and the graphs the caller could have named. |
| Reader or analysis tool | Treat it as "every graph in the asset". Name each graph scanned and its own counts. Set `complete=false` if any graph could not be read. A reader that cannot produce an asset-wide answer refuses instead. |

Asset-wide scanning is an explicit iteration at the call site.
`TurboMCPNodeId::FindGraphByName` keeps refusing an empty name.
This decision supersedes the five calling tests and must update them deliberately.

### D-009: a safety rule is never narrowed to pass a case

ARC04, ARC05, and ARC07 state release contracts, not suggestions.
If a contract refuses legitimate work, the fixture or the contract is wrong, and the owner decides which.

Narrowing a contract is a product change that the owner must authorize.
A narrowed contract must never report `compile=passed`, `changed=true`, or "verified" while it excludes evidence
that would have decided the outcome.

The first run narrowed ARC04 to diagnostics whose text named the edited graph, and reported `compile=passed` while
six diagnostics in the same asset were excluded. The cause was a discarded baseline in the same block. Both are
repaired; the rule is restored to package-wide identity and multiplicity.

## Owner-authorized daily source publication (2026-11-16)

The owner authorizes daily source prereleases after successful main-push
validation in the cleaned FloStyle/TurboMCP repository. This supersedes earlier
manual-only wording for that development channel. The exact guards, rolling
daily tag rule and source-only limits are in [the release process](release.md).
Stable version tags, installation packages and stable acceptance remain
manual and unchanged. This automation does not change repository visibility.
