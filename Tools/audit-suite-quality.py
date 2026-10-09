"""Audit whether each suite can actually FAIL, without running anything.

WHY THIS EXISTS
---------------
This project's most-recorded failure is A CHECK THAT PASSES WHILE THE THING IT VERIFIES IS BROKEN. It has
happened at least eight times, and every time the fix was to make the check able to fail and then SHOW it
failing. That is a property of the CODE of the check, not of a run, so it can be partly audited statically.

WHAT THIS LOOKS FOR, and each is a way a suite can be unable to fail:

  NO-ASSERT    a suite that calls tools but never appends to its failure list. It can pass on anything;
  ALWAYS-TRUE  a comparison against a literal that is trivially true, such as `x == x` or `0 == 0`;
  ALWAYS-PASS  a `check(...)` whose condition is a constant rather than an expression over a response;
  NO-PARSE     a suite that asserts on text it never PARSED out of a response, which usually means it is
               matching the harness's own echo rather than the tool's answer;
  TAUTOLOGY    a check that asserts a variable is truthy immediately after assigning it a truthy constant.

IT IS DELIBERATELY CONSERVATIVE. A static reader cannot prove a check meaningful, and a false positive here
would be worse than no audit, because it would teach a reader to ignore the output. So it reports
CANDIDATES with the line, and says plainly that each needs a human eye.

WHAT IT CANNOT SEE, stated rather than implied: whether the ASSERTION is the right one. A suite can parse a
value, compare it, and still assert something that does not matter. Only a run against a broken subject
settles that.
"""

import os
import re
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TOOLS = os.path.join(REPO, "Tools")
SKIP = {"tmcp.py", "check-includes.py", "check-mcp-server.py", "test-helper.py",
        "audit-tool-coverage.py", "audit-suite-quality.py"}


def strip_strings(line):
    line = re.sub(r'""".*?"""', '""', line)
    line = re.sub(r'"[^"]*"', '""', line)
    line = re.sub(r"'[^']*'", "''", line)
    return line


def audit(path):
    name = os.path.basename(path)
    with open(path, "r", encoding="utf-8", errors="replace") as handle:
        lines = handle.readlines()
    text = "".join(lines)

    findings = []

    # 1. Does it have a failure list at all?
    has_failures = re.search(r"failures\s*=\s*\[", text) is not None
    if not has_failures:
        findings.append(("NO-ASSERT", 0,
                         "no `failures = []` list, so nothing can be recorded as a failure"))

    # 2. Does anything ever go into it?
    appends = len(re.findall(r"failures\.append\(", text))
    # A helper named `check` that appends counts, and most suites use one.
    helper_appends = len(re.findall(r"def check\(.*?\n(?:.*?\n)*?.*?failures\.append", text))
    if has_failures and appends == 0 and helper_appends == 0:
        findings.append(("NO-ASSERT", 0, "a failures list exists but NOTHING is ever appended to it"))

    # 3. Constant comparisons that cannot fail.
    for index, raw in enumerate(lines, start=1):
        line = strip_strings(raw)
        for match in re.finditer(r"(\w+)\s*==\s*(\w+)\s*$", line):
            left, right = match.group(1), match.group(2)
            if left == right:
                findings.append(("ALWAYS-TRUE", index, "%s == %s compares a value with itself" % (left, right)))
        if re.search(r"check\(\s*(True|False|1|0)\s*,", line):
            findings.append(("ALWAYS-PASS", index, "a check whose condition is a constant"))

    # 4. Does it parse anything out of a response?
    #
    # THE RULE IS NARROW ON PURPOSE, and the first version was too broad. It reported sixteen suites for
    # "no regex", which is a FALSE POSITIVE: a substring test is the CORRECT tool for a NAME, a LABEL or a
    # REFUSAL MESSAGE, because those are exact strings a reader is meant to recognise. The project's
    # recorded lesson is narrower than "always parse": PARSE A VALUE, never substring-match it BESIDE THE
    # VALUE. So what this looks for is a suite that ASSERTS ON A NUMBER without extracting it, which is
    # the shape that lets `7` in a timestamp satisfy a check about a count.
    parses = len(re.findall(r"re\.(?:search|findall|match)\(", text))
    checks = len(re.findall(r"\bcheck\(", text))
    # A check whose condition compares to a NUMERIC LITERAL, with no regex anywhere in the file, is the
    # candidate: the number could be matching anything on the line.
    numeric_checks = 0
    for raw in lines:
        line = strip_strings(raw)
        if "check(" in line and re.search(r"==\s*\d+|\d+\s*==|in\s+str\(", line):
            numeric_checks += 1
    if checks >= 3 and parses == 0 and numeric_checks > 0:
        findings.append(("NUMBER-NOT-PARSED", 0,
                         "%d numeric assertion(s) and no regex: a number matched as a substring can be "
                         "satisfied by a number beside it, such as a count in a node id"
                         % numeric_checks))

    # 5. A value assigned a constant and then asserted truthy in the next few lines.
    for index in range(len(lines) - 3):
        window = "".join(lines[index:index + 4])
        stripped = strip_strings(window)
        assignment = re.search(r"(\w+)\s*=\s*\[?[\"']?[\w.]+[\"']?\]?\s*\n\s*.*?check\([^,]+,\s*(\w+)", stripped)
        if assignment and assignment.group(1) == assignment.group(2):
            findings.append(("TAUTOLOGY", index + 1,
                             "%s is asserted after being assigned in the same block" % assignment.group(1)))

    return findings


def main():
    suites = sorted(f for f in os.listdir(TOOLS)
                    if f.endswith(".py") and f not in SKIP and f.startswith(("test-", "check-")))
    print("=== %d suite(s) audited for checks that CANNOT fail ===" % len(suites))
    print()

    total = 0
    by_kind = {}
    for entry in suites:
        found = audit(os.path.join(TOOLS, entry))
        if not found:
            print("  ok   %-28s no candidate found" % entry)
            continue
        for kind, line, why in found:
            total += 1
            by_kind[kind] = by_kind.get(kind, 0) + 1
            where = ":L%d" % line if line else ""
            print("  WARN %-28s %-11s %s%s" % (entry, kind, why, where))

    print()
    print("=== summary ===")
    for kind, count in sorted(by_kind.items(), key=lambda kv: -kv[1]):
        print("    %-12s %d" % (kind, count))
    print("    %d candidate(s) in %d suite(s)" % (total, len(suites)))
    print()
    print("A CANDIDATE IS NOT A DEFECT. A static reader cannot prove a check is meaningful, and a false")
    print("positive would teach a reader to ignore this output, so each one needs a human eye. What this")
    print("CANNOT see: whether the assertion is the RIGHT one - a suite can parse a value, compare it,")
    print("and still assert something that does not matter. Only a run against a broken subject settles")
    print("that, which is why every suite is expected to have shown its check failing at least once.")
    print()
    # This audit is ADVISORY and does not fail a build: it reports candidates for a human, and a
    # non-zero exit would make it a gate it is not equipped to be.
    print("RESULT: PASS (advisory: %d candidate(s) reported)" % total)
    return 0


if __name__ == "__main__":
    sys.exit(main())
