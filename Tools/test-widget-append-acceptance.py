"""Engine acceptance for Widget Blueprint append through the real Plan/Apply.

The compiled fixture creates a transient Widget Blueprint with real tree,
slot, sibling-graph and function-graph state, then drives the production
Append Plan/Apply path. This wrapper only invokes and strictly parses the
pinned AutomationTestToolset report; it creates no fixture and saves nothing.
"""

import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from tmcp import Session  # noqa: E402

AUTO_TOOLSET = "AutomationTestToolset.AutomationTestToolset"
WIDGET_APPEND_TEST = "TurboMCP.Widget.AppendAcceptance"

RUN_REPORT_KEYS = {"tests", "passed", "failed", "skipped", "total", "duration"}
TEST_ENTRY_KEYS = {"name", "state", "duration", "errors", "warnings"}
AUTOMATION_STATES = {"NotRun", "InProcess", "Fail", "Success", "Skipped"}

failures = []


def _detail_text(detail):
    """Render arbitrary structured detail safely and boundedly."""
    if detail is None or detail == "" or detail == []:
        return ""
    if isinstance(detail, str):
        text = detail
    else:
        text = repr(detail)
    return text[:500]


def check(name, ok, detail=""):
    detail_text = _detail_text(detail)
    print("  %-4s %s%s" % ("ok" if ok else "FAIL", name,
                           ("  " + detail_text) if detail_text else ""))
    if not ok:
        failures.append(name)
    return bool(ok)


def run_self_check():
    """Exercise nonempty-list detail without connecting to the editor."""
    saved = list(failures)
    failures.clear()
    try:
        result = check("self-check: nonempty list detail is stringified", False,
                       ["HTTP request timed out"])
        recorded = (result is False
                    and failures == ["self-check: nonempty list detail is stringified"])
    finally:
        failures[:] = saved
    if not recorded:
        print("RESULT: SELF-CHECK FAIL")
        return 1
    print("RESULT: SELF-CHECK PASS (failure remains recorded; normal failure path exits nonzero)")
    return 0


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


def main():
    if "--self-check" in sys.argv[1:]:
        return run_self_check()
    session = Session()
    session.connect()

    print("=== the compiled Widget append acceptance test is discovered ===")
    session.call(AUTO_TOOLSET, "DiscoverTests", {})
    listed = str(session.call(AUTO_TOOLSET, "ListTests",
                              {"nameFilter": "TurboMCP.Widget", "tagFilter": ""}))
    print(listed[:500])
    check("the Widget append acceptance test is discovered",
          WIDGET_APPEND_TEST in listed, listed[:300])
    if WIDGET_APPEND_TEST not in listed:
        check("the Widget append acceptance RunTests answer is the pinned report",
              False, "the test is not registered, so no check ran")
    else:
        raw = session.call(AUTO_TOOLSET, "RunTests", {"testNames": [WIDGET_APPEND_TEST]})
        print(str(raw)[:1600])
        try:
            entry, report = parse_run_tests_report(raw, WIDGET_APPEND_TEST)
        except (ValueError, TypeError) as exc:
            check("the Widget append acceptance RunTests answer is the pinned report",
                  False, str(exc)[:240])
        else:
            check("the Widget append acceptance RunTests answer is the pinned report",
                  True, "passed=%d failed=%d skipped=%d total=%d"
                  % (report["passed"], report["failed"], report["skipped"], report["total"]))
            check("the Widget append acceptance test reported Success",
                  entry["state"] == "Success", entry["state"])
            check("the Widget append acceptance test reported no errors",
                  not entry["errors"], entry["errors"][:1])
            check("the Widget append acceptance test reported no warnings",
                  not entry["warnings"], entry["warnings"][:1])

    if failures:
        print("RESULT: FAIL (%d check(s) failed: %s)" % (len(failures), ", ".join(failures)))
        return 1
    print("RESULT: PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
