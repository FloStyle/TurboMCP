"""Instrument the executor with node counts at the four points the readings identify.

The readings already located the loss to the EDIT. This prints the count at each step INSIDE the edit, so the
step where the count changes is the line. Counting `K2Node_CallFunction` and total nodes, because ids are not
stable across a rebuild but classes are.
"""

import pathlib

PATH = pathlib.Path(r"C:\Users\Example\Desktop\TurboMCP\Source\TurboMCP\TurboMCPGraphRepoint.cpp")
text = PATH.read_text(encoding="utf-8")

# 1. A counting helper next to the other file-local helpers.
HELPER_ANCHOR = "UEdGraphNode* FindNode(UEdGraph* G, const FString& Id) {"
HELPER = """// *** INSTRUMENTATION FOR THE P0: WHERE DOES THE GRAPH LOSE A NODE? ***
//
// The three readings (.agent/tmp/three-readings.py) located the loss to the EDIT: the plan left the graph at
// 5 nodes / 3 call functions, the edit took it to 4 / 2, and the count was stable before and after. This prints
// the count at each step INSIDE the edit so the step that changes it is the line. Classes, not ids: a rebuild
// changes ids but not classes.
void LogNodeCount(const TCHAR* Where, UEdGraph* G)
{
	int32 Total = 0, Calls = 0;
	if (G)
	{
		for (UEdGraphNode* N : G->Nodes)
		{
			if (!N) continue;
			++Total;
			if (N->IsA(UK2Node_CallFunction::StaticClass())) ++Calls;
		}
	}
	UE_LOG(LogTemp, Display, TEXT("[TurboMCP/P0] %s: %d node(s), %d call function(s)"), Where, Total, Calls);
}

UEdGraphNode* FindNode(UEdGraph* G, const FString& Id) {"""

if HELPER_ANCHOR in text and "LogNodeCount" not in text:
    text = text.replace(HELPER_ANCHOR, HELPER, 1)
    print("patched: the counting helper")
else:
    print("SKIP: helper")

POINTS = [
    ("\tFScopedTransaction Tx(NSLOCTEXT(\"TurboMCP\", \"GraphRepoint\", \"TurboMCP repoint graph readers\"));\n\tB->Modify();\n\tG->Modify();",
     "\tFScopedTransaction Tx(NSLOCTEXT(\"TurboMCP\", \"GraphRepoint\", \"TurboMCP repoint graph readers\"));\n\tB->Modify();\n\tG->Modify();\n\tLogNodeCount(TEXT(\"A: transaction open\"), G);"),
    ("\t\tOut.Effects.Add(TEXT(\"graph_node_created\"));\n\t}",
     "\t\tOut.Effects.Add(TEXT(\"graph_node_created\"));\n\t\tLogNodeCount(TEXT(\"B: after SpawnNodeOfType\"), G);\n\t}"),
    ("\t\t\treturn false;\n\t\t}\n\t}\n\n\t// Step 11:",
     "\t\t\treturn false;\n\t\t}\n\t}\n\tLogNodeCount(TEXT(\"C: after the pin-move loop\"), G);\n\n\t// Step 11:"),
]

for old, new in POINTS:
    if old in text:
        text = text.replace(old, new, 1)
        print("patched: %s" % new.split("LogNodeCount(")[1].split(")")[0])
    else:
        print("MISS: %s" % old.splitlines()[-1][:60])

PATH.write_text(text, encoding="utf-8")
