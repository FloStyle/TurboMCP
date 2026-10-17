---
id: "71e2e86a-9854-4a25-9d17-18f18f5679e5"
title: "T-SUITE-02 (P3, low) — The FAILS ON convention measures a vocabulary, not a quality — two full suites tested, and one of the 34 silent ones is exemplary"
status: backlog
priority: "P3"
tags: ["P3", "low", "tools", "layer", "X1", "suites"]
workspace: "7d139a89-61a3-4566-b367-cf7a89b9969b"
created: "2026-10-12T10:28:07Z"
updated: "2026-10-16T19:57:32Z"
executions: 0
board_revision: 1803
exported: "2026-10-17T09:13:25Z"
---

# T-SUITE-02 (P3, low) — The FAILS ON convention measures a vocabulary, not a quality — two full suites tested, and one of the 34 silent ones is exemplary

| Field | Value |
|---|---|
| Card ID | `71e2e86a-9854-4a25-9d17-18f18f5679e5` |
| Status | backlog |
| Priority | P3 |
| Tags | `P3` `low` `tools` `layer` `X1` `suites` |
| Workspace | `7d139a89-61a3-4566-b367-cf7a89b9969b` |
| Created (UTC) | 2026-10-12T10:28:07Z |
| Updated (UTC) | 2026-10-16T19:57:32Z |
| Executions recorded | 0 |

## Description

Summary
    This card's finding was tested against two full suites, and the test changed what it claims.

What was measured
    `Tools/checks/check-fails-on.py` reads 52 suites and 1546 `check()` calls: 18 name a failing input by the broad pattern, 9 by the narrow one, and 34 do not.
    `test-uncovered-tools.py` (176 lines) is one of the nine, and it also records the two wrong versions of its own checks — its best line is the best statement of the comment-versus-code problem in this audit: "the output is asserted, not the doc comment … a check written from a doc comment tests the comment."
    `test-asset-cost.py` (175 lines) is one of the 34 and it is complete. It has no `FAILS ON` line, and 14 active checks that each name what they prove: "it names soft references as their own kind", "the split states how many dependency entries were examined", "an unknown size comes with a reason rather than alone", "it says explicitly that this is not zero", "the refusal does not claim a zero size, which would read as free".

The conclusion, and why the first version of this card was wrong
    The convention measures a vocabulary, not a quality. A suite can satisfy every rule the template states and never write `FAILS ON`; a suite can write it and assert something its own author later recorded as wrong. That is exactly why the first version of this card was wrong to imply that 43 suites were defective, and why the tool now carries its own limit in its output.

The decision, with both costs measured
    Drop it: the phrase measures a comment, not a check, and `test-asset-cost.py` is the evidence — fourteen checks that state their own failure conditions without using the phrase. Removing the rule from `make-tools-doc.py` costs nothing that anything else depends on.
    Or rewrite it: "name an input that makes the check fail" is a good requirement, and `FAILS ON` is only one way of meeting it. As it stands the template asks for a sentence shape, and a sentence shape is not a property of a test.
    Recommendation: drop it, or rewrite the rule to ask for what it means. A rule that a good suite violates is a rule that will be ignored.

Keep the tool
    The measurement is still useful: 34 suites do not say what makes them fail, and a reader who wants to spot-check them now has the list instead of the whole directory.

## Execution prompt

```text
Testing policy: Before editing, run the fastest relevant focused/static check. Re-run it after the change. Avoid a complete suite by default; run one only when this change spans integrations or the focused test cannot establish confidence, and record why. For audit/documentation-only work, use direct validation only; do not run a complete Unreal suite.
```

## Execution history

_No execution record. The card state is a board declaration, not run evidence._

---

_Exported from the DSH task board, revision 1803, at 2026-10-17T09:13:25Z. The board is authoritative; this file is a snapshot._
