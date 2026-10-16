"""Prove the acceptance asset's probe graphs contain the structures the specs need.

WHY THIS EXISTS
---------------
Two specification documents, written independently, both found the same hole:

`Spec/WORKING_GRAPH_KNOTS.md` found that `KnotProbe` cannot test the CROSSING case - its three knots
are already on the wire and the straight route between its two calls passes over nothing, so a crossing
predicate finds nothing and the primary positive case is unreachable.

`Spec/WORKING_GRAPH_TIDY.md` found the asset had ZERO COMMENT BOXES, measured through `R-COMMENT-LIST`
as `0 box(es), 0 nested`, so tidy's box-corner rule, its wire-length rule and nesting itself all rested
on code review.

`LayoutProbe` was added to the commandlet to close both holes. THIS script proves the structures are
actually in the saved asset rather than in the code that claims to build them.

WHAT IT CHECKS, AND HOW EACH CHECK COULD FAIL
---------------------------------------------
- the commandlet EXITS ZERO. It refuses to save when any probe graph is incomplete, so a zero exit is
  itself the assertion that every graph built;
- every probe graph is reported with a node count. Fails if a graph is missing or empty;
- `LayoutProbe` reports MORE THAN ONE node. A graph with one node cannot contain a crossing, a nesting
  or a neighbouring wire;
- the reported CROSSING count is greater than zero, for wires AND for distinct nodes. This is the exact
  number the knot spec said was unreachable. It was zero on the first attempt, which is how the
  coordinates were found to be wrong - so this check has ALREADY FAILED once, which is what makes it
  worth trusting;
- the reported NESTING is at least two boxes with at least one nested. The material and layout fixtures
  both need this and nothing in the asset had it before;
- at least one NEIGHBOURING-COLUMN wire exists, because the knot spec says a pass must SKIP those and a
  skip rule with nothing to skip is untested;
- the counts are PARSED, never pinned. A literal would have to be edited every time a probe grows, and
  this project has recorded five separate rounds lost to a pinned count.

THE EDITOR HALF IS OPTIONAL AND SAID SO. The commandlet needs no editor, and this script does NOT start
one. When an editor HAPPENS to be running, the readers are driven against the saved asset as a second,
independent witness; when none is running the script says so and the commandlet's own measurements stand
alone. A check that silently skipped itself would be worse than no check.
"""

import os
import re
import subprocess
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LOG = os.path.join(REPO, ".agent", "logs", "make-acceptance-asset.log")
SCRIPT = os.path.join(REPO, "Tools", "make-acceptance-asset.ps1")
ASSET = "TurboMCPAcceptance"

# The graphs every build must produce. LayoutProbe is the one this round added.
REQUIRED_GRAPHS = [
    "KnotProbe", "DeadProbe", "LocalVarProbe", "SaneProbe", "LayoutProbe", "ScopedTidyProbe",
    "ValueCycleProbe", "SelfLinkProbe", "DepthLimitProbe", "TargetPinRoundTripProbe",
    "LargeGraphProbe",
]
# The graph added to close the two specifications' holes, and the one with structures to assert.
LAYOUT = "LayoutProbe"


class Report:
    def __init__(self, text, exit_code):
        self.text = text
        self.exit_code = exit_code

    def log_text(self):
        try:
            with open(LOG, "r", encoding="utf-8", errors="replace") as handle:
                return handle.read()
        except OSError:
            return ""

    def number(self, pattern, default=None):
        """One parsed number from the WHOLE run, log included.

        BOTH the captured stdout AND the log file are searched, because `UE_LOG` output reaches the log
        reliably and the console only sometimes. Searching one of them made an earlier version of this
        script report "0 crossing" for a build that had six, which is the probe-reader-wrong failure
        this project keeps recording.
        """
        for source in (self.text, self.log_text()):
            match = re.search(pattern, source)
            if match:
                return int(match.group(1))
        return default


