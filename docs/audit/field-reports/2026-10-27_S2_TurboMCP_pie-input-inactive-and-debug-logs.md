# TurboMCP play input and debug log session report

**Report ID:** 2026-10-27_S2
**Date:** 2026-10-27
**Review state:** Pending human review
**Plugin version:** Local install; exact commit not recorded in this session
**Engine version:** Unreal Engine 5.8, source build; revision Unknown
**Client:** Claude Code desktop app; version Unknown
**Test case:** Debug log nodes added to three menu widget Blueprints. One attempt to drive a menu repro in play mode with synthetic gamepad input.
**Context:** Edits outside play mode. Play mode was started for the input attempt. Perforce source control in use; one needed asset was locked by another user.
**Tools used:** EditTurboMCPGraphAppend, PreviewTurboMCPGraphAppend, EditTurboMCPGraphTidy, EditTurboMCPCompile, EditTurboMCPSave, EditTurboMCPVcsCheckout, ReadTurboMCPVcsState, ReadTurboMCPNodeFind, ReadTurboMCPOverrides, ReadTurboMCPDirtyList, ReadTurboMCPToolHelp, ReadTurboMCPPlayInputState, EditTurboMCPPlayInput. Other tools: the engine play-mode start tool and the engine Blueprint node-type search.
**Outcome:** Debug logs were added, compiled clean and saved. The input repro did not run. The user took over the repro by hand. No crash.
**Session limits:** None observed.
**Correction of:** None

## 1. Cross-cutting issues

None observed.

## 2. Tool observations

### Play input

**PI-03: Synthetic input refuses while the editor application is not focused (medium)**
- **Type:** Limitation.
- **Scenario:** Play mode was started by a tool call while the user's active window was another application.
- **Reproduction status:** Observed once in this session. Same result as PI-01.
- **Steps to reproduce:**
  1. Start play mode through a tool while another application has focus.
  2. Call EditTurboMCPPlayInput with action focus, then with action hold on a gamepad face button.
- **Observed:** Focus reported success. Hold refused with "The editor application is inactive. No input was sent." ReadTurboMCPPlayInputState reported application active: no.
- **Expected:** A way to send input to play mode without asking the user to click into the editor.
- **Impact:** The repro could not be automated. The user had to take over.
- **State after:** Play mode left running. No input sent.
- **Workaround:** The user runs the repro by hand. Not tested further.
- **Cause:** The cause is not confirmed.
- **Proposed behavior:** An option to activate the editor window first, or a clear precondition check in the focus action.

### Graph append

**GC-65: Append does not accept the self-reference node by an intuitive type ID (low)**
- **Type:** Usability.
- **Scenario:** A log string needed the widget's own object name.
- **Reproduction status:** Observed once.
- **Steps to reproduce:**
  1. Append a self-reference node with a guessed type ID.
- **Observed:** Refused with no close match. The engine node-type search returned the real type ID, and append then accepted it.
- **Expected:** The refusal suggests the self-reference type ID.
- **Impact:** One extra engine search.
- **State after:** Unchanged by the refused call.
- **Workaround:** Engine node-type search. This worked.
- **Cause:** The cause is not confirmed.
- **Proposed behavior:** Add common pure nodes such as self-reference to the close-match suggestions.

### Layout

**TD-43: Tidy rolled back for small appended nodes in two event graphs (low)**
- **Type:** Usability.
- **Scenario:** Single log nodes spliced after event nodes in two existing event graphs.
- **Reproduction status:** Observed twice.
- **Observed:** Both scoped tidies predicted longer wires and rolled back. The new nodes stay where append placed them.
- **Expected:** A placement next to the event that does not increase wire length much.
- **Impact:** Minor. The nodes are temporary debug logs.
- **State after:** Original positions restored. Compiled clean.
- **Workaround:** None used.
- **Cause:** The cause is not confirmed.
- **Proposed behavior:** For one spliced node, offer to shift downstream nodes right.

## 3. Recurrence

- PI-03 matches PI-01 (2026-10-26_S1). Same refusal on application focus. This time play mode was started by a tool.
- TD-43 relates to TD-41 (2026-10-26_S8). Same rollback on a small scoped tidy.

## 4. Features that would have helped

| ID | Proposed feature | Observed work it would replace |
| --- | --- | --- |
| FT-119 | Activate the editor window before sending play input | A manual handover of the repro to the user |

## 5. Suggested priority

1. PI-03 and FT-119: automated menu repros are blocked without user focus.
2. GC-65 and TD-43: minor friction.

## 6. Completeness and privacy review

All TurboMCP tools used in this session are listed. Item IDs: PI-03, GC-65, TD-43, FT-119. Missing evidence: plugin commit, engine revision and client version. The glossary was not updated because it is in the archive. The report was read back. Local lint passed. Review state stays Pending human review. Automated checking does not prove confidentiality.
