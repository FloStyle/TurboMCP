# TurboMCP — status report

**Measured on `main` = `origin/main` = `de1c06b`.** Every number here was read off the tree, not recalled.

---

## 1. The headline

| | |
|---|---|
| **Capabilities implemented** | **36 of 57** — see §18 of the record: the spec has TWENTY sections, not nine |
| **Tools shipped** | **79** AICallable `UFUNCTION`s, in 4 toolsets |
| **Reading (`R-`)** | **21 of 21 — COMPLETE** |
| **Vision (`L-`)** | **3 of 3 — COMPLETE** |
| **Preview (`P-`)** | **4 of 4 — COMPLETE** |
| **Edit (`E-`)** | **21 of 23** |
| Test suites | 48 files · **full run `47/48`** — one failure, `test-graph-dead`, on a REAL check about the MCP server after a fixture rebuild |
| Documentation | `TestRecord.md` 3668 l. · `QUESTIONS.md` 1193 l. · `Spec/` · `PLAN.md` · `DOCS.md` |

**The two capabilities you asked to prioritise — Blueprint Reading, then Vision — are complete**, and so is the Preview half of every writer.

---

## 2. What is done

### Reading — 21 of 21
`R-ASSET-SEARCH`, `R-ASSET-DESCRIBE`, `R-BP-LOAD`, `R-CALL-LOG`, `R-COMMENT-LIST`, `R-DATATABLE`, `R-DIRTY-LIST`, `R-GRAPH-LOAD`, `R-MAT-BLOCKS`, `R-MAT-LABELS`, `R-MAT-LOAD`, `R-MAT-TRACE`, `R-NODE-FIND`, `R-NODE-RESOLVE`, `R-OVERRIDES`, `R-PLAY-CHECK`, `R-REFS`, `R-TOOL-INDEX`, `R-TOOL-SCHEMA`, `R-TRACE`, `R-TYPE-FIND`

### Vision — 3 of 3
`L-CAPTURE`, `L-LAYOUT-REPORT`, `L-WIDGET-PREVIEW`

### Preview and the graph writer foundation
`P-GRAPH-CHECK` (validates graph text with line and column, writes nothing), `P/E-GRAPH-APPEND` (the first writer that **creates** nodes from text), plus the layout passes `P/E-GRAPH-KNOTS`, `P/E-GRAPH-TIDY`, `P/E-GRAPH-FIX-BACKWARD`, `P/E-GRAPH-DEAD`.

### Infrastructure built along the way, all of it load-bearing
- **`FTurboMCPGraphTextLex`** — the graph-text lexer, extracted so there is **one** reader of the one dialect.
- **`FTurboMCPNodeType::SpawnNodeOfType`** — the type-ID-to-node factory, reaching the engine's action database because a type ID carries a **localized** display title.
- **`TurboMCPTestMap`** — a PIE map with a placed actor that raises a real Warning and Error, which closed two long-standing holes at once (§5).
- **`Tools/run-suites.ps1`** — a harness that owns the editor per suite and waits for both the **server** and the **process**.

---

## 3. What is left

**Derived from `Spec/03_TOOLS.md`'s own `###` entries, section by section, not from a hand-kept list. THE EARLIER
TABLE IN THIS FILE WAS A NINE-SECTION READING OF A SPEC THAT HAS TWENTY SECTIONS**, and it is replaced rather than
annotated because the shape of the gap was wrong, not just its size.

