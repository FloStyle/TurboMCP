# ACCEPTANCE ROW THIS SUITE CLAIMS TO PROVE: T-WRITE-1
"""
`P-GRAPH-CHECK` (`T-WRITE-1`): graph text is checked, and NOTHING is written.

THE GATE EVERY WRITE ENTRY POINT DEPENDS ON, so this suite is not only asking whether the checker answers -
it is asking whether its answer can be TRUSTED. That means three separate things, and each is a way to be
wrong:

  1. IT FINDS WHAT IS WRONG. `T-WRITE-1` requires a text with a wrong node type, a wrong pin and a type
     mismatch to produce EXACTLY THREE findings, each with line, column and close matches.
  2. IT DOES NOT INVENT FAULTS. Valid text - real text, read out of a real graph - must be accepted, because
     a checker that refuses correct input stops every write this project has.
  3. IT WRITES NOTHING, PROVED RATHER THAN PROMISED. The claim is checked from OUTSIDE the tool: the asset's
     hash, size and mtime before and after, plus a read of the graph through a DIFFERENT tool. A tool
     reporting its own innocence is not evidence.

The counts are pinned for the texts THIS SUITE supplies, which is the opposite of the shared-asset rule and
is correct here: the suite owns the input, so the expected finding count is known rather than observed.
"""

import hashlib
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from tmcp import Session  # noqa: E402

PREVIEW_TOOLSET = "TurboMCP.TurboMCPPreviewToolset"
READ_TOOLSET = "TurboMCP.TurboMCPReadToolset"

ASSET = "BP_NodeTest"
GRAPH = "EventGraph"

# The asset's file, so "nothing was written" can be checked on disk rather than from the tool's word.
REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ASSET_FILE = os.path.join(REPO, "TestProject", "Content", "Tests", "BP_NodeTest.uasset")

# A REAL node in BP_NodeTest:EventGraph, and its real type ID. Taken from `R-GRAPH-LOAD`, not invented -
# a fixture this suite made up could not tell a working checker from one that refuses everything.
REAL_TYPE = "Development|PrintString"
REAL_NODE_ID = "/Game/Tests/BP_NodeTest.BP_NodeTest:EventGraph:09723b"

# Valid text. Every pin named here is a real pin of PrintString.
GOOD = '(Development|PrintString ; %s\n    InString "hi" ; in 3\n    bPrintToScreen true ; in 4)\n' % REAL_NODE_ID

# T-WRITE-1's three faults, one per line, each in a text this suite controls.
BAD_TYPE = "(Development|PrintStringg ; fake:1111\n    InString \"hi\")\n"
BAD_PIN = "(%s ; %s\n    NotAPin \"hi\")\n" % (REAL_TYPE, REAL_NODE_ID)
BAD_MISMATCH = "(%s ; %s\n    InString 42.0)\n" % (REAL_TYPE, REAL_NODE_ID)

# THE EXACT TEXT `T-WRITE-1` DESCRIBES: a wrong node type, a wrong pin and a type mismatch.
THREE_FAULTS = BAD_TYPE + BAD_PIN + BAD_MISMATCH

failures = []


def check(name, ok, detail=""):
    if ok:
        print("  ok   %s  %s" % (name, detail))
    else:
        print("  FAIL %s  %s" % (name, detail))
        failures.append(name)


def check_tool(name, text, expect_kind=None, expect_line=None):
    """Calls the checker and returns its answer, printing it. Kind/line are asserted by the caller."""
    out = session.call(PREVIEW_TOOLSET, "PreviewTurboMCPGraphCheck",
                       {"asset": ASSET, "graphName": GRAPH, "text": text})
    return out


def finding_count(out):
    """
    The tool's counts, parsed from its own numbers.

    THE ZERO CASE IS PARSED TOO, AND THE FIRST VERSION OF THIS FUNCTION MISSED IT. The tool prints the
    `N finding(s): ...` totals line only when there ARE findings, and says `the text No issues found by these checks`
    instead when there are none. So a clean run returned `None` here and the suite reported a failure on
    the CLEAN case - an assertion that could not be satisfied by the correct answer. Reading the accepted
    sentence as a parsed zero rather than as an absent number is the fix.
    """
    m = re.search(r"(\d+) finding\(s\):\s*(\d+) node-type,\s*(\d+) pin-name,\s*(\d+) type-mismatch,"
                  r"\s*(\d+) refused,\s*(\d+) unjudged", out)
    if m is not None:
        return {
            "total": int(m.group(1)),
            "node-type": int(m.group(2)),
            "pin-name": int(m.group(3)),
            "type-mismatch": int(m.group(4)),
            "refused": int(m.group(5)),
            "unjudged": int(m.group(6)),
        }
    if "No issues found by these checks" in out:
        return {"total": 0, "node-type": 0, "pin-name": 0, "type-mismatch": 0,
                "refused": 0, "unjudged": 0}
    return None


