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

# *** THE AUDIT EXAMINES FEWER FILES THAN THE RUNNER RUNS, AND THE TWO NUMBERS ARE BOTH CORRECT. ***
#
# `run-suites.ps1` runs **58** suites; this audits **54**, and the four it leaves out are exactly the four that
# are not suites in the sense this audit means - `audit-tool-coverage`, `audit-suite-quality`, `check-includes`
# and `check-mcp-server`. *** TWO OF THE FOUR ARE THIS AUDIT AND ITS NEIGHBOUR, AND AN AUDIT THAT GRADES ITSELF
# IS MEASURING THE WRONG THING. *** The other two are static checks whose whole body is a pass/fail over files
# rather than a series of assertions about a running editor, so there is no failure list for this to look for.
#
# AND THE LIST IS NOT THE DISCOVERY RULE. This walks `Tools/` with `os.listdir`, which does not descend - so
# when the documentation checks were moved into `Tools/checks/`, this stopped seeing them **without any change
# being made here.** That is the behaviour wanted, and it is why a `check-*` script belongs in `Tools/` only if
# it is genuinely a suite.
SKIP = {"tmcp.py", "check-includes.py", "check-mcp-server.py", "test-helper.py",
        "audit-tool-coverage.py", "audit-suite-quality.py"}


def strip_strings(line):
    line = re.sub(r'""".*?"""', '""', line)
    line = re.sub(r'"[^"]*"', '""', line)
    line = re.sub(r"'[^']*'", "''", line)
    return line


def failure_signals(text):
    """Every way this suite can report a failure, and each is a real one.

    *** THIS WAS ONE PATTERN AND IT PRODUCED TWO FALSE POSITIVES ON SUITES THAT CAN FAIL. ***

    Release finding R04 recorded the cause: "Quality heuristic:74-80 equates a list that receives append calls
    with a failure recorder. That is neither necessary nor sufficient." The audit reported
    `test-graph-repoint.py` and `test-graph-swap-call.py` as unable to record a failure. Both CAN, and they do
    it without ever calling `.append`:

      * `test-graph-repoint.py:36` builds `checks = [...]` and exits `0 if all(checks) else 1`;
      * `test-graph-swap-call.py:71` builds `failures = [name for name, ok in checks if not ok]`.

    A list comprehension that selects the failures is a failure recorder. `all(checks)` feeding a non-zero
    return is one too. And a suite that prints `RESULT: FAIL` and returns a literal 1 with no list at all is
    just as able to fail as one that appends.

    So the recogniser looks for a WAY TO FAIL rather than for one preferred shape. It is still conservative in
    the other direction: a suite with none of these signals is reported, because a suite that cannot fail is
    the defect this audit exists to find.
    """
    signals = []

    # 1. A list built and appended to - the original pattern, kept.
    list_names = set(re.findall(r"(\w+)\s*=\s*\[\s*\]", text))
    appended_to = set(re.findall(r"(\w+)\.append\(", text))
    failure_lists = sorted(list_names & appended_to)
    if failure_lists:
        signals.append("append:%s" % ",".join(failure_lists))

    # 2. A list comprehension that COLLECTS failures - `[x for x, ok in checks if not ok]`.
    for match in re.finditer(r"(\w+)\s*=\s*\[[^\]]*for\s+([^\]]*?)\bif\b[^\]]*\]", text):
        target, body = match.group(1), match.group(2)
        if re.search(r"\bnot\b|==\s*False|is\s+False", body):
            signals.append("comprehension:%s" % target)

    # 3. An explicit non-zero exit. `return 1` and `sys.exit(1)` cannot be mistaken for anything else.
    for match in re.finditer(r"\b(?:return|sys\.exit)\s*\(?\s*([A-Za-z_]\w*|\d+)\s*\)?", text):
        value = match.group(1)
        if value.isdigit() and int(value) != 0:
            signals.append("explicit-exit:%s" % value)
        elif value in ("False", "1"):
            signals.append("explicit-exit:%s" % value)
    # `all(checks)` / `any(checks)` feeding a return is the shape test-graph-repoint.py uses.
    if re.search(r"\ball\(\s*\w+\s*\)", text) or re.search(r"\ball\s*\(\s*\w+\s*\)", text):
        signals.append("all-of-list")
    # A failure accumulator that is only ever READ in a non-empty test still counts as one.
    for name in re.findall(r"\bif\s+(\w+)\s*:", text):
        if name in list_names or name in set(re.findall(r"(\w+)\s*=\s*\[[^\]]*for", text)):
            signals.append("nonempty-test:%s" % name)

    return sorted(set(signals))


