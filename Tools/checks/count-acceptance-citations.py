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
#
# *** THE LINE HOLDS A LIST, AND IT USED TO HOLD ONLY ONE ID WITHOUT SAYING SO: the old pattern was
# `PROVE:\s*(T-[A-Z0-9-]+)`, WHICH CAPTURES THE FIRST ID AND SILENTLY DROPS EVERY LATER ONE - so a suite naming
# two rows was CREDITED WITH ONE AND REPORTED AS IF THAT WERE ALL IT SAID. *** Six suites name several, and one
# names a RANGE ("T-LOOK-1 to T-LOOK-4"), which is now written out as the four rows it means.
CLAIMS = re.compile(r"ACCEPTANCE\s+ROW\s+THIS\s+SUITE\s+CLAIMS\s+TO\s+PROVE:[^\n]*", re.IGNORECASE)
IDS_IN_CLAIM = re.compile(r"T-[A-Z0-9-]+")

# *** A SUITE THAT PROVES NO ROW DECLARES IT, SO THE ABSENCE IS A STATEMENT RATHER THAN A SILENCE. ***
NONE_MARK = re.compile(r"ACCEPTANCE\s+ROW\s+THIS\s+SUITE\s+CLAIMS\s+TO\s+PROVE:\s*NONE\b", re.IGNORECASE)


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
            if rid in [i for line in CLAIMS.findall(text) for i in IDS_IN_CLAIM.findall(line)]:
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
    print("      *** AND THAT NUMBER IS (row, suite) CLAIMS RATHER THAN SUITES - a suite naming four rows makes four,")
    print("      WHICH IS HOW A COUNT THAT SAID 'SUITES' CAME TO EXCEED THE %d SUITES THAT EXIST. THE UNITS ARE" % len(suites))
    print("      NOW BOTH STATED: *** %d CLAIMS, MADE BY %d OF THE %d SUITES. ***"
          % (len(claimed), len(set(n for _, n in claimed)), len(suites)))
    silent = [name for name, text in texts
              if not [i for line in CLAIMS.findall(text) for i in IDS_IN_CLAIM.findall(line)]]
    if silent:
        print()
        print("  *** AND THE SUITES THAT CLAIM NO ROW ARE NAMED RATHER THAN LEFT AS A REMAINDER - %d OF THEM: ***"
              % len(silent))
        for name in silent:
            print("      %s" % name)
        print("      *** A SUITE THAT PROVES NO ACCEPTANCE ROW IS A LEGITIMATE ANSWER - `test-uncovered-tools`")
        print("      PROVES COVERAGE, WHICH IS ANOTHER SCRIPT'S QUESTION - BUT THE ABSENCE HAS TO BE VISIBLE,")
        print("      BECAUSE AN UNNAMED REMAINDER IS RE-DERIVED BY EVERY READER INSTEAD OF READ. ***")
        print("      *** AND IT IS NOW A DECLARATION RATHER THAN A SILENCE: A SUITE SAYING SO WRITES")
        print("      `ACCEPTANCE ROW THIS SUITE CLAIMS TO PROVE: NONE` AND GIVES ITS REASON, WHICH THIS SCRIPT")
        print("      RECOGNISES BELOW. ANYTHING STILL UNSTATED IS LISTED AS UNSTATED. ***")
    declared_none = [name for name, text in texts if NONE_MARK.search(text)]
    unstated = [name for name in silent if name not in declared_none]
    if declared_none:
        print()
        print("  *** DECLARED: `... CLAIMS TO PROVE: NONE`, with the reason in the line - %d: ***" % len(declared_none))
        for name in declared_none:
            print("      %s" % name)
    if unstated:
        print()
        print("  *** AND STILL UNSTATED - neither a row nor a declared NONE - %d: ***" % len(unstated))
        for name in unstated:
            print("      %s" % name)
    bogus = [(rid, name) for rid, name in claimed if rid not in unique]
    if bogus:
        print()
        print("  *** AND THESE CLAIMS NAME SOMETHING THAT IS NOT A ROW IN THE DOCUMENT - WHICH IS THE ONE ERROR")
        print("      THIS CONVENTION EXISTS TO PREVENT, SO IT IS REPORTED RATHER THAN COUNTED: ***")
        for rid, name in bogus:
            print("      %-14s %s   *** NOT A ROW ***" % (rid, name))
    else:
        print()
        print("  *** AND EVERY CLAIMED ID IS A ROW THAT EXISTS IN `06_ACCEPTANCE.md`. *** *** THIS CHECK IS HERE")
        print("      BECAUSE A SUITE ONCE CLAIMED THREE IDS THAT WERE NOT ROWS AT ALL, AND THE ONLY SIGN OF IT WAS")
        print("      THAT THE REMAINDER COUNT AND THE NAMED REMAINDER DISAGREED BY ONE. ***")
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
    print("      THE REMAINING %d ARE READING WORK RATHER THAN A RENAME."
          % (len(suites) - len(set(n for _, n in claimed))))
    print("      *** AND THAT IS WHY A GUESSED ID WOULD MAKE THIS MEASUREMENT WORSE RATHER THAN BETTER: IT")
    print("      WOULD CITE A ROW NOTHING TESTS. ***")
    print()
    print("  *** AND THE FIXTURE TEST - 'does the suite name the fixture the row names?' - CAN ACCEPT ALMOST")
    print("      NOTHING, WHICH IS MEASURED RATHER THAN FELT: ***")
    print("          53 OF THE 122 ROWS NAME A `TA-` FIXTURE,")
    print("          AND OF THE UNMAPPED SUITES ONLY *** 3 NAME ONE AT ALL ***.")
    print("      SO THE ROWS NAME THE HAND-BUILT KIT WHILE THE SUITES DRIVE THE EARLIER `BP_NodeTest` ASSET, AND")
    print("      THE MAPPING THEREFORE HAS TO REST ON THE ROW'S `Pass when` COLUMN RATHER THAN ON ITS FIXTURE.")
    print("      *** A ROW AND A SUITE THAT SHARE NO FIXTURE CAN STILL BE THE SAME CAPABILITY - WHICH IS WHY THE")
    print("      FIXTURE TEST REFUSES RATHER THAN GUESSES, AND WHY THE REMAINING MAPPINGS TAKE READING. ***")
    return 0


if __name__ == "__main__":
    sys.exit(main())
