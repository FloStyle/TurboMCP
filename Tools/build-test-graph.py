"""Build the Phase 2 test graph, using the ENGINE's own writer.

    python Tools/build-test-graph.py

WHY THE ENGINE WRITES IT
------------------------
`06_ACCEPTANCE.md` requires the asset **structure** the acceptance tests measure
to be hand-built, because a tool tested against an asset another tool built proves
nothing. So this script does not create nodes itself: it asks the engine's own
`write_graph_dsl` to create them. What this plugin's readers are then pointed at
was written by Epic's writer, which is what makes the comparison meaningful and
keeps the scaffolding rule intact.

WHAT IT DOES, IN ORDER
----------------------
1. starts from a known graph state: it refuses if the Blueprint is missing
2. writes the graph through `write_graph_dsl`
3. **saves the asset**, then proves the save from the asset's own unsaved flag

Step 3 is not optional and was learned the hard way. An earlier run wrote 13 nodes
through the engine's writer and read them back correctly, then the editor was
stopped for a rebuild and all 13 were gone: `write_graph_dsl` compiles the
Blueprint, which marks it dirty, but nothing saved it. The verification looked
perfect and the work did not exist.

The save goes through the plugin's own `E-SAVE`, so a bug in it shows up here
rather than hiding until later.
"""

import json
import sys

sys.path.insert(0, __file__.rsplit("\\", 1)[0])
sys.path.insert(0, __file__.rsplit("/", 1)[0])

from tmcp import EDIT_TOOLSET, READ_TOOLSET, Session  # noqa: E402

BLUEPRINT = "/Game/Tests/BP_NodeTest"
GRAPH_REF = {"refPath": "/Game/Tests/BP_NodeTest.BP_NodeTest:EventGraph"}
GRAPH_NAME = "EventGraph"

# A chain that exercises the reader rather than flattering it: an event, a write
# to a member variable, a branch that READS that variable, and a different call on
# each arm. That gives the reader a multi-node exec chain, a variable get and a
# variable set, and a node with two exec outputs.
GRAPH_CODE = """(event EventBeginPlay
  (Variables|Default|SetHealth 42.0)
  (Development|PrintString :InString "before the branch")
  (if (> (Variables|Default|GetHealth) 10.0)
    (Development|PrintString :InString "healthy")
    (else
      (Development|PrintString :InString "hurt"))))"""


def main():
    session = Session()
    session.connect()

    print("=== 1. is the Blueprint there? ===")
    described = session.call(READ_TOOLSET, "ReadTurboMCPAssetDescribe",
                             {"assets": "BP_NodeTest"})
    print(described)
    if "error:" in described:
        print("\nFAIL  BP_NodeTest is missing. Create it first:")
        print("      UnrealEditor.exe <project> -run=PythonScriptCommandlet "
              "-Script=Tools/make-test-blueprint.py -unattended -nullrhi -culture=en")
        return 1

    print()
    print("=== 2. the engine writes the graph ===")
    result = session.call(
        "editor_toolset.toolsets.blueprint.BlueprintTools",
        "write_graph_dsl",
        {"graph": GRAPH_REF, "code": GRAPH_CODE})
    print("  write_graph_dsl: %s" % result)

    # The engine's writer returns null on success, so the return value is not
    # evidence. The node count is.
    print()
    print("=== 3. count what actually landed ===")
    found = session.call(READ_TOOLSET, "ReadTurboMCPNodeFind",
                         {"asset": "BP_NodeTest", "words": "PrintString",
                          "graphName": GRAPH_NAME})
    print(found)
    if "0 match(es)" in found:
        print("\nFAIL  the writer returned success but no PrintString node exists.")
        return 1

    print()
    print("=== 4. save, and prove it from the asset's own flag ===")
    saved = session.call(EDIT_TOOLSET, "EditTurboMCPSave", {"assets": BLUEPRINT})
    print(saved)
    if "failed" in saved and "0 failed" not in saved:
        print("\nFAIL  the save reported a failure.")
        return 1

    after = session.call("TurboMCP.TurboMCPReadToolset", "ReadTurboMCPAssetDescribe",
                         {"assets": "BP_NodeTest"})
    print()
    print(after)
    if "unsaved changes: yes" in after:
        print("\nFAIL  the asset still has unsaved changes, so the graph is not "
              "on disk and will be lost when the editor stops.")
        return 1

    print()
    print("RESULT: PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
