# ACCEPTANCE ROW THIS SUITE CLAIMS TO PROVE: T-HELP-1, T-HELP-2, T-HELP-3, T-HELP-5
"""Exercise the Phase 1 agent-help tools against the running server.

Run after the editor is up. Checks the rules the spec actually states, rather
than just printing output:

  T-HELP-1  the index reports server state and bounded toolset counts
  T-HELP-2  one tool's help is under 2000 characters and carries its traps
  T-HELP-3  a wrong toolset name points to the registered toolset index
  T-HELP-5  unknown names get actionable discovery and policy-aware suggestions

Exits non-zero if a check fails.
"""

import json
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from tmcp import PREVIEW_TOOLSET, READ_TOOLSET, Session  # noqa: E402

HELP_BUDGET = 2000
INDEX_BUDGET = 4000

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
    toolset_lines = re.findall(r"^([^\n]+) \(source: (TurboMCP|Unreal MCP), (\d+) tools\)$", index, re.M)
    totals = re.search(r"(\d+) toolsets in total, (\d+) of them TurboMCP, (\d+) available tools", index)
    omitted = re.search(r"Showing (\d+) of (\d+) toolsets", index)
    check("index stays within its character budget", len(index) <= INDEX_BUDGET, f"{len(index)} characters")
    check("index gives discovery cursors", "list:<offset>" in index and "find:0:<words>" in index)
    check("index omits individual tool entries", not re.search(r"^  (Read|Look|Preview|Edit|Engine) ", index, re.M))
    check("index reports totals and known sources", totals is not None and
          {"TurboMCP", "Unreal MCP"} <= {source for _, source, _ in toolset_lines})
    if totals and omitted:
        check("index discloses the number of omitted toolsets",
              tuple(map(int, omitted.groups())) == (len(toolset_lines), int(totals.group(1))))
    elif totals:
        check("counts match emitted registry entries", tuple(map(int, totals.groups())) ==
              (len(toolset_lines), sum(source == "TurboMCP" for _, source, _ in toolset_lines),
               sum(int(count) for _, _, count in toolset_lines)))

    registry_reply = s.rpc("tools/call", {
        "name": "describe_toolset", "arguments": {"toolset_name": READ_TOOLSET},
    })
    registry_schema = json.loads(registry_reply["content"][0]["text"])
    read_tools = {row["name"].rsplit(".", 1)[-1]: row for row in registry_schema["tools"]}
    index_tool = read_tools.get("ReadTurboMCPToolIndex")
    check("index keeps a zero-argument schema",
          index_tool is not None and not index_tool.get("inputSchema", {}).get("properties"))
    read_index_count = next((int(count) for name, _, count in toolset_lines if name == READ_TOOLSET), None)
    check("read toolset count agrees with its exact registry schema", read_index_count == len(read_tools))

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
    for tool_name in ("x", "", "find:0:graph"):
        bad = s.call(
            READ_TOOLSET, "ReadTurboMCPToolHelp",
            {"toolsetName": "NoSuchToolset", "toolName": tool_name},
        )
        check("unknown toolset refuses exact help, list and search: " + repr(tool_name),
              bad.startswith("error: no toolset named") and "ReadTurboMCPToolIndex" in bad
              and len(bad) <= HELP_BUDGET, bad)

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
    check("unknown help gives a scoped next step", "same ToolsetName and an empty ToolName" in bad2)

    print("=== a typo in a real MCP dispatch ===")
    typo_reply = s.rpc("tools/call", {
        "name": "call_tool",
        "arguments": {"toolset_name": READ_TOOLSET, "tool_name": "GraphLod", "arguments": {}},
    }, mutates=False)
    typo = typo_reply["content"][0]["text"]
    check("dispatch refuses the unknown bare name", typo_reply.get("isError") and "Unknown tool 'GraphLod'" in typo)
    check("dispatch suggests the exact scoped name", "Did you mean ReadTurboMCPGraphLoad" in typo)
    check("dispatch gives actionable discovery without restart advice",
          "ReadTurboMCPToolHelp" in typo and "restart" not in typo.lower())

    automation = "AutomationTestToolset.AutomationTestToolset"
    test_name = "TurboMCP.Discovery.UnknownTools"
    s.call(automation, "DiscoverTests", {})
    policy_report = json.loads(s.call(automation, "RunTests", {"testNames": [test_name]}))
    policy_tests = policy_report.get("tests", [])
    check("native tests verify denied names and preserve real policy refusals",
          policy_report.get("passed") == 1 and len(policy_tests) == 1
          and policy_tests[0].get("name") == test_name and policy_tests[0].get("state") == "Success"
          and policy_tests[0].get("errors") == [], str(policy_report))

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
    first_names = re.findall(r"^(?:Read|Execute[^\n]*?\)) (ReadTurboMCP\w+) -", summary, re.M)
    first_cursor = re.search(r'ToolName="list:(\d+)"', summary)
    check("first list page continues after the rows it showed",
          "offset 0" in summary and first_cursor is not None
          and 0 < len(first_names) == int(first_cursor.group(1)))

    print("=== paged help and search ===")
    expected = set(read_tools)
    seen = set()
    cursor = ""
    for page_number in range(100):
        page = s.call(READ_TOOLSET, "ReadTurboMCPToolHelp", {"toolsetName": READ_TOOLSET, "toolName": cursor})
        check("page stays within help budget", len(page) <= HELP_BUDGET)
        names = re.findall(r"^(?:Read|Execute[^\n]*?\)) (ReadTurboMCP\w+) -", page, re.M)
        check("page makes progress without duplicates", bool(names) and not seen.intersection(names))
        seen.update(names)
        next_page = re.search(r'ToolName="(list:\d+)"', page)
        if not next_page:
            check("pagination ends explicitly", "End of toolset." in page)
            break
        cursor = next_page.group(1)
    else:
        check("pagination terminates", False)
    check("all read-tool entries are reachable", seen == expected, str(sorted(seen.symmetric_difference(expected))))
    for cursor in ("list:-1", "list:99999999", "find:-1:graph", "find:0:"):
        refused = s.call(READ_TOOLSET, "ReadTurboMCPToolHelp", {"toolsetName": READ_TOOLSET, "toolName": cursor})
        check("invalid cursor/query refused: " + cursor, refused.startswith("error:"))
    search = s.call(READ_TOOLSET, "ReadTurboMCPToolHelp", {"toolsetName": "", "toolName": "find:0:TurboMCP"})
    print(search)
    check("search obeys compact budget", len(search) <= HELP_BUDGET)
    check("search provides exact names and continuation", "TurboMCP." in search and "next:" in search)
    absent = s.call(READ_TOOLSET, "ReadTurboMCPToolHelp", {"toolsetName": "", "toolName": "find:0:NoSuchToolUnique987"})
    check("empty search is explicit", "no registered tools match" in absent)

    print("=== status, schema and window metadata ===")
    status = s.call(READ_TOOLSET, "ReadTurboMCPStatus")
    print(status)
    check("status distinguishes declaration from registration", "declared toolsets" in status and "registered:" in status)
    check("status does not invent revision provenance", "source revision: unknown" in status)
    schema = s.call(READ_TOOLSET, "ReadTurboMCPToolSchema", {"toolset": "Read", "tool": "ReadTurboMCPStatus"})
    print(schema)
    check("schema announces omitted details", "Compact summary:" in schema and "constraints may be omitted" in schema and "describe_toolset" in schema)
    check("schema stays within budget", len(schema) <= HELP_BUDGET)
    check("schema resolves exact tool with short toolset", "ReadTurboMCPStatus" in schema and not schema.startswith("error:"))
    refused = s.call(READ_TOOLSET, "ReadTurboMCPToolSchema", {"toolset": "Read", "tool": "NoSuchTool"})
    check("schema refuses unknown tool", refused.startswith("error:"))
    preview_help = s.call(READ_TOOLSET, "ReadTurboMCPToolHelp", {"toolsetName": PREVIEW_TOOLSET, "toolName": "PreviewTurboMCPToolset"})
    check("preview help describes schema sizes accurately", "Report registered schema sizes" in preview_help and "flags any toolset" not in preview_help)
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