def findings(out):
    """Every finding as (kind, line, column), PARSED from its own header rather than substring-matched."""
    return [(m.group(1), int(m.group(2)), int(m.group(3)))
            for m in re.finditer(r"^\s+([a-z-]+)\s+line (\d+), column (\d+):", out, re.M)]


def asset_fingerprint():
    """The asset's identity on disk, so a write would be visible even if the tool said otherwise."""
    if not os.path.isfile(ASSET_FILE):
        return None
    st = os.stat(ASSET_FILE)
    h = hashlib.md5()
    with open(ASSET_FILE, "rb") as handle:
        h.update(handle.read())
    return (st.st_size, st.st_mtime_ns, h.hexdigest())


session = None


def main():
    global session
    session = Session()
    session.connect()

    print()
    print("=== T-WRITE-1: one text, three faults, EXACTLY three findings ===")
    out = check_tool("three", THREE_FAULTS)
    print(out)
    print()

    counts = finding_count(out)
    check("the totals line is parseable", counts is not None, out[-160:])

    if counts is not None:
        check("EXACTLY THREE findings", counts["total"] == 3, str(counts))
        check("one of them is a node-type finding", counts["node-type"] == 1, str(counts))
        check("one of them is a pin-name finding", counts["pin-name"] == 1, str(counts))
        check("one of them is a type-mismatch finding", counts["type-mismatch"] == 1, str(counts))
        check("none of them is a refusal", counts["refused"] == 0, str(counts))
        check("none of them is unjudged, so every literal WAS judged",
              counts["unjudged"] == 0, str(counts))

    parsed = findings(out)
    check("three findings each carry a line and a column", len(parsed) == 3, str(parsed))

    if len(parsed) == 3:
        kinds = [p[0] for p in parsed]
        lines = [p[1] for p in parsed]
        columns = [p[2] for p in parsed]
        check("the kinds are exactly the three T-WRITE-1 names",
              sorted(kinds) == ["node-type", "pin-name", "type-mismatch"], str(kinds))

        # ONE FINDING PER LINE, because each fault is on its own line. A checker that reported the first
        # fault at the right place and the rest at line 1 would pass a kinds-only check.
        check("each fault is reported on ITS OWN line",
              lines == [1, 4, 6], "expected [1, 4, 6], got %s" % lines)
        check("every column points at the start of what was written",
              all(c > 0 for c in columns), str(columns))

    check("the node-type finding carries the CLOSEST REAL ID",
          "Closest real IDs:" in out and REAL_TYPE in out, out[:400])

    print()
    print("=== the three faults one at a time, so a single fault cannot hide behind the others ===")
    # `expect_line` IS 1 FOR THE NODE TYPE AND 2 FOR THE OTHER TWO, AND THE FIRST VERSION GOT THIS WRONG
    # BY ASSUMING 2 FOR ALL THREE. A node form is `(<TypeId>`, so an unknown TYPE is wrong at column 1 of
    # LINE 1 - the line the type ID is written on. A wrong PIN or a mismatch is wrong on the pin's own
    # line, which is line 2 here. The suite was asserting the tool had misplaced a finding when the suite
    # had misplaced its expectation, which is the "reader fault reported as a subject fault" mistake.
    for label, text, kind, expect_line in (("a wrong node type", BAD_TYPE, "node-type", 1),
                                           ("a wrong pin name", BAD_PIN, "pin-name", 2),
                                           ("a type mismatch", BAD_MISMATCH, "type-mismatch", 2)):
        one = check_tool(label, text)
        print(one)
        print()
        counts = finding_count(one)
        check("%s gives exactly 1 finding" % label,
              counts is not None and counts["total"] == 1, str(counts))
        check("%s names its kind as %s" % (label, kind),
              counts is not None and counts.get(kind) == 1, str(counts))
        check("%s is reported at line %d, where the fault is written" % (label, expect_line),
              len(findings(one)) == 1 and findings(one)[0][1] == expect_line, str(findings(one)))

    print()
    print("=== the wrong pin is told the REAL names ===")
    pin_out = check_tool("wrong pin", BAD_PIN)
    for real in ("InString", "bPrintToScreen", "execute"):
        check("the refusal names the real pin %s" % real, real in pin_out, pin_out[:200])

    print()
    print("=== GOOD TEXT IS ACCEPTED, because a checker that refuses correct input stops every write ===")
    good_out = check_tool("good", GOOD)
    print(good_out)
    print()
    good_counts = finding_count(good_out)
    check("real text read from a real graph gives ZERO findings",
          good_counts is not None and good_counts["total"] == 0, str(good_counts))
    check("it says the text No issues found by these checks", "No issues found by these checks" in good_out, good_out[:200])
    check("it counted what it read rather than answering vaguely",
          "1 node form(s) and 2 pin line(s) read" in good_out, good_out[:200])

    print()
    print("=== a graph name is REQUIRED, because a type and a pin are resolved against one ===")
    no_graph = session.call(PREVIEW_TOOLSET, "PreviewTurboMCPGraphCheck",
                            {"asset": ASSET, "graphName": "", "text": GOOD})
    print(no_graph)
    print()
    check("an unnamed graph is refused rather than guessed at",
          no_graph.startswith("error:"), no_graph[:160])
    check("the refusal lists the graphs that would work", "EventGraph" in no_graph, no_graph[:300])

    bad_graph = session.call(PREVIEW_TOOLSET, "PreviewTurboMCPGraphCheck",
                             {"asset": ASSET, "graphName": "NoSuchGraph", "text": GOOD})
    check("an unknown graph is refused by name", bad_graph.startswith("error:"), bad_graph[:160])

    empty = session.call(PREVIEW_TOOLSET, "PreviewTurboMCPGraphCheck",
                         {"asset": ASSET, "graphName": GRAPH, "text": ""})
    check("empty text is refused rather than reported as clean",
          empty.startswith("error:"), empty[:160])

    print()
    print("=== NOTHING IS WRITTEN, PROVED FROM OUTSIDE THE TOOL ===")
    # The fingerprint is taken around a run of ALL the texts above, so a write by any of them shows up.
    before = asset_fingerprint()
    for text in (GOOD, BAD_TYPE, BAD_PIN, BAD_MISMATCH, THREE_FAULTS):
        session.call(PREVIEW_TOOLSET, "PreviewTurboMCPGraphCheck",
                     {"asset": ASSET, "graphName": GRAPH, "text": text})
    after = asset_fingerprint()

    check("the asset file exists where the check says it is", before is not None, ASSET_FILE)
    check("the asset's SIZE, MTIME and MD5 are unchanged by every check above",
          before == after,
          "before %s, after %s" % (before, after))

    # AND THROUGH A DIFFERENT TOOL, because a hash proves the file did not change but not that the graph
    # in memory did not. `R-GRAPH-LOAD` is the reader, and it is not the tool under test.
    #
    # AT `manifest` THE READER PRINTS THE ENTRY POINTS AND A NODE COUNT, NOT THE NODES - so a check for
    # `PrintString` here would fail on a correct reader. The first version of this suite asserted exactly
    # that and reported a fault in the tool. The graph is read at `bodies` instead, where the nodes ARE
    # printed, so the assertion is about the thing it claims to be about.
    loaded = session.call(READ_TOOLSET, "ReadTurboMCPGraphLoad",
                          {"asset": ASSET, "graphName": GRAPH, "detail": "bodies",
                           "format": "", "tokenBudget": ""})
    check("the graph still reads back through R-GRAPH-LOAD", not loaded.startswith("error:"),
          loaded[:160])
    check("the node the text named is still in the graph",
          REAL_TYPE in loaded, loaded[:240])

    manifest = session.call(READ_TOOLSET, "ReadTurboMCPGraphLoad",
                            {"asset": ASSET, "graphName": GRAPH, "detail": "manifest",
                             "format": "", "tokenBudget": ""})
    node_count = re.search(r";\s*(\d+)\s+node\(s\)", manifest)
    check("the manifest still reports a node count", node_count is not None, manifest[:200])
    check("and the graph still HAS nodes", node_count is not None and int(node_count.group(1)) > 0,
          node_count.group(1) if node_count else "no count")

    print()
    print("=== the tool is in the PREVIEW toolset, which cannot write ===")
    # Structural rather than behavioural: this project enforces "read-only" by WHICH CLASS a tool is in, so
    # a check that the tool is declared in the Preview toolset is the check that it cannot change an asset.
    schema = session.call(READ_TOOLSET, "ReadTurboMCPToolSchema",
                          {"toolset": "Preview", "tool": "PreviewTurboMCPGraphCheck"})
    check("it is declared in the PREVIEW toolset", "TurboMCPPreviewToolset" in schema, schema[:200])
    check("and NOT in the Edit toolset", "EditToolset" not in schema, schema[:200])

    print()
    if failures:
        print("RESULT: FAIL (%d) -> %s" % (len(failures), ", ".join(failures)))
        return 1
    print("RESULT: PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
