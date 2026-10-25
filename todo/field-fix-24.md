# Tool build identification and execution labels

ID: FIELD-FIX-24
Status: done
Priority: P2
Readiness: done
Owner: Codex
Design: docs/design/mutations.md
Kind: implementation
Review: source-reviewed
Validation: unbuilt_untested

## Scope
X-35: status exposes descriptor version and compilation date/time of the status translation unit, explicitly not Git revision proof. TH-09: index, paginated summary and single-tool help label PlayCheck Execute with PIE/project-code effects. Existing callable name and toolset preserved for compatibility.

## Completion evidence
c05d159. Implemented, unbuilt_untested. Build timestamp is compiler-local and changes only when this translation unit compiles; descriptor version is disk metadata. No source revision claim. TH-09 physical toolset migration remains open.

## Acceptance
Deferred: status fields reflect installed plugin and compiled status code; all three discovery paths label PlayCheck as execution, other tools retain kinds.

## Verification
No tests/builds per owner.
