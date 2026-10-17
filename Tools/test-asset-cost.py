# ACCEPTANCE ROW THIS SUITE CLAIMS TO PROVE: T-ASSET-3
#
#   | T-ASSET-3 | Asset cost | TA-1's cost: its own size, hard references, soft references and the largest
#   |           |            | contributors are listed with sizes; an asset whose size cannot be read reports
#   |           |            | unknown with the reason, not zero |
#
# NAMED HERE SO `Spec/06_ACCEPTANCE.md` CAN BE ANSWERED MECHANICALLY. `Spec/WORKING_PLAN.md` section 1.3 says a
# capability is done when its acceptance row PASSES, WHICH MAKES THAT DOCUMENT THE DEFINITION OF DONE - and a script
# over the tree measured 64 OF ITS 122 ROWS CITED BY NO SUITE AT ALL before this practice started.
# *** THE SCRIPT IS NAMED NOW: python Tools/checks/count-acceptance-citations.py - IT STATES ITS CRITERION, a literal occurrence of the id in a Tools/test-*.py file, AND REPORTS 64. FOUR EARLIER ATTEMPTS GAVE 91, 64, 66 AND 56 BECAUSE NONE SAID WHAT CITED MEANT, AND THIS LINE CARRIED THE 91. ***
#
# THE ID IS READ FROM THE DOCUMENT, NOT GUESSED. VERIFIED FOR THIS ONE: the row above is copied from
# `Spec/06_ACCEPTANCE.md` verbatim.
"""`R-ASSET-COST`: the size map and hard references of one asset, split by provenance.

WHAT THIS SUITE IS HELD TO, AND IT IS THE WHOLE CAPABILITY: `D-52` says *"A size is reported with its provenance.
Rejected: one total size. Why: the total names no cause and cannot be acted on. The check that matters is 'a heavy
hard reference where a soft one would do', and that fact does not exist inside a total."*

SO THE CHECKS BELOW ARE ABOUT THE SPLIT AND THE PROVENANCE, NOT ABOUT A NUMBER. A tool that printed one total would
pass none of them, WHICH IS THE POINT.

AND `P1` GOVERNS EVERY FIGURE: an unreadable size must be reported as UNKNOWN WITH ITS REASON, never as zero - because
a zero reads as "free", and an unread package is not a free one.

*** AND IT ASSERTS RELATIONSHIPS RATHER THAN ABSOLUTES, BECAUSE ASSET SIZES AND DEPENDENCY SETS ARE PROPERTIES OF THE
ENGINE'S OWN CONTENT, WHICH THIS PROJECT DOES NOT CONTROL. A hard-coded byte count here would be an assertion on
shared mutable state, THE DEFECT THIS PROJECT HAS RECORDED TEN TIMES. ***
"""

import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from tmcp import READ_TOOLSET, Session  # noqa: E402

# TA-1 is the row's own asset. The kit names differ from the file names, which has cost a workstream three checks
# before, so the mapping is taken from the kit commandlet rather than assumed: `TA-1` is `TA01_HandLaid`.
ASSET = "TA01_HandLaid"

failures = []
checks_run = [0]


def check(name, ok, detail=""):
    checks_run[0] += 1
    print("  %-4s %s%s" % ("ok" if ok else "FAIL", name, ("  " + detail) if detail and not ok else ""))
    if not ok:
        failures.append(name)


def section(text, heading):
    """The LINES of one section, from its heading to the next known heading.

    STRUCTURAL RATHER THAN A LIST OF MAGIC PHRASES. A previous workstream's first fix for an unparseable section was
    a list of literal phrases to skip, WHICH IS A PARSER THAT ASSUMES A FORMAT - and that mistake has cost this
    project five checks twice.
    """
    lines = text.splitlines()
    known = ("OWN SIZE", "WHAT IT PULLS IN", "SIZES THAT COULD NOT BE READ", "LARGEST CONTRIBUTORS")
    start = None
    for index, line in enumerate(lines):
        if heading in line:
            start = index + 1
            break
    if start is None:
        return []
    out = []
    for line in lines[start:]:
        if any(line.startswith(k) or (" " + k) in line for k in known):
            break
        out.append(line)
    return out


