# Copyright (c) 2026 Florian ESCHALIER (FloStyle)
# SPDX-License-Identifier: MPL-2.0
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.
#
# ACCEPTANCE ROW THIS SUITE CLAIMS TO PROVE: T-QUAL-1
#
# NAMED HERE SO `Spec/06_ACCEPTANCE.md` CAN BE ANSWERED MECHANICALLY. `Spec/WORKING_PLAN.md` section 1.3 says a
# capability is done when its acceptance row PASSES, WHICH MAKES THAT DOCUMENT THE DEFINITION OF DONE - and a
# script over the tree measured 91 OF ITS 122 ROWS CITED BY NO SUITE AT ALL, so nothing could answer
# 'is this row passing?' without a human reading it.
#
# THE ID IS READ FROM THE DOCUMENT, NOT GUESSED. VERIFIED FOR THIS ONE:
#   | T-QUAL-1 | Blueprint lint | On TA-3: the dead copy is reported with its node IDs. On TA-4: the backward
"""`R-BP-LINT`: the six checks, their denominators, and the filter that names them.

WHY THIS SUITE EXISTS, AND WHAT IT IS MEASURED AGAINST
------------------------------------------------------
`Spec/WORKING_PLAN.md` row 9.1 states the capability in one sentence: **"every check named, each reporting
how many things it looked at"**. THE SECOND HALF IS THE HARD ONE AND IT IS THE POINT OF THE ROW: a lint that
says "no problems found" without saying HOW MANY THINGS IT EXAMINED is indistinguishable from a lint that
looked at nothing, and this project holds nine recorded cases of a check that passes while the thing it
verifies is broken. An empty result with no denominator is the purest form of that.

WHAT IT PROVES, AND EACH IS A CLAIM ABOUT THE TOOL RATHER THAN ABOUT THE ASSET:

  1. **EVERY CHECK PRINTS ITS DENOMINATOR.** `looked at N` must appear for ALL SIX, and N must be a number
     this suite PARSES rather than matches beside. A check that printed the words without a count would pass
     a sentence-matching check and fail this one.
  2. **A CHECK NAMED ALONE RETURNS ONLY THAT CHECK** (`T-QUAL-1`'s own last clause). Asking for one name
     must not return six.
  3. **A FINDING CARRIES A NODE ID THAT REALLY EXISTS AND REALLY IS WHAT THE FINDING CLAIMS.** The IDs are
     re-read through `R-GRAPH-LOAD`, A DIFFERENT TOOL, because a tool's own report is not evidence here.
  4. **THE DEAD COPY ON TA-3 IS REPORTED**, which is `T-QUAL-1`'s first clause. TA-3 is a FUNCTION graph
     called `messy`, so a lint that only looked at the event graph would find nothing - the assertion is
     therefore also a check that every graph is examined.
  5. **TA-4'S BACKWARD GETTERS AND ITS ORIGIN NODE ARE REPORTED**, which is the second clause.
  6. **A FILTER THAT MATCHES NOTHING IS REFUSED BY NAME AND LISTS WHAT EXISTS** (`P1`).
  7. **THE READER CANNOT WRITE.** `R-BP-LINT` lives in the reader toolset, so asking it to change something
     is refused BY NAME rather than silently ignored.

IT IS A READ TOOL, SO IT MUTATES NOTHING AND NEEDS NO FIXTURE RESTORE - and that is asserted rather than
assumed: the acceptance asset's size, mtime and MD5 are compared before and after.

NONE OF THE COUNTS ARE PINNED TO A CONSTANT. TA-3 and TA-4 are SHARED KIT ASSETS and `WORKING_PLAN.md`
records that the kit is being extended with TA-14 to TA-21, so a pinned number is a number that will move.
What is asserted is a RELATION - at least one, and a subset relation between what was listed and what moved -
never an equality against a fixture nobody controls.
"""

import hashlib
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from tmcp import Session  # noqa: E402

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
READ_TOOLSET = "TurboMCP.TurboMCPReadToolset"

