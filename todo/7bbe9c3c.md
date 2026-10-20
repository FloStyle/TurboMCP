# Avoid false unnecessary-cast warnings

ID: 7bbe9c3c-19c2-4b62-aad2-9cd098d0fedf
Status: done
Priority: P1
Readiness: ready
Owner: Codex
Design: docs/design/protocol.md
Review: source-reviewed
Validation: unbuilt_untested

## Implementation update — 2026-10-21

Implemented in `22e4545`. Cast lint uses UK2Node_DynamicCast and its real result accessor, counts every connected output including execution/boolean outputs, and skips incomplete nodes. Removed the unsupported enclosing-Blueprint class inference. Only unused-output observations are reported; redundant connected casts are not inferred without reliable input/nullability evidence.

Done means implemented, not verified. No tests or builds per owner; original acceptance cases remain deferred.

## Current review (historical diagnosis)

**Decision:** confirmed. **Follow-up owner:** [R06](roadmap-r06.md).

RunUnnecessaryCasts looks for literal AsObject and treats a missing pin as zero consumers. The pinned UK2Node_DynamicCast API exposes GetCastResultPin and GetCastSourcePin. Target-class equality with the enclosing Blueprint alone does not prove the input object type.

### Next action and acceptance

Use the real cast node API and conservative input-type evidence. Test a typed result feeding a variable set, a used success/failure exec branch with unused object output, and a truly redundant cast. Unknown evidence produces no deletion recommendation; inspect only, never delete.

### Current source and verification

Reviewed at private main `6aee9dfd92a4c06a73e89c091973f982d715b7a2`. These source findings do not certify runtime behavior. Source links open the current file; line numbers refer to the reviewed revision.

- [Plugins/TurboMCP/Source/TurboMCP/TurboMCPBPLint.cpp](../Plugins/TurboMCP/Source/TurboMCP/TurboMCPBPLint.cpp#L250)

Starting checks for the assigned follow-up:

- `python Tools/execute-tests.py --profile engine --suites test-bp-lint --execute`

These suites are starting checks, not proof of the new cases. Use owned synthetic fixtures and retain local evidence. Reference records do not authorize implementation.

See the [review register](../docs/audit/task-review.md) for definitions, duplicate ownership and observed test results.

The detailed repair boundary is [cast lint](../docs/audit/repair-contracts.md#cast-lint).

<details>
<summary>Historical requirements and evidence (not execution instructions)</summary>

## Roadmap disposition

Workstream: graph-fidelity. Reconcile this finding with the current source under the ordered roadmap before implementation. Preserve its acceptance requirements and link duplicate findings to one repair task.

## Scope and acceptance

Preserve the substantive scope below. Before editing, reconcile the finding with current source and identify the focused acceptance check. Do not execute historical board/harness instructions. Record duplicates by task ID without discarding their requirements.

## Imported evidence

Historical paths and completion claims below are not current proof. This task must be made self-contained against the current source before implementation; no private archive is a prerequisite for public contributors.

> Source: Feedbacks/2026-10-16 S3. The unnecessary-cast check marked casts whose typed output is linked to a variable set, creating a dangerous false deletion recommendation.

## Verification

Pending. Record command, result, commit, and any blocked acceptance here.

</details>
