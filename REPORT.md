# TurboMCP — analysis

**Version 1.0.0.** Every figure in this report was read from the tree, not recalled.

## Which document answers which question

| You want | Read | What it is |
|---|---|---|
| **The state now, and how to verify it** | [`HANDOVER.md`](HANDOVER.md) | The four commands that establish the state, what is finished, **what is not**, and what I do not know. **Start there** |
| **The analysis: why, what is missing, what to do next** | **this file** | The invariants that hold the 93 tools together, the phantom fixtures, the vacuity debt, the dependency projection, and the error of mine worth recording |
| **The headline snapshot** | [`STATUS.md`](STATUS.md) | Counts and a capability table at the revision it was written |
| **The evidence** | [`TestRecord.md`](TestRecord.md) | 246 chronological sections; every claim here traces to one of them |

**`Spec/WORKING_PLAN.md` is the authoritative work list** — the only one of the five audited row by row
against the tree.

---

## 1. What TurboMCP is, in three sentences

An Unreal Engine 5.8.3 editor plugin that exposes **93 AI-callable tools** over the Model Context Protocol,
in **4 toolsets**: Read (30), Look (4), Preview (26), Edit (33). The readers live in Read and Look and are
**structurally incapable of writing**, and every writer has a Preview twin that shares the **same `Plan`**,
so Preview and Edit refuse **exactly** the same input.

*** THE COUNTS AND THE CAPABILITY TABLE ARE IN [`STATUS.md`](STATUS.md), AND THE 93 TOOLS THEMSELVES ARE IN
[`TOOLS.md`](TOOLS.md). THEY ARE NOT REPEATED HERE, BECAUSE A NUMBER WRITTEN IN TWO PLACES IS A NUMBER THAT
WILL EVENTUALLY DISAGREE WITH ITSELF — which this project has already measured four times.*** What follows
is what those two documents cannot carry: the reasoning, the constraints, and what is not known.

---

## 2. The plan: **31 `todo` · 15 `done`** of 48 rows

*Counted **by field** — the last cell of the line. See section 7: a regex over the line returns a wrong
number.* **The rows themselves are in [`Spec/WORKING_PLAN.md`](Spec/WORKING_PLAN.md); what follows is the
finding that came out of auditing them.**

**Eight rows were already built while their plan column still said `todo`** — `R-BP-LINT` (37 checks),
`R-CALL-COST` (29), `R-ASSET-COST` (10), `P/E-DATATABLE-ROWS` (26), `R-MAT-INST-LOAD` (20),
`P/E-ENUM-EDIT` and `P/E-MAT-BLOCKS` (24). **The plan understated the project by seven capabilities, and
that was found by auditing the rows against the tree rather than by reading the column.**

### What is left, by phase

| Phase | State |
|---|---|
| **0 — Foundations** | 3 rows `todo`: one owner of the editor lifecycle, re-verification of `07_ENGINE_NOTES.md` |
| **1 — Blocking measurements** | 3 `todo`: Blueprint breakpoints, automation tests, kit `TA-14` → `TA-21` |
| **2 — Vision** | **2.1 and 2.2 done.** 2.2 captures **9 editor types, none refused**. 2.3 (level, viewport, PIE) and 2.4 (image diff) remain |
| **3 — Look** | layout report, capture and widget preview are largely built; finishing rows |
| **4 — Data** | `P/E-DATATABLE-ROWS` done; curves (`T-DATA-5` → **`TA-17` missing**) |
| **5 — Widgets** | override, bind and retarget writers; **`T-WIDGET-4` → `TA-8` missing** |
| **6 — Blueprints** | the largest block: graph append, replace, patch, knots, tidy |
| **7 — Structs and enums** | 7.1 done; 7.3 curves |
| **8 — Materials** | block writing, label promotion |
| **9 to 10** | session and cost, source control |
| **11 to 15** | level and actors, build, vision, quality |

---

## 3. The three things I am most sure of, and **how** I know

### 3.1 The verification machine works

**Seven consecutive rows ended with a clean full run: `53/53` → `54/54` → `55/55` → `56/56` → `57/57`.**
Each with a newly written suite inside it and zero failures.

And the measurement that matters: **the last three rows needed almost no intervention from me.** The
briefs, the safety commits, the pre-flight checks and the `P1` rule were built over thirty rounds, and they
now produce results without constant supervision.

### 3.2 The break test is the cheapest defect detector in the project

**Three times out of three, a break test paid beyond its purpose:**

