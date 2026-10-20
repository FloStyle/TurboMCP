# Verify pin-only changes persist and report save state accurately

ID: FIELD-03
Status: in_progress
Priority: P1
Readiness: ready
Owner: Codex
Design: docs/design/mutations.md
Kind: investigation
Review: source-reviewed

## Problem
Two pin-only edits reportedly changed memory without dirtying the package. Compile status uses the dirty flag and save skips clean packages; this is not disk equality proof.

## Scope
Bounded investigation of the affected subsystem. Report-qualified observations: ED-04.
See [field intake](../docs/audit/field-testing-2026-10-20.json) for every imported observation and its owner.
No engine modifications, production reproduction, release claim or automatic worker dispatch is authorized by this record.

## Dependencies
Review and integrate the weekly prototype tests first. Exact repair scope and synthetic fixture are required before implementation.

## Acceptance
Trace the native setter and reproduce pin-only mutation. Verify dirty state, save outcome and reload read-back. Distinguish no dirty flag from verified persisted content.
Distinguish field reports, source corroboration, reproduced behavior and verified fixes. Retain negative controls.

## Verification
`python Tools/validate.py`
Runtime verification must use the existing supervisor and disposable fixtures after the focused reproduction is specified.

## Current review
Decision: investigate

Two pin-only edits reportedly changed memory without dirtying the package. Compile status uses the dirty flag and save skips clean packages; this is not disk equality proof.

Trace the native setter and reproduce pin-only mutation. Verify dirty state, save outcome and reload read-back. Distinguish no dirty flag from verified persisted content.

## Handoff
Open; no fix or new runtime proof. Codex owns urgent follow-up. Auto-dispatch remains paused.

## Approved source batch; regression deferred by owner

Plugin scope: TurboMCPCompile.cpp. Compile must describe dirty state without claiming disk equality. An explicit save must not skip a clean package: native mutations may omit its dirty flag. Require successful save return and clean state; no false saved result on a failed write.

Separate engine scope: Engine/Source/Editor/BlueprintEditorLibrary/Private/BlueprintEditorLibrary/BlueprintGraphPin.cpp. Validate the proposed default before mutation, reject unsupported output/exec/read-only/connected pins, capture node transaction state, mark successful edits modified, and resolve stale pin indices only by unique exact name. Do not reset a previous valid value on refusal.

FIELD-02 native containment scope: ToolsetRegistry/Public/ToolsetRegistry/ToolsetLibrary.h and Private/ToolsetRegistry/ToolsetLibrary.cpp under Engine/Plugins/Experimental/ToolsetRegistry/Source; expose active editor transaction query. Its Content/Python/toolset_registry/helpers.py must refuse compile inside a transaction before compilation. Engine changes remain outside plugin export. No tests or build during this source batch; full acceptance remains open.

## Source batch implemented; not validated

Native pin setter now validates the proposed value before mutation, refuses non-editable pins, captures node undo state and marks the Blueprint modified. Pin validity now resolves the actual pin; stale-index fallback requires a unique exact name. Plugin compile reports dirty state without claiming disk equality; explicit save no longer skips clean packages and requires both successful API return and clean state. No build or tests have run for these changes.

## Superseding delivery requirement: stock engine, plugin only

Owner explicitly rejects custom-engine deployment. Engine source and three Python dependencies restored to pinned stock revision, dependency SHA-1 verified. Previous engine commits retained as diagnostic evidence only. Source batch continues in TurboMCP with no test runs: in-place DataTable prevalidation, validated pin-default tool with dirty/undo handling, transaction/save safeguards and bounded batch outcomes. Registry filters disable only named unsafe native alternatives and direct users to plugin tools; general script batching has no equivalent and remains unavailable through the protected registry. Direct Python/editor code is outside this protection. Engine binaries must be rebuilt after stock source restoration; no engine patch is a deployment prerequisite. Exact assignment field-plugin-only owns the implementation files.

## Plugin-only batch evidence

Plugin-owned EditTurboMCPPinDefault validates exact input pin identity, records undo, marks dirty and reacquires the pin for read-back. Native set_pin_value is filtered. No automatic compile/save. Stock build passed; runtime regression deferred.
