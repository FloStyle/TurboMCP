# Working plan

**This file is a working document. It is expected to change every round.**
It is not normative: what is DECIDED lives in `05_DECISIONS.md`, what each
capability must do lives in `03_TOOLS.md`, and what counts as done lives in
`06_ACCEPTANCE.md` (`D-60`).

It exists so that whoever drives the work — a person or an agent with
sub-agents — can see, in one place: what is built, what is next, which test
proves it, and which spec section to read before starting.

---

## 1. How to use this file

1. **Pick up the first row whose state is `todo`** in the order of section 3.
   The order is not arbitrary; it is `08_ROADMAP.md` section 2, and its first
   rule is that the harness comes before new capabilities.
2. **Read the two spec references on the row before writing code.** The
   `Spec` column names the capability; the `Decisions` column names the
   decisions that constrain it. A capability built without reading the
   decisions gets rebuilt.
3. **The row is not done when the code compiles.** It is done when its test
   row in `06_ACCEPTANCE.md` passes ON THE KIT ASSET, and its measured values
   are recorded in `../TestRecord.md` with the date.
4. **Record what was measured, not what was intended.** `../STATUS.md` section 5
   lists nine recorded cases of "a check that passes while the thing it
   verifies is broken". The habit that caught them is the one this file
   depends on: a claim without a measurement is not a result.
5. **A capability that cannot be built as specified does not become a
   smaller capability silently.** It becomes a question in `../QUESTIONS.md`,
   or a refusal in its own tool description, or a decision that it is not
   built.

### 1.1 States

| State | Means |
|---|---|
| `done` | Implemented, its test passes, its values are recorded in `../TestRecord.md` |
| `built` | Implemented and passing in isolation, but not yet covered by the full run |
| `todo` | Not started |
| `gated` | Blocked on a MEASUREMENT, not on effort. The measurement is its own row |
| `open` | Specified, but a question in `../QUESTIONS.md` changes its shape |

## 2. What is already delivered

Measured on the tree, not recalled. The count is the number of
`meta = (AICallable)` functions registered, in four toolsets.

| Toolset | Tools | Notes |
|---|---|---|
| `UTurboMCPReadToolset` | 24 | |
| `UTurboMCPLookToolset` | 4 | one of them, `LookTurboMCPToolset`, is a placeholder that reports which editor windows exist |
| `UTurboMCPPreviewToolset` | 17 | one of them, `PreviewTurboMCPToolset`, is a placeholder that measures this plugin's own help size |
| `UTurboMCPEditToolset` | 27 | includes `ReadTurboMCPEditQueue`, which is a read that lives here because it reports whether a write is possible |
| **Total** | **72** | counted from the `static FString` declarations in `Public/Tools/TurboMCPToolsets.h` |

