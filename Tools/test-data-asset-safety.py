"""Reproduce stock writer behavior on disposable data assets; no user assets are edited."""
import runpy
from pathlib import Path
from tmcp import Session
support = runpy.run_path(str(Path(__file__).with_name("test-widget-roundtrip.py")))
session = Session()
session.connect()
name = "TurboMCP.DataAsset.StockWriterSafety"
session.call(support["AUTO_TOOLSET"], "DiscoverTests", {})
raw = session.call(support["AUTO_TOOLSET"], "RunTests", {"testNames": [name]})
print(raw)
path = Path(__file__).resolve().parents[1] / "TestProject/Saved/TurboMCP/Workflow/data-asset-stock-writer.txt"
if path.exists():
    print(path.read_text(encoding="utf-8-sig"))
support["parse_run_tests_report"](raw, name)
print("RESULT: PASS")
