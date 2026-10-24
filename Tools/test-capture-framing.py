"""Check capture dimensions, retained files and framing refusals in a rendered editor."""
import re
import struct
from pathlib import Path
from tmcp import Session, LOOK_TOOLSET, READ_TOOLSET

session = Session()
session.connect()
def dirty(label):
    state = session.call(READ_TOOLSET, "ReadTurboMCPDirtyList", {"limit": ""})
    assert not state.startswith(("error:", "!!")), state
    print(f"DIRTY SNAPSHOT {label}:\n{state}", flush=True)
    return state
dirty("before vision calls")

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
    dirty(f"after graph capture {size or 'default'}")
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
                         dict(args, asset="", graphName="", mode="asset-editor"))
print(catalogue, flush=True)
assert not catalogue.startswith(("error:", "!!")), catalogue
for kind in ("Blueprint", "Anim Blueprint", "Material", "Material Instance",
             "Material Function", "Widget Blueprint (Designer)", "DataTable",
             "Anim Sequence", "Niagara system"):
    assert kind in catalogue, (kind, catalogue)
assert "not capture acceptance" in catalogue, catalogue
assert "may load editor modules" in catalogue, catalogue
assert "MEASURED" not in catalogue and "distinct colours" not in catalogue, catalogue
assert "1664x849" not in catalogue, catalogue
print("RESULT: PASS; catalogue describes capability without historical evidence claims.", flush=True)


# Resolve the full current ID; short asset-name IDs are no longer the reader contract.
found = session.call(READ_TOOLSET, "ReadTurboMCPNodeFind",
                     {"asset": args["asset"], "words": "BeginPlay", "graphName": "EventGraph"})
print(found, flush=True)
ids = re.findall(r"^\s*(/[^\s]+:EventGraph:[0-9a-fA-F]+)\s*$", found, re.M)
assert len(ids) == 1, found
root = ids[0]
for mode, target, size in (("node", root, ""), ("neighbours", root, "0"),
                           ("neighbours", root, "1"), ("region", "0,0,500,500", "")):
    result = session.call(LOOK_TOOLSET, "LookTurboMCPCapture",
                          dict(args, mode=mode, target=target, size=size))
    print(result, flush=True)
    assert result.startswith("look: capture of "), result
    path = Path(re.search(r"^image: (.+)$", result, re.M).group(1).strip())
    assert path.is_absolute() and path.is_file(), result
    assert path.read_bytes()[:8] == b"\x89PNG\r\n\x1a\n", path
    if mode == "neighbours":
        count = re.search(r"within (\d+) hop\(s\), (\d+) node\(s\)", result)
        assert count, result
        assert int(count[1]) == int(size), result
        assert (int(count[2]) == 1 if size == "0" else int(count[2]) > 1), result
    dirty(f"after {mode} {size}")
for tool in ("LookTurboMCPGraphPositions", "LookTurboMCPLayoutReport", "LookTurboMCPLayoutDoneCheck"):
    result = session.call(LOOK_TOOLSET, tool, {"asset": args["asset"], "graphName": "EventGraph"})
    print(f"{tool}:\n{result}", flush=True)
    assert not result.startswith(("error:", "!!")), result
    dirty(f"after {tool}")
print("RESULT: PASS; node, neighbourhood and region captures exercised; dirty-state observations recorded, not a universal no-side-effects claim.", flush=True)

probe = session.call(LOOK_TOOLSET, "LookTurboMCPCapture",
                     dict(args, mode="select-nodes", target=root + "," + root))
print(probe, flush=True)
assert not probe.startswith(("error:", "!!")), probe
assert "asking for 1 node(s): 1 node(s)" in probe, probe
assert "original selection restored: 0 object(s)" in probe, probe
empty_selection = session.call(LOOK_TOOLSET, "LookTurboMCPCapture", dict(args, mode="selection"))
assert empty_selection.startswith("error:"), empty_selection
dirty("after selection probe")
print("RESULT: PASS; duplicate probe IDs treated as a set and initial empty selection restored. Nonempty restoration still requires separate evidence.", flush=True)
