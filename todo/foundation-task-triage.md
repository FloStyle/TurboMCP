# Reconcile imported P0/P1 tasks against current code

ID: foundation-task-triage
Status: todo
Priority: P1
Readiness: ready
Owner: unassigned

## Problem
The foundation needs a verified implementation contract for this boundary.

## Scope
Inspect imported P0/P1 findings, docs/architecture.md, Tests/acceptance.json and associated source/tests. Preserve every imported ID. Group duplicates under a primary task; explain resolved or withdrawn findings. Do not implement unrelated product changes.

## Dependencies
None for inspection and regression design. Shared engine access requires the integration owner.

## Acceptance
Each reviewed task has a current reproduction or explicit source finding, exact scope, dependencies, acceptance and commands. Only confirmed bounded tasks become ready; withdrawn findings cite evidence.

## Verification
Run python Tools/update-todo.py and python Tools/validate.py. Review source and focused tests for each selected task; report runtime checks that remain blocked.

## Handoff
Record results and remaining blockers before changing status.
