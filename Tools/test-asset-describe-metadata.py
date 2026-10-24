"""Check AssetDescribe metadata without causing Blueprint loads."""
from tmcp import Session, READ_TOOLSET
s = Session()
s.connect()
for asset in ("/Game/Tests/BP_NodeTest", "/Game/Tests/WBP_NodeTest"):
    first = s.call(READ_TOOLSET, "ReadTurboMCPAssetDescribe", {"assets": asset})
    second = s.call(READ_TOOLSET, "ReadTurboMCPAssetDescribe", {"assets": asset})
    print(first, flush=True)
    assert not first.startswith(("error:", "!!")), first
    assert "loaded: no" in first and "loaded: no" in second, (first, second)
    assert first == second, (first, second)
    assert "this asset kind has no generated class" not in first, first
    assert "generated class: unknown" in first or "_C" in first, first
    assert "parent:" in first, first
print("RESULT: PASS; Blueprint descriptions remain unloaded and do not infer class absence.")
