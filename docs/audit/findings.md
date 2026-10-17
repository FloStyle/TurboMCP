# Audit status and confirmed planning findings

[Documentation](../README.md)

Inspected baseline: private main 4984478, plugin implementation unchanged from the earlier baseline except Build.cs comment cleanup. Inspection date: 2026-10-17. This report separates complete inventory coverage from incomplete semantic/runtime audit coverage.

## Coverage

- All 101 AICallable declarations have exactly one located qualified C++ definition: 30 Read, 4 Look, 29 Preview and 38 Edit toolset declarations. Toolset membership alone does not establish side effects.
- Static test inspection finds 95 parsed calls and six name-only references. The six are Preview/Edit MaterialTidy, MaterialLabelPromote and MaterialBlocks. Those suites may call through helpers; verify the actual call path before classifying them uncovered.
- Every one of the 92 pre-roadmap tasks has a planning disposition. Positive historical audits remain evidence; superseded process tasks cannot override the new queue. This does not claim all historical defects were reproduced.
- All 16 ARC contracts have an explicit design decision review. This is not a complete review of every implementation choice inside every function.
- The engine development head was rechecked and still equals the pinned 5.9 snapshot. Native interface presence is confirmed; runtime parity is not.

## Findings that affect the sequence

| Finding | Evidence | Consequence |
| --- | --- | --- |
| Quality audit produces false NO-ASSERT findings | Running audit-suite-quality.py reports test-build-provenance.py and test-repo-boundaries.py; both have executable Python assert statements | R01 fixes recognition with negative controls; do not delete or exempt the tests |
| Coverage wording overstates its proof | audit-tool-coverage.py says every tool is called while reporting six name-only material references | Distinguish parsed calls/references/runtime observations |
| Batch claims unchanged after rollback failure | TurboMCPBatchEditTools.cpp formats unchanged with rollback=failed; all plans are prepared before apply and final compile is deferred | Repair restricted batch only; no broad multi-asset expansion |
| Baseline batch test is not release acceptance | test-batch-edit.py explicitly preserves current incomplete behavior and does not assert unknown-field refusal | Replace defective baseline assumptions with B01-B05 requirements when repairing |
| Cross-worktree resource ownership is insufficient | ownership.ps1 checks a per-checkout lease then writes it without atomic shared acquisition | Serialize engine work; R02 must prove contention behavior |
| Release matrix is not executable yet | All 97 required acceptance records remain not_mapped in Tests/acceptance.json | No v1 certification or release permission |
| Active comments can contradict current code | TurboMCPToolsets.cpp introductory comment says class U is retained; GetToolsetName removes it and nearby text documents the opposite | Keep public examples tied to observed schema; clean misleading comments within affected tasks |

## Still unverified

Historical fixture drift/first-run failures, every tool's full semantic behavior, current runtime schema/effect coverage, installed-engine packaging and 5.9 runtime compatibility still require their assigned audit/acceptance work. No existing passing smoke or static guard closes those gaps.

The prior structural pass passed 13 offline checks, 12 plugin checks and a full-tree source-copy validation. Those results prove infrastructure properties, not all product behavior. An audit must be allowed to find failing features; do not alter a check to protect a green summary.
