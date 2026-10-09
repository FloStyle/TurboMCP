"""Create the Phase 2 Blueprint scaffolding for node-reading tests.

Run headless:

    UnrealEditor.exe <project> -run=PythonScriptCommandlet ^
        -Script=Tools/make-test-blueprint.py -unattended -nullrhi -culture=en

WHAT THIS MAY AND MAY NOT DO
----------------------------
`06_ACCEPTANCE.md` requires the structure the acceptance tests measure to be built
by hand, because an asset built by a tool proves nothing about a tool. So this
script is limited to **scaffolding**:

  it creates an empty Actor Blueprint, adds two typed member variables, and saves
  it. That is enough for the asset registry, the path forms and the node readers
  to have something real to resolve.

It deliberately does NOT lay out nodes. The graph is populated afterwards through
the ENGINE's own `write_graph_dsl` tool, so that what the readers are pointed at
was written by Epic's writer rather than by anything of ours. That is what makes
the node type ID comparison in `TestRecord.md` meaningful.

THREE MISTAKES THIS SCRIPT ALREADY MADE, KEPT HERE SO THEY ARE NOT REPEATED
--------------------------------------------------------------------------
1. `print()` does not reach the editor log from a Python commandlet. The first
   version reported nothing and its failure lines went nowhere, so success was
   assumed. Everything now goes through `say()`, which calls `unreal.log`.
2. A hand-built `FEdGraphPinType` filled in with `set_editor_property` looks
   correct and is **silently rejected** by `add_member_variable`, which returns
   false rather than raising. The Blueprint was created with no variables and the
   script "succeeded". The type is now built with
   `BlueprintEditorLibrary.get_basic_type_by_name`, which is what the engine's own
   Blueprint toolset uses.
3. The return value of `add_member_variable` was ignored. It is now checked, and a
   shortfall fails the script, because a Blueprint with no variables cannot test a
   variable reader.
"""

import unreal

BLUEPRINT_PACKAGE = "/Game/Tests/BP_NodeTest"
BLUEPRINT_NAME = "BP_NodeTest"

# Two variables of different kinds, so a reader has more than one thing to find
# and so the node matcher can be pointed at a variable by name.
#
# A float is called `real` here: that is the engine's own name for it.
VARIABLE_NAMES = [
    ("Health", "real"),
    ("TargetName", "string"),
]

# What the engine's own toolset accepts as a basic type name.
BASIC_TYPES = frozenset({"bool", "int", "byte", "real", "name", "string", "text"})


def say(message):
    """Report progress where it can actually be read.

    `print` from a Python commandlet does not reach the editor log, which is how
    the first version of this script reported success while adding no variables at
    all: its failure lines went nowhere. `unreal.log` lands in the log.
    """
    try:
        unreal.log("[make-test-blueprint] %s" % message)
    except Exception:  # noqa: BLE001 - fall back rather than lose the message
        print("[make-test-blueprint] %s" % message)


def make_pin_type(type_name):
    """Build an FEdGraphPinType the way the engine's own toolset does.

    `_make_variable_type` in the engine's
    `EditorToolset/Content/Python/editor_toolset/toolsets/blueprint.py` is the
    reference for this. Building the struct by hand and filling its properties
    produces something that looks right and is refused by add_member_variable.
    """
    normalized = type_name.lower()
    if normalized == "float":
        # The engine calls it `real`.
        normalized = "real"

    if normalized in BASIC_TYPES:
        return unreal.BlueprintEditorLibrary.get_basic_type_by_name(normalized)

    struct_map = {
        "vector": unreal.Vector.static_struct(),
        "rotator": unreal.Rotator.static_struct(),
        "transform": unreal.Transform.static_struct(),
        "vector2d": unreal.Vector2D.static_struct(),
        "linearcolor": unreal.LinearColor.static_struct(),
    }
    if normalized in struct_map:
        return unreal.BlueprintEditorLibrary.get_struct_type(struct_map[normalized])

    raise ValueError("unknown variable type %r" % type_name)


def main():
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    editor_asset_library = unreal.EditorAssetLibrary

    if editor_asset_library.does_asset_exist(BLUEPRINT_PACKAGE):
        say("%s already exists, leaving it alone" % BLUEPRINT_PACKAGE)
        return 0

    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", unreal.Actor)

    blueprint = asset_tools.create_asset(
        asset_name=BLUEPRINT_NAME,
        package_path="/Game/Tests",
        asset_class=unreal.Blueprint,
        factory=factory,
    )

    if blueprint is None:
        say("FAILED to create the Blueprint")
        return 1

    say("created %s" % blueprint.get_path_name())

    # Member variables. The return value is checked: this call returns false
    # rather than raising when it refuses.
    variables_added = 0
    for name, type_name in VARIABLE_NAMES:
        try:
            pin_type = make_pin_type(type_name)
        except Exception as exc:  # noqa: BLE001
            say("FAILED to build a type for %s (%s): %s" % (name, type_name, exc))
            continue

        try:
            added = unreal.BlueprintEditorLibrary.add_member_variable(
                blueprint, name, pin_type)
        except Exception as exc:  # noqa: BLE001 - the API differs between builds
            say("FAILED to add %s: %s" % (name, exc))
            continue

        if added:
            variables_added += 1
            say("added variable %s (%s)" % (name, type_name))
        else:
            say("FAILED to add %s (%s): the library refused it" % (name, type_name))

    if variables_added != len(VARIABLE_NAMES):
        # A Blueprint with no variables cannot test a variable reader, so this is a
        # failure of the scaffolding rather than a detail to note.
        say("only %d of %d variables were added"
            % (variables_added, len(VARIABLE_NAMES)))
        return 1

    # Compile so the generated class matches the variables. A Blueprint whose
    # generated class is stale is a confusing thing to read.
    try:
        unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    except Exception as exc:  # noqa: BLE001
        say("compile reported: %s" % exc)

    saved = editor_asset_library.save_asset(BLUEPRINT_PACKAGE,
                                            only_if_is_dirty=False)
    say("saved: %s" % bool(saved))
    if not saved:
        return 1

    # Report what landed, so a caller can tell an empty Blueprint from a filled one
    # without starting an editor. Only APIs known to exist are used: the first
    # version guessed at get_graph_nodes and quietly reported nothing.
    graph_names = []
    try:
        graph_names = list(unreal.BlueprintEditorLibrary.list_graph_names(blueprint))
        say("graphs: %s" % ", ".join(graph_names))
    except Exception as exc:  # noqa: BLE001
        say("could not list graphs: %s" % exc)

    say("done: %d variable(s), %d graph(s)" % (variables_added, len(graph_names)))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
