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
- Help's 2000-character budget is a warning, not a cap; the declaration says capped.
- Status's “costs nothing and changes nothing” is false when logging is ON because the recorder writes/rotates.
- Status conflates declared, registered, enabled and discoverable layers; it reports settings/registration, not discovery.
- Help name matching is narrower than schema normalization (`Read`, space/underscore variants and full tool names).
- Help does not distinguish a missing inputSchema/properties object from a zero-parameter tool.
Each finding has an unapproved bounded proposal `R04-REPAIR-07` through `12` with exact repository-relative files, preserved behavior, synthetic probe, expected result, negative control, focused checks and rollback. Alternatives for help capping, status enablement and name normalization are recorded as unresolved decisions.

Validation:
- `python Tools/audit-inventory.py --check` exited 0: 101 definitions; 95 `parsed_call`, 6 `name_reference_only`.
- `python Tools/test-result-contract.py` exited 0: PASS, C03-C08.
- `python Tools/test-client-retry.py` exited 0: PASS, C01/C02.
- `python Tools/execute-tests.py --profile offline --execute` exited 0 at base `b353fdd`: report `.local/executions/98e74ff4df4c41c99ed12ac5e98aae9c/report.json`, tree digest `025e11e4004e45c19b1523930991be6176b711e560ecdf414c7b3d4b62586442`; all offline suites passed.
- No engine suite, editor, build, direct MCP call, packaging or recovery command was run. `test-agent-help.py`, `test-batch-tools.py` and the status uses in other suites are existing assertions only, not results here.
- Candidate-bound offline evidence is obtained after this commit and returned separately.

Changed files: `docs/audit/callable-contracts.md`, `docs/audit/callable-contracts.json`, and this Completion evidence only. No plugin, engine, test, helper, dependency, pin, routing preference, task status/readiness, TODO, review register or release control was changed.

Unresolved: status/help/schema runtime behavior; long-help cap; disabled/ambiguous name behavior; missing-schema fixture; declared/enabled/discoverable status contract; all three tools' lack of ARC02 result lines and `call_structured` implications; `R04-CALL-06` refusal recording; the 94 unreviewed rows. R04 remains incomplete; no merge, push, completion or claim release.

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

## Active dispatch checkpoint 2: status, help and schema

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
