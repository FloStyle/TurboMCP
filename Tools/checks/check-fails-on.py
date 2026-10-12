"""Measure whether each suite names the input that makes its checks FAIL.

WHY THIS EXISTS: `Tools/make-tools-doc.py` EMITS a rule in its worked example for whoever writes the next suite -
*"Every check you write must name an input that makes it fail"* - and a mechanical screen found the phrase in
**9 of 52** suites. `Tools/audit-suite-quality.py` blocks the run on six properties of the check's CODE
(NO-ASSERT, ALWAYS-TRUE, ALWAYS-PASS, NUMBER-NOT-PARSED, TAUTOLOGY, CANNOT-MATCH) and reports 0 candidates.
**The convention is a property of the COMMENT and nothing measures it.**

*** THIS TOOL DOES NOT DECIDE THAT A CHECK CANNOT FAIL. ***  That is the `V1` error: the phrase being absent and
the check being unable to fail are DIFFERENT CLAIMS, and the first is all a comment can tell you. What this
reports is narrower and true:

    A SUITE THAT NAMES A FAILING INPUT   says so, and a reader can check the claim
    A SUITE THAT DOES NOT                may still be sound - nothing here says otherwise

USAGE:   python Tools/checks/check-fails-on.py [--list]
"""

import pathlib
import re
import sys

REPO = pathlib.Path(__file__).resolve().parent.parent.parent
TOOLS = REPO / "Tools"

# The phrasings the project actually uses, found by reading the nine suites that have it.
NAMES_THE_FAILURE = re.compile(r"(?i)(fails? on|fails? when|breaks? when|goes? red|should fail|must fail)")

# A check call is `check("<text>", <condition>)`; the text is what a reader sees.
CHECK_CALL = re.compile(r'check\(\s*"((?:[^"\\]|\\.)*)"', re.MULTILINE)


def main() -> int:
    want_list = "--list" in sys.argv

    suites = sorted(p for p in TOOLS.glob("test-*.py"))
    if not suites:
        print("  NO SUITES FOUND under %s" % TOOLS)
        return 1

    named, silent = [], []
    for path in suites:
        text = path.read_text(encoding="utf-8", errors="replace")
        checks = CHECK_CALL.findall(text)
        (named if NAMES_THE_FAILURE.search(text) else silent).append((path.name, len(checks), text))

    total_checks = sum(n for _, n, _ in named + silent)

    print("  suites: %d        check() calls seen: %d" % (len(suites), total_checks))
    print()
    print("  *** NAMES A FAILING INPUT: %d of %d ***" % (len(named), len(suites)))
    for name, n, _ in named:
        print("      %-40s %3d check(s)" % (name, n))
    print()
    print("  *** DOES NOT: %d of %d ***" % (len(silent), len(suites)))
    if want_list:
        for name, n, _ in silent:
            print("      %-40s %3d check(s)" % (name, n))
    else:
        for name, n, _ in silent[:6]:
            print("      %-40s %3d check(s)" % (name, n))
        if len(silent) > 6:
            print("      ... and %d more (pass --list)" % (len(silent) - 6))
    print()
    print("  *** THIS IS A COMMENT CONVENTION, NOT A VERDICT. A suite in the second list may be sound; the only")
    print("      claim measured is that its checks do not SAY what makes them fail. ***")
    return 0


if __name__ == "__main__":
    sys.exit(main())
