---
id: "745637f4-b07e-46fc-9251-c89e62fa2f04"
title: "T-C3-01 (P5, low) — C3: 17 of the 50 comment-versus-code sites remain — 33 fixed, and this card is the resume point"
status: backlog
priority: "P5"
tags: ["P5", "low", "C3", "comment", "defect"]
workspace: "7d139a89-61a3-4566-b367-cf7a89b9969b"
created: "2026-10-12T08:29:48Z"
updated: "2026-10-16T19:57:32Z"
executions: 0
board_revision: 1803
exported: "2026-10-17T09:13:25Z"
---

# T-C3-01 (P5, low) — C3: 17 of the 50 comment-versus-code sites remain — 33 fixed, and this card is the resume point

| Field | Value |
|---|---|
| Card ID | `745637f4-b07e-46fc-9251-c89e62fa2f04` |
| Status | backlog |
| Priority | P5 |
| Tags | `P5` `low` `C3` `comment` `defect` |
| Workspace | `7d139a89-61a3-4566-b367-cf7a89b9969b` |
| Created (UTC) | 2026-10-12T08:29:48Z |
| Updated (UTC) | 2026-10-16T19:57:32Z |
| Executions recorded | 0 |

## Description

Summary
    The comment-versus-code audit found 50 places where a comment describes something the code does not do. 33 are fixed and this card is where the remaining 17 are picked up.

Done so far
    33 sites across 9 files, in 11 commits from `ddec417` to `80a244d` — 144 insertions, 125 deletions. The files: `TurboMCPKitCommandlet.cpp` (14 sites), `GraphFixBackward.cpp` (the capped-list count), `Capture.cpp` (two refusals that began with the banner), `MaterialBlocks.cpp` (five false promises of a title or a check), `Material.cpp` (a dead counter and two histories), `Toolsets.h` (an over-promise of verification), plus `GraphFixBackward.h`, `GraphTidy.cpp` and `GraphTidy.h`.

First action
    Nothing here has been compiled yet. Build before anything else, and build the whole target rather than incrementally: `FPlan` gained a field in `GraphFixBackward.h` and lost one in `GraphTidy.h`.

Two findings diagnosed but not applied
    1. `GraphKnots.cpp:1329` — `MarkPackageDirty()` runs before the rollback decision, so a pass that rolls back leaves the package dirty and the editor offers to save a change that does not exist. The obvious fix is wrong: moving the call does not clear the flag, and clearing it blindly throws away a dirty state that may belong to something else. The fix is three lines with a `bWasDirty` captured before the first write. This needs a compile, not more reading.
    2. `Struct.cpp:1534` — the earlier card was wrong twice. The rename failure *is* reported (`Vanished.Add` plus `continue`, and `++Added` is skipped), and `:1550` cannot write to a pre-existing field because `GuidOfField` matches on `FriendlyName` and the field was never renamed. The real defect is that `MakeUniqueFriendlyName` and `AddVariable` have already run, so the struct gains a field under an unrequested name while the report says the field did not arrive. Whether that branch is reachable at all needs a run: call `E-STRUCT-ADD` twice on one name and read the field list back. The kit commandlet already builds a user struct, so the fixture exists.

What remains
    `GraphKnots.cpp:1334` (the `>=` rather than `>` rollback test, to be read with the branch it guards), two more sites in `Struct.cpp`, one more in `GraphKnots.cpp`, and five files with one or two sites each, named by line number further down this card's original notes.

Already checked and correct — do not re-read
    `Struct.cpp:183` (a deliberate engine exception, `PC_Real` accepting `PC_Int`), `KitCommandlet.cpp:639`, `:832`, `:1483`, `GraphFixBackward.cpp:90` (the fallback flag *is* set at `:719` and read at `:1645`; the `:716` the old card cited is a `return;` in a different file), `Capture.cpp:906`, `Toolsets.h:1958` (`GraphReplace` does carry a title), `Toolsets.h:37`, `:410`, `:1174`, `:1244`, `Material.cpp:1450` (`; error:` is an established inline convention, documented at `BlueprintJson.cpp:183` and `GraphLoad.cpp:144`) and `Material.cpp:160`.

The shapes behind the 50 sites
    24 sites are a comment describing an intention the code never implemented — fix the comment. 3 sites are code breaching a rule the file itself states — both in `Capture.cpp`, fix the code. 5 of the 8 code defects are a field written once and read nowhere, sitting beside a comment that claims it is reported — remove the field or make it real.

Note
    The third shape cannot be found by a script, and that was tested rather than assumed: `Tools/checks/check-dead-fields.py` was written for it, ran, and returned 254 candidates with none of the three real cases among them, because they are locals rather than struct fields. `80a244d` records that failure inside the tool. Verifying each site before changing it is the larger half of this work — six of the 33 sites turned out to be cards that were simply wrong, and two of those proposed a fix that would have broken something.

## Execution prompt

```text
Testing policy: Before editing, run the fastest relevant focused/static check. Re-run it after the change. Avoid a complete suite by default; run one only when this change spans integrations or the focused test cannot establish confidence, and record why. For audit/documentation-only work, use direct validation only; do not run a complete Unreal suite.
```

## Execution history

_No execution record. The card state is a board declaration, not run evidence._

---

_Exported from the DSH task board, revision 1803, at 2026-10-17T09:13:25Z. The board is authoritative; this file is a snapshot._
