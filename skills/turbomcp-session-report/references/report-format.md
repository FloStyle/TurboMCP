# Session report format

Use the fields below. Replace bracketed placeholders; write Unknown or Not observed when evidence is absent. No empty headings. For a section with no items, write None observed. Unknown is not zero. No code blocks, paths or raw output are permitted in a report.

# TurboMCP [generic area] session report

**Report ID:** YYYY-MM-DD_S1
**Date:** YYYY-MM-DD
**Review state:** Pending human review
**Plugin version:** [public release or commit; otherwise Unknown]
**Engine version:** [version and source or installed distribution; public revision if known]
**Client:** [product and version; no account or machine identity]
**Test case:** [generic asset kinds and necessary counts]
**Context:** [play state; source control in use, unavailable or unknown]
**Tools used:** [exact public TurboMCP tool names actually used; list other tools by generic role]
**Outcome:** [task completion; compile; save; crash; each reported separately]
**Session limits:** [context loss, timeout, permissions, missing observations or None observed]
**Correction of:** [earlier report ID or None]

## 1. Cross-cutting issues

## 2. Tool observations

Use a subgroup for each relevant tool family. Each item has this form:

**GL-07: [generic title] (high)**
- **Type:** Defect, limitation, usability, proposal or positive.
- **Scenario:** Generic asset structure and starting state.
- **Reproduction status:** Observed once, repeated, or not reproduced. A proposed clean-room recipe is not a tested reproduction.
- **Steps to reproduce:**
  1. Describe the starting state and operation in prose.
  2. Describe the next necessary action without code or project identifiers.
- **Observed:** What the session demonstrated, including occurrence count if known.
- **Expected:** Desired behavior and its basis: documented contract or user expectation.
- **Impact:** Lost work, blocked workflow, manual steps, or Unknown. Do not estimate without evidence.
- **State after:** Memory changes, compile, save and recovery status separately; Unknown where unobserved.
- **Workaround:** Generic action and whether it actually worked, or None observed.
- **Cause:** The cause is not confirmed.
- **Proposed behavior:** Design suggestion only, or None.

Positive observations may use a shorter form with Scenario and Observed. They establish only what happened in that session. A restored item must name its earlier report ID and item ID, never a source file path. Do not restore content from an unsanitized source.

Severity: high means observed crash, data loss or false success; medium means wrong or misleading results or a missing common step; low means wording or minor friction; positive means observed useful behavior. A suspicion is not evidence of data loss. Severity and suggested implementation priority are different.

## 3. Recurrence

List new item ID, earlier item ID, matching observation and any changed conditions. Similar symptoms do not establish a common cause. Use report IDs, not file paths or links.

## 4. Features that would have helped

| ID | Proposed feature | Observed work it would replace |
| --- | --- | --- |

Only include proposals grounded in this session. Count calls, manual steps or graphs only if known. Allocate IDs with the same glossary rules. Do not describe proprietary algorithms.

## 5. Suggested priority

Rank crashes, data loss and false success first. Then consider blocked workflows and common missing writes, batch or transaction needs, reads and checks, and documentation. Explain the observed impact briefly. Maintainers may change this order after investigating cost and native Unreal overlap.

## 6. Completeness and privacy review

State whether all observed tools and item IDs are represented, which session evidence is missing, whether local lint passed, and whether the report was read back. Keep Review state as Pending human review. Do not claim that automated checking proves confidentiality.
