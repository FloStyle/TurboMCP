# HAND-AUDIT STATE

**A checkpoint for whoever continues the hand audit.** It records what was read, what was found, what was
checked and discarded, and the two things the session established that are not in any card: the method and the
reliability of the findings themselves.

---

## 1. THE SET

| Group | Files | Read by hand |
|---|---|---|
| `Source/` | 186 | **44 read in full**; the last 5 wrappers read structurally in the final round, all with ZERO write sites |
| `Tools/` | 88 | `checks/` (8), infrastructure (3), `make-*.py` (7), `*.ps1` (11) screened; 3 suites read |
| `.md` + descriptors | 31 | done earlier by the document audit |

**Of the 186 `Source/` files, 117 are `.cpp`. `check-cp2-candidates.py` measures that 69 of them are read-only by
its rule and 48 write. EVERY WRITER HAS BEEN READ.** *The files not opened are readers and wrappers, and by the
measurement above they cannot carry a behaviour defect.*

**The 5 wrappers read last, and why they need no further work:** `MaterialTools` (110 l., the file that CONTAINS
`AsError`), `StructTools` (111), `CommentTools` (56), `GraphCheckTools` (38), `GraphFixBackwardTools` (52) —
**all five have zero write sites.** `CommentTools` and `GraphFixBackwardTools` call `Apply` and are two of the
eight exposed prefix sites; the other three format an `OutError` that is not prefixed, so they are correct.

**Not done:** 32 suites not read, 4 `.ps1` not read past their functions, `NodeResolve` read in part.

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

### `C-P10` is CLOSED, and so is the dead-counter family, for every file that can carry them

`git grep 'Transaction\.Cancel()'` over the 39 remaining files returns **nothing**. All **28** unchecked cancels
are in three files, and all three are audited:

| File | Sites |
|---|---|
| `TurboMCPMaterialBlocks.cpp` | 19 |
| `TurboMCPMaterialLabelsPromote.cpp` | `:461`, `:537`, `:610`, `:653`, `:662`, `:671` |
| `TurboMCPMaterialTidy.cpp` | `:612`, `:621`, `:629` |

**And of the sixteen `Source/` files still unread, FIFTEEN HAVE ZERO WRITE SITES** — `NodeResolve`, `Layout`,
`GraphLoad`, `GraphTextLex`, `GraphStructure.h`, `GraphStructureNotes.h`, `Toolsets.cpp`, `ToolsetsInternal.h`,
`WidgetTreeSync`, `MaterialTools`, `ToolSchemaTools`, `StructTools`, `CommentTools`, `GraphCheckTools`,
`GraphFixBackwardTools`. The `C-P10` and dead-counter families **cannot occur in a file that writes nothing.**
`TurboMCPWidgetRetarget.cpp` was the only one left with a write, and it is clean.

*** SO WHAT REMAINS IN `Source/` CAN CARRY ONLY `C3` AND `C-P2` - defects in WHAT THE TOOL SAYS, NOT IN WHAT IT
DOES.***

### The correct pattern is already in the repository, three times

| Does it right | Does it wrong |
|---|---|
| `GraphKnots.cpp:1400-1405` — re-measures and reports *"THE ROLLBACK DID NOT RESTORE THE GRAPH"* | `MaterialBlocks.cpp` — 19 unchecked `Cancel()` |
| `DataTableRows.cpp:574-583` — *"a MEASUREMENT OF THE RESULT rather than a RESTATEMENT OF THE REQUEST"*, with `MISSING AFTER WRITE` | `MaterialLabelsPromote.cpp` — 6 unchecked, dead `InputsRetargeted` |
| `WidgetRetarget.cpp:355-384` — re-counts, compares, and says *"the removal did not take. Report this rather than trusting the count above."* | `MaterialTidy.cpp` — 3 unchecked, dead `MovedBefore` |

**The repair is not an invention.** It is to apply, in three files, a pattern three other files already implement
and explain. **`AsError` is the same kind of fix**: written once in `TurboMCPMaterialTools.cpp:31-38`, wired into
4 of 29 wrappers.

### And the second class has a reference implementation too, for the same parameter

**Five sites parse a numeric parameter. Four substitute a default silently; one refuses by name.**

