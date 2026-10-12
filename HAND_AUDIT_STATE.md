# HAND-AUDIT STATE

**A checkpoint for whoever continues the hand audit.** It records what was read, what was found, what was
checked and discarded, and the two things the session established that are not in any card: the method and the
reliability of the findings themselves.

---

## 1. THE SET

| Group | Files | Read by hand |
|---|---|---|
| `Source/` | 186 | **~30** |
| `Tools/` | 88 | **2 layer passes** (not file by file) |
| `.md` + descriptors | 31 | done earlier by the document audit |

**Not done:** ~19 `Source/` files, and most of the 88 `Tools/` scripts.

---

## 2. THE TWO SHAPES, AND WHY A DEFECT COUNT IS NOT A QUALITY MEASURE

Read by hand, the source splits cleanly:

| Shape | What it carries | Examples |
|---|---|---|
| **Writes** | `C-P10` (an unchecked rollback), `C-P1` (a counter computed and never printed) | the three material writers, the source-control writers |
| **Reads only** | `C3` (a comment that contradicts the code), `C-P2` (a refusal that names the wrong cause) | `Types`, `NodeId`, `NodeResolve`, `Traps` |

**No file has shown both.** And `WidgetTreeText.cpp` — 688 lines, a pure text transform — **cannot contain the
write family at all**, because it writes nothing. So a file with zero findings is not necessarily better written
than one with three; **the shape of the work removes the opportunities.**

---

## 3. THE DEFECTS THAT ARE ALREADY BOUNDED

**`C-P10` is CLOSED in `Source/`.** `git grep 'Transaction\.Cancel()'` over the 39 remaining files returns
**nothing**. All **28** unchecked cancels are in three files, and all three are audited:

| File | Sites |
|---|---|
| `TurboMCPMaterialBlocks.cpp` | 19 |
| `TurboMCPMaterialLabelsPromote.cpp` | `:461`, `:537`, `:610`, `:653`, `:662`, `:671` |
| `TurboMCPMaterialTidy.cpp` | `:612`, `:621`, `:629` |

`TurboMCPGraphKnots.cpp:1400-1405` re-measures a rollback and reports *"THE ROLLBACK DID NOT RESTORE THE GRAPH"*.
**The repair pattern exists in the project and is not applied in those three files.**

---

## 4. FOUR PATTERNS THAT CROSS FILES

Each was found once and then measured across the layer.

1. **A correct idea that is not wired to the place that needs it.** Four instances at four scales:
   `TurboMCPTypes.cpp` declares a `kind` no caller can ask for · `TurboMCPNodeId.cpp` ships `NormaliseKey` and
   2 of 4 callers skip it · `TurboMCPPanelToolRunner.h` documents a `mutable` the code replaced with a
   `const_cast` · the `Tools/` template requires a phrase 9 of 52 suites use.

2. **The once-only error prefix.** `TurboMCPMaterialTools.cpp:31-38` has the guard as a named helper
   (`AsError`). `TurboMCPWidgetTreeTools.cpp:32-34` states the rule and the failure it prevents
   (`error: error: ...`). **4 of 29 wrappers call it.** Twelve implementations do **not** prefix their
   `OutError`, so the `Plan` path is safe; **seven DO prefix their `Apply` return**, so the wrapper sites that
   wrap an `ApplyError` — `GraphAppendTools:65`, `GraphPatchTools:67`, `GraphReplaceTools:69`,
   `GraphStructureTools:113` — can emit `error: error:`.

3. **A numeric parameter silently substituted.** `TurboMCPTypes.cpp:395-404` (`banana`, `0`, `-5` → 50) and
   `TurboMCPCallCostTools.cpp:114-122` (`banana`, `0`, `-5` → 10). A refused value and an accepted value produce
   the same output.

4. **A comment that states the invariant the code does not hold.** `TurboMCPDataTableRows.cpp:536-537` says
   *"ONE TRANSACTION ... WITH NO EARLY EXIT"* and the function has two (`:561`, `:567`), so
   `MarkPackageDirty()` at `:572` is never reached. The four unchecked-rollback sites are the same shape: **in
   all of them the COMMENT is right and the CODE must change.**

---

## 5. FOUND AND FIXED

