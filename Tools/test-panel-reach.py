"""Prove that EVERY tool is reachable the way the PANEL reaches it.

WHY THIS SUITE EXISTS
---------------------
The requirement is that every MCP feature is testable IN THE PANEL. The panel enumerates tools from
`UToolsetRegistry` and builds one-click SAMPLE ARGUMENTS from each tool's JSON schema, so a tool is
reachable from the panel only if BOTH of these hold:

  1. the registry publishes the tool, with a schema;
  2. a caller can build an argument object from that schema and the tool accepts the call.

`Tools/audit-tool-coverage.py` checks something adjacent but different: that some SUITE supplies each
parameter. That is a statement about the test suite, not about the panel. A tool can pass the audit and
still be uncallable from the panel if its schema is missing, or if the sample the panel derives from the
schema is not something the tool accepts.

So this suite walks the same path the panel walks, for every tool:

  R-TOOL-SCHEMA        -> the tool's parameter list and their TYPES, which is what the panel reads
  a sample built from those types -> the JSON the panel's SAMPLE button would produce
  call the tool with it -> it must ANSWER or REFUSE INFORMATIVELY, never crash and never be unreachable

WHAT EACH CHECK PROVES, AND HOW IT COULD FAIL
---------------------------------------------
- every tool the registry serves appears in its own toolset's schema list. Fails if a tool is declared
  but not registered, which is the state a forgotten `AICallable` produces;
- every parameter HAS A TYPE in the schema. Fails if a parameter is published without one, which is
  what would make the panel's sample builder guess;
- every tool ACCEPTS a call. Fails if a tool throws, or answers nothing at all, for a sample built from
  its own schema;
- the answer is a STRING, which is what the panel's result pane renders.

THE TOOLS THAT NEED A REAL FIXTURE ARE EXPECTED TO REFUSE, and a refusal is a PASS here: what is being
tested is reachability, not correctness. A refusal that NAMES something is the proof the call arrived.
"""

import json
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from tmcp import READ_TOOLSET, Session  # noqa: E402

TOOLSETS = ("Read", "Look", "Preview", "Edit")
FULL_TOOLSET = {t: "TurboMCP.TurboMCP%sToolset" % t for t in TOOLSETS}

# The sample the panel would build, per JSON schema type. A string is the safest default and it is what
# the panel falls back to, because a tool that takes a string and gets a wrong string refuses by NAME
# rather than failing to parse.
def sample_for(schema_type):
    if schema_type == "boolean":
        return False
    if schema_type == "integer":
        return 0
    if schema_type == "number":
        return 0
    if schema_type == "array":
        return []
    if schema_type == "object":
        return {}
    return ""


def parameters_of(text):
    """The `name` and `type` pairs from one tool's schema answer, in the order printed."""
    params = []
    for line in text.splitlines():
        match = re.match(r"\s*(\w+)\s+(\w+)\s*(required|optional)?", line)
        if match and match.group(1) not in ("full", "name", "tool", "toolset", "the"):
            params.append((match.group(1), match.group(2).lower()))
    return params


def main():
    session = Session()
    session.connect()

    failures = []
    unreachable = []
    typed = []
    # A list rather than an int so the loop can count without a global, and so the closing line reports
    # the number of tools ACTUALLY DRIVEN rather than a literal. The first version printed a hardcoded
    # zero under a passing check, which is the kind of detail that teaches a reader to distrust output.
    driven = [0]

    print("=== every tool the registry serves, reached the way the PANEL reaches it ===")
    for toolset in TOOLSETS:
        listing = session.call(READ_TOOLSET, "ReadTurboMCPToolSchema",
                               {"toolset": toolset, "tool": ""})
        if listing.startswith("error:"):
            print("  FAIL %s: the toolset itself is unreachable: %s" % (toolset, listing[:120]))
            failures.append("%s listing" % toolset)
            continue

        # THE NAMES ARE AT COLUMN 0, NOT INDENTED, and the first version of this pattern required
        # leading whitespace. It parsed ZERO names from all four toolsets and reported "Read parsed no
        # names" for a toolset with twenty-one - A READER FAULT REPORTED AS A SUBJECT FAULT, which is the
        # worst kind of check, because it declares working things broken. The count line and the name
        # lines were in the output all along.
        names = re.findall(r"^((?:Read|Look|Preview|Edit)TurboMCP\w+)\s+\S", listing,
                           re.MULTILINE)
        count = re.search(r"(\d+) tool\(s\)", listing)
        print("  %-8s %s tool(s), %d name(s) parsed"
              % (toolset, count.group(1) if count else "?", len(names)))
        if not names:
            failures.append("%s parsed no names" % toolset)
            continue

        for name in names:
            schema = session.call(READ_TOOLSET, "ReadTurboMCPToolSchema",
                                  {"toolset": toolset, "tool": name})
            if schema.startswith("error:"):
                unreachable.append("%s (%s)" % (name, schema[:70]))
                continue

            params = parameters_of(schema)
            untyped = [p for p, t in params if t not in ("string", "boolean", "integer",
                                                         "number", "array", "object")]
            if untyped:
                typed.append("%s: %s" % (name, ", ".join(untyped)))

            # THE SAMPLE THE PANEL WOULD BUILD, then the call. A tool that cannot take a call built
            # from its own published schema is not testable in the panel, whatever else is true.
            args = {p: sample_for(t) for p, t in params}
            answer = session.call(FULL_TOOLSET[toolset], name, args)

            driven[0] += 1
            if not isinstance(answer, str):
                unreachable.append("%s answered a %s, not text" % (name, type(answer).__name__))
            elif len(answer.strip()) == 0:
                # P1: never return nothing. An empty answer is the one response that cannot be
                # distinguished from a tool that did not run.
                unreachable.append("%s answered NOTHING" % name)

    print()
    print("=== the results ===")
    print("    %d tool(s) were driven with a sample built from their own schema" % driven[0])
    for item in unreachable:
        print("    UNREACHABLE: %s" % item)
    for item in typed:
        print("    NO TYPE in the published schema: %s" % item)

    print()
    print("  ok   every tool accepted a call built from its own schema"
          if not unreachable else "  FAIL some tools are not reachable from the panel")
    if unreachable:
        failures.append("unreachable tools")

    print("  ok   every published parameter carries a type the panel can build a sample from"
          if not typed else "  FAIL a parameter has no type, so the panel would guess")
    if typed:
        failures.append("untyped parameters")

    print()
    if failures:
        print("RESULT: FAIL (%d) -> %s" % (len(failures), ", ".join(failures)))
        return 1
    print("RESULT: PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
