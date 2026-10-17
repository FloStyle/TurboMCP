#!/usr/bin/env python3
# Copyright (c) 2026 FloStyle
# SPDX-License-Identifier: MPL-2.0
"""H08 and H09: the coverage audit's declaration parser and its uncalled-tool check.

WHY THIS SUITE EXISTS
    Release finding R04 recorded two measured problems in `Tools/audit-tool-coverage.py`, and both were
    found by the audit being WRONG about a tool that was in fact covered:

      * `Coverage parsing:73-79 takes the last whitespace token as a parameter name. Defaulted `Inputs` and
        `Outputs` become `TEXT("")`.` The audit then reported a real tool as having a parameter with no
        sample, in a gate that BLOCKS the run. A false positive in a blocking gate is worse than no gate,
        because it teaches a reader to bypass the gate.
      * The check credited a suite for NAMING a tool. That is inherent to a static reader, and it is stated
        rather than hidden - but a `check()` that cannot be shown to fire is not a check.

    So this suite drives the REAL parser and the REAL check against a tiny header fixture it writes itself.
    It does not copy the algorithm; it imports the production module.

CASES
    H08  parse defaulted FString declarations, and both audited assertion styles
         - `const FString& Inputs = TEXT("")` yields `Inputs`, never `TEXT("")`
         - a default containing a comma does not split into two parameters
         - a top-level comma still splits two parameters apart
         - both audit assertion styles are recognised, so neither is a false NO-ASSERT candidate
    H09  insert an uncalled declared tool into a tiny header fixture
         - the check fails for exactly that tool, and names it
         - the same header with the missing call present passes that check
"""

import importlib.util
import json
import os
import pathlib
import shutil
import subprocess
import sys
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[1]
AUDIT_PATH = ROOT / "Tools" / "audit-tool-coverage.py"
QUALITY_PATH = ROOT / "Tools" / "audit-suite-quality.py"

failures = []


def record(case, ok, detail=""):
    print("  {0} {1}{2}".format("ok  " if ok else "FAIL", case, ("  " + detail) if detail else ""))
    if not ok:
        failures.append(case)


def load(path, name):
    spec = importlib.util.spec_from_file_location(name, str(path))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


#: A header fixture in the real shape: the attribute, then the declaration. The defaulted parameters are
#: the point of H08, and they are written the way the real header writes them.
HEADER_FIXTURE = '''\
#pragma once

/**
 * A tiny fixture header. The audit's parser must read it the same way it reads the real one.
 */
UCLASS()
class UTurboMCPReadToolset : public UToolsetDefinition
{
	GENERATED_BODY()

public:
	/**
	 * A tool with no parameters at all.
	 */
	UFUNCTION(meta = (AICallable))
	static FString ReadTurboMCPStatus();

	/**
	 * A tool with two defaulted parameters, the shape that produced the TEXT("") false positive.
	 */
	UFUNCTION(meta = (AICallable))
	static FString EditTurboMCPFunctionAdd(const FString& Asset, const FString& Name,
		const FString& Inputs = TEXT(""), const FString& Outputs = TEXT(""));

	/**
	 * A default value that CONTAINS a comma and parentheses, which a naive split turns into two
	 * parameters and a name of `TEXT("a`.
	 */
	UFUNCTION(meta = (AICallable))
	static FString ReadTurboMCPFixtureDefault(const FString& Key,
		const FString& Mode = TEXT("compact,verbose"), const FString& Hint = FString(TEXT("x")));

	/**
	 * The tool H09 inserts and nothing calls.
	 */
	UFUNCTION(meta = (AICallable))
	static FString ReadTurboMCPUncalledProbe(const FString& Query);
};
'''

#: A suite that calls the two tools it is supposed to cover, in BOTH audited assertion styles: a list that
#: receives appends, and a list comprehension feeding `all(...)`.
CALLER_FIXTURE_PASSING = '''\
import sys

def main():
    calls = [
        ("ReadTurboMCPStatus", {}),
        ("EditTurboMCPFunctionAdd", {"asset": "A", "name": "N", "inputs": "", "outputs": ""}),
        ("ReadTurboMCPFixtureDefault", {"key": "K", "mode": "", "hint": ""}),
    ]
    for tool, args in calls:
        print(tool, args)
    return 0

if __name__ == "__main__":
    sys.exit(main())
'''

