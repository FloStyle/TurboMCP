# R13: Measure 5.9 native overlap before approving new features

ID: R13
Status: todo
Priority: P1
Readiness: ready
Owner: unassigned
Design: docs/design/protocol.md
Kind: investigation

## Problem
Re-fetch the development head once at task start. If it differs, record a new pinned snapshot and compare only relevant APIs. Compare native enum/struct/source-control/widget-animation and stdio proxy against concrete TurboMCP workflows, including safety and output costs. Names alone do not prove parity.

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
