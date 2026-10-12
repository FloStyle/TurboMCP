# Audit patterns

**Every agent that audits a file in this repository uses this list.** A finding is a defect only if it
matches a pattern here. Use the pattern IDs in your report.

**File groups:** `SRC` is `Source/**`. `TOOL` is `Tools/**`. `DOC` is `**/*.md`. `CFG` is the plugin
descriptor, the project descriptor, `Config/`, `.gitignore` and `.gitattributes`.

**How to write a finding.** Give the pattern ID, the file, the line, and one sentence that names the defect.
State which side is correct when a comment and the code disagree. Do not guess about code you did not read.
Write `NOT READ` instead.

---

## A. CODE PATTERNS

### `C-P1` — A value that no measurement produced

**Test.** The code prints a number or a true/false value when it measured nothing. The value `0` looks like a
measured zero. The value `false` looks like a measured negative. The code must print `NOT ASKED` instead.

**Real failures.** `TurboMCPGraphFixBackward.cpp:1607` prints `"\n    boxes: %d seen, 0 moved, 0 resized"`.
The text `0 moved, 0 resized` is part of the format string, and the code never measured those two numbers.
`TurboMCPKitCommandlet.cpp:1131` calculates `HasOrigin = Stacked->Nodes.Num() > 0`, which is true for every
graph that holds a node, and prints it as the measured claim `originNode=%s`, so the line can never show "no".

**Find it.** Search for a `%d` or `%s` that receives a literal. Search for a true/false value that comes from
`Num() > 0`. Search for the text `0 moved`. Search for a count behind a `? 1 :` test.

---

### `C-P2` — A silent fallback where the code owes a refusal

**Test.** The code guesses, estimates, or returns a smaller answer. The code must refuse and name the blocker.
The refusal must name the engine file and the line that decides the answer.

**Real failure.** The project states the rule in one sentence: "an invisible refusal reads as a capability." A
tool that returns the whole graph after a request for box 9 breaks `Spec/03_TOOLS.md:388-390`.

**Find it.** Read every `else` on a failure path. Read every default argument that hides a missing input. Read
every `?:` whose false branch invents an answer. Read every `return {}` and every `return ""` that gives no
reason.

---

### `C-P3` — A read tool that changes something

**Test.** A tool in `Read` or `Look` changes an asset, a graph, a selection, or the editor state. The test
also covers the creation of a `UObject` inside the asset package, even when the code destroys the object
immediately after.

**Real failure.** `TurboMCPGraphKnots.cpp:410` runs inside `Plan`, which is the Preview half. The line calls
`NewObject<UK2Node_Knot>(InGraph)` with the graph in the outer position, and it calls `AllocateDefaultPins()`.
The code destroys the probe at line 415. The comment at lines 404 to 409 still says that the measurement
"cannot change the node list".

**Find it.** Search for `NewObject`, `SpawnActor`, `AddNode`, `Modify()`, `MarkPackageDirty`, and every
selection setter. Limit the search to a function that starts with `ReadTurboMCP` or `LookTurboMCP`.

---

### `C-P4` — A difference between `Plan` and `Apply`

**Test.** One half of a Preview/Edit pair holds a guard and the other half does not. The two halves then
refuse different input. The check must live in `Plan`, and `Apply` must read the result of that check.

**Real failure.** `TurboMCPGraphTidy.cpp:1136` sets `bWouldApply = WireLengthAfter < WireLengthBefore ||
Force` inside `Plan`. Line 1219 shows that `Apply` never reads `bWouldApply`; only `RenderPreview` reads it, at
line 1165. Therefore an agent can call the Edit tool on a plan that the Preview said would roll back.

**Find it.** Take each `Preview` and `Edit` pair. Compare the guard clauses. Then search whether `Apply` reads
every field that `Plan` filled.

---

### `C-P5` — An input form that the tool rejects

**Test.** The tool accepts one spelling of an asset path and rejects the others. The principle `P5` requires
all forms.

**Real failure.** `Spec/02_PRINCIPLES.md:59` lists four path forms. `Spec/04_FORMATS.md:165` lists five.
`Spec/06_ACCEPTANCE.md:85` requires "All five path forms". The three documents disagree, and no test covers the
largest set.

