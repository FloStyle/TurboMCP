# Copyright (c) 2026 FloStyle
# SPDX-License-Identifier: MPL-2.0
"""Audit that EVERY MCP tool has a testable call, without needing the editor.

WHY THIS EXISTS
---------------
The requirement is that every MCP feature is testable. A tool whose arguments nobody ever supplies is
not testable, and the dangerous version of that is a tool that REGISTERS and then fails or answers
nothing when called - it looks finished in `R-TOOL-INDEX` and in the panel's list, and no check
contradicts it.

So this audit reads the tool declarations out of the header and the calls out of the test suites, and
reports three things per tool:

  DECLARED   the parameters the tool takes;
  COVERED    whether some suite in Tools/ actually calls it with those parameters;
  BLANK      a parameter that appears in no sample anywhere.

A tool with a BLANK parameter is one a caller has to guess at, which is exactly what the panel's
one-click samples are for, and exactly what this audit is for.

*** THE DECISION IS NOW A FUNCTION OF ITS INPUTS, WHICH IS WHY THE PARSER CAN BE TESTED. ***

Release finding R04 recorded two measured defects in this file:

  1. `Coverage parsing:73-79 takes the last whitespace token as a parameter name. Defaulted `Inputs` and
     `Outputs` become `TEXT("")`.` A parameter with a default value was split on every comma - including
     the comma inside `TEXT("...")` - and its name was read as the last whitespace token, so
     `const FString& Inputs = TEXT("")` produced a parameter literally named `TEXT("")`. The audit then
     reported a real tool as having an unsampled parameter, which is a FALSE POSITIVE, and a false
     positive in a blocking gate teaches a reader to bypass the gate.

  2. The checks credited a suite for NAMING a tool without executing it. That part is inherent to a static
     audit and is now stated rather than hidden: this audit proves a tool has a caller and a sample, and it
     proves nothing about behaviour. Behaviour is the acceptance catalog's job.

Everything below is reachable as a function so `Tools/test-coverage-audit.py` can drive the real parser
against a tiny header fixture instead of against a copy of it.
"""

import os
import re
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
HEADER = os.path.join(REPO, "Source", "TurboMCP", "Public", "Tools", "TurboMCPToolsets.h")
TOOLS_DIR = os.path.join(REPO, "Tools")

# Suites that are not acceptance tests of a tool, so a tool named only there is not covered.
NON_ACCEPTANCE = {"tmcp.py", "check-includes.py", "check-mcp-server.py", "test-helper.py",
                  "make-acceptance-asset.ps1", "start-editor.ps1", "build-test-graph.py",
                  "make-test-blueprint.py"}


def split_parameters(params_text):
    """Split a C++ parameter list on TOP-LEVEL commas only.

    A default value such as `= TEXT("a,b")` or `= FString(TEXT("x"))` contains commas and parentheses that
    do not separate parameters. Splitting naively on `,` is what produced the `TEXT("")` parameter name.
    """
    pieces = []
    depth = 0
    current = ""
    in_string = False
    quote = ""
    index = 0
    while index < len(params_text):
        char = params_text[index]
        if in_string:
            current += char
            if char == "\\" and index + 1 < len(params_text):
                current += params_text[index + 1]
                index += 2
                continue
            if char == quote:
                in_string = False
            index += 1
            continue
        if char in "\"'":
            in_string = True
            quote = char
            current += char
            index += 1
            continue
        if char in "([{<":
            depth += 1
        elif char in ")]}>":
            depth = max(0, depth - 1)
        if char == "," and depth == 0:
            pieces.append(current)
            current = ""
            index += 1
            continue
        current += char
        index += 1
    if current.strip():
        pieces.append(current)
    return pieces


def parameter_name(piece):
    """The parameter's name, from `const FString& Inputs = TEXT("")` -> `Inputs`.

    The declarator is everything before the default value. The name is the last identifier in it, after any
    pointer or reference markers. A piece with no identifier yields None rather than a fabricated name.
    """
    declarator = piece.split("=", 1)[0].strip()
    declarator = declarator.rstrip()
    if not declarator:
        return None
    match = re.search(r"([A-Za-z_]\w*)\s*[\)\]]?\s*$", declarator)
    if not match:
        return None
    return match.group(1)


