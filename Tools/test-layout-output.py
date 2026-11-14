"""Validate capture numeric refusals and layout text on a disposable fixture using a rendered editor."""
import re
from tmcp import Session, LOOK_TOOLSET
session = Session()
session.connect()
args = {"asset": "/Game/Tests/BP_NodeTest", "graphName": "EventGraph"}
report_args = dict(args, scope="")
report = session.call(LOOK_TOOLSET, "LookTurboMCPLayoutReport", report_args)
assert report.startswith("look:"), report
assert "not the rendered wire curve" in report and "not a visual-quality score" in report, report
# This fixture has eight links and no boxes; all ranked wires fit in the top ten.
counts = re.search(r"counts: \d+ non-comment node\(s\).*?, (\d+) pin-link wire\(s\), (\d+) comment box", report)
assert counts and int(counts[2]) == 0, report
assert "cross-box-wires:" not in report, report
summary = re.search(r"total wire length: ([0-9.]+)  \(longest single wire: ([0-9.]+)\)", report)
ranked = [float(value) for value in re.findall(r"^  \d+\. ([0-9.]+) units:", report, re.M)]
assert summary and ranked and len(ranked) == int(counts[1]) <= 10, report
assert abs(float(summary[2]) - max(ranked)) <= 0.6, report
assert abs(float(summary[1]) - sum(ranked)) <= 0.5 + 0.05 * len(ranked), report
positions = session.call(LOOK_TOOLSET, "LookTurboMCPGraphPositions", args)
assert "node positions" in positions and "width=" in positions, positions
for value in (-1, 1025):
    result = session.call(LOOK_TOOLSET, "LookTurboMCPLayoutReport", dict(report_args, alignmentTolerance=value))
    assert result.startswith("error:") and "0 and 1024" in result, result
# Invalid input must stop DoneCheck before capture is attempted.
refused = session.call(LOOK_TOOLSET, "LookTurboMCPLayoutDoneCheck", dict(report_args, alignmentTolerance=-1))
assert refused.startswith("error:") and "full-graph capture" not in refused, refused
refused = session.call(LOOK_TOOLSET, "LookTurboMCPLayoutDoneCheck", dict(report_args, scope="box:missing"))
assert refused.startswith("error:") and "capture (review aid" not in refused, refused
scoped = session.call(LOOK_TOOLSET, "LookTurboMCPLayoutDoneCheck", dict(report_args, scope="region:0,0,400,300"))
assert scoped.startswith("look:") and "scoped capture (review aid" in scoped, scoped
assert "explicit graph-coordinate region" in scoped, scoped
node = re.search(r"^([^\s]+) x=-?\d+ y=-?\d+ width=", positions, re.M)
assert node, positions
scoped_node = session.call(LOOK_TOOLSET, "LookTurboMCPLayoutDoneCheck", dict(report_args, scope="node:" + node[1]))
assert scoped_node.startswith("look:") and "scoped capture (review aid" in scoped_node, scoped_node
assert "error:" not in scoped_node, scoped_node
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