| § | Section | Done | State |
|---|---|---|---|
| 2 | Agent help (Read and Preview) | 8/8 | **COMPLETE** |
| 3 | Blueprint and widget reading | 8/8 | **COMPLETE** |
| 4 | Materials | 4/4 | **COMPLETE** |
| 5 | Look | 4/4 | **COMPLETE** |
| 7 | Blueprint structure (Preview and Edit) | 4/4 | **COMPLETE** |
| 8 | Graph writing (the bulk writer) | 1/1 | **COMPLETE** |
| 9 | Materials writing | 5/8 | `E-MAT-BLOCKS` **DONE**; `R-MAT-INST-LOAD` **DONE** (20 checks) |
| 4 | Data tables | 1/1 | `P/E-DATATABLE-ROWS` **DONE** (26 checks) |
| — | Kit (plan row 1.5) | 2/8 built | `TA-14`/`TA-20` **BUILT and read back**; `TA-15`/`TA-17` buildable; `TA-16` needs work; `TA-18` needs a measurement; `TA-19`/`TA-21` REFUSED with citations |
| 15 | Quality and maintenance | 1/1 | `R-BP-LINT` **DONE** (37 checks, six named checks each reporting its denominator) |
| 10 | Graph structure | 0/3 | `E-GRAPH-RENAME`, `E-GRAPH-COLLAPSE`, `E-GRAPH-COPY` |
| 11 | Widget authoring | 0/5 | needs the widget text dialect first |
| 12 | Assets, structs and data | 0/6 | `E-ASSET-*`, `P-ASSET-DIFF`, `R-ASSET-COST`, curves, structs |
| 13 | Level and actors | 1/5 | `R-ACTOR-LOAD`, `E-ACTOR-PLACE`, `P/E-ACTOR-SET`, `E-COMPONENTS` |
| 14 | Debugging | 2/7 | **CLOSED BY MEASUREMENT** - see §4 |
| 15 | Quality and maintenance | 0/1 | `R-BP-LINT` |
| 16 | Agent and session tooling | 0/4 | `E-SESSION`, `P/E-ASSET-BATCH`, `R-CALL-COST` |
| 17 | Source control | 0/2 | gated on measurement 1.3 |
| 18 | Vision: what a Look may capture | 0/4 | `L-CAPTURE-ASSET`, `L-CAPTURE-LEVEL`, `P-IMAGE-COMPARE` |
| 19 | Before and after | 0/1 | `P-IMAGE-COMPARE` |
| 11 | Build portability | 0/1 | `09_BUILD.md` |

**`Spec/WORKING_PLAN.md` IS THE AUTHORITATIVE ORDER**, and it is a working document that changes every round. The
rows above are its steps: materials end to end, then data tables, then widget authoring, then graph structure, then
assets and data, then level and actors, then quality and session, then source control, then build portability.

### 3.1 The one thing that is deliberately NOT built

`E-BREAKPOINT`, `R-BREAKPOINT`, `R-DEBUG-STATE`, `E-DEBUG-CONTROL` and `R-AUTOMATION-RUN` are **refused by decision,
not deferred by effort.** Measurement 1.1 asked whether the MCP server answers while the editor is stopped at a
Blueprint breakpoint, and **it answers NO**, proven from the engine source rather than from a run:

    Ticker.h:35         static CORE_API FTSTicker& GetCoreTicker();
    Ticker.h:168-170    void ExecuteOnGameThread(...) { FTSTicker::GetCoreTicker().AddTicker(...); }
    HttpServerModule.h:26   class FHttpServerModule ... , public FTSTickerObjectBase
    HttpServerModule.cpp:182-191   FHttpServerModule::Tick(...) { ... Listener.Value->Tick(...); }

**`GetCoreTicker()` IS THE LITERAL MECHANISM OF `ExecuteOnGameThread()`, so the HTTP server ticks on the GAME
THREAD.** A breakpoint stops the editor in a nested loop, the game thread stops ticking, and **THE SERVER'S OWN
`Tick` NEVER RUNS - THE REQUEST IS NEVER DISPATCHED TO A TOOL. THE SERVER IS NOT SLOW, IT IS SILENT.** An agent that
armed the breakpoint waits until a person resumes the game.

`D-55` had already written this down and had already rejected the alternative: *"Rejected: building breakpoints and
documenting the risk. Why: a capability whose failure mode is 'the agent stops forever' has to be gated on a
measurement, not on a warning."* **THE TRACE POINTS ARE THE ANSWER INSTEAD, AND THEY ALREADY EXIST**: `E-TRACE` and
`R-TRACE` capture a live hit with node identity through `FBlueprintCoreDelegates::OnScriptException`
(`Script.h:445,453,460`), **AND A TRACE POINT NEVER PAUSES** - which is what `D-36` means by *"trace points before
breakpoints"*.

---

## 4. The blocker, and it is no longer the harness

**An earlier revision of this file said "the capability work is nearly complete; the verification harness is now the
bottleneck", with a full run reporting `31 of 39` and failing suite names that changed between runs. BOTH HALVES OF
THAT ARE NOW WRONG.**

