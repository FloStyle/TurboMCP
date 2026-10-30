# ACCEPTANCE ROW THIS SUITE CLAIMS TO PROVE: T-HELP-1, T-HELP-2, T-HELP-3, T-HELP-5
"""Exercise the Phase 1 agent-help tools against the running server.

Run after the editor is up. Checks the rules the spec actually states, rather
than just printing output:

  T-HELP-1  the index names the server state and lists every tool
  T-HELP-2  one tool's help is under 2000 characters and carries its traps
  T-HELP-3  a wrong toolset name lists the real ones
  T-HELP-5  a wrong tool name is refused with the real names

Exits non-zero if a check fails.
"""

import json
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from tmcp import PREVIEW_TOOLSET, READ_TOOLSET, Session  # noqa: E402

HELP_BUDGET = 2000

failures = []


def check(name, ok, detail=""):
    print("  %-4s %s%s" % ("ok" if ok else "FAIL", name, ("  " + detail) if detail else ""))
    if not ok:
        failures.append(name)


def main():
    s = Session()
    s.connect()

    print("=== T-HELP-1: the tool index ===")
    index = s.call(READ_TOOLSET, "ReadTurboMCPToolIndex")
    check("names the server state", "server:" in index)
    check("states tool search mode", "tool search:" in index)
    toolset_lines = re.findall(r"^([^\n]+) \(source: (TurboMCP|Unreal MCP)\)$", index, re.M)
    totals = re.search(r"(\d+) toolsets in total, (\d+) of them TurboMCP, (\d+) tools", index)
    tool_lines = re.findall(r"^  .+ - (?:TurboMCP|Unreal MCP) - .+$", index, re.M)
    check("counts match emitted registry entries", totals is not None and
          tuple(map(int, totals.groups())) == (len(toolset_lines),
          sum(source == "TurboMCP" for _, source in toolset_lines), len(tool_lines)))
    engine_lines = [line for line in index.splitlines() if " - Unreal MCP - " in line]
    check("engine kinds are not guessed from names", bool(engine_lines) and
          all(line.startswith("  Engine ") for line in engine_lines))
    check("lists our read tools", "ReadTurboMCPToolIndex" in index)
    check("flags our tools as TurboMCP", "TurboMCP -" in index)
    check("prints counts even at zero", "toolsets in total" in index)
    check("kinds are Read/Look/Preview/Edit", "  Read " in index and "  Look " in index
          and "  Preview " in index and "  Edit " in index)

    print()
    print("=== T-HELP-2: one tool's help, and the character budget ===")
    help_text = s.call(
        READ_TOOLSET,
        "ReadTurboMCPToolHelp",
        {"toolsetName": READ_TOOLSET, "toolName": "ReadTurboMCPToolIndex"},
    )
    print(help_text)
    print()
    check("starts with its kind", help_text.startswith("read:"))
    check("states the kind", "kind:" in help_text)
    check("states the source", "source:" in help_text)
    check("states the parameters", "parameters:" in help_text)
    check("states the traps", "traps:" in help_text)
    check(
        "under the %d character budget" % HELP_BUDGET,
        len(help_text) <= HELP_BUDGET,
        "%d characters" % len(help_text),
    )

    print()
    print("=== an engine tool, with traps supplied by the PROJECT file (D-07) ===")
    # The shipped traps target the Blueprint and widget tools, which are not
    # registered in this project: only AgentSkillToolset is. So the project file
    # TestProject/Config/TurboMCP/Traps.json supplies traps for ListSkills, which
    # also proves the "data, not code" rule: it took effect with no rebuild.
    engine_help = s.call(
        READ_TOOLSET, "ReadTurboMCPToolHelp", {"toolsetName": "AgentSkillToolset", "toolName": "ListSkills"}
    )
    print(engine_help)
    print()
    check("resolves an engine tool by short toolset name", "ListSkills" in engine_help)
    check("marks it as the engine's", "source: Unreal MCP" in engine_help)
    check("carries its traps", "traps: 2" in engine_help and "  - " in engine_help)
    check(
        "under the %d character budget" % HELP_BUDGET,
        len(engine_help) <= HELP_BUDGET,
        "%d characters" % len(engine_help),
    )

    print()
    print("=== a tool with no recorded traps says so and names its source ===")
    no_traps = s.call(
        READ_TOOLSET, "ReadTurboMCPToolHelp", {"toolsetName": "AgentSkillToolset", "toolName": "CreateSkill"}
    )
    print(no_traps)
    print()
    check("says none recorded rather than staying silent", "traps: none recorded" in no_traps)
    check("names the file it read", "Traps.json" in no_traps)
    check("renders parameters", "parameters: 4" in no_traps)

    print()
    print("=== T-HELP-3: a wrong toolset name ===")
    bad = s.call(
        READ_TOOLSET, "ReadTurboMCPToolHelp", {"toolsetName": "NoSuchToolset", "toolName": "x"}
    )
    print(bad)
    print()
    check("refuses rather than guessing", bad.startswith("error:"), bad[:80])
    check("lists the real toolset names", "TurboMCP.TurboMCPReadToolset" in bad)

    print()
    print("=== T-HELP-5 / P2: a wrong tool name ===")
    bad2 = s.call(
        READ_TOOLSET,
        "ReadTurboMCPToolHelp",
        {"toolsetName": READ_TOOLSET, "toolName": "NoSuchTool"},
    )
    print(bad2)
    print()
    check("refuses rather than guessing", bad2.startswith("error:"))
    check("unknown tool gives callable next step", "ReadTurboMCPToolIndex" in bad2 and "R-TOOL-INDEX" not in bad2)

    print()
    print("=== the joined name form tools/list prints ===")
    joined = s.call(
        READ_TOOLSET,
        "ReadTurboMCPToolHelp",
        {"toolsetName": "TurboMCP.TurboMCPReadToolset.ReadTurboMCPStatus", "toolName": ""},
    )
    print(joined)
    print()
    check("accepts one joined name", joined.startswith("read:") and "ReadTurboMCPStatus" in joined)

    print()
    print("=== summary mode: empty tool name ===")
    summary = s.call(
        READ_TOOLSET,
        "ReadTurboMCPToolHelp",
        {"toolsetName": "TurboMCPReadToolset", "toolName": ""},
    )
    print(summary)
    print()
    check("accepts the short toolset name", summary.startswith("read:"))
    check("summarises every tool", summary.count("\n") >= 3)

    print("=== status, schema and window metadata ===")
    status = s.call(READ_TOOLSET, "ReadTurboMCPStatus")
    print(status)
    check("status distinguishes declaration from registration", "declared toolsets" in status and "registered:" in status)
    check("status does not invent revision provenance", "source revision: unknown" in status)
    schema = s.call(READ_TOOLSET, "ReadTurboMCPToolSchema", {"toolset": "Read", "tool": "ReadTurboMCPStatus"})
    print(schema)
    check("schema resolves exact tool with short toolset", "ReadTurboMCPStatus" in schema and not schema.startswith("error:"))
    refused = s.call(READ_TOOLSET, "ReadTurboMCPToolSchema", {"toolset": "Read", "tool": "NoSuchTool"})
    check("schema refuses unknown tool", refused.startswith("error:"))
    preview = s.call(PREVIEW_TOOLSET, "PreviewTurboMCPToolset")
    print(preview)
    check("preview explains whole-schema size", "whole-toolset schema sizes" in preview and "would change: nothing" in preview)
    windows = s.call("TurboMCP.TurboMCPLookToolset", "LookTurboMCPToolset")
    print(windows)
    counts = re.search(r"(\d+) visible, showing (\d+) \(limit (\d+)\)", windows)
    check("window count is explicit or unavailable", "visible count unavailable" in windows or counts is not None)
    if counts:
        total, shown, limit = map(int, counts.groups())
        check("window sample agrees with listed titles", shown == windows.count("\nwindow:") and shown == min(total, limit))

    print()
    if failures:
        print("RESULT: FAIL (%d) -> %s" % (len(failures), ", ".join(failures)))
        return 1
    print("RESULT: PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
