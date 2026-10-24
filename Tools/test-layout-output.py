"""Validate capture numeric refusals and layout text on a disposable fixture using a rendered editor."""
from tmcp import Session, LOOK_TOOLSET
session = Session()
session.connect()
args = {"asset": "/Game/Tests/BP_NodeTest", "graphName": "EventGraph"}
report = session.call(LOOK_TOOLSET, "LookTurboMCPLayoutReport", args)
assert report.startswith("look:"), report
assert "not the rendered wire curve" in report and "not a visual-quality score" in report, report
positions = session.call(LOOK_TOOLSET, "LookTurboMCPGraphPositions", args)
assert "node positions" in positions and "width=" in positions, positions
for value in (-1, 1025):
    result = session.call(LOOK_TOOLSET, "LookTurboMCPLayoutReport", dict(args, alignmentTolerance=value))
    assert result.startswith("error:") and "0 and 1024" in result, result
# Invalid input must stop DoneCheck before capture is attempted.
refused = session.call(LOOK_TOOLSET, "LookTurboMCPLayoutDoneCheck", dict(args, alignmentTolerance=-1))
assert refused.startswith("error:") and "full-graph capture" not in refused, refused
# Numeric refusals happen before asset resolution or opening an editor.
for size in ("1.5x900", "+1600x900", "-1x900", "0x900", "4097x900", "9" * 80 + "x900", "1600x0"):
    result = session.call(LOOK_TOOLSET, "LookTurboMCPCapture", dict(args, mode="graph", target="", size=size))
    assert result.startswith("error:") and "whole-number width and height" in result, (size, result)
for size in ("-1", "+1", "1.5", "4097", "9" * 80):
    result = session.call(LOOK_TOOLSET, "LookTurboMCPCapture", dict(args, mode="neighbours", target="invalid", size=size))
    assert result.startswith("error:") and "whole-number hop count" in result, (size, result)
# An empty asset has a stable resolver refusal, allowing boundary checks without images.
for mode, sizes in (("graph", ("", "1x1", "4096,4096", " 1600 x 900 ")), ("neighbours", ("", "0", "4096"))):
    baseline = session.call(LOOK_TOOLSET, "LookTurboMCPCapture", dict(args, asset="", mode=mode, target="invalid", size=""))
    assert baseline.startswith("error:"), baseline
    for size in sizes:
        result = session.call(LOOK_TOOLSET, "LookTurboMCPCapture", dict(args, asset="", mode=mode, target="invalid", size=size))
        assert result == baseline, (mode, size, result, baseline)
# Both public box modes must reject malformed indices before resolving an asset.
for mode in ("box", "box-neighbours"):
    for target in ("1.5", "+1", "-1", "2147483648", "9" * 80):
        result = session.call(LOOK_TOOLSET, "LookTurboMCPCapture", dict(args, asset="", mode=mode, target=target, size=""))
        assert result.startswith("error:") and "Target must be a comment box index" in result, (mode, target, result)
    baseline = session.call(LOOK_TOOLSET, "LookTurboMCPCapture", dict(args, asset="", mode=mode, target="0", size=""))
    assert baseline.startswith("error:") and "Target must be" not in baseline, baseline
    result = session.call(LOOK_TOOLSET, "LookTurboMCPCapture", dict(args, asset="", mode=mode, target="2147483647", size=""))
    assert result == baseline, (mode, result, baseline)
print("Box indices: 10 malformed inputs refused; zero and int32 maximum accepted for both modes")
print("Capture numeric checks: 12 refusals and 7 accepted-input controls passed")
print(report[:1500])
print("RESULT: PASS", flush=True)
