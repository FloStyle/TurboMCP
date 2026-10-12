# Roadmap

Build in this order. Each phase ends when its tests in `06_ACCEPTANCE.md`
pass and are recorded. Start each phase only when the one before it is
done, except where noted.

**The compilation goal is 5.8.3** (`D-38`). That is the engine branch actually built, and it is the only
engine this project has ever been built or measured on, so every engine fact in `07_ENGINE_NOTES.md` comes
from it.

---

## 1. Where the project actually is

This table is the state of the plan, not the plan. `Spec/WORKING_PLAN.md`
is the live version of it and is the file a round updates.

| Phase | Capabilities | State |
|---|---|---|
| 0. Base | Plugin, module, toolset registration, shared formats, panel, test kit | done |
| 1. Agent help | R-TOOL-INDEX, R-TOOL-SCHEMA, R-ASSET-SEARCH, R-ASSET-DESCRIBE, R-TYPE-FIND, R-DIRTY-LIST, E-COMPILE, E-SAVE | done |
| 2. Blueprint reading | R-BP-LOAD, R-GRAPH-LOAD, R-REFS, R-OVERRIDES, R-NODE-RESOLVE, R-NODE-FIND, R-DATATABLE, R-COMMENT-LIST | done |
| 3. Looking | L-LAYOUT-REPORT, L-CAPTURE, L-WIDGET-PREVIEW | done for Blueprint graphs; framing and editors incomplete (`18`) |
| 4. Structure | P/E-VARS-ADD, P/E-ENUM-ADD, E-WIDGETS-AS-VARS, P/E-OVERRIDE-ADD, E-FUNCTION-ADD/REMOVE, P/E-WIDGET-RETARGET | done |
| 5. Call log | E-CALL-LOG, R-CALL-LOG | done; R-CALL-COST not built (`16`) |
| 6. Layout | graph tidy, fix backward, knots, dead, comment boxes, local vars | done |
| 7. Graph writing | P-GRAPH-CHECK, APPEND, REPLACE, PATCH | done |
| 8. Materials | R-MAT-LABELS, R-MAT-LOAD, R-MAT-TRACE, R-MAT-BLOCKS, E-MAT-TIDY, E-MAT-LABEL-PROMOTE | done; E-MAT-BLOCKS and all material WRITING not built (`9`) |
| 9. Runtime | R-PLAY-CHECK, E-TRACE, R-TRACE | done; breakpoints and scripted scenarios are measurement-gated (`14`, `D-55`) |

## 2. The build order from here

The order is decided by one rule: **a capability that makes other
capabilities trustworthy comes first.** The verification harness is the
current bottleneck (`../TestRecord.md` sections 6z and 7a), and every capability added
inherits its doubt, so it is item 0 and not an afterthought.

| Step | Work | Why here |
|---|---|---|
| 0 | **One owner of the editor lifecycle.** See section 3 | Until this is done no full run can be trusted, and every later capability inherits the same doubt |
| 1 | **Measure, then decide the debug tools.** T-MEASURE-1, T-MEASURE-2, T-VCS-1's headless half | Two of the largest groups in this roadmap are blocked on three measurements, and each measurement is smaller than any tool it gates |
| 2 | **Vision: framing and editors** (`18`, T-LOOK-7 to T-LOOK-10) | Reuses the whole existing capture path. Unblocks every UI and level task, and an agent doing UI work is blind without it |
| 3 | **Materials: finish the phase** (`9`): E-MAT-BLOCKS, then the material reader's `full` detail, then P/E-MAT-APPEND, REPLACE, PATCH | The reader already prints every input, which is the hard precondition. This closes a group that is half built |
| 4 | **DataTable rows** (`12`, T-DATA-1) | The known commandlet route exists; it is the smallest useful write into an asset class that has none |
| 5 | **Widget authoring** (`11`, T-WIDGET-1 to T-WIDGET-8) | The widget tree text dialect has to exist first, and it is the last big format the writers need |
| 6 | **Graph structure** (`10`, T-STRUCT-9 to T-STRUCT-12): rename, collapse, copy, interfaces | These are what an agent asks for right after it can write a graph, and each one uses an editor API that already exists |
| 7 | **Assets and data** (`12`): structs, enum edit, cost, diff, new/rename/move/duplicate, curves | Rounds out the asset layer. `P/E-STRUCT-*` are editor-only and say so (`D-51`) |
| 8 | **Level and actors** (`13`, T-LEVEL-1 to T-LEVEL-5) | Nothing exists here at all, and it is a self-contained group |
| 9 | **Quality and session** (`15`, `16`): lint, session, batch, call cost | Each one makes the tools already built cheaper to use |
| 10 | **Source control** (`17`, T-VCS-1) | Before any automatic checkout is wired in, and the engine's own checkout has already failed once headless (`D-57`) |
| 11 | **Build portability** (`09_BUILD.md`, T-BUILD-1, T-BUILD-2) | Written once the dependency list has stopped moving |

Two things can run in parallel with any step above, because neither depends on
the code: the `07_ENGINE_NOTES.md` re-verification against 5.8.3, and the test
kit's new assets TA-14 to TA-21.

## 3. The bottleneck, stated as work rather than as a symptom

`../TestRecord.md` sections 6z and 7a records six defects of one shape and five hypotheses
each falsified by measurement. The one thing never reproduced outside the
harness is **the state of the editor at the moment the harness hands control
to a suite**. Two structural notes already hold and they are the shape of the
fix:

- the harness **owns the editor only between suites and never inside one**;
- **one owner of the editor lifecycle** is the fix, and the hybrid where a
  suite and the harness both drive the same editor is what keeps producing
  this.

So the work is: one component owns start, wait, health check, and stop; a
suite asks it for an editor or is given none; and no suite starts or stops an
editor itself. The suite that needs a specific state asks for that state by
name and the owner reports whether it could provide it.

This is item 0 because a harness that cannot be trusted makes every
capability below it unverifiable, and `P1`'s rule about an empty result
applies to a test run: a run whose failures move between invocations is a run
whose zero means nothing.

## 4. Questions this work will raise

The spec's own rule is that a point left open is asked, not guessed
(`00_START_HERE.md` section 3). These are the ones already visible, and they
are recorded in `../QUESTIONS.md`:

| Question | Blocks |
|---|---|
| Q-1: is 5.8.3 the target? | `D-38` states it is; the brief's line and the engine notes' markers follow from the answer |
| Q-19: may a new push-down writer action exist (`P/E-GRAPH-PUSH`)? | Graph writing, only if PATCH proves insufficient |
| Q-20: which source control provider must be supported first? | `E-VCS-CHECKOUT`, and therefore every automatic checkout |
| Q-21: how deep must the asset diff go? | `P-ASSET-DIFF` |
| Q-22: is a pausing tool acceptable if the server does not answer? | The whole breakpoint group |