def read_declarations(header_path=HEADER):
    """Every AICallable tool: its name, its toolset class, and its parameters."""
    with open(header_path, "r", encoding="utf-8", errors="replace") as handle:
        text = handle.read()

    # The class each declaration sits in, so the toolset is known without a second list.
    classes = [(m.start(), m.group(1)) for m in re.finditer(r"^class (UTurboMCP\w+Toolset)", text,
                                                            re.MULTILINE)]

    tools = {}
    # A declaration is `UFUNCTION(meta = (AICallable))` then a `static FString Name(args);`.
    for match in re.finditer(
            r"UFUNCTION\(meta = \(AICallable\)\)\s*\n\s*static FString (\w+)\(([^;]*?)\);",
            text, re.DOTALL):
        name = match.group(1)
        params_text = match.group(2)
        owner = None
        for start, class_name in classes:
            if start < match.start():
                owner = class_name
            else:
                break
        params = []
        for piece in split_parameters(params_text):
            if not piece.strip():
                continue
            parsed = parameter_name(piece)
            if parsed:
                params.append(parsed)
        tools[name] = {"owner": owner, "params": params}
    return tools


def read_calls(tools_dir=TOOLS_DIR):
    """Every tool name and the parameter keys used in any suite, as one blob per tool."""
    calls = {}
    for entry in sorted(os.listdir(tools_dir)):
        if not entry.endswith(".py") or entry in NON_ACCEPTANCE:
            continue
        path = os.path.join(tools_dir, entry)
        if not os.path.isfile(path):
            continue
        with open(path, "r", encoding="utf-8", errors="replace") as handle:
            text = handle.read()

        # `session.call(TOOLSET, "ToolName", {...})` - the blob after the name is the arguments.
        for match in re.finditer(
                r'\.call\(\s*[A-Z_]+\s*,\s*"(\w+)"\s*,\s*(\{.*?\})\s*\)', text, re.DOTALL):
            tool = match.group(1)
            blob = match.group(2)
            keys = set(re.findall(r'"(\w+)"\s*:', blob))
            calls.setdefault(tool, {"files": set(), "keys": set()})
            # Recorded so the verdict can tell a real call from a name that was merely mentioned.
            calls[tool]["parsed"] = True
            calls[tool]["files"].add(entry)
            calls[tool]["keys"] |= keys

        # A tool called through a helper that passes named arguments is still covered; the helper's
        # own body is where the keys appear, so the whole file is scanned for the tool name.
        #
        # ONLY NAMES THAT ARE TOOL FUNCTIONS, filtered against the header afterwards. The first
        # version matched any `*TurboMCP*` string and so counted the TOOLSET CLASS names
        # (`UTurboMCPLookToolset`) as uncalled tools - two false positives that made the audit look
        # like it had found more than it had.
        for name in re.findall(r'"((?:Read|Look|Preview|Edit)TurboMCP\w+)"', text):
            entry_info = calls.setdefault(name, {"files": set(), "keys": set()})
            entry_info["files"].add(entry)
            entry_info.setdefault("named", set()).add(entry)

        # *** AND `.call(` WITH A LITERAL NAME IS PROOF ENOUGH, WHATEVER THE ARGUMENTS ARE. *** The strict
        # pattern above requires a LITERAL `{...}`, so it MISSES the perfectly ordinary shape where the
        # arguments are assembled first and passed by name - `session.call(PREVIEW_TOOLSET,
        # "PreviewTurboMCPComment", args)` - WHICH IS HOW ELEVEN TOOLS ACROSS FIVE SUITES ARE ACTUALLY CALLED.
        # MEASURED BEFORE THIS LINE EXISTED: all eleven were reported as "named but not parsed", a list long
        # enough to look like noise, WHICH IS THE ONE FAILURE MODE AN AUDIT MUST NOT HAVE - a false positive
        # makes a real finding look like noise.
        for match in re.finditer(r'\.call\(\s*(?:[A-Z_]+|"[^"]*")\s*,\s*"((?:Read|Look|Preview|Edit)TurboMCP\w+)"', text):
            call_name = match.group(1)
            call_info = calls.setdefault(call_name, {"files": set(), "keys": set()})
            call_info["files"].add(entry)
            call_info["in_call"] = True

    return calls


def local_keys_for(entry, tools_dir=TOOLS_DIR):
    """Every `"key":` that appears anywhere in one suite, so a call assembled through a helper is covered."""
    path = os.path.join(tools_dir, entry)
    try:
        with open(path, "r", encoding="utf-8", errors="replace") as handle:
            return set(re.findall(r'"(\w+)"\s*:', handle.read()))
    except OSError:
        return set()


