# TurboMCP editor settings session report

**Report ID:** 2026-10-26_S7
**Date:** 2026-10-26
**Review state:** Pending human review
**Plugin version:** eb7a9073f47b8d11781744d2f33da1bcab968260 (main, development preview, before the first release).
**Engine version:** Unreal Engine 5.8.3, licensee source build.
**Client:** Claude Code desktop app. The user asked for the feature. Claude read the plugin settings source in the TurboMCP repository.
**Test case:** One project with a project-local TurboMCP installation that is now in team source control.
**Context:** Editor closed. No play session. Source control in use. The plugin folder is marked for add, which means other team members may soon receive it.
**Tools used:** No TurboMCP tools were called.
**Outcome:** Proposal only. No change was made.
**Session limits:** The feature was not prototyped. The settings layout was read from source, not observed in the editor.
**Correction of:** None

## 1. Cross-cutting issues

**FT-112: Add an Editor Preferences page with a per-user switch to turn TurboMCP on or off (medium)**
- **Type:** Proposal.
- **Scenario:** The plugin is installed in a shared project. Some team members use an MCP client and some do not. One user wants to stop TurboMCP for a while, for example while profiling, investigating a crash or working without an agent.
- **Reproduction status:** Not applicable. The current behavior was read from source.
- **Steps to reproduce:**
  1. Open the editor with TurboMCP enabled in the project.
  2. Look for a per-user option to turn TurboMCP off.
- **Observed:** TurboMCP has one settings page, in Project Settings under Plugins. It is project-wide: it writes to the shared default editor config, which goes into source control. Its only option is the call log switch. No Editor Preferences page exists. No per-user switch stops the plugin from registering its toolsets or exposing its tools. The only way to turn TurboMCP off is to disable the plugin in the project descriptor. That changes a shared file, affects the whole team and needs a restart.
- **Expected:** A user can turn TurboMCP on or off for themselves without changing shared project files or affecting teammates. This becomes important once the plugin ships in a team project, as in the first release.
- **Impact:** A user cannot stop TurboMCP for themselves. Changing the shared setting would affect every teammate. Unknown in this session whether other teams have this need.
- **State after:** Not applicable.
- **Workaround:** Disable the plugin in the project descriptor without submitting the change, or remove the plugin folder from the local workspace. Not tried in this session. Both are fragile and easy to submit by mistake.
- **Cause:** Not applicable. This is a missing feature.
- **Proposed behavior:**
  1. Add a TurboMCP page under Editor Preferences, stored in the per-user editor settings, not in the shared project config.
  2. Put a master switch on it, named something like Enable TurboMCP, on by default. When off, TurboMCP registers no toolsets, so no TurboMCP tool appears in the MCP tool list, and its panel and menu entries are hidden or show that it is disabled. The engine MCP server and other toolsets are not affected.
  3. Apply the switch live if registration can be undone safely. Otherwise apply it at next editor start and say clearly that a restart is needed.
  4. Show the switch state in the plugin status tool, so an agent can tell "disabled by the user" apart from "failed to register".
  5. Consider moving the per-user options there too, such as the call log switch, because call logging is a personal privacy choice. Keep only team-wide options in Project Settings.

## 2. Tool observations

None observed. No TurboMCP tool ran in this session.

## 3. Recurrence

None observed. Related: the call log default changed to on in an earlier update. Today that setting is project-wide, so one user's choice is shared with the team. This proposal would make it per-user.

## 4. Features that would have helped

| ID | Proposed feature | Observed work it would replace |
| --- | --- | --- |
| FT-112 | Per-user Editor Preferences page with an Enable TurboMCP switch | Editing the shared project descriptor or removing the plugin folder locally to turn the plugin off |

## 5. Suggested priority

1. FT-112: Do it before the first release, because team projects will share the plugin through source control.

## 6. Completeness and privacy review

The report covers FT-112. The session used no TurboMCP tools. Missing evidence: the feature was not prototyped and the settings page was not observed in the editor. The local lint ran and the report was read back. Review state stays Pending human review. Automated checking does not prove confidentiality.
