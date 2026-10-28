# TurboMCP installation documentation session report

**Report ID:** 2026-10-27_S1
**Date:** 2026-10-27
**Review state:** Pending human review
**Plugin version:** 5a7c5f401fc30daf19a34b4aec66c929de318119 (main, development preview, before the first release).
**Engine version:** Unreal Engine 5.8.3, licensee source build. Not relevant to this finding.
**Client:** Claude Code desktop app. Claude compared the plugin settings source with the user documentation while installing the update.
**Test case:** One project-local plugin update that adds the per-user Enable TurboMCP preference proposed in FT-112.
**Context:** Editor closed. No play session. The update was installed but not yet built, so the editor behavior was not observed.
**Tools used:** No TurboMCP tools were called.
**Outcome:** The documentation and the code disagree about the default of the new preference. No change was made.
**Session limits:** Only the source and the documentation were compared. The editor was not started with this build, so the startup behavior and the Editor Preferences page were not observed.
**Correction of:** None

## 1. Cross-cutting issues

**X-106: The installation guide says Enable TurboMCP defaults to enabled, but the code defaults it to off (medium)**
- **Type:** Defect. Documentation.
- **Scenario:** A user installs or updates the plugin and follows the installation guide.
- **Reproduction status:** Observed in source and documentation at 5a7c5f4. Not observed in the editor.
- **Steps to reproduce:**
  1. Read the plugin settings class at 5a7c5f4. The Enable TurboMCP property is false by default, and its tooltip says Off by default.
  2. Read the section of the installation guide about turning TurboMCP off for yourself.
- **Observed:** 4 problems.
  1. The guide says the preference defaults to enabled. The code default is off. The commit that changed the default is titled Default TurboMCP user preference to off, so the code looks intentional and the guide looks out of date.
  2. The guide's section is about disabling TurboMCP for yourself. With an off default, most users first need to turn it on.
  3. The install steps say to enable TurboMCP in the project, build and open the editor. They do not say that each user must also tick Enable TurboMCP in Editor Preferences and restart. The README quick start has the same gap. A user who follows the steps gets an editor with no TurboMCP tools, panel or hooks.
  4. When TurboMCP is off, the status tool is also absent, as the guide says. An agent that cannot find any TurboMCP tool cannot ask TurboMCP why. The guide points to Editor Preferences and the startup log message, but the install steps do not.
- **Expected:** The guide matches the code. The install steps include the per-user enable step, so a new user gets working tools after following the steps. The troubleshooting page lists the preference as the first check when no TurboMCP tools appear.
- **Impact:** After the first release, new users and teammates who receive the plugin through source control will see no TurboMCP tools, and nothing in the install steps tells them why. An agent cannot diagnose it through MCP. Unknown in practice, because this build was not run.
- **State after:** Not applicable. Nothing was changed.
- **Workaround:** Tick Enable TurboMCP in Editor Preferences and restart the editor. Not yet tried in this session.
- **Cause:** The cause is not confirmed.
- **Proposed behavior:**
  1. Correct the guide to say the preference defaults to off, or change the code if on was intended. The FT-112 proposal suggested on by default, so decide which is intended.
  2. Add an install step: open Editor Preferences, Plugins, TurboMCP, tick Enable TurboMCP and restart. Add the same step to the README quick start.
  3. Rename the guide section so it covers turning TurboMCP on and off.
  4. Add a troubleshooting entry: no TurboMCP tools appear, so check the Enable TurboMCP preference and the TurboMCP startup log line.
  5. Consider a docs check that compares documented defaults with the settings source, so this cannot drift again.

## 2. Tool observations

None observed. No TurboMCP tool ran in this session.

## 3. Recurrence

- X-106 follows FT-112 (2026-10-26_S7), which proposed the preference with an on default. The implementation chose off, and the guide kept the on wording.

## 4. Features that would have helped

None new.

## 5. Suggested priority

1. X-106: Fix the guide and the install steps before the first release, because with an off default every new user hits this.

## 6. Completeness and privacy review

The report covers X-106. The session used no TurboMCP tools. Missing evidence: the editor behavior with the preference off and the Editor Preferences page were not observed. The local lint ran and the report was read back. Review state stays Pending human review. Automated checking does not prove confidentiality.
