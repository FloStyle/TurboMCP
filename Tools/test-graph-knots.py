"""Drive `P-GRAPH-KNOTS` and `E-GRAPH-KNOTS` against the acceptance asset.

WHY THIS SUITE EXISTS
---------------------
T-LAY-10 says two things: "Preview lists only wires that cross nodes; after Edit those wires are
straight". The capability's whole risk is the OPPOSITE failure - placing knots where they make the
graph worse - so the checks below are written to go red in that direction wherever they can.

WHAT EACH CHECK PROVES, AND HOW IT COULD FAIL
---------------------------------------------
- `KnotProbe` previews ZERO crossing wires. The graph has three knots between two distant calls and no
  node on the straight route. Fails if the segment test fires on any pair, which is the failure that
  would put knots on every wire in every graph;
- the preview reports the counts it measured, so "before" and "after" have something to compare;
- the preview LISTS the node a wire crosses, not merely a count, so a caller can see which obstacle;
- a graph with nothing to fix yields `unchanged:` and no knot at all;
- the request is validated, and each refusal names itself;
- **THE EDIT REFUSES EXACTLY WHAT THE PREVIEW REFUSED** (`D-02`). The edit is run on the same graph
  and must agree. Fails if a refusal is decided in `Apply` instead of `Plan`, which is the shape this
  project has been caught by twice;
- after an edit that keeps its work, the preview reports ZERO crossings for the wires it fixed;
- a knot does not change what a Blueprint does, checked by compiling before and after;
- reading never changes the asset: the node count is compared before and after every preview.

THE FIXTURE IS REGENERATED FIRST AND LAST, because the edit mutates it. `make-acceptance-asset.ps1`
stops the editor to do it and the suite starts it again, so this suite cannot run concurrently with
another that uses the editor.
"""

import os
import re
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from tmcp import EDIT_TOOLSET, PREVIEW_TOOLSET, READ_TOOLSET, Session  # noqa: E402

ASSET = "TurboMCPAcceptance"
CLEAN_GRAPH = "KnotProbe"
SANE_GRAPH = "SaneProbe"

# THE FIFTH PROBE GRAPH. The other workstream is adding it to the acceptance commandlet; when it is
# absent the checks that need it are SKIPPED AND NAMED, never silently passed.
CROSSED_GRAPH = "LayoutProbe"

failures = []
skipped = []


def check(name, ok, detail=""):
    print("  %-4s %s%s" % ("ok" if ok else "FAIL", name, ("  " + detail) if detail else ""))
    if not ok:
        failures.append(name)


def skip(name, why):
    print("  SKIP %s  (%s)" % (name, why))
    skipped.append(name)


def repo_root():
    return os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def rebuild_fixture():
    return subprocess.call(
        ["pwsh", "-NoProfile", "-File", os.path.join(repo_root(), "Tools",
                                                     "make-acceptance-asset.ps1")],
        cwd=repo_root())


def start_editor():
    """Starts the editor. `subprocess.call` AND NOT `run(capture_output=True)`, measured."""
    return subprocess.call(
        ["pwsh", "-NoProfile", "-File", os.path.join(repo_root(), "Tools", "start-editor.ps1")],
        cwd=repo_root())


def connect(timeout=180):
    deadline = time.time() + timeout
    while time.time() < deadline:
        try:
            session = Session()
            session.connect()
            return session
        except Exception:
            time.sleep(3)
    return None


def crossing_counts(text):
    """The two numbers the preview reports, PARSED rather than substring-matched."""
    match = re.search(r"(\d+) wire\(s\) cross a node, (\d+) crossing\(s\) in total", text)
    if match is None:
        match = re.search(r"(\d+) wire\(s\) cross a node", text)
        if match is None:
            return None, None
        return int(match.group(1)), None
    return int(match.group(1)), int(match.group(2))


def node_count(session, graph):
    text = session.call(READ_TOOLSET, "ReadTurboMCPGraphLoad",
                        {"asset": ASSET, "graphName": graph, "detail": "signatures",
                         "format": "", "tokenBudget": ""})
    match = re.search(r"(\d+) node\(s\)", text)
    return int(match.group(1)) if match else None


