# Consider a common declaration grammar across tools

ID: FEATURE-FIELD-GRAMMAR
Status: backlog
Priority: P3
Readiness: needs_reconciliation
Owner: unassigned
Design: docs/architecture.md
Review: source-reviewed
Validation: unbuilt_untested

## Current review

2026-10-23: remaining feature scope separated from [abdd2c08](abdd2c08.md). No feature implementation is claimed or authorized by this record. The parser bug repairs are implemented and untested in the parent record.

## Remaining proposal

Decide whether struct creation/field updates, variable declarations and function signatures should share an options grammar. Row value edits are a distinct operation and must not be forced into a declaration grammar without a design decision. Preserve existing valid input and exact float precision. Adding function signature options is new capability.

Reconcile tool-specific help examples with the existing help records 9263bd95 and 0e4658ad instead of duplicating those tasks. Preserve the historical acceptance in abdd2c08, including its superseded float-widening and display-first clauses; neither is an instruction to change current semantics.

## Completion boundary

A future approved design must specify compatibility, supported options per caller and truthful help. No tests, build or editor runs were performed for this tracking split.

### Review-register reconciliation, 2026-10-25

Administrative reconciliation: this record is marked open or backlog; its existing implementation and verification evidence remain authoritative.

Preserve the recorded validation limits; do not infer runtime acceptance from review-register coverage.

This is a review of the tracking record, not a new semantic code review or test run.
