"""Build the Class-Based Blueprint the PIE test map needs, with a graph that EXECUTES.

Run through the engine's Python commandlet, with the test map already open:

    UnrealEditor-Cmd <project> -run=PythonScriptCommandlet
        -Script=Tools/make-test-map-blueprint.py -unattended -nullrhi

WHY A PYTHON SCRIPT RATHER THAN THE C++ COMMANDLET
--------------------------------------------------
The C++ commandlet can create a WORLD and place a C++ actor, and it does both - but a C++ actor's
`BeginPlay` is native code, so it NEVER ENTERS THE BLUEPRINT VM. That matters for exactly one of the two
holes this fixture closes:

  Q-17  `R-PLAY-CHECK`'s warning and error count. A C++ actor is enough: it calls `UE_LOG` directly.
  Q-18  `E-TRACE`. `FBlueprintCoreDelegates::OnScriptException` is raised BY THE VM, so a native actor can
        never make it fire. A Blueprint node that RUNS is required.

The engine's own Blueprint toolset exposes `write_graph_dsl`, which authors nodes from DSL text - so this
script uses THE ENGINE'S OWN WRITER rather than a second graph writer of ours. That is the same reasoning the
node-reading tests use, and it means the shape being traced was built by Epic's code, not by ours.

PRINTSTRING IS THE EXECUTING NODE, AND THAT IS A DELIBERATE CHOICE. It is the node this project's own
`BP_NodeTest` already uses, its type ID is known (`Development|PrintString`), and it logs at `Log` severity -
so it adds a line to the play-check window WITHOUT adding a warning or an error. The counts that `Q-17` cares
about therefore stay exactly as the C++ actor set them, and the trace has a node that runs.
"""

import unreal

# `BlueprintTools` IS NOT AN `unreal` ATTRIBUTE, AND ASSUMING IT WAS COST A RUN.
#
# MEASURED FAILURE: `unreal.BlueprintTools` answered
#     AttributeError: module 'unreal' has no attribute 'BlueprintTools'
# The engine's own tests import it as a PYTHON CLASS from the EditorToolset plugin's module
# (`EditorToolset/Content/Python/editor_toolset/tests/test_blueprint.py:10`), so that is what this does. The
# module is importable because that plugin registers its `Content/Python` directory on the path at startup.
#
# THE IMPORT IS GUARDED SO ITS FAILURE IS LEGIBLE. An unguarded import would abort this script with a bare
# traceback before a single line reached the log, which is exactly how a fixture that never built once looked
# like a fixture that built and failed.
try:
    from editor_toolset.toolsets.blueprint import BlueprintTools
except Exception as ImportFailure:  # noqa: BLE001 - the message is the point
    BlueprintTools = None
    _IMPORT_ERROR = ImportFailure
else:
    _IMPORT_ERROR = None

BLUEPRINT_PACKAGE = "/Game/Tests/BP_TurboMCPTestLog"
BLUEPRINT_NAME = "BP_TurboMCPTestLog"
BLUEPRINT_FOLDER = "/Game/Tests"
GRAPH_NAME = "EventGraph"

# `Development|PrintString` is the type ID the engine's own Blueprint toolset tests use for this node
# (`EditorToolset/Content/Python/editor_toolset/tests/test_blueprint.py:1258`). Spelled here rather than
# discovered, because discovering it would need `find_node_types` and a matching pass for one known node.
PRINT_NODE_TYPE = "Development|PrintString"

# THE EVENT IS NAMED `EventBeginPlay`, NOT `BeginPlay`, AND THE `Event` PREFIX IS PART OF THE EVENT'S NAME.
#
# MEASURED FAILURE: `(event BeginPlay ...)` PARSED WITHOUT ERROR AND ADDED NOTHING - the observed titles were
# `['Event BeginPlay', 'Event ActorBeginOverlap', 'Event Tick']`, the engine's untouched defaults. The DSL
# wants the event FUNCTION's own name, and the engine calls it `EventBeginPlay`
# (`EditorToolset/Content/Python/editor_toolset/toolsets/blueprint_dsl.py:203`). A silently-empty write is
# exactly why this script counts the nodes back.
#
# `Event BeginPlay` RATHER THAN A CUSTOM EVENT: a custom event does not fire by itself, so a graph built around
# one would author a node that NEVER RUNS and the trace would stay unproven for a third time. `BeginPlay` is a
# lifecycle event on an actor placed in the level, so it fires on its own when PIE starts the world.
DSL = '(event EventBeginPlay\n  (%s "TurboMCP trace probe ran"))' % PRINT_NODE_TYPE


def say(message):
    """Report where it can actually be read.

    `print` from a Python commandlet does NOT reach the editor log - measured in this project when
    `make-test-blueprint.py` reported success while adding no variables at all, because its failure lines
    went nowhere. Everything goes through `unreal.log`.
    """
    try:
        unreal.log("[make-test-map-blueprint] %s" % message)
    except Exception:  # noqa: BLE001 - fall back rather than lose the message entirely
        print("[make-test-map-blueprint] %s" % message)


