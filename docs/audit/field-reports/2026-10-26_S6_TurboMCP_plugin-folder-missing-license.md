# TurboMCP plugin packaging session report

**Report ID:** 2026-10-26_S6
**Date:** 2026-10-26
**Review state:** Pending human review
**Plugin version:** eb7a9073f47b8d11781744d2f33da1bcab968260 (main, development preview, before the first release).
**Engine version:** Unreal Engine 5.8.3, licensee source build. Not relevant to this finding.
**Client:** Claude Code desktop app. The user found the problem. Claude checked the files.
**Test case:** One project-local plugin installation, made by copying only the installable plugin folder, as the installation guide says.
**Context:** Editor closed. No play session. The installed plugin folder was put into source control at the user's request: marked for add, not submitted.
**Tools used:** No TurboMCP tools were called. Source control and file comparison were used.
**Outcome:** The installed plugin had no license text. Claude copied the license file from the repository root into the installed folder by hand. No crash. No assets were changed.
**Session limits:** Only the plugin folder and the repository root were checked. The planned release package was not built or inspected.
**Correction of:** None

## 1. Cross-cutting issues

**X-102: The installable plugin folder does not contain the license text (high)**
- **Type:** Defect.
- **Scenario:** A user follows the installation guide and copies only the installable plugin folder into a project. The repository root is not installed.
- **Reproduction status:** Observed once. The same result follows from the repository layout on every installation.
- **Steps to reproduce:**
  1. Check out main and look at the installable plugin folder.
  2. Copy that folder into a project, as the installation guide says.
  3. Look for a license file in the installed folder.
- **Observed:** The repository has the MPL-2.0 license text only at its root. The installable plugin folder has no license file. All C++ and build-script sources carry the SPDX MPL-2.0 header and the standard notice. That notice says a copy of the MPL is available at the public URL. 4 files in the plugin folder carry no license notice: the plugin descriptor, the content readme, the trap data file and the icon. The plugin descriptor has no license field. When the installed folder was put into source control, it contained no license text until it was added by hand.
- **Expected:** The installable plugin folder is self-contained for distribution. It includes the full license text, so every copy of the plugin, release package or source install carries its license. This matters most for the first release, where installable plugin packages are separate release assets.
- **Impact:** A team that copies only the plugin folder distributes the code without the license text. Installers must add it by hand, and they will lose it again on the next folder replacement.
- **State after:** The installed copy has the license file, added by hand and marked for add in source control. Not submitted. The repository is unchanged.
- **Workaround:** Copy the license file from the repository root into the installed plugin folder after every installation or update. It worked in this session.
- **Cause:** The cause is not confirmed.
- **Proposed behavior:** Add the MPL-2.0 license file to the installable plugin folder in the repository, as a copy of the root file or the canonical location. Make the release packaging and offline validation fail when a plugin package has no license file. Consider a notice file for the non-source files: the descriptor, the readme, the trap data and the icon. Check that the license and ownership of the icon and trap data are recorded before the first release.

## 2. Tool observations

None observed. No TurboMCP tool ran in this session.

## 3. Recurrence

None observed. This is the first report on licensing.

## 4. Features that would have helped

| ID | Proposed feature | Observed work it would replace |
| --- | --- | --- |
| FT-111 | A release check that requires a license file and a license notice for every file in the installable plugin folder | A manual license search and a manual copy after installation |

## 5. Suggested priority

1. X-102: Fix it before the first release. It affects every distributed copy of the plugin, and the fix is small.
2. FT-111: Add it with the release packaging work, so the problem cannot return.

## 6. Completeness and privacy review

The report covers X-102 and FT-111. The session used no TurboMCP tools. Missing evidence: a release package was not built or inspected. Note: the glossary counters are behind the current reports. These IDs were set above the highest numbers found in any report (X-101, FT-110). The local lint ran and the report was read back. Review state stays Pending human review. Automated checking does not prove confidentiality. This report gives no legal advice, and the licensing conclusion needs review by the maintainer.
