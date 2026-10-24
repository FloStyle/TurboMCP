"""Check checkout status/refusal wording without submitting a valid asset checkout."""
from tmcp import Session, READ_TOOLSET, PREVIEW_TOOLSET, EDIT_TOOLSET
s = Session()
s.connect()
for toolset, tool in ((READ_TOOLSET, "ReadTurboMCPVcsState"), (PREVIEW_TOOLSET, "PreviewTurboMCPVcsCheckout"), (EDIT_TOOLSET, "EditTurboMCPVcsCheckout")):
    for assets in ("", "/Game/Tests/NoSuchAsset_CheckoutOutput"):
        result = s.call(toolset, tool, {"assets": assets})
        assert "=== source control state ===" in result, result
        assert "NO PROVIDER WAS REACHED" not in result and "NO FILE WAS WRITTEN" not in result, result
        if assets:
            assert "REFUSED:" in result, result
        if toolset == EDIT_TOOLSET:
            assert "measured before checkout" in result and "ReadTurboMCPVcsState" in result, result
        print(tool, "missing asset" if assets else "empty input", "PASS")
print("RESULT: PASS (6 status/refusal responses; no live checkout certified)")
