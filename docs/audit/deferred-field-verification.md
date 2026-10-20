# Deferred field verification ledger (not a product task)

ID: FIELD-REG-01
Verification: deferred by owner
Priority: P0
Readiness: ready
Owner: Codex
Design: docs/design/mutations.md
Kind: evidence-ledger
Review: source-reviewed

## Problem
Urgent field fixes require implementation completion to be distinguished from runtime acceptance.

## Scope
Build the stock-engine plugin, then verify DataTable edits with editor open and closed; Widget compile/save preservation; graph append/patch/replace failure rollback and single undo; validated pin writes; layout and stable node identities.

## Acceptance
Implementation task closure means source changes committed only. Build and runtime acceptance remain open under FIELD-REG-01. The owner explicitly deferred all tests; this record does not authorize running them now.

## Completion evidence
None. Pending; no tests or build run for the latest source batches.

## Dependencies

FIELD-FIX-01 source implementation; owner authorization to resume testing.

## Verification

Deferred by owner. See FIELD-REG-01; implementation completion is not runtime acceptance.

## Stop conditions

Do not run builds or tests until owner resumes verification. Do not claim runtime acceptance.

## Deliverable

Committed implementation evidence and a separate pending regression record.

## Repoint regression pending

Verify failure after the first rewired reader restores all original links, failed target creation leaves no node, batch failure rolls back earlier operations, one successful batch needs one undo, compile/save are not run, and ambiguous graph names refuse. Source aad28b7 is unbuilt and untested.

## Call Swap regression pending

Verify rejected migration restores links/nodes without undoing previous work; final link and node count verification; no implicit compile; replacement retains position; overflow and ambiguous graph refusal. Source 019a1c05d6e046809a3e8b8ef170af3453164a0f is unbuilt and untested.

## Swap defaults regression pending

FIELD-FIX-04: string, numeric, object and localized-text preservation; incompatible/split pin refusal; callback readback; rollback after default transfer and link failure. No build/tests run.

## Pin-default rollback regression pending

FIELD-FIX-05: callback reconstruction, title-changing values, duplicate pin refusal, restoration on readback failure, failed undo reports partial, and prior user undo history remains untouched. Unbuilt and untested.

## Graph Copy follow-up

FIELD-FIX-06: internal and crossing links, refused destructive/conversion connections, failure rollback, node count, previous undo history, and no implicit compile. Unbuilt and untested.

## Variable-add field-note follow-up

FIELD-FIX-07: invalid later type must add zero variables; duplicate/case names; transaction/PIE refusal; dependent Widget survival and actual partial engine refusal. Unbuilt and untested.

## Field-note diagnostic follow-up

FIELD-FIX-08: live/dead exec chains sharing data, function results, macro tunnels and capped diagnostics. No build/tests run.

## Append reachability follow-up

FIELD-FIX-09: disconnected new impure chain with shared entry data refuses and restores; live chain succeeds; pre-existing dead nodes/pure nodes excluded; macro/tunnel roots; Replace composition. Unbuilt and untested.

## BL-12 follow-up

FIELD-FIX-10: local float/string/struct defaults, unset values, newline escaping and graph DSL comment handling. Unbuilt and untested.

## GL-15 follow-up

FIELD-FIX-11: two branch outputs into one exec input, reroutes, invalid endpoints, and multi-source data refusal; confirm partial evidence survives output. Unbuilt/untested.
