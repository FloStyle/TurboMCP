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

AUTO_TOOLSET = "AutomationTestToolset.AutomationTestToolset"
SHARED_VALUE_TEST = "TurboMCP.GraphText.SharedValueRepair"

# THE PINNED `AutomationTestToolset` REPORT SHAPE (5.8.3). The accepted
# graph-append suite is the reference for this shape; this suite parses it
# strictly rather than searching the report for words.
RUN_REPORT_KEYS = {"tests", "passed", "failed", "skipped", "total", "duration"}
TEST_ENTRY_KEYS = {"name", "state", "duration", "errors", "warnings"}
AUTOMATION_STATES = {"NotRun", "InProcess", "Fail", "Success", "Skipped"}

failures = []


def check(name, ok, detail=""):
    print("  %-4s %s%s" % ("ok" if ok else "FAIL", name, ("  " + detail) if detail else ""))
    if not ok:
        failures.append(name)


def _is_count(value):
    return type(value) is int and value >= 0


def parse_run_tests_report(payload, expected_test):
    """Parse the pinned RunTests JSON for exactly one requested test."""
    if not isinstance(payload, str):
        raise ValueError("the RunTests answer is not text")
    try:
        report = json.loads(payload)
    except ValueError as exc:
        raise ValueError("the RunTests answer is not JSON: %s" % exc)
    if not isinstance(report, dict):
        raise ValueError("the RunTests answer is not a JSON object")
    if set(report) != RUN_REPORT_KEYS:
        raise ValueError("unexpected report keys: %s" % sorted(set(report) ^ RUN_REPORT_KEYS))
    for key in ("passed", "failed", "skipped", "total"):
        if not _is_count(report[key]):
            raise ValueError("%s is not a non-negative integer" % key)
    if not isinstance(report["duration"], (int, float)) or isinstance(report["duration"], bool):
        raise ValueError("duration is not a number")
    tests = report["tests"]
    if not isinstance(tests, list):
        raise ValueError("tests is not an array")
    if len(tests) != 1:
        raise ValueError("the report holds %d test entries; exactly one was requested" % len(tests))
    entry = tests[0]
    if not isinstance(entry, dict):
        raise ValueError("the test entry is not an object")
    if set(entry) != TEST_ENTRY_KEYS:
        raise ValueError("unexpected test-entry keys: %s" % sorted(set(entry) ^ TEST_ENTRY_KEYS))
    if entry["name"] != expected_test:
        raise ValueError("the report names %r, not the requested %r" % (entry["name"], expected_test))
    if entry["state"] not in AUTOMATION_STATES:
        raise ValueError("unknown automation state %r" % (entry["state"],))
    for key in ("errors", "warnings"):
        values = entry[key]
        if not isinstance(values, list) or any(not isinstance(value, str) for value in values):
            raise ValueError("%s is not an array of strings" % key)
    if not isinstance(entry["duration"], (int, float)) or isinstance(entry["duration"], bool):
        raise ValueError("the test duration is not a number")
    if report["total"] != len(tests):
        raise ValueError("total %d does not match %d test entries" % (report["total"], len(tests)))
    expected_counts = {"Success": 0, "Fail": 0, "Skipped": 0}
    expected_counts[entry["state"]] = 1
    if (report["passed"], report["failed"], report["skipped"]) != (
            expected_counts["Success"], expected_counts["Fail"], expected_counts["Skipped"]):
        raise ValueError("aggregate counts do not match the single test entry")
    if entry["state"] != "Success":
        raise ValueError("the requested test is %s, not Success" % entry["state"])
    if entry["errors"]:
        raise ValueError("the requested test reported %d error(s): %s"
                         % (len(entry["errors"]), entry["errors"][0][:160]))
    return entry, report


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

    print()
    print("=== the compiled shared-value automation is discovered and passes ===")
    # A MISSING or FAILED compiled test is a failure, not "nothing to check".
    session.call(AUTO_TOOLSET, "DiscoverTests", {})
    listed = str(session.call(AUTO_TOOLSET, "ListTests",
                              {"nameFilter": "TurboMCP.GraphText", "tagFilter": ""}))
    print(listed[:500])
    check("the shared-value automation test is discovered", SHARED_VALUE_TEST in listed, listed[:300])
    if SHARED_VALUE_TEST not in listed:
        check("the shared-value RunTests answer is the pinned report for exactly the requested test",
              False, "the test is not registered, so no check ran")
    else:
        raw = session.call(AUTO_TOOLSET, "RunTests", {"testNames": [SHARED_VALUE_TEST]})
        print(str(raw)[:1200])
        try:
            entry, report = parse_run_tests_report(raw, SHARED_VALUE_TEST)
        except (ValueError, TypeError) as exc:
            check("the shared-value RunTests answer is the pinned report for exactly the requested test",
                  False, str(exc)[:240])
        else:
            check("the shared-value RunTests answer is the pinned report for exactly the requested test",
                  True, "passed=%d failed=%d skipped=%d total=%d"
                  % (report["passed"], report["failed"], report["skipped"], report["total"]))
            check("the shared-value test reported Success", entry["state"] == "Success", entry["state"])
            check("the shared-value test reported no errors", not entry["errors"], entry["errors"][:1])

    print("")
    if failures:
        print("RESULT: FAIL (%d check(s) failed: %s)" % (len(failures), ", ".join(failures)))
        return 1
    print("RESULT: PASS (cycles, depth, large graph, survival, and normal call end verified)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
