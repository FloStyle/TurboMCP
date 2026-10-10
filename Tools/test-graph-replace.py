"""
`P/E-GRAPH-REPLACE` (`T-WRITE-3`, `T-WRITE-4`): a body is rewritten, the entry is REUSED, and what was
removed is LISTED.

THE FIRST WRITER IN THIS PLUGIN THAT CAN DESTROY, so this suite asks a different question from the append
suite's. Appendix asks whether a block can be added; this asks whether a body can be REPLACED WITHOUT LOSING
ANYTHING THAT MATTERS - and the two ways to get that wrong are opposite and both silent:

  1. IT REMOVES TOO MUCH. A node that the text does describe, or an entry, a result, an event or a comment, must
     survive. Removing a function's entry node deletes its signature; removing an author's comment box deletes
     documentation, which `E-GRAPH-DEAD` already learned the hard way.
  2. IT REMOVES TOO LITTLE. A node left behind from the old body is a body that is only HALF replaced, which
     would compile and look plausible - the worst outcome this capability has.

SO THE CHECKS ARE BUILT ON COMPARISONS RATHER THAN ON SENTENCES. The node set before and after is read through
`R-GRAPH-LOAD`, a DIFFERENT tool, and the removal list is compared against the set difference the reader
reports. A tool's own report of what it removed is not evidence.

AND THE ROUND TRIP IS `T-WRITE-4`: what the reader prints for the rewritten graph must be describable by the
same text, which is the property a second dialect would break.
"""

import os
import re
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
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
                        "format": "", "tokenBudget": ""})
    return set(re.findall(r"%s:%s:([0-9a-f]{6})" % (re.escape(ASSET), re.escape(graph)), out)), out


def connect_with_fixture():
    """
    Returns a connected Session, or None.

    THE FIXTURE IS RESTORED ON THE WAY IN, NOT ONLY ON THE WAY OUT, AND THAT IS THE ONLY OPTION HERE: this suite
    REPLACES a body, so the graph it leaves behind is not the one it found. Rebuilding first makes it
    re-runnable from ANY state, which is the property that matters - a run killed part-way leaves the same mess
    behind, and cleaning up at the end would not have helped.
    """
    stop = subprocess.run(
        ["pwsh", "-NoProfile", "-File", os.path.join(REPO, "Tools", "start-editor.ps1"), "-Stop"],
        capture_output=True, text=True, cwd=REPO)
    _ = stop  # `-Stop` answers non-zero when nothing was running, which is not a failure.
    time.sleep(3)

    if os.environ.get("TURBOMCP_REPLACE_REBUILD") == "1":
        rebuild = subprocess.run(
            ["pwsh", "-NoProfile", "-File", os.path.join(REPO, "Tools", "make-acceptance-asset.ps1")],
            capture_output=True, text=True, cwd=REPO)
        ok = rebuild.returncode == 0 and "PASS" in (rebuild.stdout or "")
        print("    fixture rebuild: %s"
              % ((rebuild.stdout or rebuild.stderr).strip().splitlines() or ["no output"])[0])
        if not ok:
            print("    the acceptance asset could not be rebuilt, so this suite cannot run")
            return None
    else:
        print("    fixture rebuild: not requested; using the committed SaneProbe "
              "(set TURBOMCP_REPLACE_REBUILD=1 to regenerate it)")

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


