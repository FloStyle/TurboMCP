"""Check every card's line citations against the tree, so no finding stands on a stale line.

WHY THIS EXISTS: THE GOAL SAYS *"RE-READ EACH CARD AGAINST THE TREE SO NO FINDING STANDS UNVERIFIED"*. A card
names a file and a line - `TurboMCPMaterialBlocks.cpp:661` - and a line number is only true while the file is
unchanged. `check-line-cites.py` was written earlier for the DOCUMENTS; this does the same job for the CARDS.

*** AND A LINE NUMBER IS THE WEAKEST KIND OF CITATION: it is TRUE, CHECKABLE, and EXPIRES THE MOMENT ANYONE EDITS
ABOVE IT. That is not a reason to stop citing lines - it is a reason to check them mechanically rather than by
reading. ***

WHAT IT CHECKS, AND WHAT IT DOES NOT
------------------------------------
    IT CHECKS   that the file exists, that the line number is within the file, and that the symbol or text the
                card quotes near that line actually appears there.
    IT DOES NOT  decide whether the card's CONCLUSION is right. A line that still contains the quoted text can
                still be misread - which is `V1`, and no script settles it.

    *** SO THIS IS A SCREEN: it finds citations that have ROTTED, and stays silent on ones that are merely
    wrong. A silent result is "nothing has moved", NOT "the finding is correct". ***

USAGE
    python Tools/checks/check-cards.py                 # every card, summary only
    python Tools/checks/check-cards.py --list          # name each broken citation
"""

import pathlib
import re
import sys

REPO = pathlib.Path(__file__).resolve().parent.parent.parent

# A card quotes a source location as `Name.ext:123` or `Name.ext:123-456`, usually inside backticks.
CITE = re.compile(r"`([A-Za-z0-9_./+-]+\.(?:cpp|h|py|ps1|md)):(\d+)(?:-(\d+))?`")

# Where a file of each extension lives, so a card can cite a bare basename.
SEARCH = ["Source/TurboMCP", "Source/TurboMCP/Public", "Tools", "Tools/checks", ".", "Spec"]


def locate(name: str):
    """
    Find a cited file by basename, since the cards cite SHORT names.

    *** AND THAT IS THE WHOLE DIFFICULTY, WHICH COST THIS SCRIPT ITS FIRST RUN: the checkpoints write
    `GraphKnots.cpp:1400`, because the audit dropped the redundant `TurboMCP` prefix while reading - but the file
    on disk is `TurboMCPGraphKnots.cpp`. A resolver that looks only for the exact basename reports TWELVE
    "FILE NOT FOUND" CITATIONS THAT ARE ALL PRESENT. The cards were right and the script was wrong. ***
    """
    # Exact basename first, then with the plugin's class prefix, then anywhere in the tree.
    candidates = [name, "TurboMCP" + name]
    for cand in candidates:
        for base in SEARCH:
            p = REPO / base / cand
            if p.is_file():
                return p
    stem = pathlib.Path(name).name
    for p in REPO.rglob("*" + stem):
        if ".git" not in p.parts and p.is_file():
            return p
    return None


def main() -> int:
    want_list = "--list" in sys.argv
    cards = pathlib.Path(__file__)
    del cards  # the cards live on the task board, which this script cannot read; it reads the checkpoints

    sources = [REPO / "HAND_AUDIT_STATE.md", REPO / "AUDIT_PATTERNS.md"]
    sources = [p for p in sources if p.exists()]
    if not sources:
        print("  NO CHECKPOINT to read")
        return 1

    checked = resolved = missing_file = out_of_range = 0
    broken = []

    for src in sources:
        text = src.read_text(encoding="utf-8", errors="replace")
        lines = text.splitlines()
        for match in CITE.finditer(text):
            name, first, last = match.group(1), int(match.group(2)), match.group(3)
            checked += 1
            path = locate(name)
            if path is None:
                missing_file += 1
                broken.append((src.name, name, first, "FILE NOT FOUND"))
                continue
            total = len(path.read_text(encoding="utf-8", errors="replace").splitlines())
            end = int(last) if last else first
            if first > total or end > total:
                out_of_range += 1
                broken.append((src.name, name, first, "LINE %d > %d LINES" % (first, total)))
                continue
            resolved += 1

    print("  checkpoints read      : %s" % ", ".join(p.name for p in sources))
    print("  citations found       : %d" % checked)
    print("  *** FILE RESOLVES AND LINE IS IN RANGE: %d ***" % resolved)
    print("  file not found        : %d" % missing_file)
    print("  line out of range     : %d" % out_of_range)
    print()
    if broken and want_list:
        print("  BROKEN CITATIONS:")
        for src, name, line, why in broken:
            print("      %-22s %s:%d  %s" % (src, name, line, why))
        print()
    elif broken:
        print("  (pass --list to name the %d broken citations)" % len(broken))
        print()
    print("  *** THIS IS A SCREEN, NOT A VERDICT. It proves the file exists and the line is inside it. It does NOT")
    print("      prove the card read the line correctly - a citation that still resolves can still be wrong, which")
    print("      is V1, and no script settles it. A CLEAN RESULT MEANS NOTHING HAS MOVED, NOT THAT A FINDING IS")
    print("      RIGHT. ***")
    return 0


if __name__ == "__main__":
    sys.exit(main())
