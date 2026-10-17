# Mutation lifecycle

[Documentation](../README.md)

## Ownership

A public wrapper resolves inputs and formats the result. A domain planner computes an explicit change set. A mutation coordinator owns transaction boundaries and lifecycle. Domain primitives must not create hidden nested transactions, compile repeatedly or save implicitly. Existing integration points include [TurboMCPMutation.cpp](../../Plugins/TurboMCP/Source/TurboMCP/TurboMCPMutation.cpp), TurboMCPRollback.cpp and TurboMCPCompile.cpp; their presence does not prove every caller uses the contract.

## Required sequence

1. Resolve the asset, graph and object identities; refuse missing or ambiguous mutation targets.
2. Validate all inputs and supported operation kinds without editing. Capture the relevant baseline and preconditions.
3. Plan the exact changes and effects. A rejected operation cannot appear as an accepted preview change.
4. Revalidate the plan against current state before applying. Refuse stale state.
5. Apply under one owned transaction on the game thread.
6. Independently read the changed state and compare it with the plan. Compile when required, using the baseline and diagnostic rules in ARC04.
7. On failure, restore the captured state and verify restoration. Distinguish restored, rollback failed and unknown outcomes.
8. Report actual effects and dirty state. Save only when the public operation explicitly requests it.

Memory changes, compilation, verification, saving and rollback are distinct states. Successful API return values and requested counts are not independent read-back. Undo cannot reverse already persisted disk writes. A partial save lists saved, failed and unattempted packages; do not promise filesystem atomicity.

## Scope and extension

The existing release contract permits only the restricted batch domain in ARC09. A general multi-asset batch proposal cannot override it. A new operation must define the baseline it captures, the stale-state fingerprint, expected effects, rollback mechanism and independent comparison before implementation.

## Acceptance

Test valid edits, no-ops, invalid inputs, ambiguous targets, pre-existing compile diagnostics, new diagnostics, mid-apply failure, failed restoration, save failure and client disconnect. Verify identities, links/properties, dirty state, undo and persisted bytes separately. Never narrow diagnostic capture or fixture assertions merely to obtain a pass.