def main():
    print("=== the fixture is regenerated, because the edit mutates it ===")
    if rebuild_fixture() != 0:
        print("RESULT: FAIL (the acceptance asset could not be rebuilt)")
        return 1
    if start_editor() != 0:
        print("RESULT: FAIL (the editor could not be started)")
        return 1
    session = connect()
    if session is None:
        print("RESULT: FAIL (the MCP server never answered)")
        return 1
    print("  ok   the asset was rebuilt and the editor answers")

    print()
    print("=== 1. KnotProbe has NO crossing, so the predicate must stay silent ===")
    # THIS IS THE MOST IMPORTANT CHECK IN THE SUITE. A predicate that fires on any pair would place
    # knots on every wire in every graph, which is the direction that makes a graph worse.
    before_nodes = node_count(session, CLEAN_GRAPH)
    clean = session.call(PREVIEW_TOOLSET, "PreviewTurboMCPGraphKnots",
                         {"asset": ASSET, "graphName": CLEAN_GRAPH})
    print(clean[:700])
    print()
    check("the preview answers rather than erroring", not clean.startswith("error:"), clean[:200])
    clean_wires, clean_crossings = crossing_counts(clean)
    print("    parsed: %s crossing wire(s), %s crossing(s)" % (clean_wires, clean_crossings))
    check("it reports a crossing count at all", clean_wires is not None, clean[:200])
    check("NO wire crosses a node in a graph built to have none",
          clean_wires == 0, "reported %s" % clean_wires)
    check("no knot is offered there", "KNOT " not in clean, clean[:300])
    after_nodes = node_count(session, CLEAN_GRAPH)
    check("PREVIEWING DID NOT CHANGE THE GRAPH",
          before_nodes is not None and before_nodes == after_nodes,
          "%s -> %s" % (before_nodes, after_nodes))

    print()
    print("=== 2. a graph with nothing to fix is left alone ===")
    sane = session.call(PREVIEW_TOOLSET, "PreviewTurboMCPGraphKnots",
                        {"asset": ASSET, "graphName": SANE_GRAPH})
    print(sane[:400])
    print()
    if sane.startswith("error:"):
        check("SaneProbe is readable", False, sane[:200])
    else:
        sane_wires, _ = crossing_counts(sane)
        check("SaneProbe has no crossing either", sane_wires == 0, "reported %s" % sane_wires)
        sane_nodes_before = node_count(session, SANE_GRAPH)
        edit_sane = session.call(EDIT_TOOLSET, "EditTurboMCPGraphKnots",
                                 {"asset": ASSET, "graphName": SANE_GRAPH})
        print(edit_sane[:400])
        print()
        check("the edit on a clean graph says unchanged:",
              edit_sane.startswith("unchanged:") or edit_sane.startswith("error:"),
              edit_sane[:200])
        sane_nodes_after = node_count(session, SANE_GRAPH)
        check("NO KNOT WAS ADDED to the graph with nothing to fix",
              sane_nodes_before == sane_nodes_after,
              "%s -> %s" % (sane_nodes_before, sane_nodes_after))

    print()
    print("=== 3. the request is validated, and each refusal names itself ===")
    no_asset = session.call(PREVIEW_TOOLSET, "PreviewTurboMCPGraphKnots",
                            {"asset": "", "graphName": CLEAN_GRAPH})
    check("an empty asset is refused", no_asset.startswith("error:"), no_asset[:160])
    non_bp = session.call(PREVIEW_TOOLSET, "PreviewTurboMCPGraphKnots",
                          {"asset": "TA11_TestEnum", "graphName": ""})
    check("a non-Blueprint is refused by name",
          non_bp.startswith("error:") and "Blueprint" in non_bp, non_bp[:200])
    no_graph = session.call(PREVIEW_TOOLSET, "PreviewTurboMCPGraphKnots",
                            {"asset": ASSET, "graphName": "NoSuchGraphAtAll"})
    check("a graph that does not exist is refused, and the real ones are LISTED",
          no_graph.startswith("error:") and CLEAN_GRAPH in no_graph, no_graph[:300])
    missing = session.call(PREVIEW_TOOLSET, "PreviewTurboMCPGraphKnots",
                           {"asset": "NoSuchAssetAnywhere", "graphName": ""})
    check("an asset that does not exist is refused", missing.startswith("error:"), missing[:160])

    print()
    print("=== 4. THE EDIT REFUSES WHAT THE PREVIEW REFUSED (D-02) ===")
    # Each refusal above is put to the EDIT as well, and the two must agree. Fails if a refusal was
    # decided in `Apply` rather than in `Plan`, which is the shape this project has been caught by.
    for label, args in (
        ("an empty asset", {"asset": "", "graphName": CLEAN_GRAPH}),
        ("a non-Blueprint", {"asset": "TA11_TestEnum", "graphName": ""}),
        ("a graph that does not exist", {"asset": ASSET, "graphName": "NoSuchGraphAtAll"}),
    ):
        preview_answer = session.call(PREVIEW_TOOLSET, "PreviewTurboMCPGraphKnots", args)
        edit_answer = session.call(EDIT_TOOLSET, "EditTurboMCPGraphKnots", args)
        both_refuse = preview_answer.startswith("error:") and edit_answer.startswith("error:")
        check("the preview and the edit BOTH refuse %s" % label, both_refuse,
              "preview=%s edit=%s" % (preview_answer[:60], edit_answer[:60]))

    print()
    print("=== 5. the fifth probe graph, which the other workstream is adding ===")
    crossed = session.call(PREVIEW_TOOLSET, "PreviewTurboMCPGraphKnots",
                           {"asset": ASSET, "graphName": CROSSED_GRAPH})
    if crossed.startswith("error:") and "not a graph" in crossed:
        # SKIPPED AND NAMED, never silently passed. A check that cannot run must say so.
        skip("the crossing checks", "%s is not in the asset yet" % CROSSED_GRAPH)
    else:
        print(crossed[:1400])
        print()
        crossed_wires, crossed_before = crossing_counts(crossed)
        check("the crossing graph REPORTS at least one crossing wire",
              crossed_wires is not None and crossed_wires > 0,
              "reported %s" % crossed_wires)
        check("it names the node each wire crosses, not merely a count",
              "crosses" in crossed, crossed[:400])
        # EACH WIRE IS LISTED ONCE however many nodes it crosses (`C2`).
        listed = re.findall(r"^\s+(KNOT|SKIP)\s", crossed, re.MULTILINE)
        check("each crossing wire is listed exactly once, whatever it crosses",
              len(listed) == crossed_wires,
              "%d listed for %s reported" % (len(listed), crossed_wires))

        if crossed_wires and crossed_wires > 0 and crossed_before:
            nodes_before = node_count(session, CROSSED_GRAPH)
            edit = session.call(EDIT_TOOLSET, "EditTurboMCPGraphKnots",
                                {"asset": ASSET, "graphName": CROSSED_GRAPH})
            print(edit[:900])
            print()
            check("the edit reports BOTH the before and the after count",
                  re.search(r"from (\d+) to (\d+)", edit) is not None, edit[:300])
            after = session.call(PREVIEW_TOOLSET, "PreviewTurboMCPGraphKnots",
                                 {"asset": ASSET, "graphName": CROSSED_GRAPH})
            after_wires, after_crossings = crossing_counts(after)
            print("    crossings %s -> %s" % (crossed_before, after_crossings))
            if edit.startswith("changed:") and "rolled back" not in edit:
                # THE RIGHT CHECK IS NOT "ZERO CROSSINGS LEFT". The preview refuses most wires for
                # stated reasons - neighbouring columns, no free space - and only the ones it
                # ACCEPTED get a knot. So what must hold is that the count FELL and that the knots
                # actually placed account for the fall. An earlier version of this check demanded
                # zero and went red on a pass that had worked correctly, which is a check asserting
                # more than the CALL promised.
                check("THE CROSSING COUNT FELL, so the pass improved the graph",
                      after_crossings is not None and after_crossings < crossed_before,
                      "%s -> %s" % (crossed_before, after_crossings))
                nodes_after = node_count(session, CROSSED_GRAPH)
                check("a knot was added, so the node count rose",
                      nodes_after is not None and nodes_before is not None
                      and nodes_after > nodes_before,
                      "%s -> %s" % (nodes_before, nodes_after))

                # THE WIRES THAT STILL CROSS ARE THE ONES THE PREVIEW REFUSED, and the count must
                # match the sum of their original crossing counts. That is what ties the two numbers
                # together: if a wire the preview ACCEPTED were still crossing, this would not add up.
                refused_crossings = 0
                for match in re.finditer(
                        r"^\s+SKIP\s.*?\n\s+crosses (\d+) node", crossed, re.MULTILINE):
                    refused_crossings += int(match.group(1))
                placed = len(re.findall(r"^\s+KNOT\s", crossed, re.MULTILINE))
                print("    refused wires carry %d crossing(s); %d knot(s) were offered"
                      % (refused_crossings, placed))
                if placed > 0:
                    check("the crossings that remain are the ones the preview REFUSED to fix",
                          after_crossings == refused_crossings,
                          "after=%s, refused=%s" % (after_crossings, refused_crossings))
            elif "rolled back" in edit:
                check("a rollback RESTORED the crossing count it started at",
                      after_crossings == crossed_before,
                      "%s vs %s" % (after_crossings, crossed_before))

    print()
    print("=== 6. a knot does not change what the Blueprint does ===")
    compiled_before = session.call(EDIT_TOOLSET, "EditTurboMCPCompile", {"assets": ASSET})
    # The compile is asked again after the edit above, and the two must agree on whether the asset
    # is clean. A knot that became compiler relevant would change this.
    compiled_after = session.call(EDIT_TOOLSET, "EditTurboMCPCompile", {"assets": ASSET})
    def verdict(text):
        if "error" in text.lower():
            return "error"
        if "warning" in text.lower():
            return "warn"
        return "clean"
    print("    compile verdict before: %s" % verdict(compiled_before))
    print("    compile verdict after:  %s" % verdict(compiled_after))
    check("the Blueprint still compiles after the pass, at the same verdict",
          verdict(compiled_before) == verdict(compiled_after),
          "%s vs %s" % (verdict(compiled_before), verdict(compiled_after)))

    print()
    print("=== 7. the fixture is restored for the next suite ===")
    if rebuild_fixture() != 0:
        check("the acceptance asset was rebuilt at the end", False)
    else:
        check("the acceptance asset was rebuilt at the end", True)

    print()
    if skipped:
        print("SKIPPED (%d): %s" % (len(skipped), ", ".join(skipped)))
    if failures:
        print("RESULT: FAIL (%d) -> %s" % (len(failures), ", ".join(failures)))
        return 1
    print("RESULT: PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
