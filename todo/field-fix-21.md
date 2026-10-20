# Blueprint summary completeness and graph labels

ID: FIELD-FIX-21
Status: done
Priority: P2
Readiness: ready
Owner: Codex
Design: docs/design/mutations.md
Kind: implementation
Review: source-reviewed
Validation: unbuilt_untested

## Problem
BL-13 reports a possibly missing empty function; BL-14 reports confusing events and graphs.

## Scope
List each function graph independently of entry nodes. Label unavailable reflected signatures instead of implying an empty signature. Label manifest graph kinds and include owning graph on event signature rows.

## Completion evidence
Source aa15bdb. Implemented, unbuilt and untested. BL-13 original cause remains uncertain; no runtime closure claim. BL-14 presentation adjustment is implemented, not a claim that prior graph collection contained event nodes.

## Acceptance
Deferred: empty and entry-less function graphs remain listed once; normal signatures preserved; manifest kinds and event owner names match editor structures.

## Verification
No tests or builds per owner. Evidence state remains unbuilt_untested.
