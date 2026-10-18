#!/usr/bin/env python3
"""Run the compiled panel-history monitoring regression through AutomationTestToolset.

The fixture is an isolated in-memory call log; the editor and its Slate application
are required only because the panel is a Slate widget. No asset, settings file or
production call log is touched. `--parser-only` validates the pinned report shape
without opening a Session or contacting an editor.
"""
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from tmcp import Session  # noqa: E402

AUTO_TOOLSET = "AutomationTestToolset.AutomationTestToolset"
TEST_NAME = "TurboMCP.Panel.History"

RUN_REPORT_KEYS = {"tests", "passed", "failed", "skipped", "total", "duration"}
TEST_ENTRY_KEYS = {"name", "state", "duration", "errors", "warnings"}


def check(name, ok, detail=""):
    print("  %s %s%s" % ("ok  " if ok else "FAIL", name,
                         ("  " + str(detail)[:240]) if detail else ""))
    return bool(ok)


def parse_report(payload):
    if not isinstance(payload, str):
        raise ValueError("RunTests answer is not text")
    report = json.loads(payload)
    if not isinstance(report, dict) or set(report) != RUN_REPORT_KEYS:
        raise ValueError("unexpected RunTests report keys")
    tests = report["tests"]
    if not isinstance(tests, list) or len(tests) != 1:
        raise ValueError("the report must contain exactly one test")
    entry = tests[0]
    if not isinstance(entry, dict) or set(entry) != TEST_ENTRY_KEYS:
        raise ValueError("unexpected test entry keys")
    if entry["name"] != TEST_NAME:
        raise ValueError("the report names a different test")
    if entry["state"] != "Success":
        raise ValueError("the test state is %s" % entry["state"])
    if entry["errors"]:
        raise ValueError("the test reported errors: %s" % entry["errors"][:1])
    for key in ("errors", "warnings"):
        if not isinstance(entry[key], list) or any(not isinstance(item, str) for item in entry[key]):
            raise ValueError("%s is not a string array" % key)
    if report["passed"] != 1 or report["failed"] or report["skipped"] or report["total"] != 1:
        raise ValueError("aggregate counts do not describe one successful test")
    return entry


def parser_checks():
    def report(**overrides):
        base = {
            "tests": [{"name": TEST_NAME, "state": "Success", "duration": 0.1,
                       "errors": [], "warnings": []}],
            "passed": 1, "failed": 0, "skipped": 0, "total": 1, "duration": 0.1,
        }
        base.update(overrides)
        return json.dumps(base)
    parse_report(report())
    bad = [
        report(total=2),
        report(passed=0),
        json.dumps({"tests": [], "passed": 0, "failed": 0, "skipped": 0, "total": 0,
                    "duration": 0}),
    ]
    for payload in bad:
        try:
            parse_report(payload)
        except (ValueError, KeyError, TypeError):
            continue
        raise AssertionError("a malformed RunTests report was accepted")


def main():
    parser_checks()
    if "--parser-only" in sys.argv:
        print("RESULT: PASS (parser checks only; no editor contacted)")
        return 0
    session = Session()
    session.connect()
    session.call(AUTO_TOOLSET, "DiscoverTests", {})
    listed = str(session.call(AUTO_TOOLSET, "ListTests",
                              {"nameFilter": TEST_NAME, "tagFilter": ""}))
    if not check("the panel history automation test is discovered", TEST_NAME in listed, listed):
        print("RESULT: FAIL")
        return 1
    raw = session.call(AUTO_TOOLSET, "RunTests", {"testNames": [TEST_NAME]})
    try:
        parse_report(raw)
    except (ValueError, KeyError, TypeError) as exc:
        check("the panel history automation test returned a strict success report", False, exc)
        print("RESULT: FAIL")
        return 1
    check("the panel history automation test returned a strict success report", True)
    print("RESULT: PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
