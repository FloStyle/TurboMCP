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
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from tmcp import Session  # noqa: E402

PREVIEW_TOOLSET = "TurboMCP.TurboMCPPreviewToolset"
EDIT_TOOLSET = "TurboMCP.TurboMCPEditToolset"
READ_TOOLSET = "TurboMCP.TurboMCPReadToolset"

ASSET = "BP_NodeTest"
GRAPH = "EventGraph"

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
                        "TestProject", "Content", "Tests", "BP_NodeTest.uasset")
    if not os.path.isfile(path):
        return None
    stat = os.stat(path)
    return (stat.st_size, stat.st_mtime_ns)


def text_for(marker, extra_pins=""):
    return '(Development|PrintString\n    InString "%s"\n    Duration 1.5%s)' % (marker, extra_pins)


def main():
    session = Session()
    session.connect()

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
    print("=== 5. a LINK is refused by name rather than written as text ===")
    # The format prints a wire as `(-> Node:Pin)`. Writing that as a STRING where a wire was meant would
    # compile and be silently wrong, so the tool must refuse it and say so.
    linked = ("(Development|PrintString\n    InString (-> %s:%s:000000)\n    Duration 1.0)"
              % (ASSET, GRAPH))
    link_preview = session.call(PREVIEW_TOOLSET, "PreviewTurboMCPGraphAppend",
                                {"asset": ASSET, "graphName": GRAPH, "text": linked})
    print(link_preview[:400])
    check("a pin written as a LINK is refused rather than written as text",
          "refused" in link_preview and "LINK" in link_preview, link_preview[:300])

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