def main():
    failures = []

    def check(name, ok, detail=""):
        # THE DETAIL IS SHOWN ONLY WHEN THE CHECK FAILS, and the ARGUMENT ORDER is (ok, detail) - the
        # first version of this helper passed them the other way round on the node-count checks, so a
        # passing check printed "no count found in stdout or the log" underneath it. A passing check
        # that prints a failure message is worse than a bare pass: it teaches the reader to ignore the
        # text.
        print("  %-4s %s%s" % ("ok" if ok else "FAIL", name, ("  " + detail) if detail and not ok else ""))
        if not ok:
            failures.append(name)

    print("=== running the commandlet, which needs no editor ===")
    # `shell=True` BECAUSE THE COMMAND REDIRECTS ITS OWN OUTPUT. Passing the whole string as one
    # argument to `["cmd.exe", "/c", ...]` makes Python quote it and cmd rejects it. The command owns
    # the redirect and Python only reads the log - letting subprocess open the same file gives "the
    # process cannot access the file because it is being used by another process" and an empty log.
    # A TIMEOUT OF ITS OWN, BECAUSE THE SCRIPT IT CALLS CAN LEGITIMATELY WAIT A LONG TIME AND NOTHING BOUNDED IT.
    #
    # MEASURED, WITH THE HARNESS DEADLINE IN PLACE: a full run reported `passed 37, failed 2, blocked 0, of 39` and
    # THIS SUITE WAS ONE OF THE TWO - `RESULT: TIMED OUT (600s)`. Run ALONE it also exceeds 600 seconds with no
    # other suite involved, so it is THE UNBOUNDED UNIT that blocked the whole run for four rounds. It stayed
    # invisible because a hang and a slow failure look identical from outside.
    #
    # THE TWO TERMS THAT ADD UP PAST ANY DEADLINE: this call had NO `timeout=`, and
    # `Tools/make-acceptance-asset.ps1` now waits up to 300 SECONDS on a named mutex, waits for the editor PROCESS
    # rather than sleeping, and the harness's start path can spend two 240-second timeouts. EVERY ONE OF THOSE WAITS
    # IS CORRECT AND WAS ADDED BECAUSE A RACE PRODUCED A WRONG ANSWER, and their sum still exceeds the budget.
    #
    # SO THE SUITE NOW BOUNDS ITSELF AND SAYS SO, INSTEAD OF BEING KILLED BY THE HARNESS. A suite that reports
    # "the rebuild did not finish in N seconds" is DIAGNOSABLE; a suite the harness had to kill is visible only
    # because the deadline exists at all. THE FAILURE IS REPORTED, NEVER SWALLOWED - an expired rebuild must not
    # look like a rebuild that ran.
    REBUILD_TIMEOUT_SECONDS = 240
    try:
        completed = subprocess.run(
            ["pwsh", "-NoProfile", "-File", SCRIPT],
            capture_output=True, text=True, cwd=REPO,
            timeout=REBUILD_TIMEOUT_SECONDS)
    except subprocess.TimeoutExpired as expired:
        print()
        print("THE FIXTURE REBUILD DID NOT FINISH IN %d SECONDS." % REBUILD_TIMEOUT_SECONDS)
        print("  This is a TIMEOUT, not a build failure and not a passing run. The script was still working:")
        print("  it waits up to 300s on the acceptance-asset mutex, waits for the editor process, and the")
        print("  harness's start path can spend two 240s timeouts of its own. Every one of those waits is")
        print("  deliberate, and their SUM is what this bound exists to catch.")
        # A STAND-IN WITH A NON-ZERO EXIT SO EVERY DOWNSTREAM CHECK FAILS FOR THE RIGHT REASON rather than
        # silently reading whatever partial stdout happened to be buffered.
        class _Expired(object):
            stdout = ""
            stderr = "timed out after %ds" % REBUILD_TIMEOUT_SECONDS
            returncode = 124
        completed = _Expired()
    report = Report(completed.stdout or "", completed.returncode)
    print(report.text.strip() or "(no stdout; the log is read instead)")

    check("the commandlet exited zero, which it only does when EVERY probe graph built",
          completed.returncode == 0, "exit %s" % completed.returncode)

    print()
    print("=== every probe graph is present with nodes ===")
    for graph in REQUIRED_GRAPHS:
        count = report.number(r"%s:\s*(\d+)\s*node" % re.escape(graph))
        check("%s has a reported node count" % graph, count is not None,
              "no count found in stdout or the log")
        if count is not None:
            check("%s is not empty" % graph, count > 0, "%d node(s)" % count)

    print()
    print("=== LayoutProbe: THE CROSSING, which KnotProbe could not provide ===")
    crossing_wires = report.number(r"LayoutProbe crossing:\s*(\d+)\s*wire")
    crossing_nodes = report.number(r"cross\s*(\d+)\s*distinct node")
    print("    %s wire(s) cross %s distinct node(s)" % (crossing_wires, crossing_nodes))
    check("it reports a crossing count at all", crossing_wires is not None)
    # THE CHECK THAT ALREADY FAILED ONCE. The first coordinates produced "10 wire(s) cross 0 distinct
    # node(s)", which is why the geometry is dumped when this fails.
    check("at least one wire crosses a node", crossing_wires is not None and crossing_wires > 0,
          "the knot spec's primary case is unreachable if this is zero")
    check("at least one distinct node is crossed", crossing_nodes is not None and crossing_nodes > 0,
          "a wire count without a node is not a crossing")

    print()
    print("=== LayoutProbe: THE NESTING, which the asset had none of ===")
    boxes = report.number(r"LayoutProbe boxes:\s*(\d+)\s*box")
    nested = report.number(r"boxes:\s*\d+\s*box\(es\),\s*(\d+)\s*nested")
    print("    %s box(es), %s nested" % (boxes, nested))
    check("it reports a box count at all", boxes is not None)
    check("there are at least two boxes", boxes is not None and boxes >= 2, str(boxes))
    check("at least one box is geometrically nested in another",
          nested is not None and nested >= 1,
          "tidy's nesting rule rests on code review without this")

    print()
    print("=== LayoutProbe: a NEIGHBOURING-column wire for the knot SKIP rule ===")
    neighbours = report.number(r"neighbouring-column wires:\s*(\d+)")
    print("    %s neighbouring wire(s)" % neighbours)
    check("at least one wire joins neighbouring columns",
          neighbours is not None and neighbours > 0,
          "a skip rule with nothing to skip is untested")

    print()
    print("=== every graph is named as built in the commandlet summary ===")
    # The summary is the commandlet's own all-probes verdict. Assert every guard fixture too,
    # rather than allowing stale per-graph log lines to make a partial rebuild look complete.
    summary = re.search(
        r"KnotProbe=(\w+) DeadProbe=(\w+) LocalVarProbe=(\w+) AppendChainProbe=(\w+) "
        r"SaneProbe=(\w+) ScopedTidyProbe=(\w+) LayoutProbe=(\w+) ValueCycleProbe=(\w+) "
        r"SelfLinkProbe=(\w+) DepthLimitProbe=(\w+) TargetPinRoundTripProbe=(\w+) "
        r"LargeGraphProbe=(\w+) GraphReaderFixture=(\w+)",
        report.log_text() + report.text)
    summary_names = ["KnotProbe", "DeadProbe", "LocalVarProbe", "AppendChainProbe", "SaneProbe",
                     "ScopedTidyProbe", "LayoutProbe", "ValueCycleProbe", "SelfLinkProbe",
                     "DepthLimitProbe", "TargetPinRoundTripProbe", "LargeGraphProbe",
                     "GraphReaderFixture"]
    check("the summary names every probe graph", summary is not None,
          "the commandlet summary is incomplete")
    if summary:
        for name, state in zip(summary_names, summary.groups()):
            check("the summary says %s built" % name, state == "ok", state)

    print()
    print("=== the editor half, which is OPTIONAL and says so ===")
    # The commandlet needs no editor and this script does not start one. When an editor is already
    # running, the readers are driven as an INDEPENDENT witness to the saved asset; when none is, the
    # commandlet's own measurements stand alone and that is stated rather than silently skipped.
    sys.path.insert(0, os.path.join(REPO, "Tools"))
    editor_reachable = False
    try:
        from tmcp import READ_TOOLSET, Session  # noqa: PLC0415
        session = Session()
        session.connect()
        editor_reachable = True
    except Exception as error:  # noqa: BLE001 - any failure means "no editor", which is allowed
        print("    no editor is running (%s), so only the commandlet's measurements are checked"
              % type(error).__name__)

    if editor_reachable:
        listing = session.call(READ_TOOLSET, "ReadTurboMCPCommentList",
                               {"asset": ASSET, "graphName": LAYOUT})
        print(listing[:700])
        boxes_seen = re.search(r"(\d+) box\(es\)", listing)
        nested_seen = re.search(r"(\d+) nested", listing)
        check("R-COMMENT-LIST sees the boxes the commandlet reported",
              boxes_seen is not None and int(boxes_seen.group(1)) == (boxes or -1),
              "the reader says %s, the commandlet said %s"
              % (boxes_seen.group(1) if boxes_seen else "?", boxes))
        check("R-COMMENT-LIST sees the NESTING the commandlet reported",
              nested_seen is not None and int(nested_seen.group(1)) >= 1,
              "the asset had 0 nested before this round")
    else:
        print("    SKIPPED, and recorded as skipped rather than passed")

    print()
    if failures:
        print("RESULT: FAIL (%d) -> %s" % (len(failures), ", ".join(failures)))
        return 1
    print("RESULT: PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
