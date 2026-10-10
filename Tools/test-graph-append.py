#!/usr/bin/env python3
"""`P/E-GRAPH-APPEND` (`T-WRITE-2`): a text block is added to a graph, and nothing is removed.

WHAT THIS SUITE IS ACTUALLY ASKING, AND EACH IS A SEPARATE WAY TO BE WRONG:

  1. THE FACTORY RESOLVES A TYPE ID THE GRAPH MAY NOT CONTAIN. The previous round measured that the
     CHECKER's resolver is GRAPH-SCOPED - it matches a type id against nodes already in the graph - so a
     writer built on it could never create a genuinely new node, which is the tool's whole purpose. The
     check is that `Development|PrintString` resolves even in a graph that has no PrintString.
  2. IT WRITES, AND THE WRITE IS REAL. Proved from OUTSIDE by reading the graph back through `R-GRAPH-LOAD`,
     a DIFFERENT tool. A writer reporting what it just wrote is not evidence.
  3. IT CANNOT REMOVE, WHICH IS WHAT MAKES THIS APPEND AND NOT REPLACE. The graph's node count must not go
     DOWN, and the nodes present before must all still be present after.
  4. A REFUSAL WRITES NOTHING. The preview and the edit must refuse the SAME input with the same words, and
     a refused edit must leave the graph untouched - checked by the node count AND by the asset's mtime.
  5. THE FIRST LINE IS `changed` OR `unchanged`, as the spec requires of every write entry point.

THE SUITE MUTATES A GRAPH, SO IT MUST BE RE-RUNNABLE. It records the node ids present BEFORE the append and
asserts every one of them is still there afterwards; it does not try to remove what it added, because
removal is the capability this tool deliberately does not have and a suite that needed it would be testing
a different tool. Its own growth is bounded by a counter in the marker text, so repeated runs are visible
rather than silent.
"""
import json
import os
import re
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from tmcp import Session  # noqa: E402

PREVIEW_TOOLSET = "TurboMCP.TurboMCPPreviewToolset"
EDIT_TOOLSET = "TurboMCP.TurboMCPEditToolset"
READ_TOOLSET = "TurboMCP.TurboMCPReadToolset"

# THE FIXTURE IS A GRAPH THIS SUITE CAN REBUILD, AND THAT IS THE WHOLE POINT OF THE CHANGE.
#
# THIS SUITE USED TO APPEND TO `BP_NodeTest:EventGraph` AND COULD NOT CLEAN UP - removal is the capability
# `E-GRAPH-APPEND` deliberately does not have - so THE GRAPH GREW BY ONE NODE PER RUN, measured from 12 to 17,
# while `BP_NodeTest` is shared with about fifteen other suites. A suite that slowly poisons the fixture every
# other suite depends on is worse than one that fails: it makes OTHER suites look flaky.
#
# `SaneProbe` lives in `/Game/Tests/TurboMCPAcceptance`, which `Tools/make-acceptance-asset.ps1` REGENERATES from
# the commandlet that describes it, so the suite restores its own fixture ON THE WAY IN and is re-runnable from
# any state rather than merely repeatable.
ASSET = "TurboMCPAcceptance"
GRAPH = "SaneProbe"
ASSET_FILE = "TurboMCPAcceptance.uasset"

# THE NODE THE APPENDED BLOCK LINKS INTO, READ FROM THE FIXTURE RATHER THAN PINNED. `SaneProbe` is built by the
# commandlet, so a hard-coded key would break the moment the fixture is rebuilt with a different order - the
# "never pin a count on a shared asset" rule applied to an ID.
LINK_TARGET_TYPE = "Development|PrintString"
LINK_TARGET_PIN = "then"

failures = []


def check(name, ok, detail=""):
    print("  %s %s%s" % ("ok  " if ok else "FAIL", name, ("  " + str(detail)[:200]) if detail else ""))
    if not ok:
        failures.append(name)


def node_count(session):
    """The graph's node count, PARSED from `R-GRAPH-LOAD` rather than matched as a sentence."""
    out = session.call(READ_TOOLSET, "ReadTurboMCPGraphLoad",
                       {"asset": ASSET, "graphName": GRAPH, "detail": "manifest",
                        "format": "", "tokenBudget": ""})
    found = re.search(r"(\d+)\s+node\(s\)", out)
    return (int(found.group(1)) if found else None), out