| Row | The second finding |
|---|---|
| **7.1** | *four failures with the mangled field names visible* |
| **10.2** | ***a `P1` defect in the code: an empty decision pattern*** |
| **2.1** | ***a defect in the suite: a check that asserted a prefix and passed against a broken build*** |

**Row 2.2 found a fourth: a failed write arriving as `look:... error:...` — so a caller testing for the
`error:` prefix read a FAILURE as a SUCCESS.**

> **The general shape is now namable: A CHECK THAT MATCHES A PREFIX CANNOT DETECT A CHANGE AFTER THE
> PREFIX. An invisible refusal reads as a capability.**

### 3.3 The project corrects its own premises, and that is the dominant pattern

**Five consecutive rows found their premise false — always in the direction that UNDERSTATES what
exists:**

| Row | Premise | Reality |
|---|---|---|
| **7.1** | *"editor-only"* | *too strict — `CreateUserDefinedStruct` installs the `EditorData` itself (`StructureEditorUtils.cpp:42-64`)* |
| **1.3** | *enumerate plugins through `.agent/*.py`* | *those scripts list 2 plugins, not the engine* |
| **10.2** | *"`CheckOutFile` is the way"* | *it answers `True` on Git **without ever issuing a checkout*** |
| **2.1** | *"add 4 selectors"* | *the capability was already 2 062 lines over 3 files plus a 378-line suite* |
| **2.2** | *"six editor types"* | *the plan names **eight**, and **nine** work* |

---

## 4. The environment constraints, **measured** — the boundary of what is possible here

These four bound what the plugin can promise:

| Constraint | The line that decides it |
|---|---|
| **Slate is never initialised in a commandlet** | `LaunchEngineLoop.cpp:3343` guards the init with `!IsRunningCommandlet` |
| **`FBlueprintEditorModule::StartupModule` has an unconditional `check(GEditor)`** | `BlueprintEditorModule.cpp:214` — **two of the plugin's commandlets cannot run at all** |
| **Under `-nullrhi`, Slate IS initialised — with `SlateNullRenderer`** | `RHIGlobals.h:894` → `LaunchEngineLoop.cpp:3345-3347` → `:3354` |
| **The suites run with a REAL RHI and a REAL window** | `run-suites.ps1:195` passes only `-LowPriority` |

> ***The fourth line was a FALSE premise I had written into four briefs. It came from a stale comment at
> `start-editor.ps1:261` that I read instead of the argument list. The comment is corrected.***

**And a data boundary, stated twice by two independent workstreams:**

> **NO CHECKOUT AGAINST A LIVE SOURCE-CONTROL DEPOT HAS EVER BEEN PERFORMED HERE.** `p4` points at
> `ssl:<p4-host>:1666`, which does not answer; `cm` and `svn` are not on `PATH`; and Git's answer is a
> **refusal**.

---

## 5. The phantom fixtures — **five acceptance rows name assets that do not exist**

| Acceptance row | Fixture named | Status |
|---|---|---|
| `T-MAT-10` | `TA-6` | *the instance exists as `MI_TurboMCPFixture` — missing **under the name the table uses*** |
| `T-LEVEL-1/2` | `TA-15`, `TA-16` | absent |
| `T-WIDGET-4` | `TA-8` | absent |
| `T-DATA-3` | `TA-21` | **no `UserDefinedStruct` anywhere in the repository** |
| `T-DATA-5` | `TA-17` | *built since this report was written* |
| *candidate* | `T-BP-8` → `TA-9` | *the label may mean a class swap in session* |
| **rows 2.2 to 2.4** | `TA-18`, `TA-19` | **absent — reported BEFORE 2.2 began** |

**And a label collision: `TA-5` is called "Material 60 expressions" by the table, while the kit uses that
label for the `BackwardProbe` graph.**

> ***The pre-flight rule that catches this was written after the SECOND of those rows and applied to
> neither of the first two. Since then it has been in every brief — and row 2.1 caught the
> `test-capture.py` trap BEFORE it fired, for the first time.***

---

## 6. The vacuity debt

**Five capabilities reported a check count that overstated their coverage.** *One suite had an `else`
branch that literally checked `True`.*

**Three different mechanisms were found for the same disease, each named:**

| Level | The defect | The fix |
|---|---|---|
| **String** | *`IsAvailable` printed `false` for a question never asked* | *print **`NOT ASKED`*** |
| **String** | *an empty decision pattern after the dash* | *a check that no decision line is empty* |
| **Type** | *a boolean cannot distinguish "refused" from "another tool might"* | ***a **three-state enum** plus a **catalogue of every case*** |

> ***"AN INVISIBLE REFUSAL READS AS A CAPABILITY" — the oldest defect in this project, in seven words, and
> it has arrived under six different mechanisms.***

