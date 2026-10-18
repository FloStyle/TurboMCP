"""Compiled Widget Blueprint reader -> targeted append -> fresh reader acceptance.

The C++ fixture builds a transient Widget Blueprint in memory, obtains the
production graph-text reader output, targets one new PrintString link from that
read text, and also exercises a connected data input from an emitted pure-value
source before planning/applying and reading the graph again. This wrapper
only discovers and runs the exact test through the pinned AutomationTestToolset
and strictly parses its single report entry.
"""

import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from tmcp import Session  # noqa: E402

AUTO_TOOLSET = "AutomationTestToolset.AutomationTestToolset"
ROUNDTRIP_TEST = "TurboMCP.Widget.RoundtripAcceptance"

RUN_REPORT_KEYS = {"tests", "passed", "failed", "skipped", "total", "duration"}
TEST_ENTRY_KEYS = {"name", "state", "duration", "errors", "warnings"}
AUTOMATION_STATES = {"NotRun", "InProcess", "Fail", "Success", "Skipped"}

failures = []


def _detail_text(detail):
    if detail is None or detail == "" or detail == []:
        return ""
    text = detail if isinstance(detail, str) else repr(detail)
    return text[:500]


def check(name, ok, detail=""):
    rendered = _detail_text(detail)
    print("  %-4s %s%s" % ("ok" if ok else "FAIL", name,
                           ("  " + rendered) if rendered else ""))
    if not ok:
        failures.append(name)
    return bool(ok)


def _is_count(value):
    return type(value) is int and value >= 0


def parse_run_tests_report(payload, expected_test):
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
        saved = list(failures)
        failures.clear()
        try:
            result = check("self-check: list detail is safe", False, ["report noise"])
            recorded = (result is False
                        and failures == ["self-check: list detail is safe"])
        finally:
            failures[:] = saved
        print("RESULT: %s" % ("SELF-CHECK PASS" if recorded else "SELF-CHECK FAIL"))
        return 0 if recorded else 1

    session = Session()
    session.connect()

    print("=== the compiled Widget roundtrip test is discovered ===")
    session.call(AUTO_TOOLSET, "DiscoverTests", {})
    listed = str(session.call(AUTO_TOOLSET, "ListTests",
                              {"nameFilter": "TurboMCP.Widget", "tagFilter": ""}))
    print(listed[:500])
    check("the Widget roundtrip test is discovered", ROUNDTRIP_TEST in listed, listed[:300])
    if ROUNDTRIP_TEST not in listed:
        check("the Widget roundtrip RunTests answer is the pinned report",
              False, "the test is not registered, so no check ran")
    else:
        raw = session.call(AUTO_TOOLSET, "RunTests", {"testNames": [ROUNDTRIP_TEST]})
        print(str(raw)[:1600])
        try:
            entry, report = parse_run_tests_report(raw, ROUNDTRIP_TEST)
        except (ValueError, TypeError) as exc:
            check("the Widget roundtrip RunTests answer is the pinned report",
                  False, str(exc)[:240])
        else:
            check("the Widget roundtrip RunTests answer is the pinned report",
                  True, "passed=%d failed=%d skipped=%d total=%d"
                  % (report["passed"], report["failed"], report["skipped"], report["total"]))
            check("the Widget roundtrip test reported Success",
                  entry["state"] == "Success", entry["state"])
            check("the Widget roundtrip test reported no errors",
                  not entry["errors"], entry["errors"][:1])
            check("the Widget roundtrip test reported no warnings",
                  not entry["warnings"], entry["warnings"][:1])

    if failures:
        print("RESULT: FAIL (%d check(s) failed: %s)" % (len(failures), ", ".join(failures)))
        return 1
    print("RESULT: PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
