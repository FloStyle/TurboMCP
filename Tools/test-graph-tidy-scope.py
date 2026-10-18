# ACCEPTANCE ROW THIS SUITE CLAIMS TO PROVE: FT-30, FT-31, TD-05
"""Exercise a scoped P/E-GRAPH-TIDY block against fixed surrounding anchors.

The acceptance commandlet builds `ScopedTidyProbe`: two PrintString nodes marked SELECTED are a
small messy exec block; Entry plus the BEFORE/AFTER marker calls and the comment box are fixed
surrounding anchors. The node ids are discovered from R-GRAPH-LOAD, never pinned, because the
commandlet creates fresh ids on every rebuild.

This is deliberately a focused interaction test. It does not rebuild the shared fixture or start an
editor: the batch fixture owner does that. It checks the preview boundary, the edit boundary, the
preview score against the score the edit ACTUALLY measured, and that the edit says whether those
numbers agree instead of reporting a planned number as an achieved one.
"""

import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from tmcp import EDIT_TOOLSET, PREVIEW_TOOLSET, READ_TOOLSET, Session  # noqa: E402

ASSET = "TurboMCPAcceptance"
GRAPH = "ScopedTidyProbe"
SELECTED = ("SCOPED-TIDY-SELECTED-FIRST", "SCOPED-TIDY-SELECTED-SECOND")
ANCHORS = ("SCOPED-TIDY-ANCHOR-BEFORE", "SCOPED-TIDY-ANCHOR-AFTER")

failures = []


def check(name, ok, detail=""):
    print("  %-4s %s%s" % ("ok" if ok else "FAIL", name, ("  " + detail) if detail else ""))
    if not ok:
        failures.append(name)


def graph_text(session):
    return session.call(READ_TOOLSET, "ReadTurboMCPGraphLoad", {
        "asset": ASSET, "graphName": GRAPH, "detail": "full", "format": "", "tokenBudget": ""
    })


def marker_key(text, marker):
    """The key on the PrintString form that owns this unique marker."""
    # Bind the literal to its own PrintString header; never scan across nodes.
    headers = list(re.finditer(r"(?m)^\s*\(Development\|PrintString ; ([^ ;\r\n]+)", text))
    matches = []
    for index, header in enumerate(headers):
        end = headers[index + 1].start() if index + 1 < len(headers) else len(text)
        body = text[header.end():end]
        if re.search(r'(?m)^\s+InString "' + re.escape(marker) + r'"(?=\s|\)|$)', body):
            matches.append(header.group(1))
    return matches[0] if len(matches) == 1 else None



def score_pair(text):
    match = re.search(r"total wire length\s+([\d.]+)\s*->\s*([\d.]+)", text)
    return (float(match.group(1)), float(match.group(2))) if match else None


def main():
    session = Session()
    session.connect()

    before = graph_text(session)
    print(before)
    selected = [marker_key(before, marker) for marker in SELECTED]
    anchors = [marker_key(before, marker) for marker in ANCHORS]
    check("the focused fixture exists and exposes two selected ids", all(selected), str(selected))
    check("the focused fixture exposes fixed surrounding anchor ids", all(anchors), str(anchors))
    check("selected nodes and anchors are distinct", len(set(selected + anchors)) == 4)
    if not all(selected) or not all(anchors) or len(set(selected + anchors)) != 4:
        return 1

    args = {"asset": ASSET, "graphName": GRAPH, "bForce": True, "nodeIds": selected}
    preview = session.call(PREVIEW_TOOLSET, "PreviewTurboMCPGraphTidy", args)
    print(preview[:1200])
    check("the scoped preview answers", not preview.startswith("error:"), preview[:200])
    check("the preview names a fixed-anchor scope", "fixed anchors" in preview, preview[:400])
    planned = score_pair(preview)
    check("the preview supplies a score pair", planned is not None, preview[:300])

    preview_moves = re.search(r"scope moves only:\s*(.*)", preview)
    listed_preview_moves = preview_moves.group(1) if preview_moves else ""
    check("the preview names every selected move", all(key.rsplit(".", 1)[-1] in listed_preview_moves for key in selected),
          listed_preview_moves)
    check("the preview does not offer an anchor for movement", not any(key.rsplit(".", 1)[-1] in listed_preview_moves for key in anchors),
          listed_preview_moves)

    # Preview is required to be observational: ids remain available before the edit.
    after_preview = graph_text(session)
    check("preview preserved every selected and anchor node id",
          all(key in after_preview for key in selected + anchors), "fixture node disappeared during preview")

    edited = session.call(EDIT_TOOLSET, "EditTurboMCPGraphTidy", args)
    print(edited[:1200])
    check("the forced scoped edit reports a change", edited.startswith("changed:"), edited[:240])
    applied_moves = re.search(r"scope moved:\s*(.*)", edited)
    listed_applied_moves = applied_moves.group(1) if applied_moves else ""
    check("the edit reports only the selected node ids as moved",
          all(key.rsplit(".", 1)[-1] in listed_applied_moves for key in selected)
          and not any(key.rsplit(".", 1)[-1] in listed_applied_moves for key in anchors), listed_applied_moves)
    check("the edit states that surrounding nodes and comment boxes stayed anchored",
          "remained fixed anchors" in edited, edited[-500:])

    achieved = re.search(r"score: preview predicted ([\d.]+); post-edit measured ([\d.]+) \(([^)]+)\)", edited)
    check("the edit reports planned and achieved scores", achieved is not None, edited[:500])
    if planned and achieved:
        predicted = float(achieved.group(1))
        actual = float(achieved.group(2))
        check("the edit repeats the preview score", abs(predicted - planned[1]) <= 1.0,
              "%s vs %s" % (predicted, planned[1]))
        check("the score verdict is honest", ("MATCHES" in achieved.group(3)) == (abs(predicted - actual) <= 1.0),
              achieved.group(0))

    after_edit = graph_text(session)
    check("the edit preserved the selected and fixed anchor node identities",
          all(key in after_edit for key in selected + anchors), "a scoped edit replaced or removed a node")

    if failures:
        print("RESULT: FAIL (%d) -> %s" % (len(failures), ", ".join(failures)))
        return 1
    print("RESULT: PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
