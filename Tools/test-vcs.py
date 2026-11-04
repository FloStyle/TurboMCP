"""Check source-control state and checkout responses in the owned editor."""
import runpy
import hashlib
from pathlib import Path
from tmcp import Session, READ_TOOLSET, PREVIEW_TOOLSET, EDIT_TOOLSET

support = runpy.run_path(str(Path(__file__).with_name("test-widget-roundtrip.py")))
name = "TurboMCP.SourceControl.Quality"
session = Session()
session.connect()
for toolset, tool in ((READ_TOOLSET, "ReadTurboMCPVcsState"), (PREVIEW_TOOLSET, "PreviewTurboMCPVcsCheckout"), (EDIT_TOOLSET, "EditTurboMCPVcsCheckout")):
    help_text = session.call(READ_TOOLSET, "ReadTurboMCPToolHelp", {"toolset": toolset, "tool": tool})
    print(help_text)
    for phrase in ("assets", "provider", "state"):
        assert phrase in help_text.lower(), f"{tool} help lacks {phrase}"
content = Path(__file__).resolve().parents[1] / "TestProject" / "Content"
def asset_hashes():
    return {str(p.relative_to(content)): hashlib.sha256(p.read_bytes()).hexdigest() for p in content.rglob("*") if p.is_file()}
before = asset_hashes()
session.call(support["AUTO_TOOLSET"], "DiscoverTests", {})
raw = session.call(support["AUTO_TOOLSET"], "RunTests", {"testNames": [name]})
print(raw)
entry, report = support["parse_run_tests_report"](raw, name)
if entry["warnings"]:
    raise RuntimeError("Unexpected native warnings: " + repr(entry["warnings"]))
assert asset_hashes() == before, "Source-control workflow changed asset files"
print("RESULT: PASS")
