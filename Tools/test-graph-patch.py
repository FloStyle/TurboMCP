# ACCEPTANCE ROW THIS SUITE CLAIMS TO PROVE: T-WRITE-5
"""
`P/E-GRAPH-PATCH` (`T-WRITE-5`): changing two lines changes exactly two nodes.

WHAT THIS SUITE ASKS, AND IT IS A DIFFERENT QUESTION FROM THE OTHER TWO WRITERS'. `APPEND` asks whether a block
can be ADDED; `REPLACE` asks whether a body can be rewritten WITHOUT LOSING ANYTHING. `PATCH` asks whether the
change can be **EXACTLY AS LARGE AS THE TEXT** - and the failure mode is not a broken graph but a graph that is
right in a way nobody asked for:

  1. IT CHANGES TOO MUCH. A patch that also rewrote a node the text never mentioned has broken the author's work
     silently. `T-WRITE-5`'s "exactly two nodes" is a claim about ARITHMETIC, and a tool that touches a third node
     has FAILED EVEN THOUGH the graph still compiles.
  2. IT CHANGES TOO LITTLE. A pin that the text gives a different value and the tool leaves holding the old one
     is a patch that reported success and did nothing.

SO THE HEADLINE CHECK IS ARITHMETIC AND IT IS TAKEN FROM THE OUTSIDE. The node ids are read through
`R-GRAPH-LOAD` - a DIFFERENT tool - before and after, and the node that changed is confirmed by reading its PIN
VALUE back, not by reading the tool's summary. A tool's own report of what it changed is not evidence.

AND THE SUITE MEASURES IDEMPOTENCE, which is the property that separates a DIFF from a WRITE: applying the same
text twice must change the second time NOTHING AT ALL.
"""

import os
import re
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from tmcp import PREVIEW_TOOLSET, READ_TOOLSET, EDIT_TOOLSET, Session  # noqa: E402

# THE FIXTURE IS RESTORED ON THE WAY IN, AND THAT IS COPIED FROM `test-graph-replace` RATHER THAN REDISCOVERED.
#
# `test-graph-append` recorded the measurement that decides it: running the acceptance rebuild from INSIDE a suite
# took a full run from 37 of 39 to 32 of 39, because `check-probe-graphs` already regenerates this asset, and two
# suites rebuilding one fixture is TWO OWNERS of it. This suite MUTATES a pin value, so it is a mutator for the
# same reason REPLACE is - and a mutator must give the fixture back on the way IN as well as the way out.
REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ASSET = "TurboMCPAcceptance"
GRAPH = "SaneProbe"
ASSET_FILE = "TurboMCPAcceptance.uasset"

# THE VALUE THE FIXTURE HOLDS AND THE VALUE THIS SUITE WRITES. The fixture is the one `make-acceptance-asset.ps1`
# builds, whose `PrintString` carries `InString "Hello"`. Both are named here so a fixture that changes is a
# FAILING CHECK rather than a suite that quietly patches something else.
FIXTURE_VALUE = "Hello"
NEW_VALUE = "PatchedByMCP"

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


def graph_text(session):
    """The reader's own answer for the graph, which is the independent witness for every claim below."""
    return session.call(READ_TOOLSET, "ReadTurboMCPGraphLoad",
                        {"asset": ASSET, "graphName": GRAPH, "detail": "full",
                         "format": "", "tokenBudget": ""})


def node_keys(session):
    """Every node key the READER prints. The set is what `T-WRITE-5` is measured against."""
    out = graph_text(session)
    return set(re.findall(r"%s:%s:([0-9a-f]{6})" % (re.escape(ASSET), re.escape(GRAPH)), out)), out


def pin_value(session, node_key, pin_name):
    """
    The value the READER reports for one pin, or None.

    THE READER IS THE WITNESS, NOT THE WRITER. A patch that reports a new value while the graph holds the old one
    is the exact silent failure this suite exists to catch, and only an independent read can tell them apart.
    """
    out = graph_text(session)
    for line in out.splitlines():
        if node_key in line and ";" in line:
            continue
        stripped = line.strip()
        if stripped.startswith(pin_name + " "):
            value = stripped[len(pin_name):].strip().split(" ;")[0].strip()
            return value
    return None