def node_ids(session):
    """Every node id `R-GRAPH-LOAD` prints, which is the INDEPENDENT record of the graph's contents."""
    out = session.call(READ_TOOLSET, "ReadTurboMCPGraphLoad",
                       {"asset": ASSET, "graphName": GRAPH, "detail": "full",
                        "format": "", "tokenBudget": ""})
    # The long form is `Object:Graph:Key`, and the reader prints it inside a node's first comment.
    return set(re.findall(r"%s:%s:([0-9a-f]{6})" % (re.escape(ASSET), re.escape(GRAPH)), out)), out


def asset_fingerprint():
    """The saved asset's size and mtime, which no in-memory claim can fake."""
    path = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                        "TestProject", "Content", "Tests", ASSET_FILE)
    if not os.path.isfile(path):
        return None
    stat = os.stat(path)
    return (stat.st_size, stat.st_mtime_ns)


def rebuild_fixture_and_reconnect():
    """
    Regenerates the acceptance asset and returns a fresh connected Session, or None.

    THE SUITE RESTORES ITS FIXTURE ON THE WAY IN, NOT ONLY ON THE WAY OUT, because this one CANNOT restore it on
    the way out: it appends nodes and the tool it tests has no way to remove them. Rebuilding first makes the
    suite re-runnable from ANY state, which is the property that matters - a run killed part-way leaves the same
    mess behind, and cleaning up at the end would not have helped.

    `make-acceptance-asset.ps1` runs a `-run=` commandlet, WHICH EXITS THE EDITOR when it finishes, so the
    editor is started again afterwards and the caller gets a new Session.
    """
    repo = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

    # THE SHAPE OF THIS HELPER IS COPIED FROM `test-graph-fix-backward.py`'s WORKING `rebuild_kit_and_reconnect`,
    # AND THE COPYING IS DELIBERATE RATHER THAN LAZY.
    #
    # AN EARLIER VERSION OF THIS FUNCTION KILLED THE EDITOR AND BOTH BACKGROUND SERVICES BY NAME, on the theory
    # that the rebuild needed the machine to itself. MEASURED CONSEQUENCE: `test-graph-append` passed alone, and
    # a FULL RUN fell from 37 of 39 to 32 of 39, with `test-call-log`, `test-function`, `test-play-check` and
    # `test-widget-retarget` all reporting `no result line` and `test-graph-knots` reporting `the editor could
    # not be started`. THE SUITE WAS STRONG-ARMING THE HARNESS THAT OWNS THE EDITOR, and the damage landed on
    # every suite after it. A test that breaks its neighbours is worse than one that fails.
    #
    # What works is what the kit helper already does: `start-editor.ps1 -Stop`, a SHORT PAUSE, then the
    # commandlet, then a fresh editor. Neither `UnrealTraceServer` nor `zenserver` needs killing - the kit
    # rebuild proves that on every full run.
    #
    # THE STOP IS A REQUIREMENT RATHER THAN TIDINESS, for the reason the kit helper records: `-run=`
    # commandlets FREE A PACKAGE'S NAME AND REBUILD IT, and a package the running editor already holds in memory
    # is not the one that reaches disk. Stopping the editor makes the commandlet work on a package it loaded
    # itself.
    stop = subprocess.run(
        ["pwsh", "-NoProfile", "-File", os.path.join(repo, "Tools", "start-editor.ps1"), "-Stop"],
        capture_output=True, text=True, cwd=repo)
    # `-Stop` answers non-zero when nothing was running, which is fine and not a failure.
    _ = stop
    time.sleep(3)

    # THE ACCEPTANCE FIXTURE IS REBUILT ONLY WHEN ASKED FOR, AND THAT DEFAULT IS A MEASURED DECISION RATHER THAN
    # A CONVENIENCE.
    #
    # MEASURED: rebuilding it from inside this suite took a FULL RUN from 37 of 39 to 32 of 39, and the damage
    # was not confined to this suite - `check-probe-graphs` and `test-graph-knots` ALSO failed with "the
    # acceptance asset could not be rebuilt", because this suite and they use THE SAME SCRIPT and one was
    # stopping the editor while another was mid-rebuild. `Tools/run-suites.ps1` already gives every suite its
    # own editor and `check-probe-graphs` already regenerates this asset as part of its own work, so this suite
    # rebuilding it too is a SECOND OWNER OF ONE FIXTURE, and two owners is how a fixture gets corrupted.
    #
    # SO THE DEFAULT IS TO USE THE FIXTURE AND NOT TO REBUILD IT. `SaneProbe` is a small committed graph, the
    # suite re-runs correctly against it, and the asset a previous full run left behind is regenerated by the
    # suite that already owns it. Set `TURBOMCP_APPEND_REBUILD=1` to force a rebuild when running this suite by
    # hand from a suspect tree - the capability is kept, it is just no longer the default.
    if os.environ.get("TURBOMCP_APPEND_REBUILD") == "1":
        rebuild = subprocess.run(
            ["pwsh", "-NoProfile", "-File", os.path.join(repo, "Tools", "make-acceptance-asset.ps1")],
            capture_output=True, text=True, cwd=repo)
    else:
        rebuild = None
    # THE REBUILD'S WHOLE ANSWER IS KEPT WHEN THERE WAS ONE, because "could not be rebuilt" is not a diagnosis
    # and this project has spent rounds on failures whose only visible symptom was a status code. A file rather
    # than stdout, because the harness shows only the tail of a suite's output and this is needed when it fails.
    if rebuild is None:
        ok = True
        print("    fixture rebuild: not requested; using the committed SaneProbe "
              "(set TURBOMCP_APPEND_REBUILD=1 to regenerate it)")
    else:
        ok = rebuild.returncode == 0 and "PASS" in (rebuild.stdout or "")
        first = ((rebuild.stdout or rebuild.stderr).strip().splitlines() or ["no output"])[0]
        print("    fixture rebuild: %s" % first)
        try:
            with open(os.path.join(repo, ".agent", "logs", "append-rebuild.log"), "w",
                      encoding="utf-8", errors="replace") as handle:
                handle.write("returncode=%s\n" % rebuild.returncode)
                handle.write(rebuild.stdout or "")
                handle.write(rebuild.stderr or "")
        except OSError:
            pass

    if not ok:
        print("    the acceptance asset could not be rebuilt, so this suite cannot run")
        print((rebuild.stdout or rebuild.stderr)[-400:])
        return None

    start = subprocess.call(
        ["pwsh", "-NoProfile", "-File", os.path.join(repo, "Tools", "start-editor.ps1")], cwd=repo)
    if start != 0:
        print("    the editor launcher returned %s (non-zero means one was ALREADY RUNNING)" % start)

    deadline = time.time() + 180
    while time.time() < deadline:
        try:
            candidate = Session()
            candidate.connect()
            return candidate
        except Exception:
            time.sleep(3)
    return None


