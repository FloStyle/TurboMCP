"""UNIFY the remaining graph resolvers onto the one the reader uses.

The defect, measured and systemic: EIGHT graph resolvers each assemble their own candidate list, and the reader's
`TurboMCPNodeId::FindGraphByName` searches four arrays while the tool-local ones search three - `EventGraphs` is
missing. A node id is `...:GraphName:<hash>`, carrying the graph NAME ONLY, so an ambiguous name can make a tool
EDIT ONE GRAPH AND REPORT ON ANOTHER and no caller can tell. That is the ARC06 family.

Each resolver keeps its NAME and SIGNATURE, so no call site changes; only the BODY is replaced with a delegation.
"""

import pathlib
import re

TARGETS = [
    # (file, signature marker, delegation body)
    ("TurboMCPComment.cpp",
     "UEdGraph* FindGraph(UBlueprint* InBlueprint, const FString& InGraphName, FString& OutError)",
     "\t\tOutError.Empty();\n\t\treturn TurboMCPNodeId::FindGraphByName(InBlueprint, InGraphName);\n"),
    ("TurboMCPGraphAppend.cpp",
     "UEdGraph* FindGraph(UBlueprint* InBlueprint, const FString& InGraphName)",
     "\t\treturn TurboMCPNodeId::FindGraphByName(InBlueprint, InGraphName);\n"),
    ("TurboMCPGraphStructure.cpp",
     "UEdGraph* FindGraph(UBlueprint* InBlueprint, const FString& InGraphName)",
     "\t\treturn TurboMCPNodeId::FindGraphByName(InBlueprint, InGraphName);\n"),
    ("TurboMCPGraphSwapCall.cpp",
     "UEdGraph* FindGraph(UBlueprint* Blueprint, const FString& Name)",
     "\t\treturn TurboMCPNodeId::FindGraphByName(Blueprint, Name);\n"),
]

NOTE = ("\t\t// *** ONE RESOLUTION POLICY WITH EVERY OTHER TOOL AND THE READER. ***\n"
        "\t\t// This was one of EIGHT graph resolvers. It searched UbergraphPages, FunctionGraphs and "
        "MacroGraphs;\n"
        "\t\t// the reader's TurboMCPNodeId::FindGraphByName searches those THREE PLUS EventGraphs, and nothing\n"
        "\t\t// compared them. A node id is `...:GraphName:<hash>` - the graph NAME ONLY - so a name matching\n"
        "\t\t// two graphs in different arrays would make this tool EDIT ONE AND REPORT ON ANOTHER, and no\n"
        "\t\t// caller could tell. That is the ARC06 family: identity that cannot be verified across calls.\n"
        "\t\t// It now delegates, so this tool and the reader resolve a name identically.\n")

for filename, signature, delegation in TARGETS:
    path = pathlib.Path(r"C:\Users\Example\Desktop\TurboMCP\Source\TurboMCP") / filename
    text = path.read_text(encoding="utf-8")
    if signature not in text:
        print("MISS signature: %s" % filename)
        continue
    if "FindGraphByName" in text:
        print("ALREADY unified: %s" % filename)
        continue

    start = text.index(signature)
    body_open = text.index("{", start)
    # Walk to the matching closing brace.
    depth, i = 0, body_open
    while i < len(text):
        if text[i] == "{":
            depth += 1
        elif text[i] == "}":
            depth -= 1
            if depth == 0:
                break
        i += 1
    body = text[body_open + 1:i]
    indent = "\t\t"
    replacement = "{\n" + NOTE + indent + delegation.rstrip("\n") + "\n\t}"
    text = text[:body_open] + replacement + text[i + 1:]
    path.write_text(text, encoding="utf-8")
    print("UNIFIED %s (%d chars of body replaced)" % (filename, len(body)))
