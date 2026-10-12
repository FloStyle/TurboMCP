"""Measure what two documents actually share, so a claim about overlap is a reading rather than an estimate.

WHY: `STATUS.md` and `REPORT.md` were said to overlap by "about 35%" for three rounds, and that was a guess
made by reading both once. *** A NUMBER LIKE THAT IS EITHER MEASURED OR IT IS DECORATION, and decoration in a
project whose whole method is measurement is the worst kind. ***

HOW IT MEASURES, and this is deliberately crude: a claim is a SENTENCE, and two documents share a claim when
a sentence in one is most of a sentence in the other. It normalises case and whitespace, drops Markdown
punctuation, and compares the FIRST EIGHT WORDS - which is what a heading or a topic sentence carries and
what two documents restating the same fact will have in common.

*** IT OVER-REPORTS RATHER THAN UNDER-REPORTS: A SHARED OPENING PHRASE CAN BE COINCIDENCE. *** The output
therefore groups by the shared phrase so a human can dismiss a whole group in one read, and it prints the
actual sentences rather than only a percentage. A number nobody can check is what this script exists to
replace.

USAGE:   python Tools/checks/measure-overlap.py STATUS.md REPORT.md
"""

import pathlib
import re
import sys
from collections import defaultdict

REPO = pathlib.Path(__file__).resolve().parent.parent.parent

WORD = re.compile(r"[a-z0-9]+")


def sentences(path: pathlib.Path):
    """Every sentence of the document, with its line, stripped of Markdown emphasis."""
    out = []
    for i, raw in enumerate(path.read_text(encoding="utf-8", errors="replace").split("\n"), 1):
        line = raw.strip()
        if not line or line.startswith("```") or line.startswith("|---") or set(line) <= set("-|: "):
            continue
        line = re.sub(r"[*`_\[\]()>#|]", " ", line)
        line = re.sub(r"\s+", " ", line).strip()
        for s in re.split(r"(?<=[.!?])\s+", line):
            s = s.strip()
            if len(WORD.findall(s)) >= 6:
                out.append((i, s))
    return out


def key(sentence: str, n: int = 8):
    return " ".join(WORD.findall(sentence.lower())[:n])


def main() -> int:
    if len(sys.argv) < 3:
        print("  usage: measure-overlap.py FILE_A FILE_B")
        return 2
    a_path, b_path = (REPO / sys.argv[1]), (REPO / sys.argv[2])
    a, b = sentences(a_path), sentences(b_path)

    index = defaultdict(list)
    for line, s in b:
        index[key(s)].append((line, s))

    shared = []
    for line, s in a:
        for other_line, other in index.get(key(s), []):
            shared.append((key(s), line, s, other_line, other))

    print("  %s: %d sentences (%d lines)" % (a_path.name, len(a), len(a_path.read_text(errors="replace").split("\n"))))
    print("  %s: %d sentences (%d lines)" % (b_path.name, len(b), len(b_path.read_text(errors="replace").split("\n"))))
    print()
    print("  shared claims: %d" % len(shared))
    if a:
        print("  *** %.0f%% of %s's sentences restate one in %s ***" % (100.0 * len(shared) / len(a), a_path.name, b_path.name))
    print()

    groups = defaultdict(list)
    for k, line, s, other_line, other in shared:
        groups[k].append((line, other_line, s))
    for k in sorted(groups, key=lambda k: -len(groups[k])):
        hits = groups[k]
        print("  --- %d x  \"%s...\"" % (len(hits), k))
        print("        %s:%d  %s" % (a_path.name, hits[0][0], hits[0][2][:108]))
        print("        %s:%d  (same claim)" % (b_path.name, hits[0][1]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
