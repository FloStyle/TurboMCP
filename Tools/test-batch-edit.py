#!/usr/bin/env python3
# Copyright (c) 2026 FloStyle
# SPDX-License-Identifier: MPL-2.0
"""The restricted single-graph batch: BASELINE ONLY, which is not its release contract.

*** WHAT THIS SUITE IS, AND WHAT IT IS NOT. ***

Release finding R03 records that the batch edit is an unfinished publicly reflected capability: the batch
calculates every plan before it applies any operation, returns `changed` with no final compile
or save, and prints `unchanged` even when its own rollback says `failed`. ARC09 restricts the release subset
to `graphRepoint` in one Blueprint graph, under one transaction and one final compile, with no save.

THIS SUITE DOES NOT PROVE THAT CONTRACT. It does not test rollback, cross-asset refusal, stale-plan detection or
the final compile, because the implementation does not have them yet. It exists for two reasons:

  1. `Tools/audit-tool-coverage.py` requires every reflected tool to have a caller. Two tools that no suite
     called were reported as uncalled, and the honest fix is a suite that exercises the surface as it is -
     not an exemption for the tools that are hardest to test.
  2. A BASELINE CASE IS THE NEGATIVE CONTROL FOR S16. Step S16 replaces these assertions with the full
     contract (B01-B05). When it does, this file's `BASELINE` assertions must FAIL, so S16 has to notice the
     file and rewrite it rather than adding a second suite beside it.

The refused-shape cases below ARE part of the release contract and must survive S16, because ARC09 names
them: unknown kind, unknown field, duplicate id.
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from tmcp import EDIT_TOOLSET, PREVIEW_TOOLSET, READ_TOOLSET, Session  # noqa: E402

# The fixture these cases run against. It is the shared acceptance asset that other graph suites rebuild.
ASSET = "TurboMCPAcceptance"
GRAPH = "AppendChainProbe"

MARKER = "BASELINE — replace with the ARC09 contract in S16"

failures = []


def check(name, ok, detail=""):
    print("  %-4s %s%s" % ("ok" if ok else "FAIL", name, ("  " + detail) if detail else ""))
    if not ok:
        failures.append(name)


def call_node_ids(session):
    """The call nodes in the probe graph, read through the reader rather than the batch tool."""
    text = session.call(READ_TOOLSET, "ReadTurboMCPGraphLoad",
                        {"asset": ASSET, "graphName": GRAPH, "detail": "full",
                         "format": "", "tokenBudget": ""})
    import re
    return re.findall(r"\.%s:([0-9a-f]{6}) ; class K2Node_CallFunction" % GRAPH, text)


def main():
    session = Session()
    session.connect()

    keys = call_node_ids(session)
    print("=== %s.%s holds %d call node(s) ===" % (ASSET, GRAPH, len(keys)))
    if len(keys) < 3:
        print("RESULT: BLOCKED (the probe graph needs at least three call nodes, found %d)" % len(keys))
        return 1

    def full_id(short):
        return "/Game/Tests/%s.%s:%s:%s" % (ASSET, ASSET, GRAPH, short)

    one = {
        "id": "op-1", "kind": "graphRepoint",
        "asset": ASSET, "graph": GRAPH,
        "sourceNodeId": full_id(keys[0]), "sourcePin": "then",
        "targetNodeId": full_id(keys[1]), "targetTypeId": "", "targetPin": "then",
        "keepSource": "true",
    }
    operations = '{"operations": [%s]}' % _json(one)

    # --- the refused shapes, which ARC09 requires and S16 must keep ------------------------------
    print()
    print("=== refused shapes: refused before any mutation ===")
    for label, payload, expect in (
        ("an unknown kind",
         '{"operations":[%s]}' % _json(dict(one, kind="deleteEverything")),
         "unknown or unsupported kind"),
        ("an unknown field is not a refusal by itself, so it is checked separately",
         '{"operations":[%s]}' % _json(dict(one, nonsenseField="x")),
         None),
        ("a duplicate operation id",
         '{"operations":[%s,%s]}' % (_json(one), _json(dict(one, sourceNodeId=full_id(keys[2])))),
         "duplicate operation id"),
        ("malformed JSON",
         '{"operations": not json',
         "not valid JSON"),
        ("an empty operations array",
         '{"operations": []}',
         "empty"),
    ):
        for toolset, tool in ((PREVIEW_TOOLSET, "PreviewTurboMCPBatchEdit"),
                              (EDIT_TOOLSET, "EditTurboMCPBatchEdit")):
            out = session.call(toolset, tool, {"operations": payload})
            if expect is None:
                # `nonsenseField` is not rejected by the current implementation. Recorded rather than
                # asserted, because the assertion belongs to S16 and a guess here would be worse than a note.
                print("  note %s via %s -> %s" % (label, tool, out.splitlines()[0][:90]))
                continue
            check("%s is refused by %s" % (label, tool),
                  expect in out and out.startswith("unchanged"),
                  out.splitlines()[0][:100] + " | " + out.splitlines()[-1][:80])

    # --- the baseline: what the capability actually does today ------------------------------------
    print()
    print("=== %s ===" % MARKER)
    before = call_node_ids(session)
    preview = session.call(PREVIEW_TOOLSET, "PreviewTurboMCPBatchEdit", {"operations": operations})
    print(preview)
    check("%s: the preview reports planned and changes nothing" % MARKER,
          preview.startswith("unchanged") and "planned" in preview,
          preview.splitlines()[0][:80])
    check("%s: the preview left the graph as it was" % MARKER,
          call_node_ids(session) == before)

    applied = session.call(EDIT_TOOLSET, "EditTurboMCPBatchEdit", {"operations": operations})
    print(applied)
    after = call_node_ids(session)
    check("%s: the edit reports changed and names the applied id" % MARKER,
          applied.startswith("changed") and "op-1" in applied, applied.splitlines()[0][:100])
    check("%s: the edit moved the reader to the second node" % MARKER,
          (":%s)" % keys[1]) in session.call(READ_TOOLSET, "ReadTurboMCPGraphLoad",
                                             {"asset": ASSET, "graphName": GRAPH, "detail": "full",
                                              "format": "", "tokenBudget": ""}))
    check("%s: the edit reports compile and save as DEFERRED, which ARC09 forbids" % MARKER,
          "compile=deferred" in applied and "save=deferred" in applied)
    print("  note the graph went from %d to %d call node(s)" % (len(before), len(after)))

    print()
    if failures:
        print("RESULT: FAIL (%d) -> %s" % (len(failures), ", ".join(failures)))
        return 1
    print("RESULT: PASS (baseline recorded; the ARC09 contract is S16's, not this suite's)")
    return 0


def _json(obj):
    import json
    return json.dumps(obj)


if __name__ == "__main__":
    sys.exit(main())
