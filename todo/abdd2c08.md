# Define field syntax without changing value semantics

ID: abdd2c08-6544-4730-b183-e4d856aad868
Status: done
Priority: P3
Readiness: done
Owner: unassigned
Design: docs/design/editor-domains.md
Review: source-reviewed
Validation: unbuilt_untested

## Current review

2026-10-23 additional name safety: field names reserve space for the engine counter/GUID and collision suffix before creation or field-edit planning; struct creation rejects invalid or oversized package paths before lookup/creation. Source-reviewed against stock engine naming/API source, unbuilt_untested.

2026-10-23 follow-up 265cced: oversized struct category options refuse in ParseFields before FName construction or mutation. Shared planning covers struct creation and field edits. Implemented / unbuilt_untested; no tests, builds or editor runs.

2026-10-23 scope reconciliation: the concrete parser defects are implemented through cf03659, unbuilt_untested. The remaining unified-grammar proposal is preserved separately in [field-grammar-feature](field-grammar-feature.md); it is a feature, not another open parser bug. Historical requirements below remain for traceability.

2026-10-23 follow-up 05aab8f/cf03659, unbuilt_untested: struct fields reject case-equivalent duplicate names and repeated options. Duplicate checking compares normalized strings without interning unbounded names. Function signature source already rejects duplicate names and invalid types; its supported grammar is Name: Type, so adding option syntax is feature scope. Remaining: reconcile shared grammar proposal/help scope separately from implemented parser defects. No tests/build/editor runs.

2026-10-23 follow-up 3d74f98, unbuilt_untested: variable-add refuses repeated options (including default/value aliases) and invalid booleans; accepted boolean forms are true/false, 1/0, yes/no, on/off, case-insensitive. Precision reconciliation: current TypeParser deliberately preserves float as PC_Float (FIELD-FIX-17 / 0a5e7d0); the old demand to silently widen float to double is superseded, not an open bug. Remaining: struct/signature option grammar inventory and separation of the common-grammar feature proposal. No tests/build/editor runs.

2026-10-23 follow-up bca2be8, unbuilt_untested: caller row syntax refuses unmatched parentheses and unterminated quotes before exposing parsed operations. Escaped quotes retain backslash-parity handling. Strict mode applies to caller input only; engine-export merge behavior is preserved. Remaining broader parser/precision work unchanged. No tests/build/editor runs.

2026-10-23 follow-up 1cc5714, unbuilt_untested: DataTable field resolution accepts a unique display label, preserves exact internal-name priority, and normalizes before duplicate detection, preview merging and apply. Unknown/ambiguous labels refuse with display/internal alternatives. Internal priority intentionally preserves existing valid calls rather than silently retargeting a field on a label collision. Remaining: nested grammar/precision reconciliation across other parsers and the historical shared-grammar proposal. No tests/build/editor runs.

2026-10-23 d3678da implements strict DataTable scratch-row import completion: captured Error/Fatal diagnostics refuse even with a non-null importer result, and trailing non-whitespace refuses. Validation occurs before any removal/write. Done implementation slice, unbuilt_untested. Remaining abdd2c08: field/display-name ambiguity, nested syntax and precision consistency across other parsers. No tests/build/editor runs.

**Decision:** implemented bug scope closed; broader feature split out. **Follow-up owner:** [R04](roadmap-r04.md), [R08](roadmap-r08.md).

A common type resolver already maps float/real/double to PC_Real, while field/options and row operations have different grammars. A shared type resolver is not a shared field parser.

### Next action and acceptance

Deferred verification: display/internal-name ambiguity, nested arrays, duplicate options and invalid types. These are evidence flags for the implemented repairs, not new tasks. Common grammar and help expansion remain in the linked feature record.

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