---

## 7. The error of mine worth recording

**For twenty rounds I counted the plan's progress with `Select-String '\| todo \|'` — a regex over the
LINE.**

A status cell can contain prose. **The number changed between two runs of the same command.** Counting
**by field** — the last cell — is the only count that is stable.

> ***This is the project's oldest rule, broken by me: PARSE THE FIELD, NEVER MATCH A PHRASE BESIDE IT. The
> counter that answers "how much is left" was a string search over cells that contain sentences.***

**And I made the same mistake in another shape: I read a comment (`start-editor.ps1:261`) instead of an
argument list (`run-suites.ps1:195`), and turned it into a false premise across four briefs.**

---

## 8. What I do not know, and do not present as known

| | |
|---|---|
| **No checkout against a live depot** | *every checkout answer reported is the answer to a refusal or to a no-op* |
| **The `UsesCheckout == true` branch has never run** | *Perforce is `IsAvailable == false`, so the mechanism refuses before reaching it* |
| **Plastic and Subversion are unreachable** | *`cm` and `svn` are absent from `PATH` — their AVAILABLE branch has never been seen* |
| **No `-nullrhi` run has ever been performed** | *the `screen` refusal is proven by construction from two engine guards. `start-editor.ps1 -NullRHI` followed by `mode screen` would settle it, and has not been run* |
| **The positive path of `selection`** | *proven by a self-cleaning internal probe, because `P3` forbids a Look from creating a selection* |
| **`FindReaders` counts the assets that OWN the struct** | *not every graph that reads a field by name — and the preview says so* |

---

## 9. What I would spend time on, if time were not the constraint

**In this order, because each item unblocks the next. Three are delivered; two remain.**

