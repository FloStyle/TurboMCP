"""Create the Phase 2 Blueprint scaffolding for node-reading tests.

Run headless:

    UnrealEditor.exe <project> -run=PythonScriptCommandlet ^
        -Script=Tools/make-test-blueprint.py -unattended -nullrhi

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
the type ID comparison in `TestRecord.md` meaningful.
"""

import unreal

BLUEPRINT_PACKAGE = "/Game/Tests/BP_NodeTest"
BLUEPRINT_NAME = "BP_NodeTest"

# Two variables of different kinds, so a reader has more than one thing to find
# and so the node matcher can be pointed at a variable by name.
#
# Built by make_pin_type() rather than constructed inline: the Python wrapper for
# FEdGraphPinType takes no constructor arguments, so `unreal.EdGraphPinType(...)`
# raises "call() takes at most 0 arguments". Each field is set individually.
VARIABLE_NAMES = [
    ("Health", "float"),
    ("TargetName", "string"),
]


def make_pin_type(pin_category, sub_object=None, is_array=False):
    """Build an FEdGraphPinType the way the Python API expects it."""
    pin_type = unreal.EdGraphPinType()
    pin_type.set_editor_property("pin_category", pin_category)
    if sub_object is not None:
        pin_type.set_editor_property("pin_sub_category_object", sub_object)
    pin_type.set_editor_property("is_array", is_array)
    return pin_type


def main():
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    editor_asset_library = unreal.EditorAssetLibrary

    if editor_asset_library.does_asset_exist(BLUEPRINT_PACKAGE):
        print("[make-test-blueprint] %s already exists, leaving it alone"
              % BLUEPRINT_PACKAGE)
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
        print("[make-test-blueprint] FAILED to create the Blueprint")
        return 1

    print("[make-test-blueprint] created %s" % blueprint.get_path_name())

    # Member variables, added through the Blueprint editor library so the
    # generated class is kept consistent.
    for name, category in VARIABLE_NAMES:
        try:
            unreal.BlueprintEditorLibrary.add_member_variable(
                blueprint, name, make_pin_type(category))
            print("[make-test-blueprint] added variable %s (%s)" % (name, category))
        except Exception as exc:  # noqa: BLE001 - the API differs between builds
            print("[make-test-blueprint] could not add %s: %s" % (name, exc))

    # Compile so the generated class matches the variables, then save. A
    # Blueprint whose generated class is stale is a confusing thing to read.
    try:
        unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    except Exception as exc:  # noqa: BLE001
        print("[make-test-blueprint] compile reported: %s" % exc)

    saved = editor_asset_library.save_asset(BLUEPRINT_PACKAGE,
                                            only_if_is_dirty=False)
    print("[make-test-blueprint] saved: %s" % bool(saved))

    if not saved:
        return 1

    print("[make-test-blueprint] done")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