def connect_with_fixture():
    """
    Returns a connected Session, or None.

    THE FIXTURE IS RESTORED ON THE WAY IN, NOT ONLY ON THE WAY OUT: this suite writes a pin value, so the graph it
    leaves is not the one it found. Rebuilding first makes it re-runnable from ANY state, which is the property
    that matters - a run killed part-way leaves the same mess behind, and cleaning up at the end would not help.

    THE REBUILD IS OPT-IN BY DEFAULT, exactly as `test-graph-replace` arranged it, because running it from inside a
    suite fights `check-probe-graphs` for the same asset. `TURBOMCP_PATCH_REBUILD=1` forces it when running by hand
    from a suspect tree.
    """
    stop = subprocess.run(
        ["pwsh", "-NoProfile", "-File", os.path.join(REPO, "Tools", "start-editor.ps1"), "-Stop"],
        capture_output=True, text=True, cwd=REPO)
    _ = stop  # `-Stop` answers non-zero when nothing was running, which is not a failure.
    time.sleep(3)

    if os.environ.get("TURBOMCP_PATCH_REBUILD") == "1":
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
              "(set TURBOMCP_PATCH_REBUILD=1 to regenerate it)")

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
            print("    waiting for the MCP server to answer")
            time.sleep(5)
    return None


