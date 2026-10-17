# Adapt the call recorder to Unreal 5.9 registry identity

ID: ue59-recorder
Status: backlog
Priority: P1
Readiness: needs_reconciliation
Owner: unassigned
Design: docs/design/protocol.md

## Scope and acceptance

FToolset::GetToolsetClass is removed in the pinned 5.9 development tree. Replace the override and identity calls in TurboMCPCallRecorder.cpp through a version adapter. Acceptance: both supported baseline compilation and a separate 5.9 build, plus recorder registration/unregistration behavior. Do not upgrade engine.lock.json baseline.

## Verification

Pending.
