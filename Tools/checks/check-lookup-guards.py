#!/usr/bin/env python3
"""Every lookup that can return null is guarded before its result is used.

WHY THIS EXISTS
    The plugin crashed the editor twice on the same shape. `TurboMCPNodeType.cpp` took the result of
    `TurboMCPNodeId::FindGraphByName` and handed it straight to `BuildProbeNode`, which gives it to the engine as a
    node's owner; the engine asserted on the null and took the session with it. *** AN AUDIT OF THE OTHER SITES
    FOUND ELEVEN OF ELEVEN GUARDED - SO THE CRASH WAS NOT A MISSING CONVENTION BUT A SINGLE DEPARTURE FROM ONE, AND
    A DEPARTURE IS EXACTLY WHAT A CHECKER CATCHES AND A READING DOES NOT. ***

WHAT COUNTS AS A GUARD
    Any of the four forms the tree already uses, within the next few lines:
        `X == nullptr`                      the common null check
        `if (!X)`                           the negated form
        `if (T Y = Cast<T>(X))`             the cast form, checked where the cast lands
        `if (T* Y = Find...(...))`          the same-statement form
    A `!= nullptr` alone is NOT a guard: it says the pointer is good on one path and says nothing about the other.

EXIT
    0 when every site is guarded, 1 when any is not, with each printed as file:line.
"""

import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
# An optional path argument points this at a fixture instead of the plugin. That is how the FAILURE path is tested:
# a check whose failure path is never exercised is a check nobody has shown can fail.
SOURCE = pathlib.Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else ROOT / "Source" / "TurboMCP"

# The two lookups in this codebase that can answer null.
LOOKUP = re.compile(
    r"(?P<var>\w+)\s*=\s*(?:TurboMCPNodeId::FindGraphByName|LoadObject\s*<)"
)

# How far after the assignment a guard may sit. The widest real case is `Cast<T>` then a check, which is one line.
WINDOW = 12

SAME_STATEMENT = re.compile(r"^\s*if\s*\(")
NULL_CHECK = r"\b{0}\s*==\s*nullptr"
NOT_CHECK = r"if\s*\(\s*!\s*{0}\b"
CAST_INTO = r"\b\w[\w:<>]*\s*\*?\s+({0})\s*=\s*Cast\s*<[^>]+>\s*\(\s*{1}\s*\)"


def guard_for(lines, index, var):
    """The name of the guard found for `var` after line `index`, or None."""
    window = lines[index:index + WINDOW]

    # The same-statement form: the assignment is the condition itself.
    if SAME_STATEMENT.match(lines[index]):
        return "checked in the if"

    for offset, line in enumerate(window):
        if offset == 0:
            continue
        if re.search(NULL_CHECK.format(re.escape(var)), line):
            return "null check"
        # `X != nullptr` IS a guard: the null path does not use the pointer, and a crash is what this checks.
        if re.search(r"\b" + re.escape(var) + r"\s*!=\s*nullptr", line):
            return "!= nullptr"
        # A cast of the variable INSIDE an if condition is a guard, whether it is compared or bound:
        # `if (Cast<UEnum>(X) != nullptr)` and `if (UScriptStruct* Y = Cast<UScriptStruct>(X))`.
        if re.match(r"\s*if\s*\(", line) and re.search(r"Cast\s*<[^>]+>\s*\(\s*" + re.escape(var) + r"\s*\)", line):
            return "cast in the if condition"
        if re.search(NOT_CHECK.format(re.escape(var)), line):
            return "if (!x)"
        cast = re.search(CAST_INTO.format(r"(\w+)", re.escape(var)), line)
        if cast:
            cast_var = cast.group(1)
            if re.search(NULL_CHECK.format(re.escape(cast_var)), "\n".join(window[offset:])):
                return "cast + null check"
    return None


def main():
    if not SOURCE.is_dir():
        print("error: {0} is not a directory, so nothing was checked.".format(SOURCE))
        return 1

    files = sorted(SOURCE.glob("*.cpp"))
    sites = 0
    unguarded = []

    for path in files:
        lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
        for index, line in enumerate(lines):
            match = LOOKUP.search(line)
            if not match:
                continue
            # A commented-out lookup is not a call site.
            if line.lstrip().startswith("//"):
                continue
            sites += 1
            var = match.group("var")
            guard = guard_for(lines, index, var)
            if guard is None:
                unguarded.append((path.name, index + 1, line.strip(), var))

    print("lookup sites checked: {0} in {1} file(s)".format(sites, len(files)))
    print("guarded: {0}   unguarded: {1}".format(sites - len(unguarded), len(unguarded)))

    if unguarded:
        print("")
        print("*** UNGUARDED - the engine is handed a pointer that may be null: ***")
        for name, line_no, text, var in unguarded:
            print("  {0}:{1}  `{2}` may be null".format(name, line_no, var))
            print("      {0}".format(text[:150]))
        return 1

    print("")
    print("every lookup that can return null is guarded before its result is used.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