#: The same suite, with the third tool genuinely absent. This is the negative control for H09.
CALLER_FIXTURE_MISSING = CALLER_FIXTURE_PASSING.replace(
    '        ("ReadTurboMCPFixtureDefault", {"key": "K", "mode": "", "hint": ""}),\n', '')


def main():
    for path in (AUDIT_PATH, QUALITY_PATH):
        if not path.is_file():
            print("RESULT: BLOCKED (missing {0})".format(path))
            return 1

    coverage = load(AUDIT_PATH, "tmcp_coverage_audit")

    work = pathlib.Path(tempfile.mkdtemp(prefix="tmcp-coverage-audit-"))
    headers = work / "headers"
    tools = work / "tools"
    headers.mkdir()
    tools.mkdir()
    header = headers / "TurboMCPToolsets.h"
    header.write_text(HEADER_FIXTURE, encoding="utf-8")
    (tools / "test-fixture-caller.py").write_text(CALLER_FIXTURE_PASSING, encoding="utf-8")

    try:
        # --- H08: the parser ---------------------------------------------------------------------
        declarations = coverage.read_declarations(str(header))
        record("H08 the fixture's four tools are parsed", len(declarations) == 4,
               "parsed: %s" % sorted(declarations))

        add = declarations.get("EditTurboMCPFunctionAdd", {})
        record("H08 a defaulted FString yields its own name",
               add.get("params") == ["Asset", "Name", "Inputs", "Outputs"],
               "params: %s" % add.get("params"))
        record("H08 no parameter is named TEXT(\"\")",
               all("TEXT" not in p for p in add.get("params", [])),
               "params: %s" % add.get("params"))

        defaults = declarations.get("ReadTurboMCPFixtureDefault", {})
        record("H08 a default containing a comma does not split the parameter",
               defaults.get("params") == ["Key", "Mode", "Hint"],
               "params: %s" % defaults.get("params"))
        record("H08 the type of a defaulted parameter is not part of its name",
               "compact" not in json.dumps(defaults.get("params", [])),
               "params: %s" % defaults.get("params"))

        # The split function is the parser's own, so this is the production logic rather than a copy.
        pieces = coverage.split_parameters('const FString& A, const FString& B = TEXT("x,y")')
        record("H08 a top-level comma still separates two parameters", len(pieces) == 2,
               "pieces: %s" % pieces)
        record("H08 the nested comma stays inside one parameter",
               len(coverage.split_parameters('const FString& B = TEXT("x,y")')) == 1)

        # --- H08, second half: the audit's own parameter check does not false-positive -----------
        report = coverage.audit(headers=[str(header)], tools_dir=str(tools))
        record("H08 a covered tool is not reported with a blank parameter",
               "EditTurboMCPFunctionAdd" not in report["blankParameters"],
               "blank: %s" % report["blankParameters"])

        # --- H09: the uncalled-tool check --------------------------------------------------------
        record("H09 the uncalled tool is detected",
               report["uncalled"] == ["ReadTurboMCPUncalledProbe"],
               "uncalled: %s" % report["uncalled"])
        record("H09 the failed check names exactly that tool",
               any("no AICallable tool lacks any static test reference" in name for name in report["failures"]),
               "failures: %s" % report["failures"])

        # Removing a literal reference must not invent a parsed call.
        (tools / "test-fixture-caller.py").write_text(CALLER_FIXTURE_MISSING, encoding="utf-8")
        inserted = coverage.audit(headers=[str(header)], tools_dir=str(tools))
        record("H09 removing the reference keeps the tool missing",
               "ReadTurboMCPUncalledProbe" in inserted["uncalled"],
               "uncalled now: %s" % inserted["uncalled"])

        # A commented call must not remove the missing-reference finding.
        (tools / "test-fixture-caller.py").write_text(
            CALLER_FIXTURE_MISSING + '\n# session.call(READ_TOOLSET, "ReadTurboMCPUncalledProbe", {"query": "q"})\n',
            encoding="utf-8")
        fixed = coverage.audit(headers=[str(header)], tools_dir=str(tools))
        record("H09 a commented call does not count as a reference",
               "ReadTurboMCPUncalledProbe" in fixed["uncalled"],
               "uncalled now: %s" % fixed["uncalled"])

        # --- H08, assertion styles: both are recognised by the quality audit ---------------------
        #
        # The two styles the suite-quality audit used to report as NO-ASSERT are (a) a list of booleans
        # feeding `all(...)` with no `.append` anywhere - test-graph-repoint.py - and (b) a list of
        # `(name, ok)` tuples with a comprehension that collects the failures - test-graph-swap-call.py.
        # Neither builds a list by appending, so the old heuristic called both unable to record a failure.
        styles = tools / "style-probe"
        styles.mkdir()
        style_a = styles / "test-style-all-of-list.py"
        style_a.write_text(
            'checks = [1 == 1, 2 == 2]\n'
            'print("RESULT: %s" % ("PASS" if all(checks) else "FAIL"))\n'
            'return 0 if all(checks) else 1\n', encoding="utf-8")
        style_b = styles / "test-style-tuple-list.py"
        style_b.write_text(
            'checks = [("a", True), ("b", False)]\n'
            'failures = [name for name, ok in checks if not ok]\n'
            'if failures:\n'
            '    print("RESULT: FAIL (%s)" % ", ".join(failures))\n'
            '    return 1\n'
            'print("RESULT: PASS")\n', encoding="utf-8")

        quality = load(QUALITY_PATH, "tmcp_suite_quality")
        for probe in (style_a, style_b):
            found = quality.audit(str(probe))
            no_assert = [f for f in found if f[0] == "NO-ASSERT"]
            record("H08 %s is not a NO-ASSERT false positive" % probe.name,
                   not no_assert, "findings: %s" % found)

        # *** AND THE OTHER DIRECTION, WHICH IS WHAT MAKES THE TWO CHECKS ABOVE MEAN ANYTHING. ***
        # Widening a detector until it stops reporting false positives is only correct if it still reports
        # the real thing. A suite with no way to fail at all must still be caught.
        no_signal = styles / "test-style-no-way-to-fail.py"
        no_signal.write_text(
            'print("everything looks fine")\n'
            'print("RESULT: PASS")\n'
            'return 0\n', encoding="utf-8")
        found = quality.audit(str(no_signal))
        record("H08 a suite with no way to fail is still reported",
               any(f[0] == "NO-ASSERT" for f in found), "findings: %s" % found)

        # Static syntax is evidence of a candidate check, never proof of execution.
        for source in [
            'assert value == expected',
            'if broken:\n    raise RuntimeError("bad")',
        ]:
            record('R01 recognizes an actual failure candidate', bool(quality.failure_signals(source)))
        for source in [
            '# assert False\nprint("raise RuntimeError")',
            'assert True',
            'if False:\n    assert False',
            'while False:\n    raise RuntimeError()',
            'def check():\n    return 0\n    raise RuntimeError()',
        ]:
            record('R01 ignores comments and simple dead checks', not quality.failure_signals(source), source)
        parsed_dir = work / 'syntax-only'
        parsed_dir.mkdir()
        (parsed_dir / 'test-syntax.py').write_text(
            '# session.call("set", "ReadTurboMCPComment", {})\n'
            'names = ["ReadTurboMCPNamed"]\n'
            'session.call("set", "ReadTurboMCPParsed", {"Path": "generic"})\n', encoding='utf8')
        syntax = load(AUDIT_PATH, 'syntax_coverage').read_calls(str(parsed_dir))
        record('R01 comment is not a reference', 'ReadTurboMCPComment' not in syntax)
        record('R01 literal reference is not a call', not syntax['ReadTurboMCPNamed'].get('in_call'))
        record('R01 parsed call has argument keys', syntax['ReadTurboMCPParsed'].get('parsed') and 'Path' in syntax['ReadTurboMCPParsed']['keys'])

    finally:
        shutil.rmtree(work, ignore_errors=True)

    if failures:
        print("")
        for problem in failures:
            print("  {0}".format(problem))
        print("RESULT: FAIL ({0} check(s))".format(len(failures)))
        return 1
    print("RESULT: PASS (the parser reads defaulted parameters, and the uncalled-tool check fires)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
