# R08: Specify domain read-back and restoration repairs

ID: R08
Status: todo
Priority: P1
Readiness: ready
Owner: unassigned
Design: docs/design/editor-domains.md
Kind: investigation
Review: source-reviewed

## Problem
Check actual read-back versus requested counts, inherited defaults versus instance overrides, table preview/refusal consistency and dependency effects. Preserve unconnected material constants. Split fixes by asset domain; consult native overlap before expansion.

## Scope
Material readers/writers, tables, enums, structs, variables, functions, overrides and widgets. Current operations only; no new authoring family.

## Dependencies
R07

## Acceptance
- Every confirmed defect has an asset-specific independent read-back and restore test.
- No missing value is reported as a default or zero without evidence.
- Widget preview/code-execution side effects are documented and tested.

## Verification
- `python Tools/execute-tests.py --profile engine --suites test-material,test-datatable-rows,test-enum-add,test-struct,test-widget-tree,test-widget-retarget --execute`

## Stop conditions
Do not expand into another domain. If a prerequisite is absent, record blocked with the exact missing input. After two identical unexplained failures, stop repeating the command and diagnose. Do not run a full engine rebuild without evidence that an incremental build cannot answer the question. Do not change public schemas, weaken safety contracts or start deferred features.

## Deliverable
A reviewed specification and evidence record for each confirmed repair, with duplicates linked and unsupported historical claims withdrawn explicitly. This investigation does not authorize implementing every finding.

## Completion evidence
Pending. Record source/engine/artifact identity, command, exit, log and remaining limitations. A reviewer must check acceptance before marking done.

## Related findings
- [1256778b-652c-4534-8c3b-133dc44c8ec0](1256778b.md)
- [1f5fe672-f84d-40f1-98c0-adc77a8a8816](1f5fe672.md)
- [214d6e7a-7b19-4efa-a08f-7cf6a4461eec](214d6e7a.md)
- [7ae3dd76-6224-4318-ac79-78fd5306dde8](7ae3dd76.md)
- [7bd81b84-da3f-4132-bb87-de189eea69ac](7bd81b84.md)
- [ba6df3c8-12e8-4678-a020-a89881e3e0d7](ba6df3c8.md)
- [c4485ba2-aa66-4413-b519-722823b1263f](c4485ba2.md)
- [f434ffba-f4cc-49a1-bb51-21b51941c8b7](f434ffba.md)

## Current review

**Decision:** plan. **Follow-up owner:** [R08](roadmap-r08.md).

Confirmed starting points include requested enum counts, grouped table operations, omitted object overrides and make/break reader gaps.

### Next action and acceptance

Produce one specification per asset domain. Pair every success case with invalid input and restoration; preserve loaded versus global coverage. Defer new authoring rather than filling gaps by feature expansion.

### Current source and verification

Reviewed at private main `6aee9dfd92a4c06a73e89c091973f982d715b7a2`. These source findings do not certify runtime behavior. Source links open the current file; line numbers refer to the reviewed revision.

- [Plugins/TurboMCP/Source/TurboMCP/TurboMCPDataTableRows.cpp](../Plugins/TurboMCP/Source/TurboMCP/TurboMCPDataTableRows.cpp#L409)
- [Plugins/TurboMCP/Source/TurboMCP/TurboMCPEnumAdd.cpp](../Plugins/TurboMCP/Source/TurboMCP/TurboMCPEnumAdd.cpp#L275)
- [Plugins/TurboMCP/Source/TurboMCP/TurboMCPStruct.cpp](../Plugins/TurboMCP/Source/TurboMCP/TurboMCPStruct.cpp#L582)
- [Plugins/TurboMCP/Source/TurboMCP/TurboMCPWidgetTree.cpp](../Plugins/TurboMCP/Source/TurboMCP/TurboMCPWidgetTree.cpp#L124)

Starting checks for the assigned follow-up:

- `python Tools/execute-tests.py --profile engine --suites test-datatable-rows,test-enum-add,test-struct,test-widget-tree --execute`

These suites are starting checks, not proof of the new cases. Use owned synthetic fixtures and retain local evidence. Reference records do not authorize implementation.

See the [review register](../docs/audit/task-review.md) for definitions, duplicate ownership and observed test results.
