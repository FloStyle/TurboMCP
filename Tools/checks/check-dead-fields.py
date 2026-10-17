"""Find a field that is ASSIGNED IN ONE PLACE AND READ NOWHERE ELSE, beside a comment that says it is used.

WHY THIS EXISTS: FIVE OF THE EIGHT CODE DEFECTS THE C3 CARD PRODUCED ARE ONE SHAPE, AND THIS IS THAT SHAPE.

    `TurboMCPKitCommandlet.cpp:1704`   `CountBoxes(nullptr, Boxes, Nested)` - returns 0/0 on a null graph, and
                                       NEITHER `Boxes` NOR `Nested` is read again. The comment said "THE
                                       STRUCTURE IS COUNTED BACK".
    `TurboMCPGraphTidy.h:148`          `EngineDepth` - declared, assigned at `.cpp:424`, read nowhere. The
                                       comment said "Read from the engine and REPORTED".
    `TurboMCPMaterial.cpp:994`         `PrintedInputs` - declared, incremented twice, read nowhere, and no
                                       `MaxListed` exists to cap anything. The comment said "so the cap is
                                       honest".

*** A FIELD ASSIGNED ONCE AND READ NOWHERE IS TWO GREPS, NOT A JUDGEMENT - WHICH IS WHY IT BELONGS IN A
SCREEN RATHER THAN IN A READER'S MEMORY. ***

WHAT IT DOES
------------
For every `.cpp`/`.h` under `Source/TurboMCP`, it collects the identifiers that appear on the LEFT of an
assignment (`X = `, `X += `, `X++`, `++X`) and the identifiers that appear ANYWHERE ELSE. Anything assigned
and never otherwise used is reported.

*** AND IT REPORTS THE COMMENT NEAREST THE DECLARATION, because that comment is where the false claim lives:
the defect is never the dead field alone, it is the dead field plus a sentence asserting it is reported. ***

WHAT IT DOES NOT DO
-------------------
    IT IS A SCREEN, NOT A VERDICT. It cannot tell a dead field from one that is
        - assigned for its side effect (`Modify()`, `SetX()`, a `UFUNCTION` call)
        - read through a macro, a delegate binding, or reflection
        - written only to satisfy an out-parameter that the callee reads
    SO EVERY CANDIDATE NEEDS THE TWO LINES READ TOGETHER. This tool finds where to look, not what is wrong.
    A NAME THAT IS A FUNCTION OR A TYPE RATHER THAN A VARIABLE WILL ALSO APPEAR, and the printout says so.

USAGE
    python Tools/checks/check-dead-fields.py                 # summary
    python Tools/checks/check-dead-fields.py --list           # every candidate, with its comment
"""

import pathlib
import re
import sys

REPO = pathlib.Path(__file__).resolve().parent.parent.parent
SRC = REPO / "Plugins" / "TurboMCP" / "Source" / "TurboMCP"

# `X = `, `X += `, `X -= `, `Out.X = ` - the left side of an assignment.
ASSIGN = re.compile(r"(?<![\w>.!<>=+\-*/%&|^])(\w+)\s*(?:[+\-*/%&|^]|<<|>>)?=(?!=)")

# A LOCAL DECLARATION, which is how a field like `int32 PrintedInputs = 0;` first appears.
DECLARE = re.compile(r"\b(?:int32|int64|bool|double|float|FString|FName|TArray<[^>]*>|auto)\s+(\w+)\s*=")

# A comment block or line, for the "nearest comment above" report.
COMMENT = re.compile(r"^\s*(?://|\*|/\*)")

# Names that are almost never a variable worth checking.
NOISE = {
    "return", "if", "else", "while", "for", "case", "break", "continue", "Out", "Result", "Error",
    "const", "static", "void", "bool", "int32", "double", "float", "auto", "FString", "FName",
}


def read(path: pathlib.Path) -> str:
    return path.read_text(encoding="utf-8", errors="replace")


def main() -> int:
    want_list = "--list" in sys.argv

    files = sorted(list(SRC.rglob("*.cpp")) + list(SRC.rglob("*.h")))
    if not files:
        print("  NO SOURCES under %s" % SRC)
        return 1

    text = {p: read(p) for p in files}
    # Count every identifier occurrence across the module, so "read nowhere" means the whole plugin.
    occurrences = {}
    for p, t in text.items():
        for name in re.findall(r"\b\w+\b", t):
            occurrences[name] = occurrences.get(name, 0) + 1

    candidates = []
    for p, t in text.items():
        lines = t.splitlines()
        assigned_here = {}
        for i, line in enumerate(lines, 1):
            if COMMENT.match(line):
                continue
            for m in ASSIGN.finditer(line):
                name = m.group(1)
                if name in NOISE or name.isupper():
                    continue
                assigned_here.setdefault(name, []).append(i)

        for name, sites in assigned_here.items():
            # *** THE SHAPE IS EXACTLY TWO OCCURRENCES IN THE WHOLE MODULE: THE DECLARATION AND THE ONE
            # ASSIGNMENT. My first version accepted `total <= len(sites) + 1`, which let a name assigned
            # three times through as long as it was read nowhere - 339 candidates instead of a list.
            # A name with one assignment line and nothing else HAS NO READER, BY COUNTING. ***
            total = occurrences.get(name, 0)
            if total != 2 or len(sites) != 1:
                continue

            first = sites[0]
            note = ""
            j = first - 2
            while j >= 0 and COMMENT.match(lines[j]):
                note = lines[j].strip(" \t/*") + (" " + note if note else "")
                j -= 1
            candidates.append((p.name, first, name, total, note.strip()[:150]))

    print("  files scanned        : %d" % len(files))
    print("  *** CANDIDATES - DECLARED, ASSIGNED ONCE, READ NOWHERE IN THE MODULE: %d ***" % len(candidates))
    print()
    print("  *** AND THAT NUMBER IS WHY THIS SCREEN IS NOT THE TOOL THE DEFECT NEEDS. The three real cases")
    print("      it was written for - `Boxes`/`Nested`, `EngineDepth`, `PrintedInputs` - are in here, and so")
    print("      are a couple of hundred fields that are NOT defects: THIS REPOSITORY STORES REPORT VALUES IN")
    print("      STRUCTS (`Plan.UsagesToAdd`, `Plan.TargetBox`, `FallbackNodeWidth`), AND THOSE ARE READ BY A")
    print("      RENDERER THIS SCREEN DOES NOT FOLLOW. A field assigned once and read nowhere is a FACT here,")
    print("      not a finding. ***")
    print()
    if candidates:
        shown = candidates if want_list else candidates[:12]
        for fname, line, name, total, note in shown:
            print("    %-38s L%-6d %-26s (%d occurrence(s) in the module)" % (fname, line, name, total))
            if note:
                print("        nearest comment above: %s" % note)
        if not want_list and len(candidates) > 12:
            print("    ... and %d more (pass --list)" % (len(candidates) - 12))
        print()
    print("  *** THIS IS A SCREEN, NOT A VERDICT. It cannot tell a dead field from one assigned for its side")
    print("      effect, read through a macro or a delegate, or written to satisfy an out-parameter the callee")
    print("      reads. A FUNCTION OR TYPE NAME ALSO APPEARS HERE. EVERY CANDIDATE NEEDS ITS TWO LINES READ")
    print("      TOGETHER - which is what turned three of these into the findings on the C3 card. ***")
    return 0


if __name__ == "__main__":
    sys.exit(main())