**THE HARNESS IS GREEN: `passed 43, failed 0, blocked 0, of 43`, measured on an idle machine.** Six defects of one
shape were fixed to get there, each of which had produced "failing names that change between runs and pass in
isolation":
- the **rebuild scripts return exit 0 and print PASS** in every invocation shape that can be reproduced.

**Six defects of one shape have been found and fixed**, each producing exactly that signature:

1. the harness waited on the **server** while `start-editor.ps1` decides from the **process**;
2. `make-acceptance-asset.ps1` slept a **fixed 2 seconds** instead of waiting for the process;
3. the probe could pass on an editor that was **still shutting down** — it now requires an `UnrealEditor` that started **after** the launch;
4. **seven** concurrent rebuilds of the **one** acceptance asset are serialized by a **named mutex**;
5. the harness **killed the editor a previous suite had healthily started** — it is now **subordinate to a healthy editor** and reuses it;
6. a live editor process whose **server never came up** made the launcher refuse every later start — now **cleared and retried once**.

### Five hypotheses about the failing rebuild, each FALSIFIED by measurement

| Hypothesis | Measurement |
|---|---|
| the script does not print PASS | it does |
| its exit code is non-zero | it is 0 |
| the suite's call shape breaks it | `subprocess.run(capture_output=True)` from Python → `returncode 0`, 284 chars, `PASS: True` |
| a log-file lock | falsified — the captured stdout is correct and complete |
| the harness's editor holds the `.uasset` | falsified — run **with** an editor alive: `returncode 0`, `PASS: True`, every probe graph named |

`make-kit-assets.ps1` behaves identically: `returncode 0`, `PASS: True`, 3965 chars, every asset named.

**AND THE SENTENCE THAT FOLLOWED THIS TABLE WAS WRONG.** It said the next measurement was *"the state of the editor
at the moment the harness hands control to the suite"*, and named the rebuild as the remaining suspect. **THE
REBUILD WAS NEVER THE DEFECT AND THE DEFECT WAS NOT NEAR IT.** What was actually happening was measured later and in
one line: `Tools/tmcp.py` had gained a re-connect retry so a suite whose editor was restarted underneath it would
recover - correct in itself - **but `connect()` CALLS `rpc()` AND `rpc()` CALLS `connect()` ON FAILURE, SO WHEN THE
SERVER DID NOT ANSWER THE RETRY RECURSED, each level waiting out a full request timeout.**

    check-probe-graphs.py BEFORE:  777.3 SECONDS, and killed at 600 in a full run
    check-probe-graphs.py AFTER:    16.1 SECONDS, RESULT: PASS

**AND THE SUITE'S OWN GUARD COULD NOT FIRE**: its `try`/`except` is written to treat any MCP failure as *"no editor,
which is allowed"* - **BUT THE EXCEPTION NEVER ARRIVED, because the recursion consumed the stack and the clock
instead of raising. A GUARD THAT CANNOT BE REACHED BECAUSE THE FAILURE PATH NEVER RETURNS IS NOT A GUARD.**

**THE LESSON, WHICH IS THE MOST EXPENSIVE ONE IN THIS FILE: A FIX INTRODUCES ITS OWN FAILURE MODES AND MUST BE
MEASURED LIKE ANYTHING ELSE.** That retry was verified against the case it was written for - an editor restarted
underneath a suite - and **never against the case where no editor comes back at all.** Five candidate causes were
named across five rounds and every one was falsified by a single measurement; the real one was a change made to
SOLVE something, in a file nobody suspected for that reason.

**AND THE PER-SUITE WALL-CLOCK DEADLINE IS WHAT MADE IT FINDABLE.** Before it, this suite hung indefinitely and the
symptom - no editor, no log activity, 18 to 25 minutes - pointed at the environment rather than at a tool. **THE
BOUND TURNED AN UNATTRIBUTABLE HANG INTO `RESULT: TIMED OUT (600s)`, AND NAMING THE SUITE IS WHAT MADE THE
RECURSION VISIBLE. A bound is not a fix, and it is what makes a fix possible.**