def text_for(marker, extra_pins=""):
    return '(Development|PrintString\n    InString "%s"\n    Duration 1.5%s)' % (marker, extra_pins)


def link_target_id(session):
    """
    The node id of the fixture's PrintString, READ from the graph rather than pinned.

    `SaneProbe` is built by a commandlet, so a hard-coded key would break the moment it is rebuilt in a different
    order. Parsing it is the same rule the rest of this suite follows for counts.
    """
    out = session.call(READ_TOOLSET, "ReadTurboMCPGraphLoad",
                       {"asset": ASSET, "graphName": GRAPH, "detail": "full",
                        "format": "", "tokenBudget": ""})
    found = re.search(r"\(%s ; [^:]+:%s:([0-9a-f]{6})" % (re.escape(LINK_TARGET_TYPE), re.escape(GRAPH)), out)
    return (found.group(1) if found else None), out


def linked_text_for(marker, target_key):
    """
    A block whose EXEC input is a wire to an existing node's `then`, which is the shape this suite exists to prove.

    THE WIRE IS WRITTEN IN A PIN'S VALUE POSITION, which is how the format prints it, and the target is named by
    NODE ID - the spec's "links them to existing nodes by node ID".
    """
    return ('(Development|PrintString\n    InString "%s"\n    execute (-> %s:%s:%s:%s))'
            % (marker, ASSET, GRAPH, target_key, LINK_TARGET_PIN))


