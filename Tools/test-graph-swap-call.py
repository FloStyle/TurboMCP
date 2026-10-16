#!/usr/bin/env python3
"""Focused proof for the dedicated named-pin call swap capability."""
import os
import re
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import tmcp_editor  # noqa: E402
from tmcp import EDIT_TOOLSET, PREVIEW_TOOLSET, READ_TOOLSET, Session  # noqa: E402

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ASSET = "TurboMCPAcceptance"
GRAPH = "AppendChainProbe"


def connect():
    ok, detail = tmcp_editor.rebuild_fixture("make-acceptance-asset.ps1", ["TURBOMCP_SWAP_CALL_REBUILD"])
    if ok is False:
        print("fixture rebuild failed: %s" % detail)
        return None
    subprocess.call(["pwsh", "-NoProfile", "-File", os.path.join(REPO, "Tools", "start-editor.ps1")], cwd=REPO)
    deadline = time.time() + 180
    while time.time() < deadline:
        try:
            session = Session()
            session.connect()
            return session
        except Exception:
            time.sleep(3)
    return None


def main():
    session = connect()
    if session is None:
        print("RESULT: FAIL (could not connect)")
        return 1
    before = session.call(READ_TOOLSET, "ReadTurboMCPGraphLoad", {
        "asset": ASSET, "graphName": GRAPH, "detail": "full", "format": "", "tokenBudget": "",
    })
    match = re.search(r"\(Development\|PrintString ; ([^\n]*:%s:([0-9a-f]{6})) ; class K2Node_CallFunction" % GRAPH, before)
    if not match:
        print("RESULT: FAIL (no PrintString call node in fixture)")
        return 1
    node_id = match.group(1)
    preview = session.call(PREVIEW_TOOLSET, "PreviewTurboMCPGraphSwapCall", {
        "asset": ASSET, "graphName": GRAPH, "nodeId": node_id,
        "replacementTypeId": "Development|PrintString", "keepOld": "true",
    })
    checks = [
        ("preview is unchanged", preview.startswith("unchanged:")),
        ("preview names migration", "links to migrate:" in preview),
        ("preview uses safe keepOld default", "keepOld: true" in preview),
    ]
    edited = session.call(EDIT_TOOLSET, "EditTurboMCPGraphSwapCall", {
        "asset": ASSET, "graphName": GRAPH, "nodeId": node_id,
        "replacementTypeId": "Development|PrintString", "keepOld": "true",
    })
    after = session.call(READ_TOOLSET, "ReadTurboMCPGraphLoad", {
        "asset": ASSET, "graphName": GRAPH, "detail": "full", "format": "", "tokenBudget": "",
    })
    replacement_match = re.search(r"to (/.+?:%s:[0-9a-f]{6});" % GRAPH, edited)
    replacement_id = replacement_match.group(1) if replacement_match else ""
    checks.extend([
        ("edit reports a changed swap", edited.startswith("changed:")),
        ("readback contains the replacement node", bool(replacement_id) and replacement_id in after),
        ("readback preserves the migrated execute link", bool(replacement_id) and (":%s)" % replacement_id.rsplit(":", 1)[-1]) in after),
    ])
    failures = [name for name, ok in checks if not ok]
    for name, ok in checks:
        print("  %s %s" % ("ok" if ok else "FAIL", name))
    if failures:
        print("RESULT: FAIL (%s)" % ", ".join(failures))
        return 1
    print("RESULT: PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
