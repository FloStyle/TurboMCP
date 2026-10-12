# TurboMCP — handover

**Version 1.0.0.** The working tree is clean. This file exists so the next reader can **VERIFY** rather than
believe: every claim below is followed by the command that establishes it.

---

## 0. Security — what was checked before publishing

| Check | Result |
|---|---|
| API keys, tokens, SSH keys, connection strings | **none.** Scanned for PEM private-key headers, classic and fine-grained GitHub tokens, OpenAI keys, AWS credentials, Slack tokens, Google keys, Bearer tokens, JWTs, storage-account keys and connection strings |
| `.env`, `.pem`, `.key`, `.pfx`, `id_rsa` files | **none** |
| Private URLs, internal hostnames, RFC1918 addresses | **none** |
| Customer data | **none.** The only assets are the test kit, which the commandlet generates |
| Private dependencies | **none.** `ModelContextProtocol`, `ToolsetRegistry` and `SlateInspectorToolset` are all engine plugins |
| Actions workflows, Pages, package registries | **none** |
| **The one finding** | ***a generated `SecurityToken` in `TestProject/Config/DefaultEngine.ini`*** |

**What that token was, and why it is gone.** The engine writes a **random** value into that key when a
project is created, and the repository's value was **not** the engine's own default — so it had been
**generated** rather than copied. A generated token is a secret. Nothing here uses it: **the Android plugin
is not listed in `TurboMCPTest.uproject`, the connection type is `USBOnly`, and the file server is never
started**, so the token guarded a server this project does not run.

**It was removed from two places, because one is not enough.** The file now reads `SecurityToken=` with the
key kept and the reason written above it — an empty value **disables** the server by the engine's own rule
— **and the value was purged from the history as well**, verified at **zero occurrences** across every tree
of every ref. Deleting it from the tip would not have been enough: an earlier commit still carried it, and
`git log -p` would have published it anyway.

### 0.1 Author identity — the pseudonym stays, the address does not

The commit history carried **1012 commits** under one personal e-mail address. **That address has been
replaced with `FloStyle@users.noreply.github.com`**, which is GitHub's own pseudonymous domain: the commits
remain attributable to one author, and the author's mailbox is not in them.

**The pseudonym is deliberately kept, and it appears in three places on purpose:**

| Where | What it says |
|---|---|
| **Every commit's author name** | **`FloStyleHome`** — unchanged by the rewrite. Only the address moved |
| **The copyright notice on every first-party C++ file** | **`Copyright (c) 2026 FloStyle`** — **193** files: 186 under `Source/` and 7 under `TestProject/Source/` |
| **`TurboMCP.uplugin`** | **`"CreatedBy": "FloStyle"`**, with `CreatedByURL` pointing at the GitHub account |

The attribution is **required by MPL-2.0** — Section 3.4 forbids removing a file's license notice — and it
is also what the owner asked for: **the name is public, the address is not.** The second identity in the
history, `TurboMCP Agent <turbomcp-agent-history@example.invalid>`, was **already anonymous** and was left exactly as it was; a
blanket rule that rewrote every address would have replaced an identity that needed no change.

### 0.2 White room

**This project is a white room: the specification describes itself and nothing else.** There is no
reference to any earlier, separate, previous or third-party plugin, and none to a named tool or agent.

The rules in `Spec/00_START_HERE.md` section 1 are stated **positively** — they say what to do rather than
what not to look at: **use only the specification, the engine source, public documentation and the answers
in `QUESTIONS.md`; describe every capability by what it must do, with its inputs, its outputs, its refusals
and a test that proves it; start from an empty workspace; choose every name here; and ask a question rather
than guess.**

> ***A rule that says "do not look at X" tells the reader that X exists, what it is, and that this
> specification was written in its shadow.***

**And the search for those references was done with word boundaries**, because a substring search produced
**28 false positives**: one tool's name appears inside the English word *declined* in three files, and
another inside a **local variable** in `TurboMCPCallCost.cpp`. *A search that cannot tell a tool from a loop
counter is the same defect as a regex that assumes a format.* **The patterns themselves are not written
here, because writing them would make this file match its own search** — which happened twice while it was
being prepared.

---

## 0.5 Which document answers which question

