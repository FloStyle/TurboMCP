#!/usr/bin/env python3
"""Focused proof for append-at-tail and safe insertion into an exec chain.

`AppendChainProbe` deliberately combines a branch/continuations, a b-prefixed
member-variable getter, and library calls.  The reader output is first passed
through GraphCheck, so this exercises the shared grammar before either write.
The test then appends after a tail and inserts between the then-arm head and
its former successor.  Every introduced wire is witnessed through the reader.
"""
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
FAILURES = []


def check(name, ok, detail=""):
    print("  %s %s%s" % ("ok  " if ok else "FAIL", name,
                          ("  " + str(detail)[:300]) if detail else ""))
    if not ok:
        FAILURES.append(name)


def read_graph(session):
    return session.call(READ_TOOLSET, "ReadTurboMCPGraphLoad", {
        "asset": ASSET, "graphName": GRAPH, "detail": "full",
        "format": "", "tokenBudget": "",
    })


def key_for_marker(text, marker):
    """Read a PrintString key from its marker rather than pinning fixture GUIDs."""
    lines = text.splitlines()
    for index, line in enumerate(lines):
        match = re.search(r"\(Development\|PrintString\s*;[^\n]*:%s:([0-9a-f]{6})" %
                          re.escape(GRAPH), line)
        if match and marker in "\n".join(lines[index:index + 10]):
            return match.group(1)
    return None


def has_wire(text, target_key, source_key):
    """The target's execute input is the independent, exact witness for one wire."""
    lines = text.splitlines()
    for index, line in enumerate(lines):
        if (":%s:%s" % (GRAPH, target_key)) not in line:
            continue
        block = "\n".join(lines[index:index + 12])
        if re.search(r"execute \(-> [^\s)]*:%s\)" % re.escape(source_key), block):
            return True
    return False


def append_text(marker, source_key):
    return ('(Development|PrintString\n'
            '    InString "%s"\n'
            '    execute (-> %s:%s:%s:then))' % (marker, ASSET, GRAPH, source_key))


def connect_fixture():
    ok, detail = tmcp_editor.rebuild_fixture("make-acceptance-asset.ps1", ["TURBOMCP_APPEND_CHAIN_REBUILD"])
    if ok is False:
        print("fixture rebuild failed: %s" % detail)
        return None
    print("fixture rebuild: %s" % detail)
    subprocess.call(["pwsh", "-NoProfile", "-File", os.path.join(REPO, "Tools", "start-editor.ps1")], cwd=REPO)
    deadline = time.time() + 180
    while time.time() < deadline:
        try:
            session = Session()
            session.connect()
            return session
        except Exception:  # editor startup is expected to race this first connection
            time.sleep(3)
    return None


def main():
    session = connect_fixture()
    if session is None:
        print("RESULT: FAIL (could not connect to the focused fixture)")
        return 1

    before = read_graph(session)
    print("=== focused shared-grammar fixture ===")
    checked = session.call(PREVIEW_TOOLSET, "PreviewTurboMCPGraphCheck", {
        "asset": ASSET, "graphName": GRAPH, "text": before,
    })
    check("the reader's branch fixture passes the shared parser", "0 finding(s)" in checked, checked[:700])
    check("the graph prints both branch continuations", "(:then" in before and "(:else" in before, before[:1200])
    check("the graph prints the b-prefixed variable", "bAppendFixtureBranch" in before, before[:1200])
    check("the graph prints library calls", "Development|PrintString" in before, before[:1200])

    head = key_for_marker(before, "APPEND-FIXTURE-THEN-HEAD")
    tail = key_for_marker(before, "APPEND-FIXTURE-THEN-TAIL")
    check("the then head was discovered", head is not None, before[:1200])
    check("the then tail was discovered", tail is not None, before[:1200])
    if head is None or tail is None:
        print("RESULT: FAIL (%s)" % ", ".join(FAILURES))
        return 1
    check("the fixture begins with the original then chain", has_wire(before, tail, head), before[:1400])

    print("=== append after the exec tail ===")
    tail_edit = session.call(EDIT_TOOLSET, "EditTurboMCPGraphAppend", {
        "asset": ASSET, "graphName": GRAPH,
        "text": append_text("APPEND-FIXTURE-AFTER-TAIL", tail),
    })
    after_tail = read_graph(session)
    appended_tail = key_for_marker(after_tail, "APPEND-FIXTURE-AFTER-TAIL")
    check("tail append reports a wire", re.search(r"[1-9]\d* wire\(s\) made", tail_edit) is not None, tail_edit[:500])
    check("tail append created its node", appended_tail is not None, after_tail[:1400])
    check("the new tail link is present", appended_tail is not None and has_wire(after_tail, appended_tail, tail), after_tail[:1600])

    print("=== insert between the two original nodes ===")
    insert_edit = session.call(EDIT_TOOLSET, "EditTurboMCPGraphAppend", {
        "asset": ASSET, "graphName": GRAPH,
        "text": append_text("APPEND-FIXTURE-INSERTED", head),
    })
    after_insert = read_graph(session)
    inserted = key_for_marker(after_insert, "APPEND-FIXTURE-INSERTED")
    check("insertion reports a wire", re.search(r"[1-9]\d* wire\(s\) made", insert_edit) is not None, insert_edit[:500])
    check("insertion created its node", inserted is not None, after_insert[:1600])
    check("the source now links to the inserted node", inserted is not None and has_wire(after_insert, inserted, head), after_insert[:1800])
    check("the displaced successor is linked from the inserted node", inserted is not None and has_wire(after_insert, tail, inserted), after_insert[:1800])
    check("the old direct link is gone", inserted is not None and not has_wire(after_insert, tail, head), after_insert[:1800])

    print("=== a failed replacement restores the chain ===")
    # `InString` exists but is an input text pin, so this reaches Apply and the schema must refuse the
    # requested input-to-input wire. The first form has already asked to replace Head -> Inserted at that
    # point; the only safe result is the exact original chain, witnessed below through the reader.
    failed_text = ('(Development|PrintString ; label failing-link\n'
                   '    InString "APPEND-FIXTURE-FAIL"\n'
                   '    execute (-> %s:%s:%s:then))\n'
                   '(Development|PrintString ; %s:%s:%s\n'
                   '    execute (-> failing-link:InString))' %
                   (ASSET, GRAPH, head, ASSET, GRAPH, tail))
    failed_edit = session.call(EDIT_TOOLSET, "EditTurboMCPGraphAppend", {
        "asset": ASSET, "graphName": GRAPH, "text": failed_text,
    })
    after_failure = read_graph(session)
    check("the incompatible replacement is rejected", not failed_edit.startswith("changed:"), failed_edit[:650])
    check("the source-to-inserted chain was restored", inserted is not None and has_wire(after_failure, inserted, head), after_failure[:1900])
    check("the inserted-to-successor chain was restored", inserted is not None and has_wire(after_failure, tail, inserted), after_failure[:1900])
    check("the failed node was not retained", key_for_marker(after_failure, "APPEND-FIXTURE-FAIL") is None, after_failure[:1900])

    if FAILURES:
        print("RESULT: FAIL (%d) -> %s" % (len(FAILURES), ", ".join(FAILURES)))
        return 1
    print("RESULT: PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
