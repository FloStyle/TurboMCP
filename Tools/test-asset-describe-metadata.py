"""Find and describe unloaded Blueprint assets without editing or loading them."""
import re
from tmcp import Session, READ_TOOLSET
s = Session()
s.connect()
for tool, parameters in (("ReadTurboMCPAssetSearch", ("query",)),
                         ("ReadTurboMCPAssetDescribe", ("assets",)),
                         ("ReadTurboMCPAssetCost", ("asset", "cap"))):
    help_text = s.call(READ_TOOLSET, "ReadTurboMCPToolHelp",
                       {"toolsetName": READ_TOOLSET, "toolName": tool})
    print(help_text, flush=True)
    assert tool in help_text and not help_text.startswith("error:"), help_text
    for parameter in parameters:
        assert parameter in help_text.lower(), help_text

for asset in ("/Game/Tests/BP_NodeTest", "/Game/Tests/WBP_NodeTest"):
    query = "path:/Game/Tests " + asset.rsplit("/", 1)[-1]
    found = s.call(READ_TOOLSET, "ReadTurboMCPAssetSearch", {"query": query})
    print(found, flush=True)
    assert not found.startswith(("error:", "partial:")), found
    assert asset + "." in found, found
    assert found == s.call(READ_TOOLSET, "ReadTurboMCPAssetSearch", {"query": query}), found
    first = s.call(READ_TOOLSET, "ReadTurboMCPAssetDescribe", {"assets": asset})
    second = s.call(READ_TOOLSET, "ReadTurboMCPAssetDescribe", {"assets": asset})
    print(first, flush=True)
    assert not first.startswith(("error:", "!!")), first
    assert "loaded: no" in first and "loaded: no" in second, (first, second)
    assert first == second, (first, second)
    assert "this asset kind has no generated class" not in first, first
    assert "generated class: unknown" in first or "_C" in first, first
    assert "parent:" in first, first
# A stock material is a known non-Blueprint kind; describing it must not load it.
material = "/Engine/EngineMaterials/DefaultMaterial"
first = s.call(READ_TOOLSET, "ReadTurboMCPAssetDescribe", {"assets": material})
second = s.call(READ_TOOLSET, "ReadTurboMCPAssetDescribe", {"assets": material})
assert "generated class: not applicable (non-Blueprint asset)" in first, first
assert first == second, (first, second)
# Exercise the public MCP defaults, not explicit empty-string workarounds.
index = s.call(READ_TOOLSET, "ReadTurboMCPToolHelp", {})
assert "Available toolsets:" in index and READ_TOOLSET in index, index
blueprint = s.call(READ_TOOLSET, "ReadTurboMCPBlueprintLoad", {"assets": "/Game/Tests/BP_NodeTest"})
assert "read: 1 of 1 asset(s)" in blueprint, blueprint
graph_args = {"asset": "/Game/Tests/BP_NodeTest", "graphName": "EventGraph"}
graph = s.call(READ_TOOLSET, "ReadTurboMCPGraphLoad", graph_args)
assert not graph.startswith("error:") and "EventGraph" in graph, graph
later = s.call(READ_TOOLSET, "ReadTurboMCPGraphLoad", {**graph_args, "offset": 1})
assert "subsequent pages require" in later, later
stale = s.call(READ_TOOLSET, "ReadTurboMCPGraphLoad", {**graph_args, "revision": "stale-token"})
assert "changed since the previous page" in stale, stale
for query in ("", "type:", "path:"):
    refused = s.call(READ_TOOLSET, "ReadTurboMCPAssetSearch", {"query": query})
    assert refused.startswith("error:"), refused
missing = s.call(READ_TOOLSET, "ReadTurboMCPAssetSearch", {"query": "NoSuchAssetUnique987654"})
assert "0 match(es)" in missing and "Nothing matched" in missing, missing
mixed = s.call(READ_TOOLSET, "ReadTurboMCPAssetDescribe", {"assets": "/Game/Tests/BP_NodeTest|/Game/NoSuchAssetUnique987654"})
assert "1 described, 1 refused" in mixed, mixed
print("RESULT: PASS; search/refusal and unloaded Blueprint descriptions checked.")
