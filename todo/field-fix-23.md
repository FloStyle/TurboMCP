# Concise and accurate save reporting

ID: FIELD-FIX-23
Status: done
Priority: P2
Readiness: done
Owner: Codex
Design: docs/design/mutations.md
Kind: implementation
Review: source-reviewed
Validation: unbuilt_untested

## Problem
SV-10: repeated fixed checkout policy explanations overwhelm save results. Source inspection also found widget save success messages could appear after a failed save when the package was clean.

## Scope
Compact the acting checkout header/footer while retaining per-asset provider outcomes/errors and preview detail. Gate successful widget-write explanations on both save success and clean state. No checkout or save behavior changed.

## Completion evidence
9c20aac. Implemented, unbuilt_untested.

## Acceptance
Deferred: success/failure/no-provider reports preserve actual per-asset outcomes; failed clean-package saves never report success; preview detail remains available.

## Verification
No tests/builds per user. Runtime correctness unverified.
