"""Check the live property reader schema and run the native property workflow."""
import json
import runpy
from pathlib import Path
from tmcp import READ_TOOLSET, Session

support = runpy.run_path(str(Path(__file__).with_name("test-widget-roundtrip.py")))
session = Session()
session.connect()
tool = "ReadTurboMCPDataAssetProperty"
discovery = session.rpc("tools/call", {
    "name": "describe_toolset", "arguments": {"toolset_name": READ_TOOLSET},
})
schema = json.loads(discovery["content"][0]["text"])
reader = next(row["inputSchema"] for row in schema["tools"] if row["name"].rsplit(".", 1)[-1] == tool)
assert set(reader["required"]) == {"asset", "property"}, reader
assert {key: item["type"] for key, item in reader["properties"].items()} == {
    "asset": "string", "property": "string", "mode": "string", "source": "string",
    "offset": "integer", "pageSize": "integer",
}, reader

missing = "/Game/Tests/NoSuchPropertyReaderAsset"
legacy = session.call(READ_TOOLSET, tool, {"asset": missing, "property": "Numbers"})
assert legacy.startswith("refused: error: no asset named"), legacy
response = session.call(READ_TOOLSET, tool, {
    "asset": missing,
    "property": "Numbers",
    "mode": "keys",
    "source": "asset",
    "offset": 0,
    "pageSize": 1,
})
page = json.loads(response.splitlines()[0])
assert (page["total"], page["returned"], page["knownErrors"], page["nextOffset"]) == (1, 1, 1, -1), page
assert page["rows"][0]["asset"] == missing and page["rows"][0]["code"] == "asset_unavailable", page

name = "TurboMCP.DataAsset.PropertyWorkflow"
session.call(support["AUTO_TOOLSET"], "DiscoverTests", {})
raw = session.call(support["AUTO_TOOLSET"], "RunTests", {"testNames": [name]})
print(raw)
support["parse_run_tests_report"](raw, name)
print("RESULT: PASS")
