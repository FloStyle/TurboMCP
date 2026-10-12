# TurboMCP — status

**Version 1.0.0.** A snapshot of the state at the revision it was written. Every number is read from the
tree, and each one names the command that produces it. **For the analysis, read [`REPORT.md`](REPORT.md);
for the evidence, [`TestRecord.md`](TestRecord.md); to hand the project on, [`HANDOVER.md`](HANDOVER.md).**

---

## 1. The headline

| | How to check it |
|---|---|
| **Version** | **1.0.0** — `TurboMCP.uplugin`, `VersionName`, no longer marked experimental |
| **Tools** | **93** `AICallable` in 4 toolsets — Read 30 · Look 4 · Preview 26 · Edit 33. `Select-String -Path Source\TurboMCP\Public\Tools\TurboMCPToolsets.h -Pattern 'static FString'` |
| **Coverage** | **`uncalled: (none)`** — every declared tool is reached by at least one suite. `python Tools\audit-tool-coverage.py` |
| **Suites** | **52 files** under `Tools/test-*.py`, **58 suites** in the harness |
| **FULL RUN** | ***`passed 58, failed 0, blocked 0, of 58` — exit code 0.*** `pwsh -NoProfile -File Tools\run-suites.ps1` |
| **Suite-quality audit** | **BLOCKS the run** when a check cannot fail **or cannot pass** — 0 candidates. `python Tools\audit-suite-quality.py` |
| **Licence** | **MPL-2.0**, the notice on **193 of 193** source files, `LICENSE` verbatim |
| **Secrets** | **none** — keys, tokens, SSH keys, connection strings, private URLs and paths of anyone's machine were all checked, and one finding was removed from the file **and** the history |
| **Plan** | **31 `todo` · 15 `done`** of 48 rows, counted **by field** |
| **Size** | C++ **61 620 lines** in **186** files · Spec **7 740 lines** in **17** · `TestRecord.md` **10 800 lines** |

## 2. What is done

Every reader in `Read` and `Look` is **structurally unable to write**, because those toolsets hold no
editing code. Every `Edit` has a `Preview` twin that shares the same `Plan`, so what a plan reports and
what an edit does cannot drift apart.

| § | Area | State |
|---|---|---|
| 2 | Agent help (Read and Preview) | **complete** |
| 3 | Blueprint and widget reading | **complete** |
| 4 | Materials reading | **complete** |
| 5 | Look: captures, layout, widget previews | **complete** |
| 7 | Blueprint structure (Preview and Edit) | **complete** |
| 8 | Graph writing | **complete** |
| 9 | Materials writing | `P/E-MAT-BLOCKS` **done** (24 checks) |
| 15 | Graph linting | `R-BP-LINT` **done** (37 checks) |
| — | Data tables | `P/E-DATATABLE-ROWS` **done** (26 checks) |
| — | Call cost | `R-CALL-COST` **done** (29 checks) |
| — | Asset cost | `R-ASSET-COST` **done** (10 checks) |
| — | Material instance loading | `R-MAT-INST-LOAD` **done** (20 checks) |
| — | Enum editing | `P/E-ENUM-EDIT` **done** |

**`Spec/WORKING_PLAN.md` is the authoritative work list.** This page is a snapshot; that one is audited row
by row against the tree, and it is the only list that is.

## 3. What is left

**31 of 48 rows**, and three of them cannot be done on the machine this was built on:

- the remaining **source-control writers** need a server that answers;
- **`TA-18`** needs a `USkeleton` bone whose every route is closed in the engine.

The other 28 are capability after capability — further material, widget, graph, curve, asset and actor
tools — and the pattern for each already exists in this repository. **The order I would take them in is in
[`HANDOVER.md`](HANDOVER.md) section 3**, and the reasoning is in [`REPORT.md`](REPORT.md) section 9.

## 4. Honest limits — built, but not fully proven

| | |
|---|---|
| **No checkout against a live depot has ever run** | every answer the source-control tools give is the answer to a **refusal** or a **no-op** |
| **The `UsesCheckout == true` branch has never executed** | Perforce reports `IsAvailable == false`, so the mechanism refuses before reaching it |
| **No `-nullrhi` run** | the `screen` refusal is proven by construction from two engine guards, not executed |
| **The positive path of `selection`** | proven by a self-cleaning internal probe, because `P3` forbids a Look from creating a selection |
| **`TA-19`** | the `Niagara` module is linked, but nothing was built: the emitter still has to be authored |

## 5. The two defects that made green impossible

Both are fixed, and both are in [`REPORT.md`](REPORT.md) section 12.2 with the reasoning:

- **the full-run conflict** — `test-vcs` required a clean fixture while a suite that runs *before* it
  rebuilt that fixture. **A suite needs the editor alive for its checks, so it cannot restore the file
  itself**: `git checkout` answers `unable to unlink old ...: Invalid argument` while the editor holds it.
  Fixed in the harness.
- **a count pinned on a shared asset** — a suite asserted a literal `16` while the fixture held `12`, so it
  **passed or failed according to which fixture it was looking at**, and reported as a defect in the tool
  what was a property of the fixture. Fixed by comparing `before` against `after`.

**And the deeper finding from the harness investigation, kept because it is a real trap:**
`Tools/tmcp.py` gained a re-connect retry so a suite whose editor was restarted underneath it would
recover — correct in itself — **but `connect` calls `rpc` and `rpc` calls `connect` on failure, so when the
server did not answer the retry recursed, each level waiting out a full request timeout.** The full
investigation, including the five hypotheses that were falsified by measurement, is in
[`TestRecord.md`](TestRecord.md).
