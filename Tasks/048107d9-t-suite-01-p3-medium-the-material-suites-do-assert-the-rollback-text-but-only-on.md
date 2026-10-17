---
id: "048107d9-a439-4e28-955a-562804215ac9"
title: "T-SUITE-01 (P3, medium) — The material suites DO assert the rollback text — but only on the paths the code verifies, which is why 28 unchecked sites are untested"
status: backlog
priority: "P3"
tags: ["P3", "medium", "pattern-C-P10", "suites", "cross-layer", "findings"]
workspace: "7d139a89-61a3-4566-b367-cf7a89b9969b"
created: "2026-10-12T10:50:59Z"
updated: "2026-10-16T19:57:52Z"
executions: 0
board_revision: 1803
exported: "2026-10-17T09:13:25Z"
---

# T-SUITE-01 (P3, medium) — The material suites DO assert the rollback text — but only on the paths the code verifies, which is why 28 unchecked sites are untested

| Field | Value |
|---|---|
| Card ID | `048107d9-a439-4e28-955a-562804215ac9` |
| Status | backlog |
| Priority | P3 |
| Tags | `P3` `medium` `pattern-C-P10` `suites` `cross-layer` `findings` |
| Workspace | `7d139a89-61a3-4566-b367-cf7a89b9969b` |
| Created (UTC) | 2026-10-12T10:50:59Z |
| Updated (UTC) | 2026-10-16T19:57:52Z |
| Executions recorded | 0 |

## Description

Summary
    A cross-layer result that reconciles two earlier ones. Ten suites were read, and the material suites do assert the exact rollback text from the C++ — but only on the paths the code itself verifies.

What the suites assert
    `test-material-tidy.py:53` asserts "FAILED: the layout changed the SET OF EXPRESSIONS, so it was ROLLED BACK." `test-material-label-promote.py:71` asserts "was ROLLED BACK." `test-material-blocks.py:72` asserts "FAILED: the edit LOST 1 expression(s) it did not list, so it was ROLLED BACK." All three also state that the fixture is restored on the way in and on the way out and that the restore is asserted (`:34`, `:43`, `:49`), and `test-material-label-promote.py:52` asserts the restore against what the suite did rather than against a count read from the asset — the verify-a-restore-against-the-set rule, already applied in the test layer.
    So the suites do test the rollback, and they assert the exact text the C++ prints.

Why the two earlier results coexist rather than contradict
    The suites assert the rollback on the paths the code verifies — "the set of expressions changed", "lost 1 expression it did not list" — and those paths compare something and report the comparison. The 28 unchecked cancels are on different paths: the ones that fire when `CreateMaterialExpressionEx` returns null, or when a usage node cannot be created. No suite makes an expression creation fail, so no suite exercises those paths.
    So the precise statement is narrower than "the rollback is untested": it is tested where the code measures, and untested where the code only assumes — and those are the twenty-eight sites.

The actionable form
    To close the class, a suite must make an expression creation fail and then assert that the transaction did not commit. `test-material-tidy.py` already has the machinery — `:374` mentions a run interrupted part-way that left the material mid-layout — so the fixture for the failure exists; what is missing is a case where the engine refuses the creation.

And it settles the question the suite rounds raised
    The test layer is not the problem. It tests what the code lets it test. The gap is that the code gives it nothing on twenty-eight paths, because on those paths the code does not check either.

## Execution prompt

```text
Testing policy: Before editing, run the fastest relevant focused/static check. Re-run it after the change. Avoid a complete suite by default; run one only when this change spans integrations or the focused test cannot establish confidence, and record why. For audit/documentation-only work, use direct validation only; do not run a complete Unreal suite.
```

## Execution history

_No execution record. The card state is a board declaration, not run evidence._

---

_Exported from the DSH task board, revision 1803, at 2026-10-17T09:13:25Z. The board is authoritative; this file is a snapshot._