**Four documents describe this project's state and they are not interchangeable.** A reader who opens the
wrong one gets a different answer to what looks like the same question.

| You want | Read | What it is |
|---|---|---|
| **The state now, and how to verify it** | **this file** | The four commands that establish it, what is finished, **what is not**, and what I do not know |
| **The analysis: why, what is missing, what to do next** | [`REPORT.md`](REPORT.md) | The invariants that hold the 93 tools together, the phantom fixtures, the vacuity debt, the dependency projection |
| **The headline snapshot** | [`STATUS.md`](STATUS.md) | Counts and a capability table at the revision it was written |
| **The evidence** | [`TestRecord.md`](TestRecord.md) | 246 chronological sections; every claim above traces to one of them |

**`Spec/WORKING_PLAN.md` is the authoritative work list** — the only one of the five that has been audited
row by row against the tree.

---

## 1. The state, in four commands

```powershell
git rev-parse --short HEAD                                     # the revision this file describes
git status --porcelain                                         # clean apart from generated fixtures
python Tools\audit-tool-coverage.py                            # 93 AICallable, uncalled: (none)
python Tools\audit-suite-quality.py                            # 0 candidates — AND IT BLOCKS
pwsh -NoProfile -File Tools\run-suites.ps1                     # passed 58, failed 0, blocked 0, of 58
```

| | |
|---|---|
| **Tools** | **93** `AICallable`, in 4 toolsets — Read 30 · Look 4 · Preview 26 · Edit 33 |
| **Full run** | ***`passed 58, failed 0, blocked 0, of 58` — exit code 0*** |
| **Coverage audit** | `uncalled: (none)` — every declared tool is reached by at least one suite |
| **Suite-quality audit** | **BLOCKS the run** when a check cannot fail **or cannot pass** — currently 0 candidates |
| **Plan** | **31 `todo` · 15 `done`** of 48 — *counted **by field**, never by a regex over the line* |
| **Size** | C++ **61 620 lines** in **186** files · Spec **7 740 lines** in **17** · `TestRecord.md` **10 800 lines** |

---

## 2. What is solid, and **how** it is known

| | |
|---|---|
| ***The full run is green, exit 0*** | *not "Result: Succeeded" — **58 suites, 0 failures**, and it was impossible before the two fixes in section 4* |
| ***The suite-quality audit blocks*** | *proven in both directions: 0 candidates → PASS, exit 0; 1 candidate → FAIL, exit 1* |
| ***A foreign editor is detected*** | *proven on three cases — nothing running, another project's editor, the right one — `exit 3`* |
| ***The break test bites*** | *run five times; five times the check went red **with the false value visible**, and **four times** it found an additional defect* |
| ***The fixtures are reproducible*** | *`TA-17` was **deleted and rebuilt**, and the script reports `PASS`* |

---

## 3. The 31 remaining rows — and the three that cannot be done here

### Bounded by the environment this was built in

| Row | What is missing |
|---|---|
| **10.x** *(the remaining source-control writers)* | ***A SERVER THAT ANSWERS.*** `p4` points at `ssl:<p4-host>:1666`, which does not respond; `cm` and `svn` are not on `PATH`; and Git's answer is a **refusal**. Measured twice by two separate workstreams. |
| **1.5 / `TA-18`** | *a `USkeleton` bone.* `Skeleton.h:365-369` returns the reference skeleton as **`const` on purpose**, `ReferenceSkeleton` is `protected` (`:301/:317`), the bone-adding functions take a `USkinnedAsset` (`:797/:809/:820`) and **there is no skeletal mesh here**; the last route is `FEditableSkeleton`, a **private** header. **The `AnimGraph`, `Niagara` and `NiagaraEditor` dependencies are now in `Build.cs`, so the module blocker is REMOVED for `TA-18` and `TA-19` — what remains is authoring, not a permission.** |

### Volume rather than insight — precedents to follow

