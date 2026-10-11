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

    # 6. *** A REGEX THAT CANNOT MATCH THE TEXT IT IS GIVEN. ***
    #
    # THIS IS THE MIRROR OF EVERY OTHER CHECK IN THIS FILE. The five above look for assertions that cannot
    # FAIL; this one looks for an assertion that cannot PASS, and it is here because row 2.2 found one the
    # hard way: `re.search(r"^size: (\d+)x", out)` WITHOUT `re.MULTILINE` matched only the start of the
    # WHOLE string, so the size check could never pass and 16 of 18 failures came from one function.
    #
    # A check that cannot pass is as useless as one that cannot fail, and this project had spent five
    # sections on the second and never once on the first.
    #
    # THE RULE IS NARROW ON PURPOSE, because a broad version is a false-positive machine. It fires ONLY on:
    #   * an anchored pattern - one containing `^`
    #   * used WITHOUT `re.MULTILINE`
    #   * on a call whose SUBJECT is a variable that the suite has already built by joining or splitting
    #     lines, or has read from a file - i.e. a value that is very likely to contain newlines.
    # A single-line subject with an anchor is CORRECT usage and is not reported.
    # *** THE ASSIGNMENT IS RECOGNISED BY THE CALL THAT BUILDS IT, NOT BY THE DELIMITER. ***
    # The first version tried to spell the join delimiter and got it wrong: the source says `"\\n".join(` (an
    # ESCAPED backslash inside a string literal in the suite's own source), and the alternative written for it
    # matched a LITERAL newline instead - so the name was never collected, and the check below could not fire
    # even when its other two conditions were satisfied. Any `x = ...join(` / `.splitlines()` / `.read()` /
    # `.readlines()` produces a value built from LINES, and that is the only thing this needs to know.
    multiline_hint = re.compile(
        r"(\w+)\s*=\s*[^\n]*?\.(?:join|splitlines|read|readlines)\(")
    multiline_names = set(multiline_hint.findall(text))

    # *** AND A CALL IS A LOGICAL STATEMENT, NOT A LINE. ***
    # The second version ran the detection per line and produced TWO FALSE POSITIVES on the first real corpus
    # it saw, both in `check-plugin.py`:
    #   * `re.finditer(r"^FString...", handle.read(),` / `re.MULTILINE):` - THE FLAG IS ON THE NEXT LINE, so a
    #     per-line search cannot see it and reported a correct call as broken.
    #   * a pattern containing `\)` - the closing-delimiter search stopped at the ESCAPED parenthesis, took the
    #     wrong closing quote, and captured the tail of the line as if it were the pattern.
    # BOTH ARE THE DEFECT THIS CHECK EXISTS TO FIND, COMMITTED BY THE CHECK. So statements are joined by
    # parenthesis balance first, and the pattern's closing quote is the first quote NOT preceded by a backslash.
    statements = []
    buffer_text = ""
    depth = 0
    start_line = 1
    for index, raw in enumerate(lines, start=1):
        if not buffer_text:
            start_line = index
        buffer_text += raw
        scan = strip_strings(raw)
        depth += scan.count("(") - scan.count(")")
        if depth <= 0:
            statements.append((start_line, buffer_text))
            buffer_text = ""
            depth = 0
    if buffer_text.strip():
        statements.append((start_line, buffer_text))

    for start_line, statement in statements:
        if "re." not in statement:
            continue
        for match in re.finditer(r"re\.(?:search|match|findall|finditer|fullmatch)\(\s*r?([\"'])((?:\\.|(?!\1).)*)\1",
                                 statement, re.DOTALL):
            pattern = match.group(2)
            # *** A `^` INSIDE A CHARACTER CLASS IS A NEGATION, NOT AN ANCHOR. ***
            # The third version of this check reported `UFUNCTION\s*\([^)]*\)` as anchored because of the `^`
            # in `[^)]`. That is a NEGATED CLASS and the pattern matches anywhere - so the check accused a
            # perfectly good call of a defect it did not have. THE CHECK WAS MATCHING SOMETHING OTHER THAN
            # WHAT IT MEANT TO, WHICH IS THE EXACT DISEASE IT EXISTS TO FIND, COMMITTED BY THE CHECK ITSELF
            # FOR THE FOURTH TIME. Character classes are removed before the anchor test.
            without_classes = re.sub(r"\[(?:\\.|[^\]\\])*\]", "", pattern)
            if "^" not in without_classes:
                continue
            if "re.MULTILINE" in statement or "re.M" in statement:
                continue
            tail = statement[match.end():]
            subject = re.search(r"\s*,\s*([A-Za-z_]\w*)", tail)
            if subject and subject.group(1) in multiline_names:
                findings.append(("CANNOT-MATCH", start_line,
                                 "pattern r'%s' is anchored with ^ but its statement has no re.MULTILINE, and "
                                 "its subject `%s` is built from LINES: ^ will match only the start of the whole "
                                 "string, so this check can never pass on a later line"
                                 % (pattern, subject.group(1))))

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