`STATUS.md` at its stated commit says "72 AICallable `UFUNCTION`s", and that
matches what this count finds in the tree. An earlier version of `STATUS.md`
said 70; that one is superseded. What IS stale in `STATUS.md` is the
**capability** table, which is a nine-section reading of a spec that now has
twenty sections, and `STATUS.md` says so itself in its section 5 ("the
project's own capability audit was wrong for four rounds"). Both corrections
are on the list in section 6 below.

The first delivery is complete for phases 0 to 8 of `08_ROADMAP.md` except
the gaps this plan lists below.

## 3. The capability plan

Ordered by `08_ROADMAP.md` section 2. `Test` names the acceptance rows;
`Spec` names the capability in `03_TOOLS.md`.

### Step 0 — the harness, before anything else

| # | Work | Spec | Decisions | Test | State |
|---|---|---|---|---|---|
| 0.1 | **One owner of the editor lifecycle.** Start, wait, health check, stop, in one place; a suite asks it for an editor and never starts or stops one itself | `08_ROADMAP.md` section 3 | — | the full run stops producing failures whose names move | todo |
| 0.2 | Make `test-override-add` and `test-comment-boxes` restore their fixture **on the way in** | `../STATUS.md` section 7 | `P8` | recorded in `../TestRecord.md` | todo |
| 0.3 | Re-verify every fact in `07_ENGINE_NOTES.md` sections 1 to 4 and 6 against 5.8.3 and change its marker | `07_ENGINE_NOTES.md` | `D-38`, `D-40` | each marker says what was run | todo |

### Step 1 — measurements that gate whole groups

| # | Work | Spec | Decisions | Test | State |
|---|---|---|---|---|---|
| 1.1 | **Does the MCP server answer while the editor is stopped at a Blueprint breakpoint?** Record one of the three outcomes with its evidence | `03_TOOLS.md` section 14 | `D-36`, `D-54`, `D-55` | T-MEASURE-1 | todo |
| 1.2 | **Can an automation test be run from a tool call without blocking the game thread?** | `03_TOOLS.md` `R-AUTOMATION-RUN` | `D-54` | T-MEASURE-2 | todo |
| 1.3 | **What does a source control provider do headless?** Perforce, git, and off are three different answers | `03_TOOLS.md` section 17 | `D-57` | T-VCS-1 | todo |
| 1.4 | **Can a Niagara system and an Anim Blueprint be captured headless?** A yes or no per editor kind | `03_TOOLS.md` section 18 | `D-41` | T-LOOK-8 | todo |
| 1.5 | Build the new kit assets TA-14 to TA-21 by hand | `06_ACCEPTANCE.md` section 1 | `D-51` | baseline numbers recorded | todo |

### Step 2 — vision

| # | Work | Spec | Decisions | Test | State |
|---|---|---|---|---|---|
| 2.1 | Add `selection`, `neighbours`, `box-neighbours`, `screen` to `L-CAPTURE` | `L-CAPTURE` framing table | `D-41`, `D-42` | T-LOOK-7 | todo |
| 2.2 | `L-CAPTURE-ASSET`: material, instance, function, widget designer, DataTable, Anim Blueprint, Anim Sequence, Niagara; every kind either supported or refused BY NAME | `L-CAPTURE-ASSET` | `D-41`, `D-06` | T-LOOK-8 | todo |
| 2.3 | `L-CAPTURE-LEVEL`: viewport, selection, one actor, and `pie` which refuses without a running session | `L-CAPTURE-LEVEL` | `D-41` | T-LOOK-9 | todo |
| 2.4 | `P-IMAGE-COMPARE`: difference count, bounding box, difference PNG, refusal on two different sizes | `P-IMAGE-COMPARE` | `D-06` | T-LOOK-10 | todo |

### Step 3 — materials, end to end

| # | Work | Spec | Decisions | Test | State |
|---|---|---|---|---|---|
| 3.1 | `P/E-MAT-BLOCKS`, the last phase 8 capability | `P/E-MAT-BLOCKS` | `D-17`, `D-44` | T-MAT-8 | todo |
| 3.2 | Material reader at `full` detail: node type ids, pin indexes, function inputs distinguished from parameters | `04_FORMATS.md` section 2.5 | `D-14`, `D-15` | T-WRITE-6 | todo |
| 3.3 | `P-MAT-CHECK`, then `P/E-MAT-APPEND` | `03_TOOLS.md` section 9 | `D-28`, `D-29`, `D-30` | T-WRITE-6 | todo |
| 3.4 | `P/E-MAT-REPLACE`, then `P/E-MAT-PATCH` | `03_TOOLS.md` section 9 | `D-28`, `D-45` | T-WRITE-6 | todo |
| 3.5 | `R-MAT-INST-LOAD` and `P/E-MAT-INST-SET` | `03_TOOLS.md` section 9 | `D-43` | T-MAT-10, T-MAT-11 | todo |
| 3.6 | `R-MAT-ARGS` and `P/E-MAT-ARGS-SET`, with the index shift stated and the removal refused by default | `03_TOOLS.md` section 9 | `D-44` | T-MAT-12, T-MAT-13 | todo |

### Step 4 — data table rows

| # | Work | Spec | Decisions | Test | State |
|---|---|---|---|---|---|
| 4.1 | `P/E-DATATABLE-ROWS`: add, change and remove rows, values through the row struct's own text import | `P/E-DATATABLE-ROWS` | `P4`, `D-45` | T-DATA-1 | todo |

### Step 5 — widget authoring

| # | Work | Spec | Decisions | Test | State |
|---|---|---|---|---|---|
| 5.1 | The widget tree text dialect, reader and lexer, sharing the graph text lexer | `04_FORMATS.md` section 8 | `D-15`, `D-48` | T-WIDGET-4 | todo |
| 5.2 | `P/E-WIDGET-TREE` with `append`, `replace`, `patch`; a rename in the text is REFUSED | `P/E-WIDGET-TREE` | `D-45`, `D-48`, `D-49` | T-WIDGET-1 to T-WIDGET-4 | todo |
| 5.3 | `P/E-WIDGET-SET`: widget property, slot layout and style, each named, plus the rename with its references | `P/E-WIDGET-SET` | `D-49`, `D-50` | T-WIDGET-5 | todo |
| 5.4 | `R-WIDGET-ANIM` and `P/E-WIDGET-ANIM`, with a broken track named rather than dropped | `03_TOOLS.md` section 11 | `P1`, `D-05` | T-WIDGET-6, T-WIDGET-7 | todo |
| 5.5 | `P/E-WIDGET-BIND`, reusing the existing single-binding code path | `P/E-WIDGET-BIND` | `D-02` | T-WIDGET-8 | todo |

### Step 6 — graph structure

| # | Work | Spec | Decisions | Test | State |
|---|---|---|---|---|---|
| 6.1 | `E-GRAPH-RENAME`, with per-kind reference counts and a rollback | `E-GRAPH-RENAME` | `D-46`, `D-45` | T-STRUCT-9 | todo |
| 6.2 | `E-GRAPH-COLLAPSE`, boundary computed from the crossing links | `E-GRAPH-COLLAPSE` | `D-47` | T-STRUCT-10 | todo |
| 6.3 | `E-GRAPH-COPY` between graphs and Blueprints | `E-GRAPH-COPY` | `D-47` | T-STRUCT-11 | todo |
| 6.4 | `P/E-INTERFACE` | `P/E-INTERFACE` | `P2`, `P8` | T-STRUCT-12 | todo |
| 6.5 | **Open question:** is a push-down writer action worth having at all? | `../QUESTIONS.md` Q-19 | `D-28` | — | open |
| 6.6 | Enumerate and test every node family of `03_TOOLS.md` section 8's table, one at a time: promotable operators, split struct pins, delegates, macros, timelines, latent nodes | `03_TOOLS.md` section 8 | `P1` | one recorded row per family | todo |

### Step 7 — assets and data

| # | Work | Spec | Decisions | Test | State |
|---|---|---|---|---|---|
| 7.1 | `P/E-STRUCT-NEW` and `P/E-STRUCT-FIELDS`, **editor-only**, and saying so | `03_TOOLS.md` section 12 | `D-51` | T-DATA-2, T-DATA-3 | todo |
| 7.2 | `P/E-ENUM-EDIT`, with reordering reported as affecting saved data | `P/E-ENUM-EDIT` | `D-45` | T-DATA-4 | todo |
| 7.3 | `R-CURVE-LOAD` and `P/E-CURVE-SET` | `03_TOOLS.md` section 12 | `P1` | T-DATA-5, T-DATA-6 | todo |
| 7.4 | `R-ASSET-COST`, split by provenance | `R-ASSET-COST` | `D-52` | T-ASSET-3 | todo |
| 7.5 | `P-ASSET-DIFF`, naming what it did not compare | `P-ASSET-DIFF` | `D-53` | T-ASSET-4 | open (Q-21) |
| 7.6 | `E-ASSET-NEW`, `E-ASSET-RENAME`, `E-ASSET-MOVE`, `E-ASSET-DUPLICATE`, with redirectors fixed and an open tab refusing | `03_TOOLS.md` section 12 | `P8`, `D-45` | T-ASSET-5, T-ASSET-6 | todo |

### Step 8 — level and actors

| # | Work | Spec | Decisions | Test | State |
|---|---|---|---|---|---|
| 8.1 | `R-LEVEL-LOAD` | `R-LEVEL-LOAD` | `P1`, `P6` | T-LEVEL-1 | todo |
| 8.2 | `R-ACTOR-LOAD`, with the component hierarchy and inherited versus instance | `R-ACTOR-LOAD` | `D-05` | T-LEVEL-2 | todo |
| 8.3 | `E-ACTOR-PLACE` | `E-ACTOR-PLACE` | `P2`, `P5` | T-LEVEL-3 | todo |
| 8.4 | `P/E-ACTOR-SET` | `P/E-ACTOR-SET` | `P4`, `D-02` | T-LEVEL-4 | todo |
| 8.5 | `E-COMPONENTS`, with its scope stated in words and an instance change refused | `E-COMPONENTS` | `P8` | T-LEVEL-5 | todo |

### Step 9 — quality, session and cost

| # | Work | Spec | Decisions | Test | State |
|---|---|---|---|---|---|
| 9.1 | `R-BP-LINT`, every check named, each reporting how many things it looked at | `R-BP-LINT` | `P1`, `D-05` | T-QUAL-1 | todo |
| 9.2 | `E-SESSION` and `R-EDIT-SESSION`, bounded, and saying when it ended itself | `E-SESSION` | `D-56` | T-QUAL-2 | todo |
| 9.3 | `P/E-ASSET-BATCH` as `E-SESSION` plus a list, one transaction | `P/E-ASSET-BATCH` | `D-56` | T-QUAL-3 | todo |
| 9.4 | `R-CALL-COST` | `R-CALL-COST` | `P1`, `P7` | T-HELP-6 | todo |

### Step 10 — source control

| # | Work | Spec | Decisions | Test | State |
|---|---|---|---|---|---|
| 10.1 | `E-VCS-CHECKOUT` through `ISourceControlModule`, with the headless behaviour measured and recorded | `E-VCS-CHECKOUT` | `D-57`, `P8` | T-VCS-1 | gated on 1.3 |
| 10.2 | Wire an automatic checkout into the write tools | `E-VCS-CHECKOUT` | `D-57` | T-VCS-1 | gated on 10.1 |

### Step 11 — build portability

| # | Work | Spec | Decisions | Test | State |
|---|---|---|---|---|---|
| 11.1 | Finish `09_BUILD.md` once the dependency list stops moving | `09_BUILD.md` | `D-39`, `D-40` | T-BUILD-1, T-BUILD-2 | todo |

### Gated on step 1, then buildable as specified

| # | Work | Spec | Decisions | Test | State |
|---|---|---|---|---|---|
| G.1 | `E-BREAKPOINT`, `R-BREAKPOINT` | `03_TOOLS.md` section 14 | `D-55`, `P8` | T-DBG-3, T-DBG-4 | gated on 1.1 |
| G.2 | `R-DEBUG-STATE`, `E-DEBUG-CONTROL` | `03_TOOLS.md` section 14 | `D-54`, `D-55`, `D-02` | T-DBG-5, T-DBG-6 | gated on 1.1 |
| G.3 | `P/E-PIE-SCENARIO` | `P/E-PIE-SCENARIO` | `D-54`, `D-58` | T-DBG-7 | gated on 1.1 |
| G.4 | `R-AUTOMATION-RUN` | `R-AUTOMATION-RUN` | `D-54` | T-DBG-8 | gated on 1.2 |

**If step 1's measurements say no**, G.1 to G.4 are NOT built, and the refusal
goes into the description of the tools an agent would reach for instead
(`E-TRACE`, `R-TRACE`, `R-PLAY-CHECK`). That is a decision, not a failure.

## 4. Questions this plan depends on

Recorded in `../QUESTIONS.md`. A row whose state is `open` should not be
started.

| Question | Changes | State |
|---|---|---|
| Q-1: is 5.8.3 the target? | the whole engine-notes re-verification | stated by `D-38`; the question stays open for the owner to confirm |
| Q-19: a push-down writer action | whether `P/E-GRAPH-PUSH` exists at all | open |
| Q-20: which source control provider comes first | `E-VCS-CHECKOUT`'s provider path | open |
| Q-21: how deep the asset diff goes | `P-ASSET-DIFF`'s scope | open |
| Q-22: is a pausing tool acceptable if the server does not answer | the whole G group | open |

## 5. Rules this plan will be tempted to break

Written down because each one has already been broken once in this project.

- **Do not count a capability as done because its code compiles or its tool
  appears.** `00_START_HERE.md` section 5 lists the five things done means.
- **Do not trust a plan you made in an earlier call.** Every writer
  re-resolves its targets by key at apply time (`D-45`).
- **Do not let a check pass while the thing it verifies is broken.** If a
  check cannot fail, it is not a check: show it failing before believing it.
- **Do not fix a fixture to make a test pass.** `../STATUS.md` section 5 records
  a workstream that diagnosed a rule defect by arithmetic instead of faking a
  fixture, and it was right.
- **Do not let one capability's test consume a shared asset.** `E-OVERRIDE-REMOVE`
  exists because a suite ran a shared Blueprint out of free events.
- **Do not add a capability whose failure mode is "the agent waits forever"**
  without the measurement that proves it does not (`D-54`, `D-55`).

## 6. Documentation to bring current, in the same work as the capability

The project's own rule is that when a tool changes its docs change in the same
piece of work (`00_START_HERE.md` section 4). These are stale or about to be:

| File | What is stale | When |
|---|---|---|
| `../STATUS.md` | "70 AICallable UFUNCTIONs" in an earlier revision against 72 in the tree, and a capability table that is a nine-section reading of a twenty-section spec | with step 0 |
| `../STATUS.md` | section 4's blocker, once step 0.1 lands | with step 0.1 |
| `Spec/07_ENGINE_NOTES.md` | every section 1 to 4 and 6 fact carries a `[5.8.1]` marker | with step 0.3 |
| `README.md`, `DOCS.md` | tool counts and the tool list | with each capability |
| `../TestRecord.md` | one row per new test with its measured values and its date | with each capability |
| `../QUESTIONS.md` | Q-19 to Q-22 are added with this plan and need answers | on the owner's next read |
