# Define field syntax without changing value semantics

ID: abdd2c08-6544-4730-b183-e4d856aad868
Status: todo
Priority: P3
Readiness: needs_reconciliation
Owner: unassigned
Design: docs/design/editor-domains.md
Review: source-reviewed
Validation: unbuilt_untested

## Current review

2026-10-23 d3678da implements strict DataTable scratch-row import completion: captured Error/Fatal diagnostics refuse even with a non-null importer result, and trailing non-whitespace refuses. Validation occurs before any removal/write. Done implementation slice, unbuilt_untested. Remaining abdd2c08: field/display-name ambiguity, nested syntax and precision consistency across other parsers. No tests/build/editor runs.

**Decision:** partial. **Follow-up owner:** [R04](roadmap-r04.md), [R08](roadmap-r08.md).

A common type resolver already maps float/real/double to PC_Real, while field/options and row operations have different grammars. A shared type resolver is not a shared field parser.

### Next action and acceptance

Inventory accepted syntax and precision for each current caller. Test display/internal-name ambiguity, nested arrays, duplicate options and invalid types. Preserve existing valid inputs; approve any common grammar before refactoring unrelated parsers.

### Current source and verification

Reviewed at private main `6aee9dfd92a4c06a73e89c091973f982d715b7a2`. These source findings do not certify runtime behavior. Source links open the current file; line numbers refer to the reviewed revision.

- [Plugins/TurboMCP/Source/TurboMCP/TurboMCPTypeParser.cpp](../Plugins/TurboMCP/Source/TurboMCP/TurboMCPTypeParser.cpp)
- [Plugins/TurboMCP/Source/TurboMCP/TurboMCPStruct.cpp](../Plugins/TurboMCP/Source/TurboMCP/TurboMCPStruct.cpp#L582)
- [Plugins/TurboMCP/Source/TurboMCP/TurboMCPVarsAdd.cpp](../Plugins/TurboMCP/Source/TurboMCP/TurboMCPVarsAdd.cpp)
- [Plugins/TurboMCP/Source/TurboMCP/TurboMCPDataTableRows.cpp](../Plugins/TurboMCP/Source/TurboMCP/TurboMCPDataTableRows.cpp#L409)

Starting checks for the assigned follow-up:

- `python Tools/execute-tests.py --profile engine --suites test-batch-tools,test-vars-add,test-struct,test-datatable-rows --execute`

These suites are starting checks, not proof of the new cases. Use owned synthetic fixtures and retain local evidence. Reference records do not authorize implementation.

See the [review register](../docs/audit/task-review.md) for definitions, duplicate ownership and observed test results.

<details>
<summary>Historical requirements and evidence (not execution instructions)</summary>

## Roadmap disposition

Workstream: results. Reconcile this finding with the current source under the ordered roadmap before implementation. Preserve its acceptance requirements and link duplicate findings to one repair task.

## Scope and acceptance

Preserve the substantive scope below. Before editing, reconcile the finding with current source and identify the focused acceptance check. Do not execute historical board/harness instructions. Record duplicates by task ID without discarding their requirements.

## Imported evidence

Historical paths and completion claims below are not current proof. This task must be made self-contained against the current source before implementation; no private archive is a prerequisite for public contributors.

> Source
>     `Feedbacks/TurboMCP_TaskList_2026-10-13.md:270-277`, group P3. Source IDs: X-10 and SF-03 (S4), DT-01 (S3), SN-02 (S4). This card came from the list. No one measured it on the tree.
>
> done when
>     1. Struct new, struct fields, variables add, function signatures and table rows all accept "Name: Type option=value". They all use the same parser.
>     2. The table tools accept display names first, then internal names. A refusal lists the display names.
>     3. "float" maps to a double-precision real, and the result says so. "real" and "double" are accepted.
>     4. Each help text has one example with options, including "force" for a removal.
>
> Why clause 2 is spelled out
>     A display name is what a person sees. An internal name is what the engine stores. This is the only place in the project where two names exist for one field. `C-P7` says that a refusal must lead to the next step, so a refusal has to name the spelling the caller typed.
>
> Clause 3 is already partly measured
>     `test-vars-add.py:30-32` uses `Speed: float default=250 category=Movement`, and the tool accepts it. `test-types` has a `kind` question on record. The first measurement is therefore a grep of every field parser, not a read of one.

## Verification

Pending. Record command, result, commit, and any blocked acceptance here.

</details>