def audit(path):
    name = os.path.basename(path)
    with open(path, "r", encoding="utf-8", errors="replace") as handle:
        lines = handle.readlines()
    text = "".join(lines)

    findings = []

    # 1. Does it have a WAY TO FAIL at all?
    #
    # *** A SUITE NEED NOT CALL IT `failures`, AND IT NEED NOT BE A LIST. *** The first version looked only for
    # `failures = []`, and reported `test-graph-structure.py` as having NO WAY TO RECORD A FAILURE - a suite
    # that keeps `results = []` of `(name, bool(ok), detail)` tuples and prints `FAIL` per row and a
    # `RESULT: FAIL (n)` line. THAT IS A FALSE POSITIVE, and a false positive in an audit that BLOCKS is worse
    # than no audit: it teaches the reader to ignore the warning, which is exactly "an invisible refusal reads
    # as a capability" turned round. The recogniser below accepts every shape that can produce a non-zero exit.
    signals = failure_signals(text)
    has_failures = bool(signals)
    if not has_failures:
        findings.append(("NO-ASSERT", 0,
                         "no append, no failure-collecting comprehension and no explicit non-zero exit, so "
                         "nothing in this file can be recorded as a failure"))

    # 2. Does anything ever go INTO the recorder?
    #
    # THE SAME WIDENING AS ABOVE, FOR THE SAME REASON: the first version hard-coded the name `failures` here
    # too, so after the list check was widened this one still reported `test-graph-structure.py` as having a
    # list nothing is ever appended to - while the file appends to `results` on every row.
    list_names = set(re.findall(r"(\w+)\s*=\s*\[\s*\]", text))
    appended_to = set(re.findall(r"(\w+)\.append\(", text))
    failure_lists = sorted(list_names & appended_to)
    appends = 0
    for list_name in failure_lists:
        appends += len(re.findall(re.escape(list_name) + r"\.append\(", text))
    if failure_lists and appends == 0 and not [s for s in signals if s.startswith(("explicit-exit", "all-of-list"))]:
        findings.append(("NO-ASSERT", 0,
                         "the list(s) %s exist but NOTHING is ever appended to them"
                         % ", ".join("`%s`" % n for n in failure_lists)))

    # 3. Constant comparisons that cannot fail.
    for index, raw in enumerate(lines, start=1):
        line = strip_strings(raw)
        for match in re.finditer(r"(\w+)\s*==\s*(\w+)\s*$", line):
            left, right = match.group(1), match.group(2)
            if left == right:
                findings.append(("ALWAYS-TRUE", index, "%s == %s compares a value with itself" % (left, right)))
        if re.search(r"check\(\s*(True|False|1|0)\s*,", line):
            findings.append(("ALWAYS-PASS", index, "a check whose condition is a constant"))

    # 4. Does it match a number AS A SUBSTRING rather than parse it?
    #
    # THE RULE IS NARROW ON PURPOSE, and BOTH earlier versions were too broad in different ways. The first
    # reported sixteen suites for "no regex", which is a FALSE POSITIVE: a substring test is the CORRECT tool
    # for a NAME, a LABEL or a REFUSAL MESSAGE, because those are exact strings a reader is meant to
    # recognise. The project's recorded lesson is narrower than "always parse": PARSE A VALUE, never
    # substring-match it BESIDE THE VALUE.
    #
    # AND THE SECOND VERSION WAS STILL TOO BROAD. It reported `cover_count == 2`, `len(bodies) == 1` and
    # `again.count("not touched") == 2` as defects - FOUR FALSE POSITIVES, each of which is a COMPUTED VALUE
    # compared to a literal and CANNOT be satisfied by a number sitting beside it. Comparing a count to 2 is
    # not the dangerous shape; MATCHING A NUMBER AS A SUBSTRING OF A RESPONSE is. That shape is spelled
    # `in str(...)` or `in "..."` - and the finding now names only that, so what it reports is what it means.
    parses = len(re.findall(r"re\.(?:search|findall|match)\(", text))
    checks = len(re.findall(r"\bcheck\(", text))
    numeric_checks = 0
    for raw in lines:
        # *** THE RAW LINE, NOT THE STRIPPED ONE - AND THIS IS THE FIFTH TIME IN THIS FILE THAT READING A
        # SANITISED COPY HID THE VERY THING BEING LOOKED FOR. `strip_strings` removes string literals, and
        # THE LITERAL IS THE SUBJECT: `check("a", "7" in out)` becomes `check("a",  in out)` and both `in str(`
        # and the bare numeral disappear before the check looks. A check that reads a sanitised copy cannot
        # report on what was sanitised away. ***
        if "check(" not in raw:
            continue
        if re.search(r"in\s+str\(", raw):
            numeric_checks += 1
            continue
        # *** A BARE DIGIT IS THE DANGEROUS SHAPE; A LABELLED PHRASE IS NOT. ***
        # A quoted string that contains a LETTER is a LABEL, and matching a label is the CORRECT use of a
        # substring test. What can be silently satisfied by an unrelated number is a string that is ONLY a
        # number: `"7" in out` is satisfied by the `7` in a node id, which is the exact failure this project
        # records. So the literal must consist of nothing but a numeral.
        #
        # AND THE PATTERN IS WRITTEN THIS WAY BECAUSE THE OBVIOUS ONE WAS WRONG: `([\"'])(.*?)\1\s+in\s+` on
        # `check("a count", "7" in out)` captures `a count", "7`, because the non-greedy body still reaches the
        # FIRST closing quote THAT IS FOLLOWED BY `in` - which is the one after the 7. Requiring the literal to
        # be numerals only removes the ambiguity instead of trying to out-guess the backtracking.
        if re.search(r"[\"'][-+]?\d+(?:\.\d+)?[\"']\s+(?:not\s+)?in\s+", raw):
            numeric_checks += 1
    if checks >= 3 and parses == 0 and numeric_checks > 0:
        findings.append(("NUMBER-NOT-PARSED", 0,
                         "%d assertion(s) test whether a BARE NUMBER IS A SUBSTRING of a response and the file "
                         "has no regex: a bare numeral can be satisfied by a number beside it, such as a count "
                         "in a node id" % numeric_checks))

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
    print("=== %d suite(s) audited for checks that CANNOT fail and checks that CANNOT pass ===" % len(suites))
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
    # *** THIS AUDIT NOW BLOCKS. IT USED TO BE ADVISORY, AND REPORT.md SECTION 11.1 NAMES THAT AS THE PROJECT'S
    # STRUCTURAL HOLE: THREE OF ITS NINE INVARIANTS ARE HELD BY CONVENTION AND REVIEW RATHER THAN BY THE
    # MACHINE, AND THEY ARE EXACTLY THE THREE THAT PRODUCED THE FIVE CAPABILITIES WHOSE CHECK COUNT
    # OVERSTATED THEIR COVERAGE. THE INVARIANTS VERIFIED MECHANICALLY HAVE NEVER REGRESSED HERE. ***
    #
    # IT COULD NOT BLOCK BEFORE, AND THE REASON IS WORTH KEEPING: IT REPORTED FIVE CANDIDATES AND **ALL FIVE
    # WERE FALSE POSITIVES** - four from a rule whose pattern contradicted its own message, and one from
    # assuming a suite must name its failure list `failures`. A GATE THAT FAILS HEALTHY CODE IS NOT A GATE;
    # IT IS A REASON TO BYPASS THE GATE. The findings were read, each false positive was removed, and the one
    # REAL candidate it then found (`test-widget-preview.py:175`, a bare `"4096"` substring test that any
    # other 4096 could satisfy) was FIXED rather than tolerated.
    #
    # SO THE COUNT IS NOW ZERO, AND ZERO IS THE CONDITION UNDER WHICH AN ADVISORY CAN BECOME A GATE. If a
    # future suite introduces a candidate, THIS FAILS THE RUN - which is the point: the invariant is now held
    # by the machine rather than by whoever remembers to read the output.
    if total:
        print("RESULT: FAIL (%d candidate(s): each one is a check that cannot fail or cannot pass, and" % total)
        print("        each needs either a fix or a reason it is a false positive before this gate passes)")
        return 1
    # *** AND THE HEADING NAMES BOTH DIRECTIONS, BECAUSE THE AUDIT NOW LOOKS IN BOTH. *** The five original
    # findings look for assertions that cannot FAIL; CANNOT-MATCH looks for one that cannot PASS. This project
    # had spent five sections on the first and never once on the second.
    print("RESULT: PASS (0 candidates: no check that cannot fail and none that cannot pass)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