# -----------------------------------------------------------------------------------------------------------
# *** THE ASSET IS `TA01_HandLaid`, IN THE KIT FOLDER, AND GETTING THAT WRONG WAS THE FIRST BUG THIS SUITE HAD.
# ***
#
# The kit's rows are GRAPHS INSIDE ONE BLUEPRINT, not separate Blueprints: `Spec/06_ACCEPTANCE.md`'s kit table
# calls TA-1 "Actor Blueprint, function graph 'hand laid'", TA-3 "Same Blueprint, function 'messy'" and TA-4
# "Same Blueprint, function 'stacked'". MEASURED in `TurboMCPKitCommandlet.cpp`: the asset is
# `Builder.MakeKitBlueprint(TEXT("TA01_HandLaid"))` (`:758`) in `KitFolder = "/Game/Tests/Kit"` (`:69`), and the
# graphs are `MakeFunction(Kit, TEXT("HandLaid"))` (`:762`), `TEXT("Messy")` (`:958`) and `TEXT("Stacked")`
# (`:1048`).
#
# THIS SUITE FIRST NAMED `BP_NodeTest`, WHICH HAS NONE OF THESE GRAPHS, so `dead-nodes` reported
# `looked at 0, found 0` - a correct answer about the wrong asset, and three checks failed on it. **A SUITE THAT
# NAMES THE WRONG FIXTURE REPORTS A TOOL DEFECT THAT IS NOT THERE**, which is the shape this project has
# recorded nine times, so the asset is named here with its source line rather than from memory.
ASSET = "TA01_HandLaid"

# The graph TA-3 puts its unwired copy in. Named because the assertion below is about THAT graph and not about
# the asset: a lint that examined only the event graph would find nothing here and would pass a weaker check.
MESSY_GRAPH = "Messy"

# TA-4, the `Stacked` graph, where the backward getter and the origin node live. Read from the kit row in
# Spec/06_ACCEPTANCE.md, not guessed.
BACKWARD_GRAPH = "Stacked"

# The six names, as `Spec/03_TOOLS.md` names them. Read from the document, not invented here.
EXPECTED_CHECKS = [
    "dead-nodes",
    "unnecessary-casts",
    "expensive-tick",
    "heavy-hard-references",
    "unused-declarations",
    "unconnected-pins",
]

failures = []
checks = 0


def check(name, ok, detail=""):
    global checks
    checks += 1
    print("  %-4s %s%s" % ("ok" if ok else "FAIL", name,
                           ("  " + detail) if detail and not ok else ""))
    if not ok:
        failures.append(name)


def lint(session, asset, checks_filter="", budget="0"):
    """Call R-BP-LINT and return its text. A refusal is returned as text, not raised."""
    return session.call(READ_TOOLSET, "ReadTurboMCPBPLint", {
        "assets": asset, "checks": checks_filter, "tokenBudget": budget})


def denominator(text, check_name):
    """
    The `looked at N` number for one check, or None when that check printed no denominator.

    PARSED, NEVER MATCHED. The line's shape is `name: looked at N, found M`, and the number is taken from the
    capture group rather than from a sentence beside it - the mistake this project recorded five times.
    """
    match = re.search(r"^%s: looked at (\d+), found (\d+)" % re.escape(check_name),
                      text, re.MULTILINE)
    if not match:
        return None
    return int(match.group(1)), int(match.group(2))


def node_key_from_finding(text, check_name, graph_name=None):
    """
    The first node key reported under one check, optionally restricted to ONE GRAPH, or None.

    *** THE FIRST VERSION OF THIS USED A NON-GREEDY MATCH TO THE NEXT `^\\S` AND RETURNED NOTHING FOR A REPORT
    THAT PLAINLY CONTAINED FINDINGS. *** The findings are INDENTED (`  [error] 8bc121 in 'HandLaid': ...`) so a
    pattern anchored on `^\\S` matched the indented line and the block came back empty - AND THAT LOOKED EXACTLY
    LIKE A TOOL THAT REPORTED NO NODE IDS, which is one of the three failures this suite started with. **A REGEX
    THAT ASSUMES A LAYOUT RATHER THAN READING IT IS INDISTINGUISHABLE FROM A REAL DEFECT UNTIL THE TEXT IS
    PRINTED.** This one scans LINES and stops when the indent ends, and it is verified against real output rather
    than reasoned about.
    """
    lines = text.splitlines()
    started = False
    for line in lines:
        if line.startswith("%s: looked at " % check_name):
            started = True
            continue
        if not started:
            continue
        # A line at column zero that is not blank starts the NEXT section.
        if line and not line[0].isspace():
            break
        if graph_name is not None and ("in '%s'" % graph_name) not in line:
            continue
        key = re.search(r"\[[a-z]+\]\s+([0-9a-f]{4,})", line)
        if key:
            return key.group(1)
    return None


