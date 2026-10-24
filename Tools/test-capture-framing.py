"""Check capture dimensions, retained files and framing refusals in a rendered editor."""
import re
import struct
from pathlib import Path
from tmcp import Session, LOOK_TOOLSET

session = Session()
session.connect()
args = {"asset": "/Game/Tests/BP_NodeTest", "graphName": "EventGraph",
        "mode": "graph", "target": "", "size": ""}

def capture(size, asset=None):
    request = dict(args, size=size)
    if asset:
        request["asset"] = asset
    text = session.call(LOOK_TOOLSET, "LookTurboMCPCapture", request)
    print(text, flush=True)
    assert text.startswith("look: capture of "), text
    match = re.search(r"^image: (.+)$", text, re.M)
    assert match, text
    path = Path(match.group(1).strip())
    assert path.is_absolute() and path.is_file(), path
    data = path.read_bytes()
    assert data[:8] == b"\x89PNG\r\n\x1a\n", path
    dimensions = struct.unpack(">II", data[16:24])
    expected = (1600, 900) if not size else tuple(map(int, size.split("x")))
    assert dimensions == expected, (dimensions, expected)
    assert f"size: {expected[0]}x{expected[1]}" in text, text
    assert "frame bounds:" in text and "native Slate scratch graph" in text, text
    return path, data

first, original = capture("")
second, _ = capture("")
assert first != second and first.read_bytes() == original, "Capture overwrote prior evidence"
capture("800x400")
capture("800x600", "/Game/Tests/WBP_NodeTest")
small = session.call(LOOK_TOOLSET, "LookTurboMCPCapture", dict(args, size="1x1"))
print(small, flush=True)
assert small.startswith("error:") and "zoom range" in small, small
assert "image:" not in small, small
print("RESULT: PASS; dimensions, retained files and small-frame refusal checked. Pixel review remains separate.", flush=True)

# A catalogue is a capability query, not historical capture evidence.
catalogue = session.call(LOOK_TOOLSET, "LookTurboMCPCapture",
                         dict(args, asset="", mode="asset-editor"))
print(catalogue, flush=True)
for kind in ("Blueprint", "Anim Blueprint", "Material", "Material Instance",
             "Material Function", "Widget Blueprint (Designer)", "DataTable",
             "Anim Sequence", "Niagara system"):
    assert kind in catalogue, (kind, catalogue)
assert "not capture acceptance" in catalogue, catalogue
assert "may load editor modules" in catalogue, catalogue
assert "MEASURED" not in catalogue and "distinct colours" not in catalogue, catalogue
assert "1664x849" not in catalogue, catalogue
print("RESULT: PASS; catalogue describes capability without historical evidence claims.", flush=True)