| Phase | Rows |
|---|---|
| **2** Capture | *2.3 `L-CAPTURE-LEVEL`, 2.4 `P-IMAGE-COMPARE`* |
| **3** Materials | *3.2 the `full` reader, 3.3 `MAT-CHECK` + `MAT-APPEND`, 3.4 `MAT-REPLACE` + `MAT-PATCH`, 3.6 `MAT-ARGS`* |
| **5** Widgets | *5.2 `P/E-WIDGET-TREE` (**the writer has NO declaration**; only the reader exists, at `:661`), 5.3 `WIDGET-SET`, 5.4 `WIDGET-ANIM`, 5.5 `WIDGET-BIND`* |
| **6** Graphs | *6.1 `GRAPH-RENAME`, 6.2 `GRAPH-COLLAPSE`, 6.3 `GRAPH-COPY`, 6.4 `INTERFACE`, 6.6 the node families* |
| **7** | *7.3 `CURVE-LOAD` / `CURVE-SET` (**`TA-17` is built**, the fixture is ready), 7.6 the asset operations* |
| **8** Levels and actors | *8.1 `LEVEL-LOAD`, 8.2 `ACTOR-LOAD`, 8.3 `ACTOR-PLACE`, 8.4 `ACTOR-SET`, 8.5 `COMPONENTS`* |
| **9** Session | *9.2 `E-SESSION` / `R-EDIT-SESSION`, 9.3 `P/E-ASSET-BATCH` — **no declaration exists*** |
| **0 / 1 / 11** | *0.1 a single owner of the editor lifecycle, 0.3 re-verify the engine notes, 1.1 breakpoints, 1.2 automation, 11.1 `09_BUILD.md`* |

### The order I would do them in

1. **9.2 / 9.3** — *`e-session` and `asset-batch` are **multipliers**: one transaction for N writes. They make the thirty unwired writers **safe** rather than merely numerous.*
2. **7.3** — *`TA-17` is already built and its suite passes (40 checks). The row is unblocked and the fixture is waiting.*
3. **5.3 / 5.4 / 5.5** — *three widget writers that reuse existing code paths.*
4. **8.x** — *levels and actors: five rows, an entire domain of the plan that has **never been started**.*
5. **6.x** — *the graphs: the largest block, and the one whose premises have not been tested.*

---

## 4. The two defects that made green **impossible**, and their root cause

| | |
|---|---|
| ***The full-run conflict*** | *`test-vcs` required `TA01_HandLaid.uasset` to be **clean**, while `test-graph-fix-backward`, which runs **before** it, rebuilds the kit. Fixed in the **harness**, because **a suite needs the editor alive for its checks and therefore cannot restore the file itself**: `git checkout` answers `unable to unlink old ...: Invalid argument` for as long as the editor holds it open.* |
| ***A count pinned on a shared asset*** | *`test-material-tidy` asserted `expressions_after == 16` while the fixture held **12**, so **the suite passed or failed according to which fixture it was looking at** — and reported as a defect in the tool what was a property of the fixture. Fixed by comparing `before` against `after` instead of against a literal.* |

---

## 5. What I do **not** know, and do not present as known

| | |
|---|---|
| ***No checkout against a live depot*** | *every answer reported is the answer to a **refusal** or to a **no-op*** |
| ***The `UsesCheckout == true` branch has never run*** | *Perforce is `IsAvailable == false`, so the mechanism refuses before reaching it* |
| ***No `-nullrhi` run*** | *the `screen` refusal is proven by construction from two engine guards, not executed* |
| ***The positive path of `selection`*** | *proven by a self-cleaning internal probe, because `P3` forbids a Look from creating a selection* |
| ***`TA-19`*** | *the `Niagara` module is linked, but **nothing was built**: the emitter still has to be authored* |

---

## 6. The lesson that cost the most, stated **once**

> ***ELEVEN TIMES IN THIS WORK, SOMETHING FAILED IN SILENCE AND WAS READ AS A SUCCESS.***
>
> *A `git checkout` whose error was **captured and never read** · a regex that assumed the wrong format —
> **inside the suite that exists to enforce the rule** · a count pinned on a shared asset · **a refusal
> message that described a `Build.cs` without the dependencies that had just been added to it** · and a
> `git add` whose output went to `Out-Null`, so a commit that recorded **none** of its intended changes
> reported success.*
>
> ## **IT IS THE SAME SENTENCE AS "AN INVISIBLE REFUSAL READS AS A CAPABILITY" — this project's most recurrent defect, and it has been found inside the tools that exist to catch it.**