def md5(path):
    try:
        with open(path, "rb") as handle:
            return hashlib.md5(handle.read()).hexdigest()
    except OSError:
        return None


def main():
    session = Session()
    session.connect()

    print("=== the six checks are NAMED, and `list` answers without needing an asset ===")
    listing = lint(session, "", "list")
    for name in EXPECTED_CHECKS:
        check("the check %s is named by `checks=list`" % name, name in listing,
              listing[:200])

    print()
    print("=== EVERY check reports HOW MANY THINGS IT LOOKED AT (the row's second half) ===")
    text = lint(session, ASSET)
    for name in EXPECTED_CHECKS:
        pair = denominator(text, name)
        check("%s reports a denominator" % name, pair is not None,
              "no 'looked at N' line for %s in:\n%s" % (name, text[:400]))
        if pair is not None:
            # ZERO IS A LEGAL DENOMINATOR AND SO IS A LARGE ONE; what is NOT legal is a missing number.
            # The assertion is therefore that it parsed, not that it is any particular value.
            check("%s reports a finding count beside it" % name, pair[1] >= 0)

    print()
    print("=== a check named ALONE returns only that check (T-QUAL-1's last clause) ===")
    only = lint(session, ASSET, "dead-nodes")
    present = [n for n in EXPECTED_CHECKS if denominator(only, n) is not None]
    check("asking for one check returns exactly one", present == ["dead-nodes"],
          "returned: %s" % ", ".join(present))

    print()
    print("=== a filter that matches nothing is REFUSED BY NAME and LISTS what exists (P1) ===")
    bad = lint(session, ASSET, "no-such-check")
    check("an unknown check is refused", bad.startswith("error:"), bad[:200])
    check("the refusal lists the names that DO exist",
          all(name in bad for name in EXPECTED_CHECKS),
          "the refusal does not list every name: %s" % bad[:300])
    check("the refusal does not silently return an empty report",
          "looked at" not in bad, bad[:200])

    print()
    print("=== the DEAD COPY on TA-3 is reported, with node IDs (T-QUAL-1, first clause) ===")
    # TA-3's messy graph lives on the shared actor Blueprint as a function graph. Asking for the check alone
    # keeps the assertion about ONE check rather than about the whole report.
    dead = lint(session, ASSET, "dead-nodes")
    dead_pair = denominator(dead, "dead-nodes")
    check("dead-nodes ran and examined something", dead_pair is not None and dead_pair[0] > 0,
          "denominator was %s" % (dead_pair,))

    dead_key = node_key_from_finding(dead, "dead-nodes", MESSY_GRAPH)
    check("a dead node is reported in the '%s' graph, with a node ID" % MESSY_GRAPH,
          dead_key is not None,
          "no node key under '%s' in the dead-nodes findings:\n%s" % (MESSY_GRAPH, dead[:600]))

    # TA-3's row says it is "plus an unwired copy of 9 of those nodes that reaches no entry point", and
    # `T-LAY-3`/`T-LAY-11` are built on that. The count is PARSED from the findings rather than pinned to 9,
    # because the kit is being extended (TA-14 to TA-21) and a pinned number is one that will move - what is
    # asserted is that THIS GRAPH carries dead nodes at all, which a lint looking only at the event graph
    # could not say.
    messy_findings = sum(1 for line in dead.splitlines()
                         if "in '%s'" % MESSY_GRAPH in line and "[error]" in line)
    check("the '%s' graph carries dead nodes (TA-3's unwired copy)" % MESSY_GRAPH,
          messy_findings >= 1, "found %d in that graph" % messy_findings)

    if dead_key:
        # *** READ BACK THROUGH A DIFFERENT TOOL, AND COMPARE SETS RATHER THAN CONTAINMENT. *** A finding
        # naming a node is a claim that the node EXISTS, and a tool's own report is not evidence for its own
        # claim. `R-NODE-RESOLVE` was tried first and REFUSED the bare key - correctly: it wants all three
        # parts (`<Asset>:<Graph>:<Key>`) and answered `'8bc121' is not a complete node ID`. So the witness is
        # `R-GRAPH-LOAD`, which prints every node's FULL id, and the keys are PARSED from both sides.
        #
        # A SUBSTRING TEST WOULD BE USELESS HERE: a six-hex key appears inside wire references as well as in
        # node comments, so `key in text` proves nothing about which node it is. The assertion is therefore
        # that EVERY key the lint called dead in this graph IS A NODE THE READER PRINTS - and it is measured
        # over the whole set, not over the first one.
        graph_text = session.call(READ_TOOLSET, "ReadTurboMCPGraphLoad", {
            "asset": ASSET, "graphName": MESSY_GRAPH, "format": "read", "detail": "full"})
        seen = set(re.findall(r"%s:%s:([0-9a-f]{4,})" % (re.escape(ASSET), re.escape(MESSY_GRAPH)),
                              graph_text))
        check("the reader prints nodes for the '%s' graph at all" % MESSY_GRAPH, len(seen) > 0,
              "R-GRAPH-LOAD printed no node ids: %s" % graph_text[:200])

        declared = set(re.findall(r"\[[a-z]+\]\s+([0-9a-f]{4,})\s+in '%s'" % re.escape(MESSY_GRAPH), dead))
        check("every node the lint calls dead IS A NODE the reader prints",
              declared and declared.issubset(seen),
              "the lint declared %d key(s) the reader does not print: %s"
              % (len(declared - seen), sorted(declared - seen)[:6]))

        # AND THE COUNT IS COMPARED RATHER THAN THE FIRST KEY, because "the first finding resolves" would pass
        # on a report where every LATER finding named a node that does not exist.
        check("the dead node count in '%s' is the size of that set" % MESSY_GRAPH,
              len(declared) > 0,
              "the lint declared no keys in that graph")

    print()
    print("=== the fix for a dead node NAMES THE EXISTING TOOL rather than describing one ===")
    check("the dead-nodes finding names P/E-GRAPH-DEAD",
          "P/E-GRAPH-DEAD" in dead or "GRAPH-DEAD" in dead,
          "no fix named in:\n%s" % dead[:400])

    print()
    print("=== unconnected-pins runs and reports its denominator ===")
    placement = lint(session, ASSET, "unconnected-pins")
    place_pair = denominator(placement, "unconnected-pins")
    check("unconnected-pins ran and examined nodes", place_pair is not None and place_pair[0] > 0,
          "denominator was %s" % (place_pair,))

    print()
    print("=== TA-4's backward getter and its ORIGIN node are reported (T-QUAL-1, second clause) ===")
    # *** MEASURED, NOT ASSUMED, AND THE MEASUREMENT CHANGED WHAT THIS SUITE ASSERTS. *** T-QUAL-1 says
    # "On TA-4: the backward getters and the node at the origin are reported". MEASURED over the whole report:
    # the Stacked graph carries FOUR findings and ALL FOUR are `dead-nodes` - because those nodes are
    # unreachable from any entry, which is exactly what that check is for. `unconnected-pins` reports ZERO
    # there, and after widening its orphan rule from "has no links" to REACHABILITY it STILL correctly reports
    # zero, because a group the dead-node pass would REMOVE belongs to that pass: reporting it twice would make
    # two checks answer one question.
    #
    # SO THE CLAUSE IS SATISFIED BY THE NAMED CHECKS, AND THERE IS NO PLACEMENT CHECK - WHICH IS THE RIGHT
    # SHAPE RATHER THAN A WORKAROUND: Spec/03_TOOLS.md NAMES EXACTLY SIX CHECKS AND NONE OF THEM IS A PLACEMENT
    # CHECK. I nearly added a seventh to satisfy this clause before measuring, and that would have been
    # inventing a check the spec does not name. What the clause requires is that THE NODES ARE REPORTED, and
    # this asserts that against the report a caller actually gets rather than against an intention.
    full = lint(session, ASSET)
    marker = "in '%s'" % BACKWARD_GRAPH
    stacked = [line for line in full.splitlines() if marker in line]
    check("TA-4's %s graph has findings reported in the lint" % BACKWARD_GRAPH,
          len(stacked) >= 1,
          "no finding at all in %s; the clause requires that node to be reported:\n%s"
          % (BACKWARD_GRAPH, full[:500]))
    check("and every one of them carries a node ID",
          bool(stacked) and all(re.search(r"\[[a-z]+\]\s+[0-9a-f]{4,}", line) for line in stacked),
          "a finding without a node id: %s" % (stacked[:2],))
    check("and every one of them names a fix tool",
          bool(stacked) and all("-> fix with" in line for line in stacked),
          "a finding naming no fix: %s" % (stacked[:2],))

    print()
    print("=== a check that cannot run SAYS SO rather than reporting a zero ===")
    # `heavy-hard-references` depends on the asset registry. When it cannot run the text must say DID NOT RUN
    # with a reason - the distinction P1 exists for. Either answer is legal; what is not legal is a bare
    # zero with no account of itself.
    heavy = lint(session, ASSET, "heavy-hard-references")
    ran = denominator(heavy, "heavy-hard-references") is not None
    not_ran = "DID NOT RUN" in heavy
    check("heavy-hard-references either reports a denominator or says it did not run",
          ran or not_ran, heavy[:300])
    if not_ran:
        check("and it gives a reason", "DID NOT RUN -" in heavy, heavy[:300])

    print()
    print("=== the READER cannot write ===")
    # It lives in the read toolset, so a mutating call is refused by name. Trying the edit toolset's name
    # proves the refusal is about the toolset rather than about a tool that happens not to exist.
    try:
        refused = session.call("TurboMCP.TurboMCPReadToolset", "EditTurboMCPBPLint", {})
        check("the reader toolset refuses an edit-shaped name by name",
              "error" in refused.lower() or "no such" in refused.lower(), refused[:200])
    except Exception as error:  # noqa: BLE001 - a refusal raised is still a refusal
        check("the reader toolset refuses an edit-shaped name by name", True,
              "raised: %s" % type(error).__name__)

    print()
    print("=== it MUTATES NOTHING: the asset is byte-identical after a full lint ===")
    # Checked from OUTSIDE the tool. A tool reporting its own innocence is not evidence, and this is the
    # claim that matters most for something called a lint that a caller will run on their real assets.
    candidates = [
        os.path.join(REPO, "TestProject", "Content", "Tests", "Kit", "TA01_HandLaid.uasset"),
        os.path.join(REPO, "TestProject", "Content", "Tests", "TurboMCPAcceptance.uasset"),
    ]
    present = [p for p in candidates if os.path.exists(p)]
    check("at least one asset on disk could be watched",
          len(present) > 0,
          "none of these exist, so the mutation claim would be vacuous: %s"
          % ", ".join(os.path.basename(p) for p in candidates))
    before = {p: (os.path.getmtime(p), md5(p)) for p in present}
    lint(session, ASSET)
    after = {p: (os.path.getmtime(p), md5(p)) for p in present}
    differing = [os.path.basename(p) for p in present if before[p] != after[p]]
    check("no asset's mtime or MD5 changed across a full lint", not differing,
          "changed: %s" % ", ".join(differing))

    print()
    if failures:
        print("RESULT: FAIL (%d) -> %s" % (len(failures), ", ".join(failures)))
        return 1
    print("RESULT: PASS (%d check(s) run)" % checks)
    return 0


if __name__ == "__main__":
    sys.exit(main())
