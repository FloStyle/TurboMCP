"""Exercise explicit Replace through registered MCP tools on SaneProbe."""

import json
import os
import re
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import tmcp_editor  # noqa: E402
from tmcp import PREVIEW_TOOLSET, READ_TOOLSET, EDIT_TOOLSET, Session  # noqa: E402

# THE FIXTURE IS A GRAPH THE HARNESS'S OWN SUITE REBUILDS, WHICH IS THE POINT.
#
# `test-graph-append` recorded the measurement that decides this: running the acceptance rebuild from INSIDE a
# suite took a full run from 37 of 39 to 32 of 39, because `check-probe-graphs` already regenerates this asset
# and two suites rebuilding one fixture is TWO OWNERS of it. So the default here is to USE the fixture and not
# to rebuild it, and `TURBOMCP_REPLACE_REBUILD=1` forces a rebuild when running by hand from a suspect tree.
REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ASSET = "TurboMCPAcceptance"
GRAPH = "SaneProbe"
ASSET_FILE = "TurboMCPAcceptance.uasset"

failures = []
checks = 0


def check(name, ok, detail=""):
    global checks
    checks += 1
    print("  %-4s %s%s" % ("ok" if ok else "FAIL", name, ("  " + str(detail)) if detail and not ok else ""))
    if not ok:
        failures.append(name)
    return ok


def asset_fingerprint():
    """The saved asset's size and mtime, which no in-memory claim can fake."""
    path = os.path.join(REPO, "TestProject", "Content", "Tests", ASSET_FILE)
    if not os.path.isfile(path):
        return None
    stat = os.stat(path)
    return (stat.st_size, stat.st_mtime_ns)


def node_keys(session, graph=GRAPH):
    """Every node key the READER prints, which is the independent witness for a removal."""
    out = session.call(READ_TOOLSET, "ReadTurboMCPGraphLoad",
                       {"asset": ASSET, "graphName": graph, "detail": "full",
                        "format": "read", "offset": 0, "maxChars": 24000})
    return set(re.findall(r"%s:%s:([0-9a-f]{6,32})" % (re.escape(ASSET), re.escape(graph)), out)), out


def connect_with_fixture():
    """
    Returns a connected Session, or None.

    THE FIXTURE IS RESTORED ON THE WAY IN, NOT ONLY ON THE WAY OUT, AND THAT IS THE ONLY OPTION HERE: this suite
    REPLACES a body, so the graph it leaves behind is not the one it found. Rebuilding first makes it
    re-runnable from ANY state, which is the property that matters - a run killed part-way leaves the same mess
    behind, and cleaning up at the end would not have helped.
    """
    # THE EDITOR LIFECYCLE IS NOT THIS SUITE'S BUSINESS ANY MORE, WHICH IS `Spec/WORKING_PLAN.md` ROW 0.1.
    # `Tools/tmcp_editor.py` owns stop, rebuild and ensure; ELEVEN SUITES USED TO RE-IMPLEMENT THESE THREE
    # SUBPROCESS CALLS WITH DIFFERENT OPT-IN VARIABLE NAMES, and that variation is what made one suite and the
    # harness disagree about who held the editor. THE PROMPT STILL SAYS `TURBOMCP_REPLACE_REBUILD`, because that
    # spelling has been in this suite's output for many rounds and a message that changes is a message nobody
    # greps for; the shared name works too, and the module names both when neither is set.
    ok, detail = tmcp_editor.rebuild_fixture(
        "make-acceptance-asset.ps1", ["TURBOMCP_REPLACE_REBUILD"])
    if ok is None:
        print("    fixture rebuild: not requested; using the committed SaneProbe (%s)" % detail)
    else:
        print("    fixture rebuild: %s" % detail)
        if not ok:
            print("    the acceptance asset could not be rebuilt, so this suite cannot run")
            return None

    started = subprocess.call(
        ["pwsh", "-NoProfile", "-File", os.path.join(REPO, "Tools", "start-editor.ps1")], cwd=REPO)
    if started != 0:
        print("    the editor launcher returned %s" % started)

    deadline = time.time() + 180
    while time.time() < deadline:
        try:
            session = Session()
            session.connect()
            return session
        except Exception:  # noqa: BLE001 - not up yet is the expected case
            time.sleep(3)
    return None


def edit_export(session):
    return session.call(READ_TOOLSET, "ReadTurboMCPGraphLoad",
                        {"asset": ASSET, "graphName": GRAPH, "format": "edit",
                         "detail": "bodies", "offset": 0, "maxChars": 24000})


