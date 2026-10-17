"""Two release-hygiene jobs from the withdrawn P0, both in TurboMCPGraphRepoint.cpp.

1. REMOVE the P0 instrumentation. `LogNodeCount`, `LogGraphResolution` and `LogNodeIds` were written to find a
   defect that turned out not to exist. They log on every edit and must not ship (S17 packages this tree).

2. UNIFY the graph resolver. The file-local `FindGraph` searched THREE arrays (UbergraphPages, FunctionGraphs,
   MacroGraphs) while the reader's `TurboMCPNodeId::FindGraphByName` searches FOUR - EventGraphs was missing - and
   nothing compared them. Measured on the acceptance fixture they happened to agree, so it caused nothing; it is a
   latent identity defect of the ARC06 family, because a graph name that matches two graphs in different arrays
   would make this tool edit one and report on another.

   The local function is kept as a NAME (call sites unchanged) but now DELEGATES, so there is ONE resolution
   policy in the repository instead of two that can drift.
"""

import pathlib
import re

PATH = pathlib.Path(r"C:\Users\Example\Desktop\TurboMCP\Source\TurboMCP\TurboMCPGraphRepoint.cpp")
text = PATH.read_text(encoding="utf-8")
before_len = len(text)

# --- 1. remove the instrumentation call sites ----------------------------------------------------
calls_removed = 0
for call in (
    '\tLogNodeIds(TEXT("before"), G);\n',
    '\t\tLogNodeIds(TEXT("after"), G);\n',
    '\tLogNodeCount(TEXT("A: transaction open"), G);\n',
    '\t\tLogNodeCount(TEXT("B: after SpawnNodeOfType"), G);\n',
    '\tLogNodeCount(TEXT("C: after the pin-move loop"), G);\n',
    '\tLogGraphResolution(B, P.GraphName);\n',
):
    if call in text:
        text = text.replace(call, "", 1)
        calls_removed += 1
print("instrumentation call sites removed: %d" % calls_removed)

# --- 2. remove the instrumentation function bodies -----------------------------------------------
# They are contiguous blocks introduced by marker comments; cut from each marker to the following blank line
# before the next declaration.
patterns = [
    r"// \*\*\* INSTRUMENTATION FOR THE P0.*?\n\}\n\n",
    r"// \*\*\* AND THE TWO-RESOLVER COMPARISON.*?\n\}\n\n",
    r"// \*\*\* THE DIFF THE P0 NEEDS.*?\n\}\n\n",
]
for pat in patterns:
    new_text, n = re.subn(pat, "", text, flags=re.DOTALL)
    if n:
        text = new_text
        print("removed a function block")

# --- 3. unify the resolver -----------------------------------------------------------------------
OLD_FIND = """UEdGraph* FindGraph(UBlueprint* B, const FString& Name) {
	if (!B) return nullptr;
	TArray<UEdGraph*> Graphs; Graphs.Append(B->UbergraphPages); Graphs.Append(B->FunctionGraphs); Graphs.Append(B->MacroGraphs);
	for (UEdGraph* G : Graphs) if (G && (Name.IsEmpty() || G->GetName().Equals(Name, ESearchCase::IgnoreCase))) return G;
	return nullptr;
}"""

NEW_FIND = """// *** ONE RESOLUTION POLICY, NOT TWO THAT CAN DRIFT. ***
//
// This was a SECOND graph resolver. It searched UbergraphPages, FunctionGraphs and MacroGraphs; the reader's
// `TurboMCPNodeId::FindGraphByName` searches those THREE PLUS EventGraphs, and nothing compared the two. MEASURED
// on the acceptance fixture they returned the same pointer with one match each, so they agreed by luck of the
// fixture rather than by construction - and a graph name matching two graphs in different arrays would have made
// this tool EDIT ONE AND REPORT ON ANOTHER, because a node id carries only the graph NAME
// (`...:GraphName:<hash>`). That is the ARC06 family: identity that cannot be verified across calls.
//
// It now DELEGATES, so the tool and the reader resolve a name identically, and the local name is kept so no call
// site changes. The ambiguity refusal belongs in the shared resolver (see its own note); until that lands, this
// delegation at least makes the tool agree with the reader.
UEdGraph* FindGraph(UBlueprint* B, const FString& Name) {
	return TurboMCPNodeId::FindGraphByName(B, Name);
}"""

if OLD_FIND in text:
    text = text.replace(OLD_FIND, NEW_FIND, 1)
    print("resolver UNIFIED: the local FindGraph now delegates to TurboMCPNodeId::FindGraphByName")
else:
    print("MISS: the local FindGraph body - inspect it by hand")

# The instrumentation include is no longer needed.
text = text.replace('#include "K2Node_CallFunction.h"\n', "")

PATH.write_text(text, encoding="utf-8")
print("bytes: %d -> %d" % (before_len, len(text)))
for token in ("LogNodeCount", "LogNodeIds", "LogGraphResolution"):
    if token in text:
        print("STILL PRESENT: %s" % token)
