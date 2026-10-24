"""Validate layout text on a disposable fixture using a rendered editor."""
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
print(report[:1500])
print("RESULT: PASS", flush=True)