def main():
    session = connect_with_fixture()
    if session is None:
        print("RESULT: FAIL (the editor did not start)")
        return 1

    before_keys, before_read = node_keys(session)
    disk_before = asset_fingerprint()
    document = edit_export(session)
    first, _, _ = document.partition("\n")
    if not first.startswith("; tmcp-edit-v1: "):
        print("RESULT: FAIL (missing edit envelope)", first)
        return 1
    header = json.loads(first.removeprefix("; tmcp-edit-v1: "))
    if not check("SaneProbe exports a complete graph", header.get("complete") is True, document):
        return 1
    check("Edit envelope carries graph identity and a SHA-256 digest",
          all(re.fullmatch(r"[a-f0-9]{%d}" % count, header[key])
              for key, count in (("graphGuid", 32), ("baseFingerprint", 64))), header)
    entries = re.findall(r'^\("Function\|Entry" ; ([a-f0-9]{32})\n', document, re.MULTILINE)
    calls = list(re.finditer(r'^\("[^"\n]*PrintString" ; ([a-f0-9]{32})\n([\s\S]*?)^\)\n',
                            document, re.MULTILINE))
    if not check("Exact entry and first call are present", len(entries) == 1 and bool(calls), document):
        return 1
    call = next((item for item in calls if f'(-> {entries[0]}:"then")' in item[2]), None)
    if not check("Find the call by its exact incoming entry link", call is not None, document):
        return 1
    old = call[1]
    body = document[:call.start()] + document[call.end():]
    body = body.replace(f"(-> {old}:", "(-> ReplacementCall:")
    body += ('(Development|PrintString ; label ReplacementCall\n'
             f' execute (-> {entries[0]}:then)\n'
             ' InString "replacement body"\n Duration 1.0\n)\n')
    directive = '; tmcp-delete-v1: {"nodeId":"' + old + '"}\n'

    def invoke(toolset, name, text):
        return session.call(toolset, name, {"asset": ASSET, "graphName": GRAPH, "text": text})

    try:
        refused = invoke(EDIT_TOOLSET, "EditTurboMCPGraphReplace", body)
        check("Omission does not authorize node deletion",
              refused.startswith("unchanged:") and "omitted semantic node" in refused, refused)
        check("Omission leaves the independent inspection unchanged", node_keys(session)[1] == before_read)

        preview = invoke(PREVIEW_TOOLSET, "PreviewTurboMCPGraphReplace", body + directive)
        check("Preview lists exactly the explicit removal",
              preview.startswith("preview:") and re.findall(r"would REMOVE ([a-f0-9]{32})", preview) == [old], preview)
        check("Preview leaves memory and disk unchanged",
              node_keys(session)[1] == before_read and asset_fingerprint() == disk_before)

        edited = invoke(EDIT_TOOLSET, "EditTurboMCPGraphReplace", body + directive)
        check("Edit removes the named node and adds one replacement",
              edited.startswith("changed:") and re.findall(r"REMOVED ([a-f0-9]{32})", edited) == [old], edited)
        after_keys, after_read = node_keys(session)
        check("Inspection reader sees one replacement and the existing entry",
              len(before_keys - after_keys) == len(after_keys - before_keys) == 1
              and any(old.startswith(key) for key in before_keys - after_keys)
              and any(entries[0].startswith(key) for key in before_keys & after_keys),
              (sorted(before_keys), sorted(after_keys)))
        check("Inspection reader sees new body literal and execution wire",
              "replacement body" in after_read
              and re.search(r'execute \(-> [^\n]*:' + entries[0][:6], after_read) is not None, after_read)
        check("Replace did not save the asset", asset_fingerprint() == disk_before)

        no_op = invoke(EDIT_TOOLSET, "EditTurboMCPGraphReplace", edit_export(session))
        check("A fresh complete export is a no-op", no_op.startswith("unchanged:") and "already matches" in no_op, no_op)
        check("No-op leaves independent inspection unchanged", node_keys(session)[1] == after_read)

        stale = invoke(EDIT_TOOLSET, "EditTurboMCPGraphReplace", body + directive)
        check("Previously applied document is stale", stale.startswith("unchanged:") and "stale" in stale, stale)
        check("Stale refusal preserves edited graph and disk",
              node_keys(session)[1] == after_read and asset_fingerprint() == disk_before)
    finally:
        print("=== giving the shared fixture back ===")
        if os.environ.get("TURBOMCP_RUNNER_OWNS_FIXTURES") == "1":
            restored_ok, restore_detail = tmcp_editor.restore_runner_fixture()
        else:
            subprocess.run(["pwsh", "-NoProfile", "-File", os.path.join(REPO, "Tools", "start-editor.ps1"), "-Stop"],
                           capture_output=True, text=True, cwd=REPO)
            time.sleep(3)
            restore = subprocess.run(
                ["pwsh", "-NoProfile", "-File", os.path.join(REPO, "Tools", "make-acceptance-asset.ps1")],
                capture_output=True, text=True, cwd=REPO)
            restored_ok = restore.returncode == 0 and "PASS" in (restore.stdout or "")
            restore_detail = (restore.stdout or restore.stderr)[-300:]
        check("The shared fixture was restored", restored_ok, restore_detail)

    if failures:
        print("RESULT: FAIL (%d) -> %s" % (len(failures), ", ".join(failures)))
        return 1
    print("RESULT: PASS (%d check(s) run)" % checks)
    return 0


if __name__ == "__main__":
    sys.exit(main())