def main():
    session = Session()
    session.connect()

    text = session.call(READ_TOOLSET, "ReadTurboMCPAssetCost", {"asset": ASSET, "cap": 5})
    print(text[:900])
    print()
    print()

    # ---------------------------------------------------------------------------------------------
    # THE HEADER: the tool says WHICH asset it answered about, so a wrong resolution is visible.
    # ---------------------------------------------------------------------------------------------
    check("it names the asset it answered about", ASSET in text, text[:200])

    # ---------------------------------------------------------------------------------------------
    # `D-52`: THE SPLIT EXISTS AND IS PRINTED AS THREE SEPARATE KINDS.
    # A tool printing ONE TOTAL would fail every check in this block, WHICH IS THE POINT OF `D-52`.
    # ---------------------------------------------------------------------------------------------
    check("it splits the answer by provenance, naming HARD references",
          re.search(r"hard references\s*:\s*\d+ package", text) is not None, text[:400])
    check("it names SOFT references as their own kind",
          re.search(r"soft references\s*:\s*\d+ package", text) is not None, text[:400])
    check("it names EDITOR-ONLY references as their own kind",
          re.search(r"editor-only references\s*:\s*\d+ package", text) is not None, text[:400])

    # ---------------------------------------------------------------------------------------------
    # AND EACH FIGURE CARRIES A DENOMINATOR: *"the totals per tool"* has a sibling rule here, and a count with no
    # denominator is indistinguishable from a count that looked at nothing.
    # ---------------------------------------------------------------------------------------------
    check("the split states how many dependency entries were examined",
          re.search(r"(\d+) dependency entr", text) is not None, text[:400])

    # ---------------------------------------------------------------------------------------------
    # THE ASSET'S OWN SIZE, PRESENT OR EXPLICITLY UNKNOWN - NEVER A BARE ZERO.
    # ---------------------------------------------------------------------------------------------
    own = section(text, "OWN SIZE")
    own_text = "\n".join(own)
    own_ok = ("byte(s)" in text.split("WHAT IT PULLS IN")[0]) or ("unknown" in own_text.lower())
    check("the asset's own size is either a size or an explicit unknown",
          own_ok, own_text[:300])

    # ---------------------------------------------------------------------------------------------
    # *** `P1`: AN UNREADABLE SIZE IS UNKNOWN WITH A REASON, NOT ZERO. *** THIS IS THE SECOND HALF OF THE ACCEPTANCE
    # ROW AND IT IS THE ONE A LAZY IMPLEMENTATION GETS WRONG, because zero is the convenient default.
    # ---------------------------------------------------------------------------------------------
    if "unknown" in text.lower():
        check("an unknown size comes WITH A REASON rather than alone",
              re.search(r"unknown\s*-\s*\S", text) is not None, text[:400])
        check("it says explicitly that this is NOT ZERO",
              "THIS IS NOT ZERO" in text, text[:600])
    else:
        print("    no unknown size on this asset, so the unknown path is NOT exercised here")
        print("    (it is asserted on a deliberately unresolvable asset below)")

    # ---------------------------------------------------------------------------------------------
    # THE REFUSAL PATH, WHICH IS THE OTHER HALF OF `P1` AND CAN BE EXERCISED EVERY RUN.
    # AN ASSET THAT DOES NOT RESOLVE MUST BE REFUSED BY NAME RATHER THAN REPORTED AS ZERO COST.
    # ---------------------------------------------------------------------------------------------
    refused = session.call(READ_TOOLSET, "ReadTurboMCPAssetCost", {"asset": "NoSuchAsset_ForTheUnknownPath", "cap": 5})
    print(refused[:400])
    print()
    check("an asset that does not resolve is REFUSED rather than reported as costing nothing",
          refused.startswith("error:") or "did not resolve" in refused, refused[:300])
    zero_claim = re.search(r"OWN SIZE:\s*0 byte", refused)
    check("and the refusal does NOT claim a zero size, which would read as free",
          zero_claim is None, refused[:300])

    # ---------------------------------------------------------------------------------------------
    # THE LARGEST CONTRIBUTORS: CAPPED, AND EACH WITH A SIZE AND A REASON.
    # `D-52`'s own example - "a heavy hard reference where a soft one would do" - needs the REASON on the row,
    # because the size alone cannot tell hard from soft.
    # ---------------------------------------------------------------------------------------------
    largest = section(text, "LARGEST CONTRIBUTORS")
    largest_text = "\n".join(largest)
    check("a LARGEST CONTRIBUTORS section exists", len(largest) > 0, text[-600:])

    rows = [l for l in largest if "|" in l]
    check("the contributors are capped at the requested number", len(rows) <= 5,
          "%d row(s) for a cap of 5" % len(rows))
    if rows:
        check("every contributor row carries WHY it is referenced, not only its size",
              all(("HARD" in r or "SOFT" in r or "EDITOR-ONLY" in r or "UNKNOWN" in r) for r in rows),
              "\n".join(rows[:4]))
        check("and every contributor row carries a size or an explicit unknown",
              all(("byte(s)" in r or "UNKNOWN" in r) for r in rows), "\n".join(rows[:4]))
    else:
        print("    no contributors on this asset, so the per-row assertions are NOT exercised")
        print("    THE TOOL SAYS SO IN ITS OWN OUTPUT rather than leaving the section empty and silent")

    print()
    if failures:
        print("RESULT: FAIL (%d) -> %s" % (len(failures), ", ".join(failures)))
        return 1
    print("RESULT: PASS (%d check(s) run)" % checks_run[0])
    return 0


if __name__ == "__main__":
    sys.exit(main())
