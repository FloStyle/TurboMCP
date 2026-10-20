# Reconcile batch failures timeouts and possible duplicate writes

ID: FIELD-04
Status: in_progress
Priority: P1
Readiness: ready
Owner: Codex
Design: docs/design/mutations.md
Kind: investigation
Review: source-reviewed

## Problem
Reports describe inconsistent partial application, stale log errors and late timeout completion. Duplicate execution is suspected, not established. S2 X-64 and S3 X-66 are one incident.

## Scope
Bounded investigation of the affected subsystem. Report-qualified observations: X-40 X-48 X-51 X-52 X-57 X-64 X-65 X-66 X-36.
See [field intake](../docs/audit/field-testing-2026-10-20.json) for every imported observation and its owner.
No engine modifications, production reproduction, release claim or automatic worker dispatch is authorized by this record.

## Dependencies
Review and integrate the weekly prototype tests first. Exact repair scope and synthetic fixture are required before implementation.

## Acceptance
Correlate request identities and per-call logs with graph deltas; reproduce mixed-operation failure and timeout completion. Report applied, rolled-back or unknown outcomes and prevent blind retries. Treat modal stalls separately from crashes.
Distinguish field reports, source corroboration, reproduced behavior and verified fixes. Retain negative controls.

## Verification
`python Tools/validate.py`
Runtime verification must use the existing supervisor and disposable fixtures after the focused reproduction is specified.

## Current review
Decision: investigate

Reports describe inconsistent partial application, stale log errors and late timeout completion. Duplicate execution is suspected, not established. S2 X-64 and S3 X-66 are one incident.

Correlate request identities and per-call logs with graph deltas; reproduce mixed-operation failure and timeout completion. Report applied, rolled-back or unknown outcomes and prevent blind retries. Treat modal stalls separately from crashes.

## Handoff
Open; no fix or new runtime proof. Codex owns urgent follow-up. Auto-dispatch remains paused.

## Approved source-batch scope; tests deferred

Engine file Engine/Plugins/Experimental/Toolsets/EditorToolset/Content/Python/editor_toolset/toolsets/programmatic.py: finish transaction unwind before publishing asynchronous result, surface failed undo as unknown partial outcome, and always release runner resources when transaction cleanup raises. This does not claim duplicate execution is reproduced or fixed. Plugin TurboMCPCompile.cpp: refuse saves in an active transaction because disk writes cannot be undone by editor rollback. No tests during this pass.

## Source batch implemented; not validated

Native script completion now unwinds before publishing results, checks the undo return, reports uncertain cleanup outcomes, and releases runner resources through finally. TurboMCP saves refuse active transactions. Duplicate execution, timeout cancellation and native save entrypoints remain unverified. No build or tests run for this batch.

## Superseding delivery requirement: stock engine, plugin only

Owner explicitly rejects custom-engine deployment. Engine source and three Python dependencies restored to pinned stock revision, dependency SHA-1 verified. Previous engine commits retained as diagnostic evidence only. Source batch continues in TurboMCP with no test runs: in-place DataTable prevalidation, validated pin-default tool with dirty/undo handling, transaction/save safeguards and bounded batch outcomes. Registry filters disable only named unsafe native alternatives and direct users to plugin tools; general script batching has no equivalent and remains unavailable through the protected registry. Direct Python/editor code is outside this protection. Engine binaries must be rebuilt after stock source restoration; no engine patch is a deployment prerequisite. Exact assignment field-plugin-only owns the implementation files.

## Plugin-only batch evidence

Batch refuses PIE/nested transactions and reports rollback failure as partial. It checks transaction identity before undo so an empty failed batch cannot undo a previous user edit. Arbitrary native script batching is filtered; only graphRepoint is supported by the bounded replacement. Stock build passed; runtime regression deferred.

## Graph-writer source pass

b0753f2 checks rollback return values and returns failure on graph node-set mismatch. Replace preflights destructive targets before append. Replacement remains two-phase and can retain the append if removals fail; report labels this partial rather than unchanged.

No build or tests run, per owner instruction. Runtime acceptance remains pending.
