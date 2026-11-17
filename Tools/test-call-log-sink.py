#!/usr/bin/env python3
# Copyright (c) 2026 FloStyle
# SPDX-License-Identifier: MPL-2.0
"""Run the isolated native call-log disk sink regression."""
import runpy
from pathlib import Path

from tmcp import Session

TEST = "TurboMCP.Diagnostics.CallLogSink"


def main():
    support = runpy.run_path(str(Path(__file__).with_name("test-widget-roundtrip.py")))
    session = Session()
    session.connect()
    toolset = support["AUTO_TOOLSET"]
    session.call(toolset, "DiscoverTests", {})
    raw = session.call(toolset, "RunTests", {"testNames": [TEST]})
    print(raw)
    entry, _ = support["parse_run_tests_report"](raw, TEST)
    if entry["warnings"]:
        raise RuntimeError("Unexpected native warnings: " + repr(entry["warnings"]))
    print("RESULT: PASS")


if __name__ == "__main__":
    main()
