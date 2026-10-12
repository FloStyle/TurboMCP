"""Find the `C-P2` shape in the read-only sources: a refusal that names the wrong cause.

WHY: the hand audit read 37 `Source/` files and found that THE DEFECTS WHICH CHANGE BEHAVIOUR ARE ALL IN FILES
THAT WRITE - the 28 unchecked cancels, the dead counters, the early exits that skip `MarkPackageDirty`. The 16
read-only files it read in full carry only `C3` (a comment contradicting the code) and `C-P2` (a refusal that
names the wrong cause), AND NEITHER CHANGES WHAT A TOOL DOES. So the remaining eleven readers do not need to be
read one at a time; they need the `C-P2` screen, which is this file.

*** WHAT `C-P2` LOOKS LIKE, FROM THE SITES THE HAND AUDIT CONFIRMED: ***

    `TurboMCPNodeResolve.cpp:343`   passes a key that may not be hexadecimal to `FindNodesByKey`, which trims
                                    and lowercases but does not validate; a bad key then reaches
                                    `:371-374`: "no node in %s:%s has the key %s. THE NODE MAY HAVE BEEN
                                    DELETED, OR THE GRAPH REBUILT." *** A TYPO IN THE CALLER IS REPORTED AS A
                                    FACT ABOUT THE GRAPH. ***
    `TurboMCPTypes.cpp:395-404`     `banana`, `0` and `-5` all give the answer for the default, silently
    `TurboMCPCallCostTools.cpp:114-122`  the same shape, a different default

*** SO THE SCREEN LOOKS FOR TWO THINGS IN A READ-ONLY FILE: A NUMERIC PARAMETER PARSED AND SUBSTITUTED, AND A
REFUSAL THAT ATTRIBUTES THE FAILURE TO THE ASSET OR THE GRAPH. ***

*** IT REPORTS CANDIDATES AND DECIDES NOTHING. ***  That is the `V1` rule: a pattern match says where to look.
`--list` prints every candidate with its line; nothing here claims a candidate is a defect.

USAGE:   python Tools/checks/check-cp2-candidates.py [--list]
"""

import pathlib
import re
import sys

REPO = pathlib.Path(__file__).resolve().parent.parent.parent
SRC = REPO / "Source" / "TurboMCP"

# A file that writes cannot be screened this way - its defects are the C-P10 family and need a human.
WRITES = re.compile(r"(Modify\(\)|MarkPackageDirty|DestroyNode|NewObject<|::Apply\()")

# Shape 1: a numeric parameter converted and then replaced when the conversion is not usable.
SUBSTITUTED = re.compile(
    r"Atoi\([^)]*\)[^;]*;.{0,200}?(?:\w+\s*=\s*(?:Default|\d+))",
    re.DOTALL,
)

# Shape 2: a refusal in a reader that blames the asset, the graph or the node.
BLAMES_THE_INPUT = re.compile(
    r"(?i)(no node|node may have been deleted|graph rebuilt|does not (?:exist|resolve)|"
    r"has no graph named|not a Blueprint|could not be (?:read|opened|loaded))"
)

# A refusal is identifiable: the project writes `error:` at the start of every one.
REFUSAL = re.compile(r'error:')


def reads_only(text: str) -> bool:
    return not WRITES.search(text)


def main() -> int:
    want_list = "--list" in sys.argv

    files = sorted(SRC.glob("*.cpp"))
    if not files:
        print("  NO SOURCES under %s" % SRC)
        return 1

    readers = [p for p in files if reads_only(p.read_text(encoding="utf-8", errors="replace"))]
    print("  .cpp files: %d        read-only by this screen: %d" % (len(files), len(readers)))
    print()

    candidates = []
    for path in readers:
        text = path.read_text(encoding="utf-8", errors="replace")
        for m in SUBSTITUTED.finditer(text):
            line = text[: m.start()].count("\n") + 1
            candidates.append((path.name, line, "SUBSTITUTED", m.group(0)[:70].replace("\n", " ")))
        for m in BLAMES_THE_INPUT.finditer(text):
            line = text[: m.start()].count("\n") + 1
            span = text[max(0, m.start() - 120) : m.start() + 120]
            if REFUSAL.search(span):
                candidates.append((path.name, line, "BLAMES-INPUT", m.group(0)[:70]))

    print("  *** CANDIDATES: %d ***" % len(candidates))
    shown = candidates if want_list else candidates[:12]
    for name, line, kind, snippet in shown:
        print("      %-34s %-5d %-13s %s" % (name, line, kind, snippet))
    if not want_list and len(candidates) > len(shown):
        print("      ... and %d more (pass --list)" % (len(candidates) - len(shown)))
    print()
    print("  *** THESE ARE CANDIDATES, NOT DEFECTS. Each needs the refusal and the value it blames read")
    print("      together - which is one file and one line, not a directory. ***")
    return 0


if __name__ == "__main__":
    sys.exit(main())