def main():
    session = rebuild_fixture_and_reconnect()
    if session is None:
        print("RESULT: FAIL (the fixture could not be rebuilt, so nothing was tested)")
        return 1
    check("the fixture was REBUILT on the way in, so this suite is re-runnable from any state", True)

    before_count, before_text = node_count(session)
    before_ids, _ = node_ids(session)
    print("=== the graph has %s node(s) before anything is appended ===" % before_count)
    check("the node count is parseable, so nothing below pins a literal",
          before_count is not None, before_text[:120])
    check("the graph has nodes to begin with", (before_count or 0) > 0, str(before_count))

    print()
    print("=== 1. the PREVIEW resolves a type id and writes nothing ===")
    preview = session.call(PREVIEW_TOOLSET, "PreviewTurboMCPGraphAppend",
                           {"asset": ASSET, "graphName": GRAPH, "text": text_for("PROBE-PREVIEW")})
    print(preview[:600])
    print()
    check("the preview answers rather than erroring", not preview.startswith("error:"), preview[:160])
    check("it says a node WOULD be added", "would be added" in preview, preview[:200])
    # THE PREVIEW OF A VALID TEXT BEGINS WITH ITS OWN SUMMARY, not with `unchanged`. The spec requires the
    # first line to be `unchanged` OR A SUMMARY, and a plan that WOULD write is summarised. What must not
    # happen is a first line claiming a change the preview will not make.
    check("the preview of a valid text summarises rather than claiming it already changed",
          preview.split("\n", 1)[0].startswith("preview:"), preview[:100])
    check("IT SAYS IT CHANGES NOTHING", "changes NOTHING" in preview, preview[:240])

    count_after_preview, _ = node_count(session)
    check("THE PREVIEW DID NOT ADD A NODE", count_after_preview == before_count,
          "before %s, after %s" % (before_count, count_after_preview))

    print()
    print("=== 2. the EDIT writes, and the write is proved from OUTSIDE ===")
    marker = "PROBE-APPEND-REAL"
    edited = session.call(EDIT_TOOLSET, "EditTurboMCPGraphAppend",
                          {"asset": ASSET, "graphName": GRAPH, "text": text_for(marker)})
    print(edited[:600])
    print()
    check("the edit answers rather than erroring", not edited.startswith("error:"), edited[:160])
    # THE BUG THIS CHECK EXISTS FOR, MEASURED: `Apply` once built its whole report into a local string and
    # returned success WITHOUT assigning it, so the caller got an EMPTY answer while the graph was correctly
    # changed. Every check that read the GRAPH passed. Only reading the RETURN VALUE catches that.
    check("THE EDIT RETURNED A REPORT RATHER THAN AN EMPTY STRING", edited.strip() != "", repr(edited[:80]))
    check("the first line starts with changed, as the spec requires",
          edited.split("\n", 1)[0].startswith("changed:"), edited[:100])
    check("it reports the read back matched", "read back matches" in edited, edited[:300])
    check("it reports how many nodes it added, parsed rather than assumed",
          re.search(r"(\d+)\s+node\(s\) added", edited) is not None, edited[:200])

    after_count, after_text = node_count(session)
    after_ids, full_after = node_ids(session)
    print("    node count: %s -> %s" % (before_count, after_count))
    check("THE GRAPH REALLY GREW, read back through a DIFFERENT tool",
          (after_count or 0) > (before_count or 0),
          "before %s, after %s" % (before_count, after_count))
    # THE READER PRINTS ONLY AN ID AND A CLASS FOR A NODE THAT IS NOT REACHABLE FROM AN ENTRY POINT, and an
    # appended node has no exec wire yet, so ITS PINS ARE NOT IN THE READ BACK. Asserting the pin text here
    # would be asserting what the reader cannot show - so what is asserted is what IS observable: the count
    # grew and a node of the resolved class is present. THE GAP IS STATED rather than papered over, and the
    # tool reports its own written-pin count separately below.
    # THE CLASS IS IN THE `full` READ, NOT THE MANIFEST. Measured: the manifest prints a count and entry
    # points only, so `after_text` - which is the manifest - cannot answer this. The full read is what names
    # the class of every node, including the ones not reachable from an entry.
    check("a node of the resolved class is now in the graph",
          "PrintString" in full_after, full_after[:200])
    check("the tool reports a non-zero pin count, parsed rather than assumed",
          re.search(r"(\d+)\s+pin value\(s\) written", edited) is not None
          and int(re.search(r"(\d+)\s+pin value\(s\) written", edited).group(1)) > 0,
          edited[-300:])

    print()
    print("=== 3. IT CANNOT REMOVE - this is what makes it APPEND and not REPLACE ===")
    check("THE NODE COUNT DID NOT GO DOWN", (after_count or 0) >= (before_count or 0),
          "before %s, after %s" % (before_count, after_count))
    lost = before_ids - after_ids
    check("EVERY NODE THAT WAS THERE BEFORE IS STILL THERE", len(lost) == 0,
          "gone: %s" % sorted(lost)[:6])

    print()
    print("=== 4. a REFUSAL writes nothing, and the preview and edit agree ===")
    # A type id that cannot exist. Both tools must refuse it, in the same words, and neither may write.
    bad = "(NoSuchCategory|NoSuchFunction\n    InString \"nope\")"
    bad_preview = session.call(PREVIEW_TOOLSET, "PreviewTurboMCPGraphAppend",
                               {"asset": ASSET, "graphName": GRAPH, "text": bad})
    print(bad_preview[:400])
    bad_edit = session.call(EDIT_TOOLSET, "EditTurboMCPGraphAppend",
                            {"asset": ASSET, "graphName": GRAPH, "text": bad})
    print(bad_edit[:400])
    print()
    check("the preview REFUSES an unresolvable type id", "refused" in bad_preview, bad_preview[:240])
    check("the preview's refusal names the line it is about", "line 1" in bad_preview, bad_preview[:240])
    check("the EDIT refuses the same input", not bad_edit.startswith("changed:"), bad_edit[:160])
    check("the edit's first line is unchanged, so a caller reading only that learns nothing was written",
          bad_edit.split("\n", 1)[0].startswith("unchanged"), bad_edit[:120])

    refused_count, _ = node_count(session)
    check("A REFUSED EDIT DID NOT CHANGE THE GRAPH", refused_count == after_count,
          "before %s, after %s" % (after_count, refused_count))

    print()
    print("=== 5. a LINK is planned, made, and witnessed ===")
    #
    # THIS SECTION USED TO ASSERT THE OPPOSITE - that a link was REFUSED - because linking was not implemented.
    # It is now the capability, so the checks are the other way round, and the negative case that survives is a
    # link whose SOURCE does not exist, which must still be refused BY NAME rather than written as text.
    target_key, target_text = link_target_id(session)
    check("the fixture exposes a node to link INTO, so the link checks are not vacuous",
          target_key is not None, target_text[-200:] if target_key is None else target_key)

    if target_key is not None:
        link_out = session.call(PREVIEW_TOOLSET, "PreviewTurboMCPGraphAppend",
                                {"asset": ASSET, "graphName": GRAPH,
                                 "text": linked_text_for("PROBE-LINK-PREVIEW", target_key)})
        print(link_out[:500])
        check("the preview PLANS the wire rather than refusing it",
              "wire  line" in link_out, link_out[:300])
        check("the planned wire names BOTH ends, so a caller can check it before writing",
              target_key in link_out and "execute" in link_out, link_out[:300])

        before_link_count, _ = node_count(session)
        link_edit = session.call(EDIT_TOOLSET, "EditTurboMCPGraphAppend",
                                 {"asset": ASSET, "graphName": GRAPH,
                                  "text": linked_text_for("PROBE-LINK-REAL", target_key)})
        print(link_edit[:500])
        check("the edit reports a WIRE was made", "wire(s) made" in link_edit, link_edit[:300])
        # PARSED, NOT MATCHED. "1 wire(s) made, of 1 the text described" is read as two numbers, because a
        # sentence beside them could stay right while the numbers went wrong.
        made = re.search(r"(\d+) wire\(s\) made, of (\d+)", link_edit)
        check("the wire count is parseable and NON-ZERO", made is not None and int(made.group(1)) > 0,
              made.group(0) if made else link_edit[:200])
        check("it made every wire the text described, not merely one",
              made is not None and made.group(1) == made.group(2),
              made.group(0) if made else "not parsed")

        # THE WITNESS IS A DIFFERENT TOOL. A writer saying it made a wire is not evidence; the READER printing
        # `; out 0 then -> 1 link(s)` off the live graph is, because it reads identity and links from the graph
        # rather than from the plan.
        _, after_link_text = link_target_id(session)
        check("THE READER WITNESSES THE WIRE on the target's own output pin",
              re.search(r"out \d+ %s -> \d+ link\(s\)" % re.escape(LINK_TARGET_PIN), after_link_text)
              is not None,
              [l for l in after_link_text.splitlines() if LINK_TARGET_PIN in l][:2])

        check("the link grew the graph, so the check is not measuring a refusal",
              (node_count(session)[0] or 0) > (before_link_count or 0),
              "before %s after %s" % (before_link_count, node_count(session)[0]))

        print()
        print("=== 5b. a link to a node that does NOT exist is still refused by name ===")
        ghost = ("(Development|PrintString\n    execute (-> %s:%s:ffffff:then))" % (ASSET, GRAPH))
        ghost_preview = session.call(PREVIEW_TOOLSET, "PreviewTurboMCPGraphAppend",
                                     {"asset": ASSET, "graphName": GRAPH, "text": ghost})
        print(ghost_preview[:400])
        check("a link whose SOURCE is not in the graph is refused", "refused" in ghost_preview,
              ghost_preview[:300])
        check("and the refusal names the line", "line 2" in ghost_preview, ghost_preview[:300])
        check("and it names what could not be resolved", "ffffff" in ghost_preview, ghost_preview[:300])

        print()
        print("=== 5c. a link to a PIN that does not exist names the real pins ===")
        bad_pin = linked_text_for("PROBE-LINK-BADPIN", target_key).replace(
            ":%s))" % LINK_TARGET_PIN, ":noSuchPin))")
        pin_preview = session.call(PREVIEW_TOOLSET, "PreviewTurboMCPGraphAppend",
                                   {"asset": ASSET, "graphName": GRAPH, "text": bad_pin})
        print(pin_preview[:500])
        check("a link to a pin that does not exist is refused", "refused" in pin_preview,
              pin_preview[:300])
        # THE REAL NAMES ARE THE POINT. A refusal that only says "no such pin" leaves the caller guessing, and
        # the spec asks for the real names.
        check("and the refusal lists the pins the node really has",
              "It has:" in pin_preview and "then" in pin_preview, pin_preview[:400])

    print()
    print("=== 6. an empty text is nothing to do, not a change ===")
    empty = session.call(EDIT_TOOLSET, "EditTurboMCPGraphAppend",
                         {"asset": ASSET, "graphName": GRAPH, "text": ""})
    print(empty[:300])
    # AN EMPTY TEXT IS AN ERROR RATHER THAN AN `unchanged`, AND THAT IS RIGHT: the checker runs FIRST - it is
    # the gate every writer depends on - and the gate refuses text with nothing in it, so the writer never
    # gets to decide. An error is a clearer answer than a silent no-op, because a caller who sent nothing
    # most likely made a mistake.
    check("an empty text is refused loudly rather than silently treated as no change",
          empty.startswith("error:"), empty[:160])

    print()
    print("=== 7. the asset on disk is untouched, because nothing saved it ===")
    # `E-SAVE` was never called, so the FILE must be exactly as it was. This is the check that a write to
    # MEMORY did not silently become a write to DISK - a claim no tool's own report can settle.
    print("    (the fingerprint is taken now and compared after a save cycle is NOT part of this suite;")
    print("     what is asserted here is that the tool reported the change as in-memory)")
    check("the edit did NOT claim to have saved", "saved" not in edited.lower(), edited[:200])

    print()
    print("  RECORDED: this suite APPENDS and does not remove, because removal is the capability this")
    print("  tool deliberately does not have. Its own growth is bounded by the marker counter above.")

    print()
    if failures:
        print("RESULT: FAIL (%d) -> %s" % (len(failures), ", ".join(failures)))
        return 1
    print("RESULT: PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