**Find it.** Read each path resolver and list the forms it handles. Compare that list with `P5`.

---

### `C-P6` — A value that the code matches instead of parses

**Test.** The code uses `Contains()` or `StartsWith()` where it must compare a value. The test also covers a
number that the code ties to the contents of one asset.

**Real failures.** `TurboMCPStruct.cpp:467` runs `Current.ToLower().Contains(Lower) ||
Lower.Contains(Current.ToLower())`, so a request for `int` returns "already correct" on a field that compiles
to `int64`. And `test-datatable.py` asserted that the readers start unloaded; the test passed once and failed
on every later run, because Blueprints stay loaded for the session, so the assertion described the test setup
and not the tool.

**Find it.** Read every `.Contains(` that takes a type, a name, or a value. Read every literal that the code
compares with the contents of an asset.

---

### `C-P7` — A result that does not lead to the next step

**Test.** The tool gives an answer and does not say what to do with it.

**Find it.** Read the output as an agent reads it. If no next call follows from the answer, the tool breaks
`P7`.

---

### `C-P8` — A log line inside a loop

**Test.** A `UE_LOG` call at Display or Warning level runs inside a loop over nodes, wires, or tool calls. A
graph with 900 nodes then fills the log and hides the summary.

**Find it.** Search `UE_LOG` inside the body of a `for` or a `while` loop in `SRC`.

---

### `C-P9` — Memory that the code never releases

**Test.** A `new` has no matching `delete` on every path. A return statement that runs early loses the
allocation. The code holds an owning raw pointer.

**Current state of the six core files.** Clean. They hold no `new`, no `delete`, no `MakeShareable`, and no
owning raw pointer. Every `NewObject` probe is destroyed on every return path.

**Find it.** Search for `new `, `MakeShareable`, and `TUniquePtr`. Then trace each early `return`.

---

### `C-P10` — A destructive path with no rollback, or with an unchecked rollback

**Test.** The code changes an asset, a graph, an index, or a file and cannot undo the change. The test also
covers an undo that nobody checks.

**Real failures.** `TurboMCPGraphKnots.cpp:1294` runs after `Schema->BreakSinglePinLink` at line 1273 has
already broken the original wire, and the restore call `TryCreateConnection(...)` discards its return value,
so a failed restore leaves the wire broken while the report only says "the schema refused a connection".
`TurboMCPKitCommandlet.cpp:342` deletes the existing package file before it saves, and when `SavePackage`
fails at lines 362 to 366 the code returns with the original file already gone and no restore.
`TurboMCPStruct.cpp:949` returns after a partial field failure and leaves the created asset in place, still
holding the engine's own boolean field and fields named `MemberVar_N`, because the renames at lines 989 to 996
are never reached.

**Find it.** List every write to an asset, a graph, an index, or a file. For each write, answer two questions.
Does an undo exist? Does the code check the undo against what changed?

---

### `C-P11` — A rollback that the code confirms with a count

**Test.** The code verifies an undo with `Num()` or with a total, so a half-restored state passes the check.

**Real failure.** `TurboMCPGraphKnots.cpp:1373` re-links each original wire with an unchecked
`TryCreateConnection` and then confirms the rollback with a crossing count at lines 1379 to 1395. It does not
compare the link set, so a half-restored route can still report RESTORED.

**Find it.** Read every rollback check. Does it compare the set of links or fields before and after, or a
derived number?

---

### `C-P12` — A pointer without a guard in a headless path

**Test.** The code dereferences the result of a factory call with no null check. The factories are
`MakeFunction`, `CreateNewGraph`, `Cast`, and `FindObject`. A commandlet has no editor and no crash handler in
the path.

**Real failure.** Four sites in `TurboMCPKitCommandlet.cpp` dereference a factory result. Line 863 uses
`Hand->Nodes`. Lines 1115 and 1131 use `Stacked->Nodes`. Line 103 lets `Place` use `InGraph` and `InNode` with
no check. `MakeFunction` returns null when `CreateNewGraph` fails at lines 178 to 181. Line 1127 already
computes that null check and uses the answer only to set a flag.

