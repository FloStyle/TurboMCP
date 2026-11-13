"""Run the focused native bulk export framing and file workflow."""
import runpy
from pathlib import Path
from tmcp import Session, READ_TOOLSET
import json

support = runpy.run_path(str(Path(__file__).with_name("test-widget-roundtrip.py")))
session = Session()
session.connect()
name = "TurboMCP.Read.BulkExportFraming"
session.call(support["AUTO_TOOLSET"], "DiscoverTests", {})
raw = session.call(support["AUTO_TOOLSET"], "RunTests", {"testNames": [name]})
print(raw)
entry, report = support["parse_run_tests_report"](raw, name)
assert not entry["warnings"], entry
# Exercise the reflected tool for all three reader contracts, including the widget trailer.
root = (Path(__file__).resolve().parents[1] / "TestProject/Saved/TurboMCP/Exports").resolve()
cases = [
    ("ReadTurboMCPRefsBatch", {"assets": "/Game/Tests/BP_NodeTest|/Game/Tests/TA11_TestEnum", "pageSize": 1}, True),
    ("ReadTurboMCPNodeFind", {"asset": "/Game/Tests/BP_NodeTest", "words": "NO_MATCH_BULK_EXPORT_7D86", "graphName": "", "scope": "asset"}, True),
    ("ReadTurboMCPWidgetInventory", {"rules": '{"class":"/Script/UMG.Widget"}', "folder": "/Game/Tests", "loadUnloaded": False}, None),
]
for reader, query, expected_finished in cases:
    summary = json.loads(session.call(READ_TOOLSET, "ReadTurboMCPBulkExport", {
        "reader": reader, "queryJSON": json.dumps(query), "maxPages": 100,
    }))
    path = Path(summary["path"]).resolve()
    assert path.parent == root, path
    try:
        blob = path.read_bytes()
        document = json.loads(blob)
        assert len(blob) == summary["bytes"]
        assert document["reader"] == reader and document["pages"]
        assert summary["pageCount"] == len(document["pages"])
        if expected_finished is not None:
            assert document["traversalFinished"] == expected_finished, document
        if reader.endswith("WidgetInventory"):
            assert all("result" in page for page in document["pages"])
            if any(page["data"].get("skipped", 0) or page["data"].get("errors", 0) for page in document["pages"]):
                assert document["complete"] is False
    finally:
        path.unlink(missing_ok=True)
print("RESULT: PASS")

