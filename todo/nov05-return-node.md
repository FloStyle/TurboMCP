# Add another function Return node through GraphAppend

ID: NOV05-13
Status: done
Priority: P1
Readiness: ready
Owner: codex-integrator
Design: docs/design/graphs.md
Review: source-reviewed
Validation: focused_native_passed; field_confirmation_pending

## Current review

Finding: The reader's `Function|Result` ID and GraphAppend's filtered action ID may differ; the synthetic preview graph also lacks the existing function signature. The field failure is repeated, but the exact requested ID is unavailable.

Action: Reproduce both discovery and preview in a synthetic function with an output, then expose an exact supported Return creation ID and preserve the real function signature through preview and apply. Refuse unsupported graph kinds without mutation.

## Problem

The November 5 field report repeatedly observed that GraphAppend refused a second Return node with `No supported creation ID`. An engine node-creation tool could add that node; GraphAppend could then link to it by node ID. A function with early exits needs multiple Return nodes. The reader identifies an existing Return node as `Function|Result`, while GraphAppend resolves creation IDs from the filtered stock action database. The original request text is unavailable, so the exact mismatch must be reproduced in a synthetic function graph before changing production behavior.

## Scope

Reconcile Return-node discovery and exact creation IDs in `TurboMCPNodeType.cpp`. Preserve the stock action filter and spawner. If a transient preview lacks the real function's output signature, mirror only the minimum entry/result context in transient objects; do not mutate the source graph during preview. Add a focused case to `TurboMCPGraphAppendAliasTest.cpp`. Update the GraphAppend/NodeType help in `TurboMCPAgentHelp.cpp` and `Public/Tools/TurboMCPToolsets.h` only as needed. No broad node-spawning or parser refactor.

## Dependencies

None. This assignment is disjoint from the two active DSH claims.

## Acceptance

- In a function graph with a result value and an existing Return node, discovery exposes one exact Return creation ID and GraphAppend preview accepts it with the same output pin shape as the real function.
- Preview leaves the source graph, links, defaults and dirty state unchanged. Apply creates one additional Return node with the stock spawner; a fresh read sees the node and its requested exec/data links. Existing Return nodes and output signature remain intact.
- Event, macro and construction graphs do not acquire Return-node creation capability. Unsupported or ambiguous IDs refuse before mutation with actionable English guidance.
- Preserve the existing Append refusal and rollback behavior; do not turn a failed preview into a partially applied edit.

## Verification

Run `python Tools/validate.py` for offline structure and the supervisor build plus the focused GraphAppend AliasCollision suite. Inspect the focused fixture's before/after node and link counts and preview dirty state. Record exact run ID, source SHA, exit codes and remaining Append-suite failures; a focused pass is not full Append acceptance.

## Handoff

Report the exact stock action ID, why the reader ID was refused, and any unsupported Return-node cases. The integration owner reviews the source and evidence, updates the field plan and dashboard, and pushes private main.

## Completion evidence

Integrated as e0484205096cfa0033da4acad2c0aa01e9ac919f. GraphAppend now exposes Function|Result as the exact supported creation ID for another function Return node. Preview mirrors the existing function output signature in a transient Blueprint-owned graph; apply uses the stock spawner. The focused AliasCollision case checks a second Return with requested exec and data links, and preserves the original Return. Unsupported graph kinds retain the existing refusal.

The supervised build passed in bfb41ede121f4533a226de3c1f45ae4f. The focused GraphAppend suite passed in 5fe0edc6782f47fb9f02347636093988; prior blocked and failed runs were retained. This is not full plugin acceptance or a field confirmation. Offline validate.py still reports the unrelated pre-existing DataAssetCreate lookup guard at TurboMCPDataAssetCreateTest.cpp:123 (78011b1c558d4fd3b5ecdb2877fdcbf7).