def parameter_missing_reason(missing, tool_name, every_suite_that_calls_it):
    """Why these parameter names count, and what would supply them.

    *** A FALSE POSITIVE AND A REAL GAP PRINTED THE SAME LINE, WHICH IS THE FAILURE MODE THIS AUDIT IS WORST
    AT. *** R04 recorded that defaulted `Inputs`/`Outputs` were parsed as a parameter named `TEXT("")`, which
    made a covered tool look uncovered. That parser defect is fixed. What the fix exposed is real: every caller
    of `EditTurboMCPFunctionAdd` omitted the two defaulted parameters, so no suite anywhere supplies a sample
    for them. The line must therefore say which case it is, because "the parser is wrong" and "no suite passes
    this argument" need different work.
    """
    return ("the parsed parameter name(s) %s appear in no sample. Suites that call %s: %s. "
            "A defaulted parameter still needs ONE caller that supplies it, or the default is the only shape "
            "anyone has ever run."
            % (", ".join("`%s`" % p for p in missing), tool_name,
               ", ".join(sorted(every_suite_that_calls_it)) or "(none)"))


def audit(headers=None, tools_dir=TOOLS_DIR):
    """The audit's findings as data. `failures` is empty only when every check passed.

    `headers` is a list of header paths; the default is the one real header. A fixture header is accepted so
    the parser and the check can be driven directly instead of trusted.
    """
    if headers is None:
        headers = [HEADER]
    tools = {}
    declared_names = []
    for header_path in headers:
        tools.update(read_declarations(header_path))
        with open(header_path, "r", encoding="utf-8", errors="replace") as handle:
            declared_names += re.findall(r"static FString (\w+)\(", handle.read())

    calls = read_calls(tools_dir)
    # THE CALLS ARE INTERSECTED WITH THE DECLARATIONS, because the registry serves one EXTRA
    # callable name per toolset that is NOT a tool this project wrote: the engine registers each
    # toolset class itself, so `LookTurboMCPToolset` and `PreviewTurboMCPToolset` appear as callable
    # names. They are not tools with a schema and no suite can meaningfully call them, so counting
    # them as uncalled tools was two false positives - which is exactly the failure mode an audit
    # must not have, because a false positive makes a real finding look like noise.
    calls = {name: info for name, info in calls.items() if name in tools}

    failures = []
    notes = []

    def check(name, ok, detail=""):
        notes.append(("ok" if ok else "FAIL", name, detail))
        if not ok:
            failures.append(name)

    check("every tool is declared inside a toolset class",
          all(info["owner"] for info in tools.values()),
          "ownerless: %s" % [n for n, i in tools.items() if not i["owner"]])
    # THE COMPARISON IS AGAINST DECLARATIONS, NOT AGAINST THE STRING `AICallable`. The first version
    # counted occurrences of that string in the header and compared 45 declarations against 46 hits,
    # because the header's own doc comment mentions it in prose at line 35. The header was right and
    # the CHECK was wrong - the sixth time this project has had to learn that.
    check("every static FString is a parsed tool, so none was skipped",
          len(tools) == len(declared_names),
          "parsed %d of %d declared" % (len(tools), len(declared_names)))
    check("no two tools share a name", len(declared_names) == len(set(declared_names)),
          "duplicates: %s" % [n for n in declared_names if declared_names.count(n) > 1])

    # *** THREE STATES, SEPARATED AT LAST, BECAUSE "uncalled" WAS CONFLATING TWO OF THEM. ***
    #
    # UNCALLED means no suite names the tool at all: the fix is to WRITE A SUITE.
    # NAMED-BUT-NOT-PARSED means a suite names it but in a shape NEITHER pass could read: THE FIX IS TO LOOK,
    # because it may well be called. Measured on four shapes: same-line -> parsed by the strict pass; SPLIT
    # ACROSS LINES -> strict fails but the bare-name pass credits it; named arguments through a helper ->
    # BOTH FAIL; a name built rather than written -> BOTH FAIL.
    #
    # AND THE MIDDLE STATE HAS ALREADY BEEN OBSERVED FROM BOTH SIDES IN ONE ROUND: `R-ASSET-COST`'s tool was
    # called TWICE through a helper named `check()` and reported uncalled until the helper was renamed, while
    # a widget workstream's tool genuinely had no suite yet. BOTH PRINTED THE SAME LINE. A REGEX THAT ASSUMES
    # ONE SHAPE IS INDISTINGUISHABLE FROM A REAL ABSENCE - this project's oldest recorded failure, and it had
    # never been applied to the audit itself.
    uncalled = []
    seen_unparsed = []
    for name in sorted(tools):
        info = calls.get(name)
        if not info:
            uncalled.append(name)
        elif info.get("named") and not info.get("parsed") and not info.get("in_call"):
            seen_unparsed.append(name)
    # THIS IS THE CHECK THE USER'S REQUIREMENT TURNS INTO. A tool no suite calls has never been run,
    # which is how a registered tool that answers nothing survives.
    check("no AICallable tool is left uncalled by every suite",
          len(uncalled) == 0, ", ".join(uncalled))

    # A tool that is called but whose parameter is never supplied is called in only one shape.
    blank = {}
    for name in sorted(tools):
        if name not in calls:
            continue
        supplied = calls[name]["keys"]
        # A PARAMETER COUNTS AS SUPPLIED WHEN ITS NAME APPEARS AS A KEY IN A FILE THAT CALLS THE
        # TOOL. Not "anywhere in any suite" - that would let the key `Asset`, which appears in a dozen
        # unrelated helpers, satisfy every tool's `Asset` parameter and make the check worthless.
        # Scoped to the calling files, a tool driven through a helper or a loop IS covered, because
        # its own suite spells the keys.
        local_keys = set()
        for entry in calls[name]["files"]:
            local_keys |= local_keys_for(entry, tools_dir)
        supplied_lower = {k.lower() for k in supplied} | {k.lower() for k in local_keys}
        missing = [p for p in tools[name]["params"]
                   if p not in supplied and p.lower() not in supplied_lower]
        if missing and len(supplied) < len(tools[name]["params"]):
            blank[name] = missing
    check("no called tool leaves a parameter with no sample anywhere",
          len(blank) == 0,
          "; ".join(parameter_missing_reason(blank[n], n, calls[n]["files"]) for n in sorted(blank)))

    per_file = {}
    for name, info in calls.items():
        for entry in info["files"]:
            per_file[entry] = per_file.get(entry, 0) + 1

    return {
        "tools": tools,
        "declaredNames": declared_names,
        "calls": calls,
        "uncalled": uncalled,
        "namedButUnparsed": seen_unparsed,
        "blankParameters": blank,
        "perFile": per_file,
        "notes": notes,
        "failures": failures,
    }


