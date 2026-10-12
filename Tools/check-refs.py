"""Check every cross-reference in the documentation against the file it points at.

WHY: this class of defect has shipped THREE TIMES - four `Spec/` files cited `STATUS.md` sections a rewrite
had moved, `README.md` pointed at a gitignored file, and four sentences described version markers that had
been removed. *** A READER WHO FOLLOWS A REFERENCE INTO NOTHING LEARNS THAT THE DOCUMENTATION CANNOT BE
TRUSTED, WHICH COSTS MORE THAN THE MISSING PARAGRAPH. ***

USAGE:   python Tools/check-refs.py            report only
         python Tools/check-refs.py --strict   also fail on a BARE filename reference

WHAT IT RESOLVES:

    `PATH.md` and [PATH.md](PATH.md)      the file exists, RELATIVE TO THE REFERRING DOCUMENT FIRST and to
                                          the repository root second - because `Content/README.md` links to
                                          `../TestRecord.md` and a resolver that only tries the root reports
                                          that as dead
    `PATH.md` section N                   that file has a heading starting with N, or with N. (a subsection)

*** AND IT SEPARATES TWO KINDS OF HIT, BECAUSE THE FIRST VERSION REPORTED TWENTY-FIVE AND TWENTY-FOUR OF THEM
WERE FINE. *** A `07_ENGINE_NOTES.md` written in backticks from the repository root is a bare filename: **a
reader resolves it inside `Spec/` for free, and so does this script, by searching the tree for a file with
that name when the reference is not a link.** Only a bare name that matches NOTHING is a finding, and
`--strict` is there for anyone who wants the weaker complaint too.
"""

import collections
import pathlib
import re
import subprocess
import sys

REPO = pathlib.Path(r"C:\Users\Example\Desktop\TurboMCP")
STRICT = "--strict" in sys.argv

HEAD_RE = re.compile(r"^#{1,6}\s+(?:Part\s+)?(\d+(?:\.\d+)*)")
LINK_RE = re.compile(r"\]\(([^)\s]+\.md)(?:#[\w.-]*)?\)")
# A reference and the numbers it names. *** THE NUMBER LIST STOPS AT `and` AND CONTAINS AT MOST TWO SEGMENTS,
# BECAUSE AN UNBOUNDED PATTERN CROSSES INTO THE NEXT TABLE ROW: *** a row 0.3 that says "sections 1 to 4 and
# 6" was read as "section 7" from the FOLLOWING row's `../STATUS.md`, and reported as a missing section in a
# file the sentence never named.
REF_RE = re.compile(r"`?([\w./-]+\.md)`?(?:\s+(?:section|sections)\s+([\d.]+(?:\s*(?:,|to|and)\s*[\d.]+){0,2}))")


def headings(path: pathlib.Path):
    out = set()
    try:
        text = path.read_text(encoding="utf-8", errors="replace")
    except Exception:
        return out
    for line in text.split("\n"):
        m = HEAD_RE.match(line)
        if m:
            out.add(m.group(1))
    return out


_by_name = {}


def find_by_name(name: str):
    """Search the tree for a file with this basename. Used for a bare filename reference."""
    if name not in _by_name:
        hits = list(REPO.rglob(pathlib.Path(name).name))
        hits = [h for h in hits if ".git" not in h.parts and "Intermediate" not in h.parts]
        _by_name[name] = hits[0].relative_to(REPO).as_posix() if len(hits) == 1 else None
    return _by_name[name]


def main() -> int:
    files = subprocess.run(["git", "ls-files", "*.md"], cwd=REPO, capture_output=True, text=True).stdout.split()
    missing_file, missing_section = [], []
    cache = {}

    def headings_of(rel):
        if rel not in cache:
            cache[rel] = headings(REPO / rel)
        return cache[rel]

    for rel in files:
        p = REPO / rel
        if not p.is_file():
            continue
        base = p.parent
        for i, line in enumerate(p.read_text(encoding="utf-8", errors="replace").split("\n"), 1):
            for m in REF_RE.finditer(line):
                target, numbers = m.group(1), m.group(2) or ""
                is_link = bool(LINK_RE.search(line) and target in line)

                resolved = None
                for cand in (base / target, REPO / target):
                    try:
                        if cand.is_file():
                            resolved = cand.resolve().relative_to(REPO).as_posix()
                            break
                    except Exception:
                        continue
                if resolved is None:
                    # A bare filename is resolved by name anywhere in the tree. A LINK is not: it is a path.
                    by_name = find_by_name(target)
                    if by_name and not is_link:
                        resolved = by_name
                    else:
                        missing_file.append("%s:%d  %s" % (rel, i, target))
                        continue

                have = headings_of(resolved)
                for raw in re.split(r"[,\s]+", numbers):
                    raw = raw.strip().rstrip(".,")
                    if not raw or not raw[0].isdigit():
                        continue
                    if raw in have or any(h.startswith(raw + ".") for h in have):
                        continue
                    missing_section.append("%s:%d  %s has no section %s" % (rel, i, resolved, raw))

    if not missing_file and not missing_section:
        print("  *** EVERY CROSS-REFERENCE RESOLVES ***")
    if missing_file:
        print("  === MISSING FILE : %d" % len(missing_file))
        for h in missing_file:
            print("      " + h)
    if missing_section:
        print("  === MISSING SECTION : %d" % len(missing_section))
        for h in missing_section:
            print("      " + h)
    print()
    print("  documents checked: %d" % len(files))
    return 1 if (missing_file or missing_section) else 0


if __name__ == "__main__":
    sys.exit(main())
