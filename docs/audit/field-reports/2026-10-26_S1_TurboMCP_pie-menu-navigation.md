# TurboMCP PIE menu navigation session report

**Report ID:** 2026-10-26_S1
**Date:** 2026-10-26
**Review state:** Pending human review
**Plugin version:** 2f7264b
**Engine version:** Unknown
**Client:** Claude Code desktop app, version Unknown
**Test case:** Two widget blueprints read (a menu page and its card entry). One live PIE session in a menu map with one local player.
**Context:** PIE was running for the navigation part. Source control was in use. No asset was edited.
**Tools used:** EditTurboMCPPlayInput, ReadTurboMCPPlayInputState, LookTurboMCPCapture (asset-editor and viewport modes), ReadTurboMCPAssetSearch, ReadTurboMCPGraphLoad, ReadTurboMCPToolHelp. Other tools by role: engine widget tree reader, engine PIE start, engine Slate inspector (snapshot, click, key press, wait).
**Outcome:** Navigation to the target menu page was not completed. No compile. No save. No crash.
**Session limits:** The user stopped the navigation and will navigate manually.
**Correction of:** None

## 1. Cross-cutting issues

**X-98: The capture hint sends the agent to a fallback that does not activate menu buttons (medium)**
- **Type:** Usability.
- **Scenario:** Live PIE in a menu map. The agent must open a submenu from a main menu button.
- **Reproduction status:** Repeated in one session.
- **Steps to reproduce:**
  1. Start PIE in a menu map.
  2. Capture the viewport. Follow the hint: observe and snapshot with the Slate inspector, then click the ref of the menu button.
  3. Capture again.
- **Observed:** The click returned success two times, but the button only took the hover state. A key press on the focused button also returned success and did nothing. On the title screen, one key press did advance the screen. The menu button never activated.
- **Expected:** The hint leads to an input path that activates a CommonUI button in PIE. This is a user expectation.
- **Impact:** The navigation was blocked. The user took over manually.
- **State after:** No asset changed. PIE was still running.
- **Workaround:** None observed.
- **Cause:** The cause is not confirmed.
- **Proposed behavior:** If the play input tool is the intended path, point the hint to it. Otherwise, say that the Slate click can deliver hover without activation.

## 2. Tool observations

### Play input

**PI-01: The play input tool refuses every action while PIE is running (high)**
- **Type:** Defect.
- **Scenario:** One in-process PIE world with one local player. The state reader confirmed the world and player.
- **Reproduction status:** Observed once.
- **Steps to reproduce:**
  1. Start PIE.
  2. Call EditTurboMCPPlayInput with the focus action and target pie:0.
- **Observed:** The tool refused. The message was the generic Edit gate: it requires no play session and no active transaction. The tool did not run.
- **Expected:** The tool help says that the tool executes input in live PIE. So it must accept a call while PIE runs.
- **Impact:** The only TurboMCP input tool cannot be used. The workflow was blocked.
- **State after:** No input was sent. No asset changed.
- **Workaround:** Engine Slate inspector input was tried. See X-98. It did not work for menu buttons.
- **Cause:** The cause is not confirmed.
- **Proposed behavior:** Exempt the play input tool from the no-play-session gate. Keep the gate for asset edits.

**PI-02: The play input state reader works in PIE (positive)**
- **Scenario:** Same PIE session.
- **Observed:** ReadTurboMCPPlayInputState reported the world, the player, the focus owner, held input, the CommonInput device type and the mapping context count in one call. It showed that focus was outside the PIE viewport.

### Capture

**CP-17: Viewport capture gives clear PIE frames (positive)**
- **Scenario:** Same PIE session, several menu screens.
- **Observed:** LookTurboMCPCapture in viewport mode returned the game layer with menu UI, without editor chrome, at native size. Six captures were taken. Each one showed the current screen state, which made it possible to verify every input attempt.

**CP-18: Asset-editor capture plus the widget tree read gives a good layout check (positive)**
- **Scenario:** A menu page widget with a horizontal list of card entries.
- **Observed:** The engine widget tree reader gave the hierarchy, sizes and slot settings as text. One asset-editor capture of the designer then showed the same layout with preview entries. Together they answered the layout question without project code execution.

## 3. Recurrence

None observed.

## 4. Features that would have helped

| ID | Proposed feature | Observed work it would replace |
| --- | --- | --- |
| FT-101 | A PIE tool that activates a named menu button (press and release through the CommonUI input path) and reports the new active screen | Two Slate clicks, one key press and four captures that did not open the submenu |

## 5. Suggested priority

1. PI-01: the tool cannot run in the only state where it is useful.
2. X-98: the hint leads to a dead end and costs several calls.
3. FT-101: a direct way to drive menus in PIE.

## 6. Completeness and privacy review

All tools used in the session are listed. Item IDs: X-98, PI-01, PI-02, CP-17, CP-18, FT-101. PI is a new prefix (Play input). The engine version and client version were not observed. The report was read back. Local lint passed. The glossary is in the archive folder, so its counters were not updated; this needs the user's decision. Automated checking does not prove confidentiality.
