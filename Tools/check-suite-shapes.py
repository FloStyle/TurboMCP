"""Check the quality audit's own detection against the suites, independently.

WHY: `audit-suite-quality.py` recognises a suite's failure list by SHAPE - a list literal that something is
appended to - and takes the INTERSECTION of the two searches. *** AN INTERSECTION IS THE RIGHT IDEA AND IT
HAS ONE WAY TO BE WRONG: A SUITE CAN NAME ITS LIST IN ONE EXPRESSION AND APPEND TO IT THROUGH ANOTHER, SO THE
TWO SETS DO NOT MEET AND A SUITE THAT ASSERTS PERFECTLY IS REPORTED AS ASSERTING NOTHING. *** That is the
`NO-ASSERT` false negative this checks for, on every suite, rather than on the one that happened to look odd.

It also reports which detection path each suite takes, so a future reader can see that the audit's silence
about a suite means the audit looked and was satisfied.
"""

import pathlib
import re
import sys

REPO = pathlib.Path(r"C:\Users\Example\Desktop\TurboMCP")
TOOLS = REPO / "Tools"

LIST = re.compile(r"(\w+)\s*=\s*\[\s*\]")
APPEND = re.compile(r"(\w+)\.append\(")
CHECK = re.compile(r"\bcheck\s*\(")


def main() -> int:
    rows = []
    for path in sorted(TOOLS.glob("test-*.py")):
        text = path.read_text(encoding="utf-8", errors="replace")
        lists = set(LIST.findall(text))
        appends = set(APPEND.findall(text))
        bound = sorted(lists & appends)
        checks = len(CHECK.findall(text))
        rows.append((path.name, sorted(lists), sorted(appends), bound, checks))

    print("  %-32s %5s %6s %6s  %s" % ("suite", "lists", "append", "bound", "check() calls"))
    problems = []
    for name, lists, appends, bound, checks in rows:
        flag = ""
        if not bound:
            flag = "  *** NO BOUND FAILURE LIST ***"
            problems.append(name)
        print("  %-32s %5d %6d %6d  %d%s" % (name, len(lists), len(appends), len(bound), checks, flag))

    print()
    print("  suites examined:            %d" % len(rows))
    print("  with a bound failure list:  %d" % sum(1 for r in rows if r[3]))
    print("  with a check() helper:      %d" % sum(1 for r in rows if r[4]))
    print("  bound list but no check():  %d" % sum(1 for r in rows if r[3] and not r[4]))
    print("  check() but no bound list:  %d" % sum(1 for r in rows if r[4] and not r[3]))
    if problems:
        print()
        print("  *** THESE WOULD BE REPORTED NO-ASSERT AND MAY BE FALSE NEGATIVES: ***")
        for name in problems:
            print("      " + name)
    return 0


if __name__ == "__main__":
    sys.exit(main())
