# Copyright (c) 2026 FloStyle
# SPDX-License-Identifier: MPL-2.0
"""Inventory static tool references in test source.

Parse declarations, call syntax and literal names separately. None proves that
a suite executes or verifies behavior. Parameter samples are whole-file hints,
not a verified association between arguments and a particular call. Dynamic
helper calls require manual review. The acceptance catalog owns runtime proof.
"""

import ast
import os
import re
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
HEADER = os.path.join(REPO, "Plugins", "TurboMCP", "Source", "TurboMCP", "Public", "Tools", "TurboMCPToolsets.h")
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
    """Static call syntax and literal references; neither proves execution."""
    calls = {}
    for entry in sorted(os.listdir(tools_dir)):
        if not entry.startswith('test-') or not entry.endswith('.py'):
            continue
        text = open(os.path.join(tools_dir, entry), encoding='utf8').read()
        tree = ast.parse(text, filename=entry)
        for node in ast.walk(tree):
            if isinstance(node, ast.Constant) and isinstance(node.value, str) and re.fullmatch(r'(?:Read|Look|Preview|Edit)TurboMCP\w+', node.value):
                info = calls.setdefault(node.value, {'files': set(), 'keys': set()})
                info['files'].add(entry)
                info.setdefault('named', set()).add(entry)
            if not isinstance(node, ast.Call) or not isinstance(node.func, ast.Attribute) or node.func.attr != 'call' or len(node.args) < 2:
                continue
            name = node.args[1]
            if not isinstance(name, ast.Constant) or not isinstance(name.value, str):
                continue
            info = calls.setdefault(name.value, {'files': set(), 'keys': set()})
            info['files'].add(entry)
            info['in_call'] = True
            if len(node.args) > 2 and isinstance(node.args[2], ast.Dict):
                info['parsed'] = True
                info['keys'].update(k.value for k in node.args[2].keys if isinstance(k, ast.Constant) and isinstance(k.value, str))
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
    check("no AICallable tool lacks any static test reference",
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
        "parsedCalls": sorted(n for n, c in calls.items() if c.get("in_call")),
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
    print("=== STATIC references only: not execution evidence ===")
    print("    missing static reference: %s" % (", ".join(report["uncalled"]) if report["uncalled"] else "(none)"))
    print("    parsed call syntax: %d; name-only references: %d; missing: %d" % (len(report["parsedCalls"]), len(report["namedButUnparsed"]), len(report["uncalled"])))
    if report["namedButUnparsed"]:
        print("    NAMED BUT NOT PARSED AS A CALL (look at these by hand): %s"
              % ", ".join(report["namedButUnparsed"]))
    show(3)

    print()
    print("=== parameter sample hints (whole-file heuristic; not verified call arguments) ===")
    for name in sorted(report["blankParameters"]):
        print("    %-40s %s" % (name, ", ".join(report["blankParameters"][name])))
    if not report["blankParameters"]:
        print("    (every parameter appears in at least one sample)")
    show(4)

    print()
    print("=== suites with the most static tool references ===")
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
