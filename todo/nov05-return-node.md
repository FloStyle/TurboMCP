# Add another function Return node through GraphAppend

ID: NOV05-13
Status: todo
Priority: P1
Readiness: ready
Owner: unassigned
Design: docs/design/graphs.md
Review: source-reviewed

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