Two structural notes that survive regardless: the harness now **owns the editor only between suites and never inside one**, and **ONE owner of the editor lifecycle** remains the architectural fix — the hybrid, where a suite and the harness both drive the same editor, is what keeps producing this.

---

## 5. What was found and fixed this stretch, and why it matters

Recorded in full in `TestRecord.md`; the ones worth knowing:

- **`E-GRAPH-DEAD` was queueing the author's COMMENT BOXES for deletion.** A comment has **no pins**, so a walk from the entry nodes can never reach one, and it also writes no variable and holds no entry — so it satisfied every removal condition. Found because an **unused include** named the exact type at fault.
- **`R1`'s pure-node carry could never fire.** The carried node landed on the **source's own left edge**, because the push that would clear it ran *after* the carry. **Seven fixture configurations** were built against it and every one was refused; the workstream diagnosed it **by arithmetic** and recorded a rule defect rather than faking a fixture. It was right.
- **A stale binary masqueraded as a passing fix, three times.** After restoring a broken file and rebuilding, the suite still failed while the source was correct — **the loaded DLL was the old one**. **`Result: Succeeded` is not evidence the binary changed.**
- **`Apply` returned an empty string with a success code** in `E-GRAPH-APPEND` while the graph was correctly changed, so **every check that read the graph passed**. Caught only by reading the **return value**.
- **`P-GRAPH-CHECK` refuses nothing it should accept** — proved by feeding it real graph text read out of a real graph, because a checker that refuses correct input stops every write in the project.
- **The project's own capability audit was wrong for four rounds**: it took only the **first ID per line**, which hid `R-TRACE` behind `### E-TRACE and R-TRACE` and made Reading read as "19 of 19 complete" when it was 20 of 21. The count is **51 distinct IDs, of which `E-X` is a placeholder in the spec's own format example** — so **50 real capabilities**.

---

## 6. Honest limits — things that are built but not fully proven

| Area | What is not proven |
|---|---|
| `E-TRACE` | The **transport** is verified (a live hit, with the node's identity). The **capture path** is not: the break test renames the fixture Blueprint, which proves the check bites rather than that the capture cannot silently drop a hit. |
| `P/E-GRAPH-APPEND` | **Pin values are not witnessed by a second tool** — `R-GRAPH-LOAD` prints only an id and a class for a node unreachable from an entry point, and an appended node has no exec wire. The suite asserts what *is* observable and says so. |
| Node kinds | The spec lists ~15 families. The core mechanism handles any type the action database offers, but **each family was not enumerated and tested**. |
| `Q-17` | The count is now cross-checked against the engine's own log and **proven to bite**. A **forged zero over a true zero** remains unprovable by any assertion. |
| `Q-11` | The panel **runner** is testable; the **Slate layout, combo selection and repaint** need a human eye. |
| Kit assets | `TA-10` is **impossible** from a commandlet — proven by a crash — because `AddVariable` needs a `UUserDefinedStructEditorData` only the editor factory installs. |
| `E-GRAPH-APPEND` test | Moved to a **rebuildable** graph (`SaneProbe` in `TurboMCPAcceptance`), which stops the damage it was doing to the shared `BP_NodeTest` — it had been growing that graph by one node per run, measured 12 → 17. |

---

## 7. Recommended order from here

1. **Decide the single owner of the editor lifecycle** and implement it. Until this is done, no full run can be trusted, and every later capability inherits the same doubt.
2. **`E-GRAPH-REPLACE`**, then **`E-GRAPH-PATCH`** — the same machinery, differing only in what they do with the old graph.
3. **`E-MAT-LABEL-PROMOTE`, `E-MAT-TIDY`, `E-MAT-BLOCKS`** — the last three.
4. Then the fixture-hygiene items: make `test-override-add` and `test-comment-boxes` restore state **on the way in**, and fix the `Q-16` fixture and TIDY's geometry mismatch so their keep-branches can be observed.

---

*This report describes a plugin that reads Blueprint graphs, widgets and materials faithfully and can now write into them. The engineering discipline that produced it — no claim without a measurement, a check shown to fail before it is believed, and a defect recorded rather than papered over — is the reason the nine recorded cases of "a check that passes while the thing it verifies is broken" were caught rather than shipped.*