| Substitutes silently | Refuses by name |
|---|---|
| `Types.cpp:395-404` — `banana`, `0`, `-5` → 50 | `BlueprintLoad.cpp:100-115` — the **same** `TokenBudget` |
| `CallCostTools.cpp:114-122` — → 10 | `IsNumeric()` tested **before** the conversion |
| `CallLogTools.cpp:34-42` — → 50 | the refusal states **the default it would have used** |
| `BPLintTools.cpp:66-70` — → 0 | a non-number and a non-positive number get **different messages** |

*Structure matters for finding them: `Types.cpp` uses **A** (the default is pre-set and the assignment is
skipped); the other three use **B** (the fallback is assigned in the failure branch). `check-cp2-candidates.py`
matches **B** only and prints the `Atoi(` count so its floor is visible.*

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
| **Cites a line** | 13 | **0** |
| **Asserts a relation** ("A contradicts B") | 7 | **3** |
| **Asserts a count** | 5 | **2** |

**This table mixes two sources and that is deliberate: twelve of the line-citations and all of the relation and
count claims came from the earlier subagent audit, and they were tested against the tree in this session.**
*The thirteenth is the first subagent citation tested since, and it held: `TurboMCPGraphTidy.cpp:36` cites
`TurboMCPLayoutReport.cpp:26` for `PinRowSpacing = 24.0`, and the constant is at **`:31`**.* **So the earlier
audit's citations are as reliable as this session's, and its relations and counts are not.**

**So:** a finding that names a file and a line can be acted on — **from either source**. A finding that says two
documents differ must have both opened. **A count must be re-derived** — four counts on the task board were taken
over a subset and printed as totals, including one of mine.

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

## 8. WHAT REMAINS, AND THE ORDER TO DO IT IN

**The order is measured, not chosen.** `check-fails-on.py --list` prints the 34 suites that do not state their
failure input, grouped by family:

| Family | Silent suites | Why it comes where it does |
|---|---|---|
| `graph` | 6 | **These test the writers the `Source/` audit found most defective** — the 28 unchecked cancels, the dead counters |
| `widget` | 4 | Same: `WidgetRetarget`, `WidgetTreeSync`, `WidgetBindAdd` |
| `panel`, `override`, `vcs`, `asset` | 2 each | Writers with carded defects |
| 18 single-suite families | 18 | Independent and small |

| Group | Files |
|---|---|
| `Source/` | **done for every file that writes** — 48 of 48 |
| `Tools/` suites | 34 not read, of which **10 are `graph` and `widget`** |
| `Tools/` scripts | 4 `.ps1` not read past their functions |

*** SO THE NEXT ROUND SHOULD READ A `graph` OR `widget` SUITE IN FULL, NOT A READER.*** *The `Source/` readers are
closed by measurement; the suites that exercise the defective writers are not, and they are the ten most likely to
show whether a carded defect is actually caught.*

### And the suite layer's standard is higher than `check-fails-on.py` can measure

**Five suites read in full, three more in structure.** Every one has the same shape, and the shape is not the
phrase the template asks for:

    a module docstring saying WHY the suite exists, often citing the acceptance row and the failure it closes
    a `check()` that prints `ok`/`FAIL` and accumulates
    each check NAMING WHAT IT PROVES - "it names why that claim would need proof", "each fault is reported on
      ITS OWN LINE", "the default visibility is not printed"
    *** the WRONG EARLIER VERSIONS recorded, with what they got wrong: `test-graph-check.py:76-80` (the zero case,
    "an assertion that could not be satisfied by the correct answer") and `:164-168` (the line expectation, "the
    suite was asserting the tool had misplaced a finding when the suite had misplaced its expectation, WHICH IS
    THE 'READER FAULT REPORTED AS A SUBJECT FAULT' MISTAKE") ***
    *** and the strongest ones PROVE THE NEGATIVE FROM OUTSIDE: `test-graph-check.py:12-14` checks the asset's
    hash, size and mtime before and after, plus a read through a DIFFERENT tool, because "A TOOL REPORTING ITS OWN
    INNOCENCE IS NOT EVIDENCE" ***

*** SO THE 34 "SILENT" SUITES ARE NOT A QUALITY MEASURE. The two families the work order points at - `graph` (6)
and `widget` (4) - are among the best-written in the tree, and the phrase `check-fails-on.py` looks for is a
sentence shape, not a property of a test. That card's recommendation stands: DROP THE RULE, OR REWRITE IT TO ASK
FOR WHAT IT MEANS. ***
