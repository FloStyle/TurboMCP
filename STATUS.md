# TurboMCP — status report

**Measured on `main` = `origin/main` = `1542e95`.** Every number here was read off the tree, not recalled.

---

## 1. The headline

| | |
|---|---|
| **Capabilities implemented** | **48 of 50** |
| **Tools shipped** | **70** AICallable `UFUNCTION`s, in 4 toolsets |
| **Reading (`R-`)** | **21 of 21 — COMPLETE** |
| **Vision (`L-`)** | **3 of 3 — COMPLETE** |
| **Preview (`P-`)** | **4 of 4 — COMPLETE** |
| **Edit (`E-`)** | **21 of 23** |
| Test suites | 36 files · **full run `42/42`, 0 failed, 0 blocked** |
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

Five capabilities, all writes:

| Capability | Phase | What it needs |
|---|---|---|
| ~~`E-GRAPH-REPLACE`~~ | 7 | **DONE** - writes into the existing entry and result nodes, never adds a second entry, removes every node the new text does not describe and LISTS what it removed. The first writer that can destroy, and the suite that uses it restores its fixture on the way IN and OUT |
| ~~`E-GRAPH-PATCH`~~ | 7 | **DONE** - matches by node ID and applies only the differences. Every other node id is proven to survive, PARSED from the reader rather than trusted from the tool |
| `E-MAT-LABEL-PROMOTE` | 8 | Material |
| `E-MAT-TIDY` | 8 | Material |
| `E-MAT-BLOCKS` | 8 | Material |

**`E-GRAPH-REPLACE` and `E-GRAPH-PATCH` share everything already built** — the lexer, the checker gate, the factory, the `Plan`/`Apply` split, the linking, and the round trip. They differ from `APPEND` only in what they do with the **old** graph, so they are the cheapest remaining work and the largest in value.

---

## 4. The blocker, stated plainly

**The capability work is nearly complete. The verification harness is now the bottleneck.**

A full run reports **`31 of 39`**, and the failures are **not** tool defects:

- every failing suite **passes in isolation**;
- the **names change between runs**;
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

**So the rebuild is not the defect in any shape reproducible from a shell or from Python.** What remains is the one thing not yet reproduced outside the harness: **the state of the editor at the moment the harness hands control to the suite.** That is the next measurement, and it is narrow.

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
