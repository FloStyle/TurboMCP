# Make engine scheduling exclusive across worktrees

ID: foundation-shared-lease
Status: todo
Priority: P1
Readiness: ready
Owner: unassigned
Design: docs/design/verification.md

## Problem
The foundation needs a verified implementation contract for this boundary.

## Scope
Audit Tools/ownership.ps1, build.ps1, start-editor.ps1 and run-suites.ps1. Existing leases are per checkout and acquisition is check-then-write. Design a shared atomic engine/port resource lock without killing foreign processes.

## Dependencies
None for inspection and regression design. Shared engine access requires the integration owner.

## Acceptance
Two simultaneous contenders from different worktrees cannot acquire one resource. A crashed owner has an explicit recoverable state. Nested same-owner operations work. Foreign editor and fixture access is refused.

## Verification
Add concurrent-process tests for contention, stale state and cleanup. Run python Tools/validate.py; then prove serialized build/editor behavior with two disposable worktrees.

## Handoff
Record results and remaining blockers before changing status.

## Roadmap disposition

Workstream: engine-ownership. Reconcile this finding with the current source under the ordered roadmap before implementation. Preserve its acceptance requirements and link duplicate findings to one repair task.