**Six tracked scripts carried this machine's absolute path**, including the user name:
`Tools/checks/check-line-cites.py`, `check-refs.py`, `check-suite-shapes.py`, `count-plan.py`,
`Tools/make-questions-index.py`, `make-tools-inventory.py`. Each read
`REPO = pathlib.Path(<a literal absolute path under a user profile>)`. Fixed in `452ddb4` by deriving the root
from `pathlib.Path(__file__).resolve().parent.parent.parent`, which serves both depths because `Tools/x.py` and
`Tools/checks/x.py` each climb three levels to the repository root.

**The literal string is deliberately not repeated here.** *A checkpoint that carries the path in order to
describe its removal reintroduces it, and the next `S3` sweep will find this file and have to decide whether it
is a defect.* **A grep for the pattern now returns nothing in the tree.**

**And `flost` alone is not a leak:** it is `FloStyle`, the author name in the MPL-2.0 header on every source
file.

---

## 6. THE RELIABILITY OF THESE FINDINGS, MEASURED

Every claim below was tested by hand against the tree. **The error rate is not uniform, and it depends on the
kind of claim the finding makes.**

| Kind of finding | Tested | Wrong |
|---|---|---|
| **Cites a line** | 12 | **0** |
| **Asserts a relation** ("A contradicts B") | 7 | **3** |
| **Asserts a count** | 5 | **2** |

**So:** a finding that names a file and a line can be acted on. A finding that says two documents differ must
have both opened. **A count must be re-derived** — four counts on the task board were taken over a subset and
printed as totals, including one of mine.

**Why relations fail:** they assert three things — that A says X, that B says Y, and that X and Y conflict. A
reader who finds X and Y stated differently stops there, because the third claim looks like the easy one.

---

## 7. THE METHOD, WHICH IS THE SESSION'S MOST REUSABLE RESULT

### A tool whose quoting you do not control is a screen, never a verdict

**`git grep -c '<pattern>' -- 'Tools/*'` inside a PowerShell loop returned ZERO four times when the symbol was in
fact used.** *`Select-String` on the file returned the right answer every time.* The failures were silent: a
zero that reads like an absence.

    `rebuild_fixture`     reported unused   -> called from `test-graph-replace.py:88`,
                                              `test-graph-structure.py:129`, and four suites that wrap it
    `OutRoute`            reported one file -> the caller is in the SAME file (`:269`) and the value is
                                              printed at `:187`
    `function\s+(\w+)`    captured verbs   -> PowerShell function names contain hyphens
    `Stop-Editor`         counted twice    -> it is a SUFFIX of `Start-EditorAndWait`

*** SO: use a pattern match to find WHERE to look, then open the file to decide. Four false positives were caught
this way and none of them was written down as a finding.***

### Read one file, then check the family

Read **one file** in a family in full. If it shows a shape, **check the whole family by pattern**, then
**measure both sides of the call** rather than assuming which side owns the convention.

That is what found the six-file machine-path defect, the 4-of-29 prefix guard, and the `FAILS ON` convention at
9 of 52. **A finding about one file would have been wrong in every case.** And for one finding the deciding fact
was a **function signature** — `bool Apply(..., FString& ApplyError)` against `FString Apply(...)` — which a grep
for an error string could never have shown.

### `X4`: follow a symbol rather than read a file

Take a parameter name, a guard name or a field name and count its occurrences across the tree.

    TWO FILES    one writer and one declaration   -> look closer
    THREE OR MORE                                 -> you have probably found the question

**Five real instances were found this way, and it also produced one false positive** (`OutRoute`), which is why
the count is a screen and the call site is the verdict.

And the pattern that produces FALSE findings is now `V1` in `AUDIT_PATTERNS.md`: a conclusion drawn from a
fragment. Five findings were wrong on the first pass; **all five** were a conclusion that claimed more than the
evidence covered — a truncated line read as the whole line, a count over a subset printed as a total, one of two
named things read and the other assumed.

**The one question a verifier asks: does the evidence cover everything the claim covers? If the answer is not
visible in the finding, it is not a finding yet.**

---

## 8. WHAT REMAINS

| Group | Files |
|---|---|
| `Source/` not read | ~19 — `NodeResolve`, `WidgetsAsVars`, `Overrides`, and ~15 `*Tools.cpp` wrappers of 30 to 180 lines |
| `Tools/` not read | 47 suites, `run-suites.ps1`, the `make-*.py` generators, `tmcp*.py`, `common.ps1` |

**The task board holds 43 cards**, each naming the file lines it concerns, so a card needs no context beyond
itself.
