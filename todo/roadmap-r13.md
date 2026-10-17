# R13: Compare native capabilities before expansion

ID: R13
Status: todo
Priority: P1
Readiness: ready
Owner: unassigned
Design: docs/design/protocol.md
Kind: investigation
Review: source-reviewed

## Problem
Inspect the upstream development head once at task start. Record its revision separately from the approved comparison pin. Do not edit engine.lock.json or upgrade the local engine. Compare only relevant APIs and label evidence from each revision explicitly. Compare native enum/struct/source-control/widget-animation and stdio proxy against concrete TurboMCP workflows, including safety and output costs. Names alone do not prove parity.

## Scope
Pinned Epic ToolsetRegistry/ModelContextProtocol/Toolsets interfaces and the deferred backlog. Keep the 5.8.3 baseline unchanged.

## Dependencies

R03

## Acceptance
- Each deferred proposal is adopt-native, thin-adapter, retain-TurboMCP or defer, with a demonstrated gap and maintenance cost.
- No feature is removed merely because Epic added a similarly named tool.
- No duplicate server, proxy or broad authoring implementation starts without the comparison.

## Verification
- `Inspect pinned upstream schemas and source interfaces`
- `Run parity probes in a separate 5.9 engine when available`
- `Update docs/audit/native-overlap.json with evidence and refresh triggers`

## Stop conditions
Do not expand into another domain. If a prerequisite is absent, record blocked with the exact missing input. After two identical unexplained failures, stop repeating the command and diagnose. Do not run a full engine rebuild without evidence that an incremental build cannot answer the question. Do not change public schemas, weaken safety contracts or start deferred features.

## Deliverable
A reviewed specification and evidence record for each confirmed repair, with duplicates linked and unsupported historical claims withdrawn explicitly. This investigation does not authorize implementing every finding.

## Completion evidence
Pending. Record source/engine/artifact identity, command, exit, log and remaining limitations. A reviewer must check acceptance before marking done.

## Related findings
- [ue59-protocol](ue59-protocol.md)
- [ue59-recorder](ue59-recorder.md)

## Source-comparison completion boundary
This early decision task can complete from pinned source/schema inspection with unsupported runtime parity explicitly marked unverified and expansion deferred. An unavailable 5.9 runtime must not block 5.8.3 safety fixes. Runtime certification belongs to the optional compatibility track.

## Three-way comparison deliverable
Use docs/tool-comparison.md, docs/audit/epic-tool-inventory.json and docs/audit/tool-routing.json. Preserve all TurboMCP rows. Review the declaration-parser candidates against pinned source and discover other native registration routes; the existing Experimental/Toolsets header scan is not an exhaustive Epic MCP inventory. Distinguish source-present, runtime-discovered, enabled-by-default and installed-distribution availability.

For each TurboMCP workflow identify exact native equivalents or explicitly unresolved parity on both versions. Record scope, inputs, output completeness, effects, failure/rollback/save semantics and candidate benefits. Use unverified preference until R04 supplies accepted runtime comparison evidence. Never infer a missing Epic feature from a missing header. Store identifiers and source references, not licensed engine implementation bodies.

Source comparison may complete without a 5.9 runtime. Pin the inspected revision, retain unverified runtime columns and keep the 5.8.3 baseline unchanged. Do not silently refresh the compatibility target while preparing an unrelated fix.

## Worker boundaries

Update the comparison Markdown/JSON and this task's evidence only. Preserve all
101 TurboMCP rows. Do not modify plugin code, engine source, engine.lock.json,
release workflows or task priorities. Propose a bounded follow-up if parser or
implementation work is necessary. Missing 5.9 runtime evidence must stay
unverified; it is not a reason to invent parity or build the whole engine.

Use the offline test profile. Return source identifiers, exact declaration
references, comparison decisions and unresolved questions. Stop for review;
do not automatically claim R16.

## Current review

**Decision:** plan. **Follow-up owner:** [R13](roadmap-r13.md).

The existing three-way inventory is a candidate source scan. This task review does not execute the separate forward-engine comparison.

### Next action and acceptance

Inspect exact pinned interfaces and current upstream revision separately. Compare each deferred workflow for inputs, effects, outputs, failure and maintenance cost. Keep all 101 rows, unverified runtime labels and no engine upgrade.

### Current source and verification

Reviewed at private main `6aee9dfd92a4c06a73e89c091973f982d715b7a2`. These source findings do not certify runtime behavior. Source links open the current file; line numbers refer to the reviewed revision.

- [engine.lock.json](../engine.lock.json)
- [docs/audit/epic-tool-inventory.json](../docs/audit/epic-tool-inventory.json)
- [docs/audit/tool-routing.json](../docs/audit/tool-routing.json)

Starting checks for the assigned follow-up:

- `python Tools/execute-tests.py --profile offline --execute`

These suites are starting checks, not proof of the new cases. Use owned synthetic fixtures and retain local evidence. Reference records do not authorize implementation.

See the [review register](../docs/audit/task-review.md) for definitions, duplicate ownership and observed test results.
