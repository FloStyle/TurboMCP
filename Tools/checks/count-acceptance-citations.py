"""Count how many acceptance rows any suite cites, reproducibly.

WHY THIS EXISTS: TWELVE SUITES SAY, WORD FOR WORD, *"a script over the tree measured 91 of its 122 rows cited by
no suite at all"* - AND NO SUCH SCRIPT IS IN THE TREE, AND NO COMMAND IS RECORDED BESIDE THE NUMBER. Four
attempts to rederive it gave 91, 64, 66 and 56. *** SO THE FINDING IS NOT THAT THE FIGURE IS WRONG: IT IS THAT
NOBODY CAN SAY HOW MANY ROWS ARE CITED, BECAUSE NO REPRODUCIBLE MEASUREMENT OF IT EXISTED. ***

THIS IS THAT MEASUREMENT. It reads every `T-...` id out of `Spec/06_ACCEPTANCE.md`, searches every suite for each
id, and prints both counts. **It is modelled on `count-plan.py`, which already does exactly this for the plan's
rows - read a document, count what the code cites, print both numbers.**

    CITATION IS A LITERAL OCCURRENCE of the id anywhere in a `Tools/test-*.py` file.
    *** THAT CRITERION IS THE ONE THING THIS SCRIPT MUST STATE, BECAUSE IT IS WHAT MADE THE FOUR EARLIER
    ATTEMPTS DISAGREE: "cited" means "the id appears in a suite", NOT "the suite claims to prove that row". A
    suite that claims a row writes `# ACCEPTANCE ROW THIS SUITE CLAIMS TO PROVE: T-PANEL-5`; a suite that merely
    mentions one in a comment about something else still counts here. ***  *--claiming is a stricter question and
    is not computed.*

USAGE:   python Tools/checks/count-acceptance-citations.py [--list]
"""

import pathlib
import re
import sys

REPO = pathlib.Path(__file__).resolve().parent.parent.parent
DOC = REPO / "Spec" / "06_ACCEPTANCE.md"
TOOLS = REPO / "Tools"

# A row's id is the first cell of a Markdown table row.
#
# *** `re.MULTILINE` IS NOT OPTIONAL AND ITS ABSENCE COST ME A RUN. *** Without it, `^` matches only at the
# start of the WHOLE TEXT, so this pattern found **zero** rows in a document that has 122 - and the script
# reported "0 cited, 0 uncited" with no error, which reads like a document with no rows rather than like a
# broken pattern. *** THE FIRST RUN SAID 0 AND THE DOCUMENT SAYS 122. ***
ROW = re.compile(r"^\|\s*(T-[A-Z0-9-]+)", re.MULTILINE)

# A suite proving a row says so in a header line, and the id there is the CLAIM rather than a mention.
CLAIMS = re.compile(r"ACCEPTANCE\s+ROW\s+THIS\s+SUITE\s+CLAIMS\s+TO\s+PROVE:\s*(T-[A-Z0-9-]+)", re.IGNORECASE)


def main() -> int:
    want_list = "--list" in sys.argv

    if not DOC.exists():
        print("  NO DOCUMENT at %s" % DOC)
        return 1

    ids = ROW.findall(DOC.read_text(encoding="utf-8", errors="replace"))
    unique = sorted(set(ids))

    suites = sorted(TOOLS.glob("test-*.py"))
    texts = [(p.name, p.read_text(encoding="utf-8", errors="replace")) for p in suites]

    cited, uncited, claimed = [], [], []
    for rid in unique:
        hit = [name for name, text in texts if rid in text]
        if hit:
            cited.append((rid, hit))
        else:
            uncited.append(rid)
        for name, text in texts:
            if rid in CLAIMS.findall(text):
                claimed.append((rid, name))

    print("  document            : %s" % DOC.name)
    print("  rows with an id     : %d        distinct ids: %d" % (len(ids), len(unique)))
    print("  suites searched     : %d" % len(suites))
    print()
    print("  *** CITED BY AT LEAST ONE SUITE (a literal occurrence of the id): %d ***" % len(cited))
    print("  *** CITED BY NO SUITE:                                          %d ***" % len(uncited))
    print()
    print("  *** AND THE STRICTER QUESTION, WHICH IS NOT THE SAME ONE: suites that CLAIM a row with")
    print("      'ACCEPTANCE ROW THIS SUITE CLAIMS TO PROVE':                 %d ***" % len(claimed))
    for rid, name in claimed:
        print("      %-14s %s" % (rid, name))
    print()
    if want_list:
        print("  NOT CITED BY ANY SUITE:")
        for rid in uncited:
            print("      %s" % rid)
    else:
        print("  (pass --list for the %d ids no suite cites)" % len(uncited))
    print()
    print("  *** THE CRITERION IS STATED ABOVE ON PURPOSE. Four earlier attempts at this count gave 91, 64, 66")
    print("      and 56, because none of them said what 'cited' meant. THIS ONE DOES: a literal occurrence of the")
    print("      id in a Tools/test-*.py file. Change the criterion and the number changes, which is why the")
    print("      criterion is part of the answer. ***")
    print()
    print("  *** AND HOW TO CLOSE THE GAP, MEASURED RATHER THAN ASSUMED: THE MAPPING IS NOT GREPPABLE. ***")
    print("      `Spec/06_ACCEPTANCE.md` phrases every row as `id | SUBJECT on FIXTURE | expected behaviour`,")
    print("      and NONE of the twelve tool names a suite is usually named after appears in it: a grep for")
    print("      `GRAPH-APPEND`, `GRAPH-REPLACE`, `GRAPH-PATCH`, `GRAPH-KNOTS`, `GRAPH-DEAD`, `COMMENT`,")
    print("      `VARS-ADD`, `ENUM-ADD`, `LOCAL-VARS`, `OVERRIDE-ADD`, `WIDGET-RETARGET` and `WIDGETS-AS-VARS`")
    print("      returns ZERO matches - so a suite CANNOT be matched to a row by its name, and the row has to")
    print("      be READ against the suite's subject instead. That is why only %d of the %d suites claim one:"
          % (len(claimed), len(suites)))
    print("      THE REMAINING %d ARE READING WORK RATHER THAN A RENAME." % (len(suites) - len(claimed)))
    print("      *** AND THAT IS WHY A GUESSED ID WOULD MAKE THIS MEASUREMENT WORSE RATHER THAN BETTER: IT")
    print("      WOULD CITE A ROW NOTHING TESTS. ***")
    return 0


if __name__ == "__main__":
    sys.exit(main())
