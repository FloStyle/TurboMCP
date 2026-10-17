---
id: "9d4624ff-4e90-4c5e-9f8f-1e621af2d537"
title: "T33 (P5, low) — Plugin window presentation"
status: todo
priority: "P5"
tags: ["P5", "low", "ui", "feedback-2026-10-13"]
workspace: "7d139a89-61a3-4566-b367-cf7a89b9969b"
created: "2026-10-13T21:05:27Z"
updated: "2026-10-16T19:57:58Z"
executions: 0
board_revision: 1803
exported: "2026-10-17T09:13:25Z"
---

# T33 (P5, low) — Plugin window presentation

| Field | Value |
|---|---|
| Card ID | `9d4624ff-4e90-4c5e-9f8f-1e621af2d537` |
| Status | todo |
| Priority | P5 |
| Tags | `P5` `low` `ui` `feedback-2026-10-13` |
| Workspace | `7d139a89-61a3-4566-b367-cf7a89b9969b` |
| Created (UTC) | 2026-10-13T21:05:27Z |
| Updated (UTC) | 2026-10-16T19:57:58Z |
| Executions recorded | 0 |

## Description

Source
    `Feedbacks/TurboMCP_TaskList_2026-10-13.md:348-355`, group P5. Source IDs: UI-01 to UI-04 (backlog). This card came from the list.

done when
    1. The plugin window shows a "Beta" flag.
    2. The plugin appears under the editor "Tools" menu, with an icon.
    3. The plugin description is updated.
    4. The plugin has an icon.

Clause 4 is already done, and it was measured this session
    `eec301f` added `Resources/Icon128.png` (a 128x128 RGBA PNG) and wired it through `TurboMCP.uplugin`'s `Icon` field. That field was absent, so the image alone would never have shown. `git check-ignore` and a `json.load` of the `.uplugin` both confirm it. So clause 4 is done, and the other three are not.

Clause 4 duplicates clause 2 in the list's own text
    The list says "with an icon" for the menu entry and "has an icon" for the plugin. One is about the menu entry and one is about the plugin, and they may be two different icon paths in the `.uplugin`. That is worth noting rather than merging silently.

Clause 2 needs a measurement
    `TurboMCPModule.cpp` already registers a nomad tab and a toolset. Whether an entry also belongs under the editor's `Tools` menu is a `UToolMenus` question, and the plugin has no `UToolMenus` reference today.

## Execution prompt

```text
Testing policy: Before editing, run the fastest relevant focused/static check. Re-run it after the change. Avoid a complete suite by default; run one only when this change spans integrations or the focused test cannot establish confidence, and record why.
```

## Execution history

_No execution record. The card state is a board declaration, not run evidence._

---

_Exported from the DSH task board, revision 1803, at 2026-10-17T09:13:25Z. The board is authoritative; this file is a snapshot._
