"""Count the plan's rows by their LAST FIELD, the way the plan itself says to count them.

WHY A SCRIPT: `Spec/WORKING_PLAN.md` states the rule - *"counted BY FIELD, never by a regex over the line"* -
because an earlier round reported a wrong number by matching a state word anywhere in the row. This does what
the rule says: split each row on `|` and read the last non-empty cell.

USAGE:   python Tools/count-plan.py            summary
         python Tools/count-plan.py --list     also name every row with its state
"""

import collections
import pathlib
import re
import sys

REPO = pathlib.Path(r"C:\Users\Example\Desktop\TurboMCP")
PLAN = REPO / "Spec" / "WORKING_PLAN.md"

STATES = ("done", "built", "todo", "gated", "open")


def main() -> int:
    rows = []
    for i, line in enumerate(PLAN.read_text(encoding="utf-8").split("\n"), 1):
        if not line.startswith("|"):
            continue
        cells = [c.strip() for c in line.strip().strip("|").split("|")]
        if len(cells) < 3:
            continue
        # A row of `---|---` is the header separator, not a row.
        if set(cells[0]) <= set("-: "):
            continue
        state = cells[-1].strip("*` ").lower()
        # A state may carry a parenthetical reason, as in `open (Q-21)`. *** THE FIRST VERSION OF THIS SCRIPT
        # COUNTED 47 ROWS INSTEAD OF 48 BECAUSE OF IT *** - the row was skipped as unrecognised, so a plan row
        # disappeared from the total rather than from a state.
        state = re.sub(r"\s*\(.*?\)\s*$", "", state).strip()
        if state not in STATES:
            continue                      # a section's index table, not a capability row
        # *** A CAPABILITY ROW IS NUMBERED. *** The dependency section ends four of its rows with the word
        # `open` - `Q-19: a push-driven ... | open` - and those are QUESTIONS the plan depends on, not rows of
        # work. Counting them added four to `open` in the summary while `--list` showed only two, because the
        # list printed the row id and the ids clearly were not rows. **A COUNT THAT DISAGREES WITH ITS OWN
        # LISTING IS WORSE THAN EITHER ALONE.**
        if not re.match(r"^\d+\.\d+$", cells[0].strip("*` ")):
            continue
        rows.append((cells[0].strip("*` "), state, i))

    counts = collections.Counter(s for _, s, _ in rows)
    print("  rows counted: %d" % len(rows))
    for state in STATES:
        if counts[state]:
            print("  %-6s %d" % (state, counts[state]))
    print()
    print("  *** %d todo · %d done · %d built · %d gated · %d open ***" % (
        counts["todo"], counts["done"], counts["built"], counts["gated"], counts["open"]))

    if "--list" in sys.argv:
        print()
        for rid, state, line in rows:
            print("  L%-5d %-10s %s" % (line, state, rid))
    return 0


if __name__ == "__main__":
    sys.exit(main())
