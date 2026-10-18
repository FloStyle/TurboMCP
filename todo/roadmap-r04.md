# R04: Audit callable behavior and result contracts

ID: R04
Status: todo
Priority: P1
Readiness: ready
Owner: unassigned
Design: docs/design/protocol.md
Kind: investigation
Review: source-reviewed

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
- `python Tools/execute-tests.py --profile offline --execute` exited 0 on the pre-commit correction tree: report `.local/executions/4e57b11fc68f4d22b42c5bc742d78758/report.json`, request commit `f9aa39fff11368622b72fde9a4a6a2218aae1b0c`, tree digest `08662611d28ae011c46da76173be9784270f2212420412675b1861871d1e5b79`.
- No engine/editor/build/MCP/recovery command was run. The proposed live acceptance is unexecuted.

Changed files: `docs/audit/widget-persistence-prerequisites.md` (new) and this Completion evidence only. No code, test, fixture, config, task status/readiness, TODO, routing, release or scope-policy change.

Unresolved: whether the `BlueprintGraph` test dependency and automation seam can be built; whether the runner profile can own the two-pass restart without broadening normal suites; whether process start-time plus loopback socket-owner identity/foreign-listener proof is accepted (the existing `--editor-pid` is insufficient); whether `GEditorPerProjectIni` plus source/project/global settings files can be observed without writing and restored/verified; whether the current KILL_ON_JOB_CLOSE outer job can preserve unrelated unsaved assets on refusal, or an outer lifecycle detach/recovery change is required; whether the no-save control can be kept isolated. If any is absent, the plan is blocked, not passed. R04 remains incomplete; no merge, push, completion marking or claim release.

Correction after review: this commit preserves candidate `f9aa39fff11368622b72fde9a4a6a2218aae1b0c` and addresses report SHA-256 `66b4b2ec3637b04233a653fd38ac26ed92c9c4d23c8ee0d71634c699e0fbea55`. The probe now binds the loopback socket owner to the expected PID/start time/canonical project and engine, with foreign/ambiguous/permission/replacement negative controls. Config provenance now distinguishes `GetDefaultConfigFilename()` from `GEditorPerProjectIni`/the loaded hierarchy, never writes settings files, restores in-memory state on every same-process path and requires a fresh token/PID `effective=Never` result. Cleanup eligibility is explicit; the current KILL_ON_JOB_CLOSE job is a blocking lifecycle gap for refusal with unrelated unsaved assets, hard death uses the journal/recovery path, and failed readback is reported **persistence unverified**. All subtasks remain `proposed_unapproved`.

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


## Active dispatch checkpoint 6: persistence prerequisite contract

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
