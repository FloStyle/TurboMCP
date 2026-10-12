"""Check every acceptance row for a pass condition that can actually FAIL.

WHY: the doc audit reported 46 of 122 rows in `Spec/06_ACCEPTANCE.md` whose "Pass when" cell names no input
that makes the row fail. *** A NUMBER LIKE THAT HAS TO BE DERIVED, AND A KEYWORD FILTER CANNOT DERIVE IT: a
row can say "refuses" about something the tool always refuses, and it can omit every failure word while still
naming a case that breaks it. *** So this reads the CELLS and reports the ones with no failure signal, and it
prints each so a human can confirm or dismiss it in one read.

WHAT COUNTS AS A FAILURE SIGNAL, and why each one was chosen:

    a refusal, an error, a rollback, a fallback
    a deliberately wrong input        ("a bad path", "an unknown name", "a missing asset")
    a missing or unreadable case      ("does not exist", "cannot be read", "not loaded")
    a stated count that must change   ("N of M", a named number that would differ)
    a comparison between two things   (one of them can then disagree)

WHAT DOES NOT: a statement of what the tool prints, or of what it verifies, with nothing to compare against.

USAGE:   python Tools/checks/check-acceptance-rows.py
"""

import pathlib
import re
import sys

REPO = pathlib.Path(__file__).resolve().parent.parent.parent
DOC = REPO / "Spec" / "06_ACCEPTANCE.md"

# One row of a Markdown table whose first cell is a test ID.
ROW = re.compile(r"^\|\s*(T-[A-Z0-9-]+)\s*\|(.*)$")

# *** THE FIRST VERSION OF THIS PATTERN WAS WRONG IN BOTH DIRECTIONS, AND BOTH ARE RECORDED HERE. ***
#
#   IT MATCHED THINGS THAT ARE NOT FAILURE CONDITIONS. `no backward wires`, `no box moved` and `no definition
#   is only another name` are **measurable properties**: the row can fail when the property does not hold. A
#   bare `no` proved nothing, and `fails?\b` matched the middle of "fails to fit", which is a description of
#   the tool's output and not a case that breaks it. The first run therefore reported 68 where the truth is
#   smaller, and a count that over-reports is how a checker gets ignored.
#
#   IT ALSO MISSED THE SHAPE THAT MATTERS MOST. A row whose condition is a NUMBER the measurement must reach
#   (`shorter by at least 30 percent`, `5 keys with times and values`) **can** fail, so it is not weak. What is
#   weak is a row that states only WHAT THE TOOL PRINTS, with nothing to compare against.
#
# SO THE PATTERN NOW REQUIRES AN EXPLICIT FAILURE WORD, AND A COUNT ROW IS EXEMPT SEPARATELY.
FAILURE_SIGNAL = re.compile(
    r"(?i)("
    r"refus\w*|rolls? ?back|rolled ?back|\berror\b|\berrors\b|"
    r"invalid|unreadable|cannot be read|does not exist|not loaded|absent|"
    r"\bwrong\b|deliberately|\binstead of\b|mismatch|disagree\w*|"
    r"\brejects?\w*|\bdeclines?\w*|\baborts?\w*|"
    r"\bfails?\b(?! to)|\bfailure\b"
    r")"
)

# A condition that names a number it must reach, or two numbers that must agree, can fail.
NUMERIC_CONDITION = re.compile(
    r"(?i)(at least|at most|no more than|exactly|\bsame\b|\bequal\b|\bmatches?\b|shorter|longer|"
    r"\bzero\b|\bnone\b|\b\d+\b)"
)


def cells(line: str):
    parts = [c.strip() for c in line.strip().strip("|").split("|")]
    return parts


def main() -> int:
    rows = []
    for i, line in enumerate(DOC.read_text(encoding="utf-8", errors="replace").split("\n"), 1):
        m = ROW.match(line)
        if m:
            rows.append((i, m.group(1), cells(line)))

    print("  acceptance rows found: %d" % len(rows))
    if not rows:
        print("  NOTHING PARSED - the table shape must have changed")
        return 1

    widths = {}
    for _, _, c in rows:
        widths[len(c)] = widths.get(len(c), 0) + 1
    print("  cell counts seen: %s" % widths)
    print()

    weak = []
    for line_no, tid, c in rows:
        # The pass condition is the LAST cell of the row; earlier cells are id, asset, and what it verifies.
        cond = c[-1]
        if FAILURE_SIGNAL.search(cond) or NUMERIC_CONDITION.search(cond):
            continue
        weak.append((line_no, tid, cond))

    print("  *** ROWS WITH NO FAILURE SIGNAL IN THEIR PASS CONDITION: %d of %d ***" % (len(weak), len(rows)))
    print()
    for line_no, tid, cond in weak:
        print("  %s:%d  %-14s %s" % (DOC.name, line_no, tid, cond[:96]))
    print()
    print("  ROWS THAT DO NAME A FAILURE: %d" % (len(rows) - len(weak)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