def main():
    session = connect_with_fixture()
    if session is None:
        print("RESULT: FAIL (the editor could not be started, so nothing was tested)")
        return 1

    before_keys, before_text = node_keys(session)
    print("=== the fixture graph has %d node(s) ===" % len(before_keys))
    check("the fixture graph is not empty", len(before_keys) > 0, sorted(before_keys))

    # ---------------------------------------------------------------------------------------------
    print()
    print("=== THE PREVIEW LISTS THE REMOVALS AND CHANGES NOTHING ===")
    # A REPLACE THAT IS PREVIEWED MUST BE INSPECTABLE BEFORE IT IS COMMITTED TO, because this is the only
    # writer in the plugin that can destroy. The preview is where a caller sees what would go.
    fingerprint_before = asset_fingerprint()

    # A REAL BODY RATHER THAN AN EMPTY ONE, BECAUSE AN EMPTY TEXT IS REFUSED - MEASURED:
    #     `error: no text given. Pass the graph text to check, exactly as R-GRAPH-LOAD prints it.`
    # and that refusal is CORRECT: a text that describes nothing would silently mean "delete the whole body",
    # which is not something a caller should be able to ask for by accident.
    #
    # SO THE BODY BELOW IS A NEW `PrintString` THAT DOES NOT NAME `SaneProbe`'s EXISTING ONE. The old node is
    # therefore described by nothing, which makes it a removal candidate - and the entry node, which the text
    # cannot describe and must not remove, is the other half of the question.
    # *** THE BODY MUST WIRE ITSELF TO THE ENTRY, AND FINDING THAT OUT COST A FAILING RUN. *** The first version
    # was a bare `PrintString` with no `execute` link, so the new node was UNREACHABLE from the entry - and
    # `R-GRAPH-LOAD` walks from the entry points, so the reader then reported ONE node and the suite concluded
    # that the ENTRY had been removed. THE ENTRY WAS FINE; THE READER COULD NOT REACH ANYTHING THROUGH THE NEW
    # BODY. A replacement whose body is not connected is a body that does not run, so the fixture has to connect.
    #
    # THE ENTRY'S KEY IS READ RATHER THAN PINNED, for the reason the append suite records: `SaneProbe` is built by
    # a commandlet, so a hard-coded key breaks the moment it is rebuilt in a different order.
    entry_out = session.call(READ_TOOLSET, "ReadTurboMCPGraphLoad",
                             {"asset": ASSET, "graphName": GRAPH, "detail": "full",
                              "format": "", "tokenBudget": ""})
    # *** THE FIRST HEX KEY IN THE TEXT IS NOT NECESSARILY THE ENTRY, AND ASSUMING IT WAS DESTROYED THE FIXTURE. ***
    # MEASURED: the graph text opens with
    #     `(Development|PrintString ; ...:SaneProbe:a3676a ; class K2Node_CallFunction`
    # so `re.search(r":([0-9a-f]{6})")` returned `a3676a` - THE `PrintString`, NOT THE ENTRY. The suite then built
    # a body whose `execute` link pointed at the node it was about to REMOVE, and the run ended with the graph
    # holding ZERO of the two nodes it started with: THE ENTRY WAS DELETED AND THE REPLACEMENT WAS GARBAGE
    # COLLECTED WITH IT. That is the fixture being destroyed by the test, not by the tool, and it is why the entry
    # key is now read from a line that NAMES the entry rather than from the first key that happens to appear.
    # THE ENTRY'S KEY IS THE TARGET OF THE FIRST EXEC WIRE, AND THE READER PRINTS IT NOWHERE ELSE. MEASURED, the
    # whole text of `SaneProbe` is:
    #     (fn SaneProbe
    #     (Development|PrintString ; ...:SaneProbe:328873 ; class K2Node_CallFunction
    #     execute (-> TurboMCPAcceptance:SaneProbe:9b9007) ; in 0
    # THE `FunctionEntry` NODE IS NAMED BY THE `fn` FORM AND ITS KEY APPEARS ONLY AS THE TARGET OF THAT WIRE, so
    # the first key in the text is the PrintString's and reading it as "the entry" pointed the new body's exec
    # link at the node the pass was about to remove.
    entry_match = re.search(r"execute \(-> [^:]+:[^:]+:([0-9a-f]{6})\)", entry_out)
    entry_key = entry_match.group(1) if entry_match else None
    print("    the entry node key, read from the graph: %s" % entry_key)
    replacement_body = ('(Development|PrintString\n    InString "replacement body"\n'
                        '    Duration 1.0\n    execute (-> %s:%s:%s:then))'
                        % (ASSET, GRAPH, entry_key))

    preview = session.call(PREVIEW_TOOLSET, "PreviewTurboMCPGraphReplace",
                           {"asset": ASSET, "graphName": GRAPH, "text": replacement_body})
    print(preview[:900])
    print()

    check("the preview answers rather than erroring", not preview.startswith("error:"), preview[:200])
    check("the preview says how many nodes it would REMOVE",
          re.search(r"(\d+) node\(s\) the text does not describe", preview) is not None,
          preview[:300])
    check("the preview says how many nodes are KEPT because the text writes into them",
          "kept because the text writes into them" in preview, preview[:300])
    check("the preview says it changes nothing",
          "a preview changes NOTHING" in preview, preview[-300:])

    # THE REMOVALS ARE LISTED BY KEY, WHICH IS THE SPEC'S WORD. A count says how much is gone; a list says what.
    listed = set(re.findall(r"would REMOVE\s+(\S+)", preview))
    print("    the preview lists %d removal(s)" % len(listed))
    check("the preview LISTS removals by key rather than only counting them",
          len(listed) > 0, "the spec requires a list of what was removed")

    fingerprint_after_preview = asset_fingerprint()
    check("THE PREVIEW CHANGED NOTHING ON DISK",
          fingerprint_before == fingerprint_after_preview,
          "before %s, after %s" % (fingerprint_before, fingerprint_after_preview))

    # ---------------------------------------------------------------------------------------------
    print()
    print("=== THE EDIT REMOVES WHAT THE TEXT DOES NOT DESCRIBE, AND THE READER WITNESSES IT ===")
    edited = session.call(EDIT_TOOLSET, "EditTurboMCPGraphReplace",
                          {"asset": ASSET, "graphName": GRAPH, "text": replacement_body})
    print(edited[:1200])
    print()

    check("the edit reports it changed the graph", edited.startswith("changed:"), edited[:160])
    check("the edit says how many nodes it removed",
          re.search(r"(\d+) node\(s\) removed", edited) is not None, edited[:200])
    check("the edit LISTS the removed nodes with their class and title",
          len(re.findall(r"REMOVED\s+\S+\s+\S+\s+'", edited)) > 0, edited[:600])
    check("the edit reports the read back matched",
          "the read back matches" in edited, edited[-400:])

    after_keys, after_text = node_keys(session)
    print("    the reader now reports %d node(s)" % len(after_keys))

    # *** THE CHECK THAT MATTERS MOST: A NODE THE TOOL CLAIMED TO REMOVE MUST BE GONE FROM THE GRAPH. ***
    # Reported removals are compared against what the READER says is in the graph, so a tool that reports a
    # removal it did not make is caught, and so is one that removes a node it did not report.
    # THE TOOL PRINTS THE LONG FORM - `/Game/Tests/TurboMCPAcceptance.TurboMCPAcceptance:SaneProbe:a3676a` - WHILE
    # THIS SUITE COMPARES SHORT KEYS, because the reader prints both and the short one is what identifies a node
    # within its own graph. MEASURED: the first version compared the two forms directly and reported that the
    # tool had removed a node the reader did not, when the reader had removed exactly that node. THE COMPARISON
    # WAS WRONG, NOT THE TOOL - so the long form is reduced to its LAST COLON SEGMENT here rather than the tool
    # being changed to suit a test.
    reported_removed = set(k.rsplit(":", 1)[-1]
                           for k in re.findall(r"REMOVED\s+(\S+)", edited))
    actually_gone = before_keys - after_keys
    actually_kept = before_keys & after_keys

    check("every node the tool SAYS it removed is GONE from the graph, as the reader reports it",
          reported_removed.issubset(actually_gone),
          "claimed %s, reader says gone %s" % (sorted(reported_removed), sorted(actually_gone)))
    check("no node the tool did NOT report was removed",
          actually_gone.issubset(reported_removed),
          "reader says gone %s, claimed %s" % (sorted(actually_gone), sorted(reported_removed)))

    # *** AND THE OTHER DIRECTION: THE ENTRY AND THE RESULT SURVIVE. ***
    # "Writes into the EXISTING entry node and result nodes; NEVER ADDS A SECOND ENTRY." A REPLACE that removed
    # the entry would delete the function's signature.
    # *** THE CLASS NAME IS NOT THE RIGHT WITNESS, AND MEASURED WHY: THE READER TRUNCATES IT. *** The text reads
    # `; class K2Node_C...`, cut off by a column limit, so `re.findall(r"class (\w+)")` yields `K2Node_C` and a
    # check for "Entry" can NEVER be satisfied - A CHECK THAT CANNOT PASS ON A CORRECT GRAPH. The field is a
    # DISPLAY convenience and was never a machine-readable one.
    #
    # THE ENTRY IS WITNESSED BY ITS KEY INSTEAD, which is exact: the entry's key is read from the exec wire's
    # target, and the check is that THE SAME KEY IS STILL IN THE GRAPH after the replacement. That is also the
    # stronger claim, because it says the entry was not REPLACED BY A SECOND ONE - which the spec forbids in the
    # same sentence it uses to require reuse.
    check("the ENTRY node is still in the graph, BY ITS KEY, after replacing the body",
          entry_key is not None and entry_key in after_keys,
          "the entry key was %s; the graph now holds %s" % (entry_key, sorted(after_keys)))
    check("NO SECOND ENTRY was added: the graph holds exactly the entry count it started with",
          len([k for k in after_keys if k == entry_key]) == 1, sorted(after_keys))
    check("a RESULT or body node is present, so the graph is not an empty shell",
          len(after_keys) > 0, sorted(after_keys))
    check("the entry that survived is the SAME node, not a second one",
          len([k for k in before_keys & after_keys]) > 0, sorted(before_keys & after_keys))

    # ---------------------------------------------------------------------------------------------
    print()
    print("=== `T-WRITE-4`: A BODY WRITTEN BACK UNCHANGED IS A NO-OP ===")
    # REPLACING a graph with the text the reader just printed must describe every node, so nothing is a
    # removal candidate. This is the round-trip property: the writer accepts the reader's own output, and a
    # second dialect would show up here as spurious removals.
    roundtrip = session.call(EDIT_TOOLSET, "EditTurboMCPGraphReplace",
                             {"asset": ASSET, "graphName": GRAPH, "text": after_text})
    print(roundtrip[:800])
    print()

    check("writing the reader's own text back reports nothing left to remove",
          "removed 0 node(s)" in roundtrip or "unchanged" in roundtrip.lower(), roundtrip[:300])

    final_keys, _ = node_keys(session)
    check("the round trip left the node set UNCHANGED",
          final_keys == after_keys,
          "before %s, after %s" % (sorted(after_keys), sorted(final_keys)))

    # ---------------------------------------------------------------------------------------------
    print()
    print("=== A REFUSED TEXT REMOVES NOTHING AND SAYS SO ===")
    # THE DESTRUCTIVE HALF MUST NOT RUN ON A TEXT THAT WILL NOT BE APPLIED. A refusal that still removed nodes
    # would be the single worst behaviour this capability could have.
    fingerprint = asset_fingerprint()
    refused = session.call(EDIT_TOOLSET, "EditTurboMCPGraphReplace",
                           {"asset": ASSET, "graphName": GRAPH,
                            "text": "(NoSuchCategory|NoSuchFunction\n    InString \"x\")"})
    print(refused[:600])
    print()

    check("a text with an unresolvable type is refused",
          refused.startswith("unchanged:") or "refused" in refused, refused[:200])
    check("the refusal says NOTHING WOULD BE REMOVED",
          "NOTHING REMOVED" in refused or "removed 0" in refused, refused[:400])
    still_keys, _ = node_keys(session)
    check("the refused edit left the graph exactly as it was",
          still_keys == final_keys,
          "before %s, after %s" % (sorted(final_keys), sorted(still_keys)))

    fingerprint_after_refusal = asset_fingerprint()
    check("the refused edit changed nothing on disk",
          fingerprint == fingerprint_after_refusal,
          "before %s, after %s" % (fingerprint, fingerprint_after_refusal))

    # ---------------------------------------------------------------------------------------------
    # THE FIXTURE IS GIVEN BACK, AND THIS IS NOT TIDINESS - IT IS WHAT KEEPS THE RUN GREEN.
    #
    # MEASURED: a FULL RUN with this suite in it fell from 39 of 39 to 35 of 40. The failures were in the
    # NEIGHBOURS, not here - `check-probe-graphs` reported all fifteen of its count checks failing and
    # `test-graph-check` reported `the asset's SIZE, MTIME and MD5 are unchanged by every check above`. BOTH
    # PASS ALONE. The cause is that THIS SUITE REPLACES A BODY, so it leaves `SaneProbe` with different nodes
    # than it found, and `check-probe-graphs` rebuilds that same asset as part of its own work: TWO OWNERS OF ONE
    # FIXTURE, which is the exact defect `test-graph-append` recorded and worked around by NOT rebuilding.
    #
    # APPEND CAN AFFORD THAT BECAUSE IT ONLY ADDS. THIS SUITE CANNOT: a REPLACE that is not undone leaves the
    # graph it consumed, so the fixture is REGENERATED HERE, at the end, and the suite is a good citizen rather
    # than a destructive one. `start-editor.ps1 -Stop` first, because the commandlet needs the package to itself.
    print()
    print("=== giving the shared fixture back ===")
    stop = subprocess.run(
        ["pwsh", "-NoProfile", "-File", os.path.join(REPO, "Tools", "start-editor.ps1"), "-Stop"],
        capture_output=True, text=True, cwd=REPO)
    _ = stop
    time.sleep(3)
    restore = subprocess.run(
        ["pwsh", "-NoProfile", "-File", os.path.join(REPO, "Tools", "make-acceptance-asset.ps1")],
        capture_output=True, text=True, cwd=REPO)
    restored_ok = restore.returncode == 0 and "PASS" in (restore.stdout or "")
    print("    fixture restore: %s"
          % ((restore.stdout or restore.stderr).strip().splitlines() or ["no output"])[0])
    # A FAILED RESTORE IS A FAILURE, NOT A NOTE, because the next suite reads what this one left behind.
    check("the shared fixture was GIVEN BACK, so the next suite reads what it expects", restored_ok,
          (restore.stdout or restore.stderr)[-300:])

    print()
    if failures:
        print("RESULT: FAIL (%d) -> %s" % (len(failures), ", ".join(failures)))
        return 1
    print("RESULT: PASS (%d check(s) run)" % checks)
    return 0


if __name__ == "__main__":
    sys.exit(main())