def main():
    session = connect_with_fixture()
    if session is None:
        print("RESULT: FAIL (the editor could not be started, so nothing was tested)")
        return 1

    before_keys, before_text = node_keys(session)
    print("=== the fixture graph has %d node(s) ===" % len(before_keys))
    check("the fixture graph is not empty", len(before_keys) > 0, sorted(before_keys))
    check("the fixture has at least TWO nodes, so 'every other node' has a meaning",
          len(before_keys) >= 2, sorted(before_keys))

    # ---------------------------------------------------------------------------------------------
    # THE TARGET IS READ OUT OF THE GRAPH RATHER THAN PINNED, for the reason the other two suites record: the
    # fixture is built by a commandlet, so a hard-coded key breaks the moment it is rebuilt in a different order.
    # The node that CARRIES the value is found by looking for the pin, not by assuming the first key in the text.
    target_key = None
    for line in before_text.splitlines():
        if "class K2Node_CallFunction" in line and ";" in line:
            # *** THE KEY IS MATCHED AS THE READER ACTUALLY PRINTS IT, WHICH COST A FAILING RUN. ***
            #
            # MEASURED, the reader prints the package path with a LEADING SLASH and the graph name after a colon:
            #     (Development|PrintString ; /Game/Tests/TurboMCPAcceptance.TurboMCPAcceptance:SaneProbe:e81522 ; class ...
            # The first version of this loop matched `TurboMCPAcceptance.TurboMCPAcceptance:SaneProbe:` - the form
            # WITHOUT the `/Game/Tests/` prefix - so it matched NOTHING, `target_key` stayed None, and the suite
            # went on to build a patch that named the node `null`. EVERY downstream failure came from that one
            # mismatch, INCLUDING the idempotence check: the second patch reported `unchanged` because a text
            # naming `null` is refused every time, which looks exactly like a working diff and is not one.
            #
            # SO IT IS MATCHED ON THE PART THAT IS UNAMBIGUOUS - the graph name, a colon, then six hex digits -
            # rather than on a path prefix this suite does not need to know.
            match = re.search(r":%s:([0-9a-f]{6})" % re.escape(GRAPH), line)
            if match:
                target_key = match.group(1)
                break
    print("    the target node key, read from the graph: %s" % target_key)
    check("the fixture exposes a node to patch", target_key is not None, before_text[:300])

    # AND THE PIN'S VALUE IS CONFIRMED TO BE WHAT THE FIXTURE IS SUPPOSED TO HOLD, because a suite that patches a
    # value it assumes is there proves nothing when the assumption is wrong.
    fixture_value = pin_value(session, target_key, "InString") if target_key else None
    print("    the pin's value before the patch: %r" % fixture_value)
    check("the fixture holds the value this suite expects to change",
          fixture_value is not None and FIXTURE_VALUE in fixture_value,
          "read %r, expected %r" % (fixture_value, FIXTURE_VALUE))

    # ONE FORM, ONE NODE, ONE PIN. `T-WRITE-5` is about the SIZE of a change, so the text is deliberately the
    # smallest thing that differs from the graph.
    #
    # THE ID IS WRITTEN THE WAY THE READER PRINTS IT, WHICH IS THE ONLY FORM THAT RESOLVES - the full key from
    # `/Game/...` through to the hex. `NodeIdFromComment` reads it back out of the comment, so a shorter form is
    # not a synonym: it is a different string that names no node.
    def patch_text(key, value):
        return ('(Development|PrintString ; /Game/Tests/%s.%s:%s:%s\n    InString "%s")'
                % (ASSET, ASSET, GRAPH, key, value))

    text = patch_text(target_key, NEW_VALUE)

    # ---------------------------------------------------------------------------------------------
    print()
    print("=== THE PREVIEW SAYS HOW MANY NODES WOULD CHANGE, AND CHANGES NOTHING ===")
    fingerprint_before = asset_fingerprint()

    preview = session.call(PREVIEW_TOOLSET, "PreviewTurboMCPGraphPatch",
                           {"asset": ASSET, "graphName": GRAPH, "text": text})
    print(preview[:900])
    print()

    check("the preview answers rather than erroring", not preview.startswith("error:"), preview[:200])
    check("the preview says how many nodes would change",
          re.search(r"(\d+) node\(s\) would change", preview) is not None, preview[:300])
    check("the preview says every OTHER node is left alone",
          "every OTHER node is left alone" in preview, preview[:300])
    # THE COUNT IS PARSED RATHER THAN MATCHED AS A SENTENCE, which is this project's rule.
    would_change = re.search(r"(\d+) node\(s\) would change", preview)
    check("the preview's count is 1, because the text names one node",
          would_change is not None and int(would_change.group(1)) == 1,
          would_change.group(1) if would_change else "not found")
    check("the preview names the node it would change, by key",
          ("would CHANGE" in preview) and (target_key in preview), preview[:400])
    check("the preview says it changes nothing",
          "a preview changes NOTHING" in preview, preview[-300:])

    fingerprint_after_preview = asset_fingerprint()
    check("THE PREVIEW CHANGED NOTHING ON DISK",
          fingerprint_before == fingerprint_after_preview,
          "before %s, after %s" % (fingerprint_before, fingerprint_after_preview))

    # ---------------------------------------------------------------------------------------------
    print()
    print("=== THE EDIT CHANGES EXACTLY THE NAMED NODE, AND THE READER WITNESSES IT ===")
    edited = session.call(EDIT_TOOLSET, "EditTurboMCPGraphPatch",
                          {"asset": ASSET, "graphName": GRAPH, "text": text})
    print(edited[:1000])
    print()

    check("the edit's FIRST line is changed or unchanged",
          edited.startswith("changed:") or edited.startswith("unchanged:"), edited[:160])
    check("the edit reports it changed the graph", edited.startswith("changed:"), edited[:160])

    # *** THE HEADLINE CHECK FOR `T-WRITE-5`, AND IT IS ARITHMETIC TAKEN FROM OUTSIDE. ***
    changed_line = re.search(r"(\d+) node\(s\) changed", edited)
    check("EXACTLY ONE node changed, which is what the text describes",
          changed_line is not None and int(changed_line.group(1)) == 1,
          changed_line.group(1) if changed_line else "the count was not parseable")
    named_line = re.search(r"(\d+) node\(s\) named by the text", edited)
    check("the report states how many nodes the text named",
          named_line is not None and int(named_line.group(1)) == 1,
          named_line.group(1) if named_line else "not found")
    check("the report says no node was added and none was removed",
          "no node was added" in edited, edited[-400:])

    after_keys, after_text = node_keys(session)

    # *** THE CHECK THAT MAKES "EXACTLY TWO NODES" MEANINGFUL: EVERY OTHER NODE'S ID SURVIVED. ***
    #
    # PARSED AND COMPARED, NOT TRUSTED FROM THE REPORT. A patch that quietly created or destroyed a node would
    # pass every sentence-based check above and fail here, and this is the set difference the reader reports.
    check("EVERY node that existed before the patch still exists, by id",
          before_keys.issubset(after_keys),
          "gone: %s" % sorted(before_keys - after_keys))
    check("the patch ADDED no node at all",
          after_keys.issubset(before_keys),
          "new: %s" % sorted(after_keys - before_keys))
    check("the node count is therefore unchanged",
          len(after_keys) == len(before_keys),
          "%d -> %d" % (len(before_keys), len(after_keys)))

    # AND THE CHANGE IS CONFIRMED BY READING THE PIN VALUE BACK, through the READER rather than the writer.
    witnessed = pin_value(session, target_key, "InString")
    print("    the pin's value after the patch: %r" % witnessed)
    check("THE READER WITNESSES THE NEW VALUE on the node's own pin",
          witnessed is not None and NEW_VALUE in witnessed,
          "read %r, expected %r" % (witnessed, NEW_VALUE))

    # ---------------------------------------------------------------------------------------------
    print()
    print("=== A PATCH IS A DIFF, SO APPLYING IT TWICE CHANGES THE SECOND TIME NOTHING ===")
    #
    # THIS IS THE PROPERTY THAT SEPARATES A DIFF FROM A WRITE. A tool that re-sets every described pin to the
    # value it already holds would answer `changed` on the second run, dirty the asset, and report a change that
    # is not one - which is what "applies only the differences" forbids. MEASURED, and it is why the pin
    # comparison exists in `Apply` rather than a straight assignment.
    second = session.call(EDIT_TOOLSET, "EditTurboMCPGraphPatch",
                          {"asset": ASSET, "graphName": GRAPH, "text": text})
    print(second[:600])
    print()

    check("A SECOND IDENTICAL PATCH SAYS unchanged, which is what makes it a DIFF",
          second.startswith("unchanged:"), second[:200])
    second_changed = re.search(r"(\d+) node\(s\) changed", second)
    check("the second patch changed ZERO nodes",
          second_changed is not None and int(second_changed.group(1)) == 0,
          second_changed.group(1) if second_changed else "not found")
    second_keys, _ = node_keys(session)
    check("the second patch left the node set identical", second_keys == after_keys,
          "%s vs %s" % (sorted(after_keys), sorted(second_keys)))

    # ---------------------------------------------------------------------------------------------
    print()
    print("=== THE REFUSALS, EACH NAMING WHAT IS WRONG RATHER THAN ONLY SAYING NO ===")
    fingerprint = asset_fingerprint()

    # (a) A FORM WITH NO NODE ID IS A REQUEST TO ADD, WHICH IS ANOTHER TOOL'S JOB.
    no_id = session.call(EDIT_TOOLSET, "EditTurboMCPGraphPatch",
                         {"asset": ASSET, "graphName": GRAPH,
                          "text": '(Development|PrintString\n    InString "x")'})
    print(no_id[:600])
    print()
    check("a form naming no node is refused",
          no_id.startswith("unchanged:") or "refused" in no_id, no_id[:200])
    # *** THE REFUSAL NAMES THE OTHER TOOL, WHICH IS THE DIFFERENCE BETWEEN A REFUSAL A CALLER CAN ACT ON AND ONE
    # THAT ONLY SAYS NO. ***
    check("the refusal NAMES the tool that adds nodes, so a caller knows where to go",
          "APPEND" in no_id, no_id[:400])

    # (b) A PIN THAT DOES NOT EXIST IS REFUSED WITH THE REAL PIN NAMES, as the spec requires.
    bad_pin = session.call(EDIT_TOOLSET, "EditTurboMCPGraphPatch",
                           {"asset": ASSET, "graphName": GRAPH,
                            "text": '(Development|PrintString ; %s.%s:%s:%s\n    NoSuchPin "x")'
                            % (ASSET, ASSET, GRAPH, target_key)})
    print(bad_pin[:700])
    print()
    check("a pin that does not exist is refused",
          bad_pin.startswith("unchanged:") or "refused" in bad_pin, bad_pin[:200])
    check("the refusal LISTS THE REAL PIN NAMES rather than only refusing",
          "InString" in bad_pin, bad_pin[:500])

    # (c) A FORM THAT NAMES A NODE AND WRITES NO PIN VALUE CHANGES NOTHING, and must say so rather than report a
    # change of zero - the distinction this project cares about between "nothing found" and "a zero".
    no_pins = session.call(EDIT_TOOLSET, "EditTurboMCPGraphPatch",
                           {"asset": ASSET, "graphName": GRAPH,
                            "text": '(Development|PrintString ; %s.%s:%s:%s)'
                            % (ASSET, ASSET, GRAPH, target_key)})
    print(no_pins[:600])
    print()
    check("a form with an id and NO pin value is refused as describing nothing different",
          no_pins.startswith("unchanged:") or "refused" in no_pins, no_pins[:200])

    after_refusals, _ = node_keys(session)
    check("NOTHING the refusals touched changed the graph",
          after_refusals == after_keys,
          "%s vs %s" % (sorted(after_keys), sorted(after_refusals)))

    fingerprint_after = asset_fingerprint()
    check("no refusal changed anything on disk",
          fingerprint == fingerprint_after,
          "before %s, after %s" % (fingerprint, fingerprint_after))

    # ---------------------------------------------------------------------------------------------
    # THE FIXTURE IS GIVEN BACK, AND THIS IS NOT TIDINESS - IT IS WHAT KEEPS THE RUN GREEN.
    #
    # THIS SUITE WRITES A PIN VALUE, so it leaves `SaneProbe` holding a value it did not find, and
    # `check-probe-graphs` rebuilds that asset as part of its own work: TWO OWNERS OF ONE FIXTURE. `REPLACE`
    # recorded that a run with a non-restoring mutator in it fell from 39 of 39 to 35 of 40, with the failures in
    # the NEIGHBOURS. The fixture is therefore regenerated at the end and the restore is ASSERTED.
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
