"""Check line-number citations into Markdown files against the text that is actually there.

WHY: *** A LINE NUMBER IN A DOCUMENT THAT IS STILL BEING EDITED IS A CITATION WITH AN EXPIRY DATE. *** This
repository cites `Spec/06_ACCEPTANCE.md:100`, `03_TOOLS.md:1348-1351`, `05_DECISIONS.md:382-388` and dozens
more from source comments, suites and other specs. Every one of them was correct when it was written and
**none of them is checked by anything**, so a reader who follows one lands in the middle of an unrelated
paragraph and concludes the documentation is unreliable.

IT DOES NOT NEED THE CITED LINE TO BE QUOTED. It reports what IS at that line, so a human can judge in one
read whether the citation still points at what it claims - which is the only thing a checker can honestly
offer for prose.

USAGE:   python Tools/check-line-cites.py            report every citation with its target line
         python Tools/check-line-cites.py --counts   one line per file, counts only
"""

import pathlib
import re
import subprocess
import sys

REPO = pathlib.Path(r"C:\Users\Example\Desktop\TurboMCP")

# `Spec/06_ACCEPTANCE.md:100`, `03_TOOLS.md:1348-1351`, `WORKING_PLAN.md:88`
CITE = re.compile(r"`?([\w./-]*\.md):(\d+)(?:-(\d+))?`?")

# Only these are unstable: a plan or a log still being edited. A file's own line numbers are its content.
# Nothing is excluded here - the count is the point - but the report marks which files are edit targets.
EDIT_TARGETS = ("WORKING_", "TestRecord.md", "REPORT.md", "HANDOVER.md", "STATUS.md", "QUESTIONS.md",
                "DOCS.md", "PLAN.md", "README.md")


def main() -> int:
    files = subprocess.run(["git", "ls-files"], cwd=REPO, capture_output=True, text=True).stdout.split()
    cache = {}
    rows = []

    for rel in files:
        p = REPO / rel
        if not p.is_file() or p.suffix not in (".md", ".py", ".ps1", ".cs", ".h", ".cpp", ""):
            continue
        text = p.read_text(encoding="utf-8", errors="replace")
        for i, line in enumerate(text.split("\n"), 1):
            for m in CITE.finditer(line):
                target, first = m.group(1), int(m.group(2))
                last = int(m.group(3)) if m.group(3) else first
                resolved = None
                for cand in (p.parent / target, REPO / target):
                    try:
                        if cand.is_file():
                            resolved = cand.resolve().relative_to(REPO).as_posix()
                            break
                    except Exception:
                        continue
                if resolved is None:
                    continue
                if resolved not in cache:
                    cache[resolved] = (REPO / resolved).read_text(encoding="utf-8", errors="replace").split("\n")
                body = cache[resolved]
                span = body[first - 1: last]
                rows.append((rel, i, resolved, first, last, len(body), " / ".join(s.strip() for s in span)[:96]))

    if "--counts" in sys.argv:
        import collections
        c = collections.Counter(r[0] for r in rows)
        for rel, n in c.most_common():
            print("  %4d  %s" % (n, rel))
        print()
        print("  total: %d line-number citations into Markdown" % len(rows))
        return 0

    print("  %-34s %-8s %s" % ("cited from", "line", "what is at the cited line"))
    problems = []
    for rel, line, target, first, last, total, text in rows:
        over = " *** BEYOND EOF ***" if first > total else ""
        flag = ""
        if over:
            problems.append((rel, line, target, first))
        print("  %-34s %-8s %-34s %s%s" % (rel[:34], "%d" % line, target, text, over))

    print()
    print("  citations found: %d" % len(rows))
    print("  pointing past the end of their file: %d" % len(problems))
    for rel, line, target, first in problems:
        print("      *** %s:%d -> %s:%d" % (rel, line, target, first))

    # *** A LINE NUMBER THAT STILL EXISTS CAN STILL BE WRONG, AND A SEPARATOR OR A HEADER IS THE TELL. ***
    # A citation into a table that now lands on `|---|---|`, or onto a blank line, or onto a heading, was
    # almost certainly written against a revision of that file which has since moved - **the content it meant
    # is no longer at that offset**. This is not proof, and it is not reported as proof: it is the short list
    # worth a human's eye out of the whole set.
    SUSPECT = ("|---", "| ---", "##", "# ", "")
    suspects = []
    for rel, line, target, first, last, total, text in rows:
        stripped = text.strip()
        if not stripped or stripped.startswith("|--") or re.match(r"^#{1,6}\s", stripped) or stripped == "```":
            suspects.append((rel, line, target, first, text))
    print()
    print("  *** CITATIONS THAT LAND ON A SEPARATOR, A HEADING OR NOTHING: %d ***" % len(suspects))
    for rel, line, target, first, text in suspects:
        print("      %s:%d -> %s:%d   %r" % (rel, line, target, first, text[:60]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
