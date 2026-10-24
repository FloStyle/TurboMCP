#!/usr/bin/env python3
"""Focused proof for existing-target and refusal-safe graph repointing."""
import os, re, subprocess, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import tmcp_editor
from tmcp import EDIT_TOOLSET, PREVIEW_TOOLSET, READ_TOOLSET, Session

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ASSET, GRAPH = "TurboMCPAcceptance", "AppendChainProbe"

def main():
    ok, detail = tmcp_editor.rebuild_fixture("make-acceptance-asset.ps1", ["TURBOMCP_REPOINT_REBUILD"])
    if ok is False:
        print("RESULT: FAIL (fixture rebuild: %s)" % detail); return 1
    subprocess.call(["pwsh", "-NoProfile", "-File", os.path.join(REPO, "Tools", "start-editor.ps1")], cwd=REPO)
    session = None
    for _ in range(60):
        try:
            session = Session(); session.connect(); break
        except Exception: time.sleep(3)
    if session is None:
        print("RESULT: FAIL (editor connection)"); return 1
    read_args = {"asset": ASSET, "graphName": GRAPH, "detail": "full", "format": "read", "offset": 0, "maxChars": 24000, "revision": "", "entryFilter": ""}
    before = session.call(READ_TOOLSET, "ReadTurboMCPGraphLoad", read_args)
    if before.startswith("error:") or "!! isError" in before:
        print("RESULT: FAIL (graph read failed: %s)" % before); return 1
    keys = re.findall(r"/Game/Tests/TurboMCPAcceptance\.TurboMCPAcceptance:%s:([0-9a-f]{6}) ; class K2Node_CallFunction" % GRAPH, before)
    if len(keys) < 3:
        print("RESULT: FAIL (fixture lacks call nodes)"); return 1
    source = "/Game/Tests/TurboMCPAcceptance.TurboMCPAcceptance:%s:%s" % (GRAPH, keys[0])
    target = "/Game/Tests/TurboMCPAcceptance.TurboMCPAcceptance:%s:%s" % (GRAPH, keys[2])
    args = {"asset": ASSET, "graphName": GRAPH, "sourceNodeId": source, "sourcePin": "then", "targetNodeId": target, "targetTypeId": "", "targetPin": "then", "keepSource": "true"}
    preview = session.call(PREVIEW_TOOLSET, "PreviewTurboMCPGraphRepoint", args)
    refusal_args = dict(args); refusal_args["targetPin"] = "NoSuchPin"
    refusal = session.call(PREVIEW_TOOLSET, "PreviewTurboMCPGraphRepoint", refusal_args)
    after_preview = session.call(READ_TOOLSET, "ReadTurboMCPGraphLoad", read_args)
    refused_edit = session.call(EDIT_TOOLSET, "EditTurboMCPGraphRepoint", refusal_args)
    after_refusal = session.call(READ_TOOLSET, "ReadTurboMCPGraphLoad", read_args)
    print("PREVIEW:", preview, "REFUSAL:", refusal, "REFUSED EDIT:", refused_edit, sep="\n")
    edit = session.call(EDIT_TOOLSET, "EditTurboMCPGraphRepoint", args)
    after = session.call(READ_TOOLSET, "ReadTurboMCPGraphLoad", read_args)
    print("EDIT:", edit, "AFTER:", after, sep="\n")
    checks = [
        before == after_preview,
        before == after_refusal,
        '"outcome":"refused"' in refused_edit.replace(" ", ""),
        preview.startswith("preview:") and "reader(s) would move" in preview,
        "unchanged:" in refusal and "NoSuchPin" in refusal,
        edit.startswith("changed:") and "1 reader(s)" in edit,
        (":%s)" % keys[2]) in after,
    ]
    for ok in checks: print("  %s" % ("ok" if ok else "FAIL"))
    print("RESULT: %s" % ("PASS" if all(checks) else "FAIL"))
    return 0 if all(checks) else 1

if __name__ == "__main__": sys.exit(main())
