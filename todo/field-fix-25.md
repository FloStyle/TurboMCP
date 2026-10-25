# Object and interface signature type support

ID: FIELD-FIX-25
Status: done
Priority: P2
Readiness: ready
Owner: Codex
Design: docs/design/mutations.md
Kind: implementation
Review: source-reviewed
Validation: unbuilt_untested

## Problem
FA-07: bare widget class paths were rejected as neither structs nor enums; explicit interface syntax was absent.

## Scope
Bare loadable Blueprint-allowable UClass paths resolve as object references (interface classes as interface pins). Add interface<class-path>; reject non-interface classes in that wrapper and request interface syntax when object/class wrappers name interfaces. Existing object/class/soft wrappers retained.

## Completion evidence
ee12e2d. Implemented, unbuilt_untested. Generated Blueprint class paths must identify the class object (usually _C), not the Blueprint asset. Existing engine eligibility checks preserved.

## Acceptance
Deferred: widget object parameter, interface parameter, invalid interface class refusal, existing class/soft references and container uses. No engine modifications.

## Verification
No tests/builds per user.

## Current review

### Review-register reconciliation, 2026-10-25

Administrative reconciliation: this record is marked done; its existing implementation and verification evidence remain authoritative.

Preserve the recorded validation limits; do not infer runtime acceptance from review-register coverage.

This is a review of the tracking record, not a new semantic code review or test run.
