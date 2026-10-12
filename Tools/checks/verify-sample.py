"""Verify a sample of audit findings by reading the code they name.

WHY: the audit produced roughly 800 findings across 305 files, and I have tested five of them by hand. *** TWO
WERE WRONG. THAT RATE DECIDES WHETHER THE REST CAN BE TRUSTED, AND A RATE NEEDS A SAMPLE RATHER THAN AN
ANECDOTE. ***

HOW IT WORKS: each case below names a finding, the file, the line, and WHAT WOULD MAKE IT TRUE. The script
prints the actual line, so the verdict is read rather than assumed. *** A CASE WITH NO PREDICTED EVIDENCE IS
NOT A CASE - if I cannot say what would make the finding true, I cannot check it and it does not belong here.
***

RUN:  python Tools/checks/verify-sample.py
"""

import pathlib
import re
import sys

REPO = pathlib.Path(__file__).resolve().parent.parent.parent

# (class, file, line, the finding in one line, what would make it TRUE)
CASES = [
    ("C-P10", "Source/TurboMCP/TurboMCPKitCommandlet.cpp", 1377,
     "the SaveContainer result is discarded, so four TA-1..4 graphs can be unsaved and the run still exits 0",
     "the line calls SaveContainer and does NOT assign or test its result"),

    ("C-P12", "Source/TurboMCP/TurboMCPKitCommandlet.cpp", 863,
     "`Hand` is dereferenced although MakeFunction can return null",
     "the line uses Hand-> or *Hand, and no null test on Hand appears above it in the same block"),

    ("C-P10", "Source/TurboMCP/TurboMCPGraphKnots.cpp", 1294,
     "the restore TryCreateConnection discards its return after BreakSinglePinLink already broke the wire",
     "the line calls TryCreateConnection and does not assign or test the result"),

    ("C-P3", "Source/TurboMCP/TurboMCPGraphKnots.cpp", 410,
     "MeasureKnotFootprint runs in Plan and creates a real UObject with the graph as outer",
     "the line calls NewObject with InGraph as the outer argument"),

    ("C-P4", "Source/TurboMCP/TurboMCPGraphTidy.cpp", 1219,
     "Apply never reads bWouldApply, so an Edit can run on a plan the Preview said would roll back",
     "bWouldApply does not appear in the Apply function body"),

    ("C-P1", "Source/TurboMCP/TurboMCPGraphFixBackward.cpp", 1607,
     "the box line pins `0 moved, 0 resized` as literals instead of measuring them",
     "the format string contains the text 0 moved and 0 resized"),

    ("C-P2", "Source/TurboMCP/TurboMCPKitCommandlet.cpp", 1793,
     "TA-14 hard-codes bMeasured = true while findings are still added below it",
     "the line sets bMeasured = true with a literal"),

    ("C-P6", "Source/TurboMCP/TurboMCPStruct.cpp", 467,
     "TypeMatches decides by substring, so int is accepted on an int64 field",
     "the line uses Contains on the two type strings"),
]


def show(path: pathlib.Path, line_no: int, span: int = 2):
    try:
        lines = path.read_text(encoding="utf-8", errors="replace").split("\n")
    except Exception as exc:
        return ["  CANNOT READ: %s" % exc]
    lo = max(0, line_no - 1 - span)
    hi = min(len(lines), line_no + span)
    out = []
    for i in range(lo, hi):
        mark = ">>" if i == line_no - 1 else "  "
        out.append("  %s L%-5d %s" % (mark, i + 1, lines[i].rstrip()[:104]))
    return out


def main() -> int:
    print("  SAMPLE OF %d FINDINGS, ONE PER CLASS WHERE POSSIBLE" % len(CASES))
    print()
    for cls, rel, line_no, finding, evidence in CASES:
        p = REPO / rel
        print("  === [%s] %s:%d" % (cls, rel, line_no))
        print("      FINDING  : %s" % finding)
        print("      TRUE IF  : %s" % evidence)
        for row in show(p, line_no):
            print("      " + row.strip() if row.startswith("  >>") else row)
        print()
    print("  *** READ EACH BLOCK AND DECIDE. THE SCRIPT DOES NOT DECIDE, BECAUSE A SCRIPT THAT GUESSES WHICH")
    print("      SIDE OF A CLAIM IS RIGHT IS THE DEFECT THIS WHOLE AUDIT EXISTS TO FIND. ***")
    return 0


if __name__ == "__main__":
    sys.exit(main())