**Find it.** Read every `->` on a pointer that came from a factory. Limit the search to a file that a
commandlet can reach.

---

## B. SECURITY AND LEAK PATTERNS

### `S1` — A secret in the repository

**Targets.** A private key block. A GitHub token that starts with `ghp_`, `gho_`, `ghu_`, `ghs_`, or
`github_pat_`. An OpenAI key that starts with `sk-`. An AWS key that starts with `AKIA`. A Slack token that
starts with `xox`. A Google key that starts with `AIza`. A JWT that starts with `eyJ`. An Azure key after
`AccountKey=`. A value of eight characters or more after `password=` or `api_key=`. A non-empty
`SecurityToken=`.

**Real failure.** `TestProject/Config/DefaultEngine.ini` held a generated `SecurityToken`. The value was
removed from the file and from the history.

---

### `S2` — Personal information

**Targets.** A private email address. A home path such as `Users\<name>` or `/Users/<name>`. A civil name. A
Perforce host. An Android token.

**Real failure.** The repository held a civil name on 193 files and a Perforce host in the history. Both are
gone. The rule now reads: keep the pseudonym, remove the address.

**Note.** Use word boundaries. A search for `Cline` once matched the word "declined", and a search for
`Cursor` once matched a local variable.

---

### `S3` — A path that belongs to one machine

