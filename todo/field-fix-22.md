# Toolset help continuation

ID: FIELD-FIX-22
Status: done
Priority: P2
Readiness: done
Owner: Codex
Design: docs/design/mutations.md
Kind: implementation
Review: source-reviewed
Validation: unbuilt_untested

## Problem
TH-10: toolset help stops after twelve tools and requires a global index to discover the remainder.

## Scope
Continue a toolset summary using ToolName=list:<offset> with the same ToolsetName. Names sorted for stable pages; each page reserves continuation space within 2000 characters and abbreviates descriptions. Full single-tool help remains available. Native engine describe_toolset and oversized single-tool help are outside this fix.

## Completion evidence
a640bd1 plus deterministic ordering follow-up. Implemented; unbuilt_untested.

## Acceptance
Deferred: traverse a toolset beyond twelve tools without skips/duplicates on an unchanged registry; invalid offsets refuse; single-tool lookup remains available. Registry changes between calls require restarting enumeration.

## Verification
No tests/builds per user. This does not close TH-12 native toolset description size.
