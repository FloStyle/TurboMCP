# Deferred regression for urgent plugin fixes

ID: FIELD-REG-01
Status: todo
Priority: P0
Readiness: ready
Owner: Codex
Design: docs/design/mutations.md
Kind: verification
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