def main():
    report = audit()

    print("=== %d AICallable tool(s) declared ===" % len(report["tools"]))
    by_owner = {}
    for name, info in report["tools"].items():
        by_owner.setdefault(info["owner"], []).append(name)
    for owner in sorted(by_owner):
        print("    %-26s %d" % (owner, len(by_owner[owner])))
    print()

    def show(index):
        """One recorded check, in the shape this report has always printed it."""
        mark, name, detail = report["notes"][index]
        print("  %-4s %s%s" % (mark, name, ("  " + detail) if detail else ""))

    show(0)
    show(1)
    show(2)

    print()
    print("=== every tool is CALLED by some suite ===")
    print("    uncalled: %s" % (", ".join(report["uncalled"]) if report["uncalled"] else "(none)"))
    if report["namedButUnparsed"]:
        print("    NAMED BUT NOT PARSED AS A CALL (look at these by hand): %s"
              % ", ".join(report["namedButUnparsed"]))
    show(3)

    print()
    print("=== every PARAMETER of every called tool appears in some sample ===")
    for name in sorted(report["blankParameters"]):
        print("    %-40s %s" % (name, ", ".join(report["blankParameters"][name])))
    if not report["blankParameters"]:
        print("    (every parameter appears in at least one sample)")
    show(4)

    print()
    print("=== the suites that cover the most tools ===")
    for entry, count in sorted(report["perFile"].items(), key=lambda kv: -kv[1])[:6]:
        print("    %-30s %d tool(s)" % (entry, count))

    print()
    if report["failures"]:
        print("RESULT: FAIL (%d) -> %s" % (len(report["failures"]), ", ".join(report["failures"])))
        return 1
    print("RESULT: PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
