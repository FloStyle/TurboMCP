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
for query in ("", "type:", "path:"):
    refused = s.call(READ_TOOLSET, "ReadTurboMCPAssetSearch", {"query": query})
    assert refused.startswith("error:"), refused
missing = s.call(READ_TOOLSET, "ReadTurboMCPAssetSearch", {"query": "NoSuchAssetUnique987654"})
assert "0 match(es)" in missing and "Nothing matched" in missing, missing
mixed = s.call(READ_TOOLSET, "ReadTurboMCPAssetDescribe", {"assets": "/Game/Tests/BP_NodeTest|/Game/NoSuchAssetUnique987654"})
assert "1 described, 1 refused" in mixed, mixed
print("RESULT: PASS; search/refusal and unloaded Blueprint descriptions checked.")