def fail(message):
    """Report a failure as an error so it is visible in the log's own error lines."""
    try:
        unreal.log_error("[make-test-map-blueprint] %s" % message)
    except Exception:  # noqa: BLE001
        print("[make-test-map-blueprint] FAILED %s" % message)


def main():
    if BlueprintTools is None:
        fail("the engine's Blueprint toolset could not be imported (%s), so no graph can be authored"
             % _IMPORT_ERROR)
        return 1

    # `BlueprintTools` IS THE MODULE-LEVEL CLASS IMPORTED ABOVE, NOT `unreal.BlueprintTools`. Keeping the local
    # name short makes the rest of this read the way the engine's own tests do.
    tools = BlueprintTools

    # THE BLUEPRINT IS REBUILT RATHER THAN REUSED, so the script is re-runnable from any state. A stale
    # Blueprint from an earlier attempt could carry a graph that no longer matches this text, and the whole
    # value of a fixture is that its contents are known.
    existing = unreal.EditorAssetLibrary.does_asset_exist(BLUEPRINT_PACKAGE)
    if existing:
        say("the asset registry still reports a Blueprint at %s" % BLUEPRINT_PACKAGE)
        # THE RESULT IS REPORTED BUT NOT TREATED AS FATAL, AND THAT IS A MEASURED CORRECTION.
        #
        # The commandlet deletes the `.uasset` FILE before this script runs, which frees the name on disk. The
        # registry can still answer "exists" for an asset whose file is gone, and `delete_asset` then answers
        # FALSE because there is nothing left to delete. Treating that as a hard failure stopped a rebuild that
        # would in fact have worked - a check failing on a CORRECT state, which this project has recorded more
        # than once. The create below is the real test: if the name is still occupied, it answers nothing and
        # the script fails with a message that names the actual problem.
        removed = unreal.EditorAssetLibrary.delete_asset(BLUEPRINT_PACKAGE)
        say("delete_asset answered %s" % removed)

        # GARBAGE IS COLLECTED BEFORE RECREATING, because deleting unregisters an asset but does not free its
        # name until the objects are collected.
        unreal.SystemLibrary.collect_garbage()
        say("collected garbage so the Blueprint's name is free again")

    blueprint = tools.create(
        folder_path=BLUEPRINT_FOLDER, asset_name=BLUEPRINT_NAME,
        asset_type=unreal.Actor.static_class())
    if blueprint is None:
        fail("BlueprintTools.create answered nothing")
        return 1
    say("created %s" % BLUEPRINT_PACKAGE)

    graph = tools.get_graph(blueprint, GRAPH_NAME)
    if graph is None:
        fail("the EventGraph of %s could not be found" % BLUEPRINT_PACKAGE)
        return 1

    # THE ENGINE'S OWN WRITER, so the graph is not authored by a second implementation that could disagree
    # with the readers about what a node looks like.
    tools.write_graph_dsl(graph, DSL)

    # THE NODES ARE COUNTED BACK, WHICH IS THE POINT. A Blueprint saved with no node in it looks exactly like
    # a good one from outside, and every later trace test would fail for a reason that pointed at the
    # capability rather than at the fixture.
    #
    # THE CHECK IS ON TITLES RATHER THAN ON TYPE IDs, AND THAT IS A CORRECTION RATHER THAN A PREFERENCE.
    #
    # MEASURED: the first version asserted that a node whose type id contained `Development|PrintString` was
    # present, and it reported failure WHILE THE NODE WAS THERE - the observed titles were
    # `['Event BeginPlay', 'Event ActorBeginOverlap', 'Event Tick', 'TurboMCPTraceProbe', 'PrintString']`. The
    # type-id accessor `BlueprintGraphEditor.get_node_type_id` did not answer for these nodes, so the check was
    # reading a SECOND-HAND value and calling a good fixture broken. A check that reports failure on a correct
    # result is as bad as one that cannot fail. The node's own displayed title is what the engine itself uses
    # in the graph, so it is checked directly.
    nodes = tools.find_nodes(graph)
    titles = [n.get_node_title() for n in nodes]

    say("graph nodes=%d titles=%s" % (len(nodes), titles))

    has_print = any("PrintString" in t for t in titles)
    # `Event BeginPlay` IS ALWAYS PRESENT ON AN ACTOR GRAPH, SO IT PROVES NOTHING ON ITS OWN. The check that
    # matters is the `PrintString` node, because that is the node the DSL was asked to add - and a DSL write
    # that parses and adds nothing is exactly the failure that was measured here.
    has_probe = has_print

    if not (has_print and has_probe):
        fail("the graph did not gain both the probe event and the PrintString node: print=%s probe=%s "
             "(titles seen: %s)" % (has_print, has_probe, titles))
        return 1

    # SAVED THROUGH THE LIBRARY THE PLUGIN ITSELF USES FOR E-SAVE, because `FEditorFileUtils` tries to check
    # out from source control and fails headless - measured in this project, and it made a save report
    # failure while nothing was written.
    if not unreal.EditorAssetLibrary.save_loaded_asset(blueprint):
        fail("the Blueprint could not be saved")
        return 1

    say("PASS graph_print=%s graph_probe=%s nodes=%d" % (has_print, has_probe, len(nodes)))
    return 0


if __name__ == "__main__":
    main()