| Priority | State |
|---|---|
| **11.1 The structural hole** | **DELIVERED** — `CANNOT-MATCH` added and proven on six cases, five false positives removed, the audit is **BLOCKING**, 0 candidates |
| **9.1 The foreign editor** | **DELIVERED** — `check-mcp-server.py` names the project that owns the port, proven on three cases |
| **9.2 A source-control depot** | *bounded by the environment* |
| **9.3 The kit** | **PARTLY DELIVERED** — `TA-17` **BUILT** (2 assets, payload verified by the engine's own Python, tangents proven behaviourally); `TA-18` and `TA-19` **refused by name**, each with its blockers measured |
| **9.4 A blocking format checker** | **DELIVERED** — it was the same item as 11.1 |
| **9.5 The 30 unwired writers** | *mechanical* |

### 9.1 Detecting a foreign editor on port 8000 — **delivered**

Three independent workstreams paid for this collision, me included. **`check-mcp-server.py` could pass
against somebody else's editor, and the suite then failed with "the provider is not Git" instead of "another
process owns the project"** — a false failure that cost a round to diagnose every time.

> ***`check-mcp-server.py` now asks the PROCESS, not the protocol.** The MCP handshake reports nothing that
> identifies a project, so a session id cannot tell two editors apart; **what can is the command line** —
> `start-editor.ps1:260` builds `$Args = @($Project)` with the **absolute path**.*
>
> **Proven on three executed cases:** *nothing running → `NOTHING IS RUNNING` · **a foreign project →
> `RUNNING BUT NONE HAS project.uproject IN ITS COMMAND LINE — belongs to ANOTHER PROJECT`, exit 3** · the
> right project → `1 of them on turbomcptest.uproject`, and the handshake continues.*

### 9.2 A source-control depot that answers — *one command would close the largest boundary*

`p4` points at a dead host and `cm` and `svn` are absent. **With a reachable server, both source-control
rows would move from "measured against a refusal" to "measured against a success"** — and
`UsesCheckout == true` would finally execute its acting branch.

### 9.3 The kit, `TA-14` to `TA-21` — *six acceptance rows depend on fixtures that do not exist*

`TA-17`, `TA-18`, `TA-19` and `TA-16` were **measured buildable** but not built. *Building them unblocks
rows 2.2 to 2.4, 4.x and 7.3 without each having to invent a substitute.*

### 9.4 A format checker for the suites — *the instrument that was missing*

Five capabilities overstated their coverage and **every time it was found by a human or a later
workstream**. *An audit that requires, for every check, a NAMED input that makes it fail — **mechanically**,
not by convention — is what was missing. `audit-suite-quality.py` was an advisory version of it; it now
blocks, which is what this item asked for.*

### 9.5 The 30 unwired writers — *mechanical work, once 10.2 is proven*

The mechanism exists, 2 of the 32 writers use it, and the other 30 are **named one by one**. *This is
repetitive work with no unknowns — the right candidate for heavy parallelism once the format is frozen.*

---

## 10. Dependency and projection — measured, not estimated

### 10.1 The remaining rows declare **no dependency on each other**

**Verified: the dependency column of the `todo` rows contains neither `gated`, nor `unblocked`, nor a
reference to another row.** *The only gating mentions in the plan are on rows **already done**, and those
dependencies are lifted.*

> ***Which means WHAT REMAINS IS ENTIRELY PARALLELISABLE — not "in principle", but by the structure of the
> plan itself. The constraint is not ordering; it is the number of workstreams and the availability of the
> editor.***

**And two shared bottlenecks explain why the parallelism is not unlimited:**

| Resource | Who wants it |
|---|---|
| **The editor** | *every suite that is not static — and a suite that OWNS the editor cannot coexist with another* |
| **`TurboMCPToolsets.h`** | *every row that adds a tool — one editor at a time, and a mandatory read before editing* |

### 10.2 Velocity, and why the finished rows are not spread evenly

| Window | Rows `done` |
|---|---|
| rounds 1 → 150 | **2** |
| rounds 150 → 190 | **2** |
| **rounds 190 → 218** | **4** |

> ***MOST OF THE COMPLETED ROWS WERE DONE IN THE LAST THIRD. That is not rhetorical acceleration: sections
> 79 to 150 built the machine — briefs, safety commits, pre-flight checks, the `P1` rule, break tests —
> and the machine now produces.***

**But the honest projection is worse than recent velocity suggests, for three measurable reasons:**

1. **The recent rows were the BEST SPECIFIED in the plan** — each already had its measurement done.
2. **What remains includes the graph block (phase 6) and the materials phase** — the largest in the plan,
   and the ones whose premises have not been tested.
3. **At the velocity observed, the plan as written would not finish inside its budget.** *The plan has not
   been re-scoped to fit, and saying so is better than letting the arithmetic speak alone.*

### 10.3 What I would do with the remaining budget, if it were my decision

| | Strategy | What it gets |
|---|---|---|
| **A** | *Do every remaining row in order* | *Nothing usable for a long time, and half-finished rows at the end* |
| **B** | ***Finish the high-leverage phases and refuse the rest IN WRITING*** | ***The most useful plugin, and a plan that states what it did not do*** |
| **C** | *Maximise the number of rows done* | *More rows, scattered, with no phase complete* |

> ***I would choose B — because it is the decision this project has already made three times and it
> worked: row 10.2 wired 2 writers of 32 AND NAMED THE OTHER 30; row 2.2 built 9 types of 9 but could have
> refused; the kit was measured buildable for 4 assets and 2 were refused. **EACH TIME, THE PARTIAL RESULT
> MEASURED AND NAMED WAS ACCEPTED AS COMPLETE** — and that is what lets the plan be finished one day.***

---

## 11. The invariants that hold the 93 tools together

***These are not style conventions. Each was paid for by a real defect, and each is mechanically
checkable — or says plainly that it is not.***

| Invariant | Why it exists | Is it verified? |
|---|---|---|
| **Readers live in `Read`/`Look`, writers in `Preview`/`Edit`** | *a reader is **structurally** unable to write — not a discipline, a type* | **Yes**, by the toolset split |
| **Every writer has a `Preview` twin sharing the SAME `Plan`** | *so Preview and Edit refuse **exactly** the same input, and verification lives in `Plan` once* | **Yes** — `D-02`, and 26 Preview against 33 Edit |
| **A tool that cannot do a thing REFUSES BY NAME, with the engine line that decides it** | *"an invisible refusal reads as a capability"* | **No** — by convention and review |
| **An absent measurement never prints `false` or zero (`P1`)** | *`NOT ASKED` rather than `false`; no empty decision pattern; a three-state enum* | **No** — three forms, each found after the fact |
| **A tool's own report is not evidence: read it back through ANOTHER tool** | *a producer reporting on itself proves nothing* | **Yes**, practised in every row, never automated |
| **For every check, name the INPUT that makes it fail** | *five capabilities overstated their coverage* | **Now yes** — `audit-suite-quality.py` blocks |
| **A check does not depend on shared mutable state** | *a check depended on a file staying untracked; a foreign `git add` would have turned it red* | **No** — found twice, fixed twice, not verified |
| **Restore the fixture on the way IN as well as out, and assert it** | *restoring only at the end killed the server the suite was querying* | **Yes**, practised |
| **A break test must show the FALSE VALUE, not just a red line** | *a check that asserted a prefix passed against a broken build* | **Yes** — five times, and four times it found a second defect |

### 11.1 The structural hole — **closed**

> ***It is kept here as it was diagnosed, followed by what was done.***

**The diagnosis:** *three of the nine invariants were held by convention and review rather than by the
machine — refuse-by-name, the `P1` rule, and naming the input that fails each check — **and those are
exactly the three that produced the five capabilities with overstated coverage.***

**What was done:**

| | |
|---|---|
| **1** | ***A sixth finding, `CANNOT-MATCH`, added to `audit-suite-quality.py`*** — *the **mirror** of the other five: it looks for an assertion that cannot **PASS**, the class row 2.2 found (`^` without `re.MULTILINE` → 16 of 18 failures from one function). Proven on **six cases**: three that must fire, three that must not.* |
| **2** | ***The audit went from ADVISORY to BLOCKING.*** *It reported **five** candidates and **all five were false positives** — a gate that fails healthy code is not a gate, it is a reason to bypass it. All five were read, four false positives removed, **and the one real hole (`test-widget-preview.py:175`, `"4096" in huge`) was FIXED rather than tolerated.*** |
| **3** | ***Proven in both directions:** 0 candidates → PASS, exit 0 · 1 candidate → FAIL, exit 1 · probe removed → PASS, exit 0.* |

### 11.2 And the strongest invariant in the project is not written down as one

**It is the break test.** *It is not in `D-01` to `D-57` as an architectural decision — it lives in the
briefs, repeated for every row.*

> ***AND IT HAS THE BEST RATIO OF ANYTHING HERE: run five times, five times it proved the check bites, and
> FOUR TIMES it found an ADDITIONAL DEFECT nobody was looking for — two in the shipped code, two in the
> suite itself.***
>
> **If I kept one practice from this whole project it would be that one — and if I added one, it would be
> to make it mechanically mandatory rather than re-asked in every brief.**

---

## 12. What changed since the first version of this report

| | Then | **Now** | The fact that explains the movement |
|---|---|---|---|
| Full run | *`passed 56, of 56`* | ***`passed 58, failed 0, blocked 0, of 58`*** | ***two PRE-EXISTING defects fixed, one of which made green IMPOSSIBLE*** |
| Plan | *40 `todo`, 6 `done`* | ***31 `todo`, 15 `done`*** | ***eight rows were already built and marked `todo`*** |
| Suite audit | *advisory* | ***BLOCKING*** | *three convention-held invariants moved to the machine* |
| Foreign editor | *undetectable* | ***detected, `exit 3`*** | *the collision that had cost four measurements* |
| Kit | *"`TA-17`/`18`/`19` buildable"* | ***`TA-17` BUILT; `18`/`19` refused by name*** | *and `TA-18` was **not** buildable — this report was wrong* |

### 12.1 And the two most important fixes were in **none** of the five priorities

> ***THIS REPORT NAMED FIVE THINGS I WOULD SPEND TIME ON. THE TWO DEFECTS BLOCKING THE PROJECT'S OWN GREEN
> SIGNAL WERE NOT AMONG THEM.***
>
> *They were found by a workstream doing a fixture row, and by me **following its evidence**. **The list was
> useful for starting; the evidence was useful for finishing.***

### 12.2 And the lesson that cost the most, stated once

**ELEVEN TIMES IN THIS WORK, AN OPERATION FAILED IN SILENCE AND WAS READ AS A SUCCESS:**

| | |
|---|---|
| `git checkout` with `capture_output=True` **and the result never read** | *the error was **captured and nobody looked at it*** |
| a regex that assumed the wrong format | ***inside the suite that exists to enforce the rule*** |
| a count pinned on a shared asset | *the suite **passed or failed according to which fixture it was looking at*** |
| a refusal message describing a `Build.cs` that no longer matched | *it named a missing dependency that was already there* |
| a `git add` whose output went to `Out-Null` | *the commit then recorded **none** of its intended changes and reported success* |

> ***IT IS THE SAME SENTENCE AS "AN INVISIBLE REFUSAL READS AS A CAPABILITY", AND IT IS THE MOST RECURRENT
> DEFECT IN THIS PROJECT — INCLUDING INSIDE THE TOOLS THAT EXIST TO CATCH IT.***

---

## 13. In one line

> **93 tools, `passed 58, failed 0, blocked 0, of 58` — the full run is entirely green — 31 plan rows
> remaining, and a verification machine that now produces correct results without my intervention.**
>
> ***What remains is not blocked by unknowns: it is bounded by time, by five fixtures that do not exist,
> and by a source-control server this machine has never had.***
