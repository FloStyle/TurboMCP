"""Opt-in disposable native import diagnostic; a crash is a failing result."""
import importlib.util
from pathlib import Path
from tmcp import Session

spec = importlib.util.spec_from_file_location("widget_report", Path(__file__).with_name("test-widget-roundtrip.py"))
report_parser = importlib.util.module_from_spec(spec)
spec.loader.exec_module(report_parser)

def main():
    session = Session()
    session.connect()
    toolset = "AutomationTestToolset.AutomationTestToolset"
    test = "TurboMCP.DataTable.ImportLifetimeDiagnostic"
    session.call(toolset, "DiscoverTests", {})
    raw = session.call(toolset, "RunTests", {"testNames": [test]})
    entry, report = report_parser.parse_run_tests_report(raw, test)
    if entry["warnings"]:
        raise RuntimeError(entry["warnings"])
    print("RESULT: PASS")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