**Targets.** A `C:\` path inside a suite. A bare drive letter. `AppData`. A desktop name such as
`DESKTOP-<id>`. `Program Files`.

**Real failure.** Six suites held `C:\ExampleUnrealengine\Engine\Binaries\Win64` as a literal. They now call
`tmcp_paths.find_engine()`. The project holds zero such literals outside the resolvers, where the attempt to
open those paths is the job.

---

### `S4` — A binary file in the repository

**Targets.** `.exe`, `.dll`, `.so`, `.dylib`, `.msi`. No reviewer and no scanner can verify a binary file.

**Real failure.** `[archived item]` was a committed binary whose job was to read the
Windows Credential Manager. The file is gone, and `*.exe` plus `GitCredentialHelper*` are in `.gitignore`.

---

### `S5` — A generated fixture that the repository tracks

**Test.** A script rebuilds an asset, and git tracks that asset.

**Real failure.** Five files held this defect. Four arrived through one `git add -A` command. The fifth,
`TA22_VcsWrite_R31976.uasset`, was committed in round 205 and made `test-vcs-write` fail from that day on five
of its 62 checks, invisibly, because the documented figure `passed 58` came from an earlier run.

---

### `S6` — A real host, port, or address

**Targets.** An IP address outside a placeholder. A private URL. A credential entry named in prose.

**Real failure.** `Tools/credential/README.md` listed the stored credentials of the machine by their exact
names. The list was a map of one machine's secrets.

---

### `S7` — A `.gitignore` rule that covers nothing

**Test.** The pattern was written against the wrong root directory, so git ignores no file.

**Real failure.** `TestProject/.gitignore` listed `Tests/Kit/TA14_...`. Git reads that file relative to
`TestProject/`, so the correct path is `Content/Tests/Kit/...`. The command `git check-ignore` returned
nothing and the files appeared as untracked. The guard was written, looked correct, and covered nothing.

---

## C. NOISE AND DRIFT PATTERNS

### `N1` — Noise from a rewrite or from a clean room

**Targets.** `***REMOVED***`. `Claude`. `another plugin`. `separate plugin`. `earlier plugin`. `clean room`.
`previous plugin`.

**Real failure.** `***REMOVED***` appeared in 80 files after a failed history rewrite. The clean-room rule
also forbids the name of any other plugin.

---

### `N2` — A note about the document itself

**Targets.** `THIS FILE SAID`. `used to say`. `no longer says`. `in an earlier revision of this`. A round log
in capital letters about the writing process.

**Real failure.** `Spec/WORKING_PLAN.md:57` reads "THIS TABLE SAID 24 / 4 / 17 / 27 = 72, AND THOSE NUMBERS
WERE OLD". The owner asked for this form to be removed.

---

### `N3` — Reasoning about time

**Targets.** `deadline`. `days left`. `as time allows`. `time budget`. Text about the writing process.

**Real failure.** `PLAN.md:261` holds the phrase "as time allows" inside a section that gives the order of
work.

---

### `N4` — A version that drifted

**Test.** The text names an engine version that is not `5.8.3` **and presents it as the target, as the engine
built, or as the version a fact came from.**

*** NOT A DEFECT, and this distinction was added after the doc audit produced a false positive on it: a
document may name another version to say that the resolver must REJECT it.*** `README.md:134` says the machine
also holds an Epic launcher install of `5.6`, and that the resolver prefers `5.8.3` because the version
decides. **That sentence is correct and it is the only place the reader learns the resolver has to choose.** A
version named as a danger, or as a second install to skip, is a fact about the environment and not drift.

**Real failure.** The string `5.8.1` appeared on 12 lines across six documents, and the markers `[5.8.1]`
covered most of the engine facts in `Spec/07_ENGINE_NOTES.md` — **all presented as the version a fact came
from.** They are gone.

**How to tell the two apart.** Read what the sentence does with the number. If the number is the subject of
the claim, it is drift. If the number is an object the text tells you to avoid, it is not.

---

### `N5` — A count in prose that no longer agrees with the tree

**Test.** A document states a number instead of reading that number from a command.

**Real failures.** Three documents stated the plan as `31 todo · 15 done`; the plan is `30 todo · 16 done`.
`PLAN.md:89` says "21 tools. Six acceptance suites pass" against 93 tools and 58 suites. `PLAN.md:80` says "13
tools".

---

## D. COHERENCE PATTERNS

### `C1` — A reference that leads nowhere

**Test.** The text names a file that does not exist, or a section that the target file does not have.

**Real failure.** Four `Spec/` files named `STATUS.md` sections that a rewrite had moved. `README.md` pointed
at a file that git ignores.

**Find it.** Run `python Tools/checks/check-refs.py`.

---

### `C2` — A line citation that landed on the wrong line

**Test.** A citation of the form `FILE.md:NNN` points at a table separator, a heading, or an empty line.

**Real failure.** Ten of 52 citations had moved. `TurboMCPKitCommandlet.cpp:1899` cited `WORKING_PLAN.md:145`
for row 7.3; line 145 is row 7.1. A citation that points at the neighbouring row is worse than a citation that
points at nothing, because it looks correct.

**Find it.** Run `python Tools/checks/check-line-cites.py`.

---

### `C3` — A comment that contradicts the code beside it

**Test.** A doc comment describes code that is not there. **The report must state which of the two is
correct.**

**Real failure.** The audit found 12 instances, and in 8 of them the comment is correct and the code is wrong.
`TurboMCPGraphTidy.cpp:1238` says "NOTHING IS WRITTEN UNTIL THE MEASURE IS IN", and lines 1266 to 1334 create
nodes and write every position before the measure at line 1338. `TurboMCPGraphFixBackward.cpp:1772` says "THE
ROLLBACK CHECK RE-MEASURES THE GRAPH AFTER THE UNDO", and `Apply` holds no undo.

**Note.** When the comment is correct, the finding also belongs to `C-P3`, `C-P10`, or `C-P1`.

---

### `C4` — Two documents that state different rules

**Real failures.** `PLAN.md:39` says the `arguments` field of `call_tool` is a JSON string, and `PLAN.md:238`
and `DOCS.md:404` say it is an object. `Spec/02_PRINCIPLES.md:59` lists four path forms and
`Spec/04_FORMATS.md:165` lists five.

---

### `C5` — A test with no condition that makes it fail

**Test.** A check states what it verifies and never states what breaks it. The rule is: name the input that
makes the check FAIL.

**Real failures.** 46 acceptance rows in `Spec/06_ACCEPTANCE.md` state only the expected output. And the
assertion in `test-datatable.py` about unloaded readers described the test setup, not the tool, so it passed
once and failed on every run after that.

**Find it.** Run `python Tools/audit-suite-quality.py`. It blocks on six finding classes and reports 0
candidates for the suites. Nobody has run the same audit on the acceptance table.

---

### `C6` — A capability with no tool, or a tool with no section

**Real failures.** 21 capabilities in `Spec/03_TOOLS.md` have no tool. And 10 published tool names appear in
`TOOLS.md` and have no section in the specification.

**Find it.** Run `python Tools/audit-tool-coverage.py`. It covers one direction only.

---

## E. FOUR PATTERNS THAT NO OTHER LIST HOLDS

### `X1` — A name that acts as a configuration value

**Test.** A tool discovers files by a name pattern, so a new file name changes the behaviour of the tool.

**Real failure.** `Tools/run-suites.ps1:226-228` discovers suites with `Get-ChildItem Tools -Filter '*.py'`
and keeps the names that match `^(test|check|audit)-`. Three documentation tools that start with `check-` were
taken as suites, and the count moved from 58 to 61 with no message.

---

### `X2` — A guard that checks part of the thing it verifies

**Test.** The guard validates one fragment of the target, and a reader then treats the guard as proof of the
whole target.

**Real failures.** `Tools/apply-license-header.ps1` decided that a file was already correct from the line
`SPDX-License-Identifier: MPL-2.0`, which is identical in every version of the notice. When the holder line
changed, the script reported `193 already correct, 0 replaced` while all 193 files held the old holder, and
its `-Check` mode passed. And the `.gitignore` rule held `tests/Kit/...` where the root is
`Content/Tests/Kit/...`, so the rule covered nothing.

**Find it.** For each guard, ask which part of the target the guard does not look at. Then change that part
and see whether the guard notices.

---

### `X3` — A count that the code does not derive

**Test.** A person writes a number by hand where a command can produce it.

**Real failures.** The plan was counted by a regular expression over the line before it was counted by field;
the figure was `31 · 15` and the truth was `30 · 16`. `TOOLS.md` was a hand-kept list of 93 names before it
became a generated file.

---

## F. THE FOUR GATES THAT ALREADY EXIST

| Tool | Pattern it proves | Current result |
|---|---|---|
| `Tools/checks/check-refs.py` | `C1` | exit 0, no dead reference |
| `Tools/checks/check-line-cites.py` | `C2` | 49 citations, 0 past the end of a file |
| `Tools/checks/count-plan.py` | `X3` | 48 rows, counted by field |
| `Tools/audit-suite-quality.py` | `C5` | six classes, blocks the run, 0 candidates |

---

## G. THE PATTERN THAT PRODUCES FALSE FINDINGS — `V1`

*** THIS ONE WAS ADDED AFTER TWENTY-FOUR FINDINGS WERE TESTED BY HAND AND FIVE WERE WRONG. ALL FIVE FAILURES
HAVE THE SAME MECHANISM, AND IT IS NOT THE REASONING. IT IS THE EVIDENCE. ***

### `V1` — A conclusion drawn from a fragment

**Test.** The finding states something about a WHOLE from evidence that covers a PART. The three shapes it takes
here, each measured:

    A TRUNCATED LINE READ AS THE WHOLE LINE
        A `Select-String` that cut `"...with arguments as a **JSO"` at 96 characters was read as a rule about a
        JSON string. *** THE FULL LINE SAYS "JSON object, not a string". THE FINDING WAS FALSE AND SO WAS MINE. ***

    A COUNT OVER A SUBSET PRINTED AS A COUNT OVER EVERYTHING
        "42 comments narrate the file's own past" - the audit had read 99 files of 186. *** THE UNAMBIGUOUS
        COUNT IS 81. ***  *Four separate counts in this audit were taken that way.*

    ONE OF TWO NAMED THINGS READ, THE OTHER ASSUMED
        "the tree holds three engine plugins and the documents say two" - *** THE TREE HOLDS THREE PLUGINS AND
        FOUR MODULES, AND ONE OF THE FOUR DOCUMENTS WAS ALREADY CORRECT. ***

**WHY IT IS NOT A CARELESSNESS PROBLEM.** In every case the auditor had a real observation. The error is the step
from the observation to the claim. *** A reader who finds two texts stated differently stops there, because the
third claim - that they CONFLICT - looks like the easy one. ***

**HOW TO REPORT WITHOUT IT.** State the evidence and the scope in the same sentence:

    "line 175 says X"                          ONE line, ONE claim, checkable by looking
    "42 sites in the 99 files read"            the scope is part of the number
    "A says X; B says Y; X and Y disagree"     three claims, so name the line for each of the first two

**HOW A VERIFIER TESTS IT.** Ask one question: *** does the evidence cover everything the claim covers? ***
If the answer is not visible in the finding, it is not a finding yet.

**WHY IT IS IN THIS FILE.** *The audit is the instrument, and an instrument that reports a fragment as a whole
is the same defect this whole project exists to find - **an invisible refusal reads as a capability**, and a
count over a subset reads as a count over everything.*

---

## H. TREES THAT ARE ALREADY KNOWN

**Do not spend time on these. They are recorded and they are queued for repair.**

| Pattern | File and line |
|---|---|
| `C-P1` | `TurboMCPGraphFixBackward.cpp:1607`, `:1793`; `TurboMCPKitCommandlet.cpp:1131` |
| `C-P10` | `TurboMCPGraphKnots.cpp:1294`, `:1373`; `TurboMCPKitCommandlet.cpp:342`; `TurboMCPStruct.cpp:949`, `:1674`, `:1662`; `TurboMCPGraphFixBackward.cpp:1804` |
| `C-P11` | `TurboMCPGraphKnots.cpp:1373` |
| `C-P12` | `TurboMCPKitCommandlet.cpp:863`, `:1115`, `:1131`, `:103` |
| `C-P3` | `TurboMCPGraphKnots.cpp:410` |
| `C-P4` | `TurboMCPGraphTidy.cpp:1136`, `:1090` |
| `C-P6` | `TurboMCPStruct.cpp:467` |
| `C-P2` | `TurboMCPKitCommandlet.cpp:1533`, `:1793`, `:1871` |
| `C3` | 12 sites, listed in the board card *"12 comments that contradict the code beside them"* |

---

## I. THE FINDING THIS AUDIT KEEPS MAKING — `X4`, AN IDEA THAT IS NOT WIRED IN

*** FIVE TIMES, AT FIVE SCALES, THE SAME DEFECT: A CONTRACT, A GUARD, A FIELD, A RULE OR A NOTE EXISTS IN ONE
PLACE AND IS CONSULTED IN NONE. ***

| # | Where | What exists and is not used |
|---|---|---|
| 1 | `TurboMCPTypes.cpp:116-119` | `KindOfType` returns `"function"` for a `UFunction`; `KnownKinds` at `:199` does not accept it, so no caller can ask for it |
| 2 | `TurboMCPNodeId.cpp:178-208` | `NormaliseKey` filters non-hex and zero-pads; 2 of its 4 callers use it, and `FindNodesByKey` at `:219` does not |
| 3 | `TurboMCPPanelToolRunner.h:175-178` | the header documents a `mutable` flag; the code at `.cpp:379` uses a `const_cast` |
| 4 | `Tools/make-tools-doc.py` | the emitted template requires every check to name its failing input; 9 of 52 suites use the phrase |
| 5 | `TurboMCPNodeType.h:140`, `:180` | `OutNote` is documented "NEVER EMPTY ON FAILURE", assigned 14 times in the `.cpp`, and read by no line in the repository |

**Test.** A name, a field, a parameter or a rule appears in a declaration or a document, and **no consumer
consults it**. The grep that finds it is on the SYMBOL, not on the file.

**Why it is not caught by reading one file.** Every one of the five is correct where it is written. The defect
only exists in the RELATION between two places, and a file-by-file read sees one of them.

**The two repairs, and one of them is always right.**

    WIRE IT    the consumer reads the thing, and the contract becomes true
    DELETE IT  the thing, its documentation and its assignments all go

*** What must not remain is a contract with no counterparty, because the next reader will trust it. ***

**Where to look for the next one.** Each was found by following a symbol rather than reading a file: take a
parameter name, a guard name or a field name, and count its occurrences across the whole tree. **Two files means
one writer and one declaration. Three or more means you have found the question.**
