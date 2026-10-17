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
Task R13, claim `452cb2dbb8364466810f876797b06abc` (owner `dsh-r13-01`). Approved base `1aea14e402bcdedce85a25381916e06d707c20e7`; branch `task/r13-native-comparison`; worktree `C:\ExampleDev\SampleRoot005\r13`. Original candidate `36bed13e2fe143a2c80337f6859ae48113e80a60` is preserved. The review corrections are a new commit; the correction commit is the commit that contains this evidence, and its full SHA is returned in the submission message.

Source identity:
- Baseline Epic 5.8.3 at `396c9f059903aed5fec78ecd3d437a40c6415368` (local `release` checkout).
- Pinned comparison target Epic 5.9 development at `5789fc07f00643fb37f85ede9894cd08f1cebcef`.
- Upstream development head read once on 2026-10-17 with `git ls-remote https://github.com/EpicGames/UnrealEngine.git refs/heads/ue5-main`; it resolved to `5789fc07f00643fb37f85ede9894cd08f1cebcef`, matching the pin. No target drift.
- TurboMCP approved base plugin tree `590661ec45c829264ea1743fccea994edb748092`; the comparison previously cited `2513a0bbb42604fd3f6427bc7536e61c7822c912`, which is superseded.

Comparison result: each deferred area has an adopt-native, thin-adapter, retain-turbomcp or defer decision with evidence, limitations and maintenance cost in `docs/audit/native-overlap.md` and `.json`. Enum named one-call workflow, struct one-call creation/type change/preview, and explicit VCS checkout are retained. That enum call is not atomic: a failed `SetEnums` can leave the asset present, and display-name failures do not roll back creation; read-back remains the existing repair `todo/214d6e7a.md`. The struct reader diagnostic covers loaded Blueprint `NewVariables` and DataTable row structs only; graph-only make/break use and unloaded assets are outside it, so zero readers are not proof of safe removal; the existing repair is `todo/7bd81b84.md`. VCS provider diagnostics are retained as diagnostic detail, but a null `GetState` is not proof that `ForceUpdate` refreshed: the pinned Perforce provider can return cached state. Failed-refresh coverage is a bounded follow-up under the R04 tool audit. Native enum/struct editing, source-control history/load/changelist, widget-animation lifecycle/bindings and the 5.9 stdio proxy are adopted. Track/key animation parity is deferred because the referenced 5.9 Sequencer toolsets were not found and the 5.8.3 Python animation toolsets are absent at 5.9. A native enum-edit preview is a thin-adapter candidate only if requested. No TurboMCP feature was removed because of a similarly named native tool.

These are investment recommendations, not live tool preferences. A live preference requires target availability plus accepted runtime and safety evidence. All 101 routing preferences remain `unverified`. R04 owns the remaining behavioral/workflow audit and R14 owns forward runtime certification for rows outside the five areas; R13 is not their pending owner.

Review corrections: removed the atomic enum-creation claim and recorded partial-failure/read-back debt; qualified struct reader safety to loaded Blueprint `NewVariables` and DataTable row structs and recorded the graph-only/unloaded limitation; qualified VCS freshness because `ForceUpdate` does not prove a successful refresh on the pinned Perforce provider; replaced the broad struct routing phrase with specific native candidates and separated investment recommendations from live preferences. No plugin, engine, test, dependency, scope policy, status or release change was made.

Discovery and availability: the 38-header C++ scan (274 baseline and 392 target declarations) is not exhaustive. R13 also found 598 candidate Python tool declarations in 29 files across 7 toolset plugins at 5.8.3; the engine-shipped Python toolset modules are absent at 5.9. Registration routes also include legacy MCP tool libraries, direct `IModelContextProtocolTool` registration, `MCPClientToolset`, AgentSkill, tool-search meta-tools and 5.9 baked manifests. All native toolsets in scope are source-present only; none is enabled by default, and runtime discovery and installed-distribution availability remain unverified. `bEnableToolSearch` is true in both revisions, so `tools/list` exposes meta-tools rather than every tool.

Validation:
- Correction candidate: `python Tools/execute-tests.py --profile offline --execute` exited 1. 22 of 23 suites passed, including `validate-repo`, `check-language`, `test-task-review` and `test-worker-scope`. `test-ownership` failed E03 again because the runner's captured PowerShell output hit a `UnicodeDecodeError` on byte `0x90`. Report: `.local/executions/335ead7d99d44769b976cdd61a2ba79b/report.json`; validation report: `.local/validation/178bdcdde0a5455faab89b02fd294d1e/report.json`; suite log: `.local/validation/178bdcdde0a5455faab89b02fd294d1e/test-ownership.log`. The run was not repeated after the same failure recurred.
- Earlier failed reports are retained, including `.local/executions/e08674fd363c4fa08b9c97a3ac11d6d6/report.json` for the original candidate `36bed13e2fe143a2c80337f6859ae48113e80a60` and `.local/executions/9adfea49139b48ae9fff8c0d4fc1a262/report.json` for the pre-commit run. All earlier attempts remain under `.local/executions/`.
- Reviewer independent observation: the unchanged original candidate `36bed13e2fe143a2c80337f6859ae48113e80a60` passed the full offline gate at `.local/executions/61754f5fc0d74577b68ac3c5f76e7c8b/report.json`. This is a separate observation, not a repair of the failed runs and not a fix for the environment-sensitive decoding problem.
- `python Tools/test-ownership.py` invoked directly: exit 0, `RESULT: PASS`. This is diagnostic evidence only, not a substitute for the failed gate.
- UTF-8 mode (`PYTHONUTF8=1`) and a UTF-8 console (`chcp 65001`) produced the same E03/`UnicodeDecodeError`. No test, runner, assertion or shell configuration was changed. The encoding debt is recorded as adjacent defect 10 in `docs/audit/native-overlap.md` for an integrator-owned repair.
- No Unreal build, editor launch, 5.9 runtime probe or installed-engine inspection was performed, as required for this source-only task.

Unresolved: 5.9 runtime discovery and behavior; installed-engine availability (R16); enum partial failure/read-back (`todo/214d6e7a.md`); struct reader coverage (`todo/7bd81b84.md`); VCS refresh freshness (R04 follow-up); native struct compile/dirty behavior after edits; exact 5.9 track/key animation equivalents; proxy replay of an in-flight mutation after reconnect; whether a project-defined Python toolset can still be registered at 5.9. Adjacent defects are recorded in `docs/audit/native-overlap.md`; none was repaired.

Changed files: `docs/tool-comparison.md`, `docs/audit/epic-tool-inventory.json`, `docs/audit/native-overlap.md`, `docs/audit/native-overlap.json`, `docs/audit/tool-routing.json`, and this Completion evidence. All 101 routing rows and every `unverified` preference are preserved. A reviewer must check acceptance before marking R13 done.

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
