"""Regression coverage for bounded pure-value graph rendering.

The fixtures model the graph shapes that made RenderPinValue call RenderNode forever:
a value cycle, a self-link, and an acyclic value chain deeper than the declared
inline limit. LargeGraphProbe separately proves that a genuinely large body is
capped without tripping the exec-chain guard.
"""

import json
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from tmcp import EDIT_TOOLSET, READ_TOOLSET, Session  # noqa: E402

ASSET = "/Game/Tests/TurboMCPAcceptance"
LOG_PATH = os.path.join(
    os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
    "TestProject", "Saved", "Logs", "TurboMCPCalls.jsonl")

failures = []


def check(name, ok, detail=""):
    print("  %-4s %s%s" % ("ok" if ok else "FAIL", name, ("  " + detail) if detail else ""))
    if not ok:
        failures.append(name)


def read_graph(session, graph):
    return session.call(READ_TOOLSET, "ReadTurboMCPGraphLoad", {
        "asset": ASSET, "graphName": graph, "format": "", "detail": "bodies",
    })


def disk_lines():
    if not os.path.isfile(LOG_PATH):
        return []
    rows = []
    with open(LOG_PATH, "r", encoding="utf-8", errors="replace") as handle:
        for line in handle:
            try:
                rows.append(json.loads(line))
            except ValueError:
                pass
    return rows


def main():
    session = Session()
    session.connect()

    # Make the normal-end-result assertion independent of a previous suite's setting.
    session.call(EDIT_TOOLSET, "EditTurboMCPCallLog", {"enabled": "false"})
    session.call(EDIT_TOOLSET, "EditTurboMCPCallLogClear", {"confirm": "yes"})
    try:
        open(LOG_PATH, "w", encoding="utf-8").close()
    except OSError:
        pass
    session.call(EDIT_TOOLSET, "EditTurboMCPCallLog", {"enabled": "true"})

    print("=== value cycle and self-link return bounded readable bodies ===")
    cycle = read_graph(session, "ValueCycleProbe")
    self_link = read_graph(session, "SelfLinkProbe")
    depth = read_graph(session, "DepthLimitProbe")
    for name, text, marker in (
        ("value cycle", cycle, "value cycle"),
        ("self-link", self_link, "value cycle"),
        ("depth limit", depth, "depth limit"),
    ):
        check("%s returns a read result" % name,
              isinstance(text, str) and text.startswith("read: "), str(text)[:120])
        check("%s prints its guard reason" % name,
              "; inline expansion stopped: %s\n" % marker in text, text[:500])
        check("%s has a stable node reference" % name,
              re.search(r"\(-> [^)\n]+\)\n\s*; inline expansion stopped:", text) is not None,
              text[:500])
        text_length = len(text) if isinstance(text, str) else -1
        check("%s stays bounded" % name, 0 <= text_length < 24000, str(text_length))

    print("=== the large graph is not reduced to a tiny input ===")
    large = read_graph(session, "LargeGraphProbe")
    check("large graph returns a read result",
          isinstance(large, str) and large.startswith("read: "), str(large)[:120])
    node_count = re.search(r"(\d+) node\(s\)", large)
    check("large graph reports a genuinely large fixture",
          node_count is not None and int(node_count.group(1)) >= 1200,
          node_count.group(1) if node_count else large[:180])
    check("large graph reaches the normal output cap",
          "more character(s) were dropped" in large, large[-180:])
    check("large graph has no exec-chain guard error",
          "exec chain did not end" not in large, large[-300:])
    check("large graph result is bounded", len(large) < 25000, str(len(large)))

    print("=== the editor remains alive after all guarded reads ===")
    status = session.call(READ_TOOLSET, "ReadTurboMCPStatus", {})
    check("a subsequent normal call succeeds",
          isinstance(status, str) and not status.startswith("error:"), str(status)[:160])

    print("=== the call log contains a normal end result for the graph read ===")
    rows = disk_lines()
    graph_ends = [row for row in rows
                  if row.get("phase") == "end"
                  and row.get("source") == "plugin"
                  and "ReadTurboMCPGraphLoad" in row.get("tool", "")]
    check("the call log file was written", bool(rows), LOG_PATH)
    check("the graph read has an end record", bool(graph_ends), str(len(rows)))
    check("the graph read ended without an error",
          bool(graph_ends) and all(row.get("error") is False for row in graph_ends),
          str(graph_ends[-1] if graph_ends else ""))
    check("the end record has a result size",
          bool(graph_ends) and all(isinstance(row.get("result_chars"), (int, float))
                                   and row.get("result_chars") > 0 for row in graph_ends),
          str(graph_ends[-1] if graph_ends else ""))

    print("")
    if failures:
        print("RESULT: FAIL (%d check(s) failed: %s)" % (len(failures), ", ".join(failures)))
        return 1
    print("RESULT: PASS (cycles, depth, large graph, survival, and normal call end verified)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
