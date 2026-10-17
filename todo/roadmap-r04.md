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
Task R04, first dispatch checkpoint (`call-log-contracts`), claim `ab2cf6535c584bc6a9419c79e85d0b04` (owner `dsh-r04-01`). Approved base `1180c6d43655195dc2772b6d1f0b80588e4b466a`; branch `task/r04-call-log-audit`; worktree `C:\ExampleDev\SampleRoot005\r04`. The candidate is the commit containing this evidence; its full SHA is returned in the submission message. R04 is not complete.

Source identity:
- Engine 5.8.3 at `396c9f059903aed5fec78ecd3d437a40c6415368` (`engine.lock.json`).
- Plugin tree at the base: `590661ec45c829264ea1743fccea994edb748092`.
- Claim store: `c:\ExampleDev\turbomcp\.git\turbomcp\task-claims.json`, SHA-256 `308037d40eec30e2e8342409642786cae795bd5914dc5081fcb358812cd92758`; active R04 claim inspected in this worktree.

Scope and inventory:
- `docs/audit/callable-contracts.json` contains schema 1, the approved base and plugin/engine identities, and exactly 101 unique tool IDs from `docs/audit/capabilities.json`.
- Four rows are `reviewed_source`: `ReadTurboMCPCallCost`, `ReadTurboMCPCallLog`, `EditTurboMCPCallLog`, `EditTurboMCPCallLogClear`. The other 97 are explicitly `not_reviewed` and retain their capability source/test references.
- The audit traces declarations, the `FRecordingToolset`/`FScopedCall` wrapper, implementations, the text/result client path, defaults, numeric parsing, effects, success/refusal and truncation. Source conclusions are kept separate from existing engine-suite assertions and from unrun runtime probes.

Findings recorded (source review only; no runtime claim):
- `R04-CALL-01`: `IsNumeric` + `Atoi` is not whole-integer validation; fractional/signed input can be truncated and overflow is unchecked.
- `R04-CALL-02`: `ReadTurboMCPCallCost` returns the empty-log report before validating `Cap`; an empty log with `banana` or `0` is accepted while `ReadTurboMCPCallLog` refuses an invalid `Limit`.
- `R04-CALL-03`: the log's truncation sentence tells the caller to use `since=<newest shown>`, but selection is newest-first and `since` is a later-than bound, so it cannot retrieve the omitted older rows.
- `R04-CALL-04`: the four tools return plain `FString` without an ARC02 result line; `Tools/tmcp.py` treats every `TurboMCP.*` toolset as migrated, so `call_structured` without `requires_result=False` would raise. Whether to migrate the tools or narrow the client is unresolved.
- `R04-CALL-05`: switching the log OFF from ON writes a start record and suppresses its end, leaving a start-only record that the timeline renders as running/missing-end.
Each finding has a bounded repair proposal in the JSON and Markdown with owner R04, exact files, preserved behavior, synthetic probe, expected result, negative control, validation and rollback. Proposals are not implementation permission.

Validation:
- `python Tools/audit-inventory.py --check` exited 0: 101 definitions; 95 `parsed_call`, 6 `name_reference_only`.
- `python Tools/test-result-contract.py` exited 0: PASS, C03-C08. It does not exercise these four tools.
- `python Tools/test-client-retry.py` exited 0: PASS, C01/C02. It does not exercise these four tools.
- `python Tools/execute-tests.py --profile offline --execute` exited 0 at base `1180c6d`: report `.local/executions/e08918e140e4430aa2d550140999f441/report.json`, tree digest `fa4e42656721dc2a9cdc6f4db8642749b916ba7e5212d5b72ed93871913d24c0`; all offline suites passed.
- No engine suite, editor, build, direct MCP call, packaging or recovery command was run. `test-call-log.py` and `test-call-cost.py` are existing engine-suite assertions only, not results for this checkpoint.
- Candidate-bound offline evidence is obtained after this commit and returned separately.

Changed files: `docs/audit/callable-contracts.md`, `docs/audit/callable-contracts.json`, and this Completion evidence only. No plugin, engine, test, helper, dependency, pin, descriptor, status/readiness, TODO, review-register, routing or release control was changed. Raw logs and machine paths outside the repository were not committed.

Unresolved: the four tools' runtime behavior; fractional/signed/overflow and empty-log invalid-cap cases; 50/500/4000/200 boundaries; truncated paging; ON->OFF and clear-with-log-on record shapes; and the `call_structured` migration decision. The 97 unreviewed rows remain unreviewed. Stopping for integration review; no merge, push, completion marking or claim release.

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
