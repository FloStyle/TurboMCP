"""Create and save the Phase 3 Widget Blueprint test asset.

    python Tools/make-test-widget.py

WHY THIS EXISTS
---------------
`L-WIDGET-PREVIEW` renders a Widget Blueprint. There was none in the project, so the
tool had nothing to render. This builds one with a known hierarchy and known
variables, so a preview can be asserted rather than eyeballed.

IT USES THE ENGINE'S OWN UMGToolSet
-----------------------------------
`UMGToolSet.CreateWidgetBlueprint` and `AddWidget` are the engine's tools, reached
over MCP. Nothing here builds widget assets by hand, which keeps the scaffolding rule
in `06_ACCEPTANCE.md`: this plugin's readers are pointed at content the engine made.

IT SAVES, AND CHECKS THAT IT SAVED
----------------------------------
This is the lesson the hard way, for the third time in this project. A Widget
Blueprint was created through these same tools, a preview was attempted, and the asset
was gone: it had never been saved, so stopping the editor destroyed it. The script ends
by asserting the asset is on disk, not that a tool returned success.
"""

import json
import sys

sys.path.insert(0, __file__.rsplit("\\", 1)[0])
sys.path.insert(0, __file__.rsplit("/", 1)[0])

from tmcp import EDIT_TOOLSET, READ_TOOLSET, Session  # noqa: E402

UMG = "UMGToolSet.UMGToolSet"

PACKAGE = "/Game/Tests/WBP_NodeTest"
OBJECT = "/Game/Tests/WBP_NodeTest.WBP_NodeTest"

failures = []


def check(name, ok, detail=""):
    print("  %-4s %s%s" % ("ok" if ok else "FAIL", name, ("  " + detail) if detail else ""))
    if not ok:
        failures.append(name)


def main():
    session = Session()
    session.connect()

    print("=== 1. is the Widget Blueprint already there? ===")
    # The FULL path is used, not the short name.
    #
    # A short name is resolved by searching /Game for assets whose own name matches,
    # and that search only sees what the asset registry knows about. An asset created
    # in this editor session but not yet saved is loaded and addressable by its full
    # path, yet the short-name search does not find it, which made this script create
    # a second asset on top of the first and read "The path already exists".
    described = session.call(READ_TOOLSET, "ReadTurboMCPAssetDescribe",
                             {"assets": OBJECT})
    print(described)
    if "error:" not in described:
        # It exists. Save it, because an unsaved asset dies with the editor, which is
        # exactly how the first attempt at this asset was lost.
        print()
        print("=== it exists but may not be on disk; saving ===")
        print(session.call(EDIT_TOOLSET, "EditTurboMCPSave", {"assets": PACKAGE}))
        after = session.call(READ_TOOLSET, "ReadTurboMCPAssetDescribe",
                             {"assets": OBJECT})
        print(after)
        check("the asset is on disk", "on disk: yes" in after)
        print()
        if failures:
            print("RESULT: FAIL (%d) -> %s" % (len(failures), ", ".join(failures)))
            return 1
        print("RESULT: PASS")
        return 0

    print()
    print("=== 2. create it through the engine's own tool ===")
    created = session.call(UMG, "CreateWidgetBlueprint", {
        "folderPath": "/Game/Tests",
        "assetName": "WBP_NodeTest",
        "parentClass": {"refPath": "/Script/UMG.UserWidget"},
    })
    print(created)
    if "refPath" not in created:
        print("\nFAIL  the engine's tool did not create the asset.")
        return 1
    check("created the asset", OBJECT in created)

    print()
    print("=== 3. add a root canvas and children ===")
    root = session.call(UMG, "AddWidget", {
        "widgetBlueprint": {"refPath": OBJECT},
        "widgetClass": {"refPath": "/Script/UMG.CanvasPanel"},
        "widgetDisplayName": "RootCanvas",
    })
    print("  canvas: %s" % str(root)[:180])
    root_ref = None
    try:
        root_ref = json.loads(root)["returnValue"]["widget"]["refPath"]
    except Exception:  # noqa: BLE001
        pass
    check("added the root canvas", root_ref is not None)

    if root_ref is None:
        print("\nFAIL  no root widget, so nothing can be parented.")
        return 1

    for name, widget_class in (("TitleText", "/Script/UMG.TextBlock"),
                               ("BodyText", "/Script/UMG.TextBlock")):
        added = session.call(UMG, "AddWidget", {
            "widgetBlueprint": {"refPath": OBJECT},
            "widgetClass": {"refPath": widget_class},
            "widgetDisplayName": name,
            "parentWidget": {"refPath": root_ref},
        })
        print("  %s: %s" % (name, str(added)[:140]))
        check("added %s" % name, name in added)

    print()
    print("=== 4. the tree, as the engine sees it ===")
    tree = session.call(UMG, "GetWidgets", {"widgetBlueprint": {"refPath": OBJECT}})
    print(str(tree)[:900])

    # `Session.call` DECODES the result, so this is a dict and not JSON text. The first
    # version of this check looked for '"widgetCount":3' in it, which could never be true
    # for a dict, so it would have failed on every run and told us nothing about the
    # widgets. A substring search on a decoded value is a check that cannot pass; the
    # count is read from the field instead.
    if isinstance(tree, dict):
        actual_count = tree.get("info", {}).get("widgetCount")
        names = [w["widget"]["refPath"].rsplit(".", 1)[-1]
                 for w in tree.get("widgets", [])]
        print("  count=%s names=%s" % (actual_count, names))
        check("the tree holds three widgets", actual_count == 3,
              "count=%s" % actual_count)
        check("the root is the canvas", names[:1] == ["RootCanvas"], str(names[:1]))
    else:
        check("the tree could be read as a structure", False, "got %s" % type(tree))

    print()
    print("=== 5. make a text widget a variable, so a sample value has a target ===")
    # A widget property is what L-WIDGET-PREVIEW's sample values address, and what the
    # widget tree section reports as 'variable'. Both need one to exist.
    toggled = session.call(UMG, "ToggleWidgetAsVariable", {
        "widgetBlueprint": {"refPath": OBJECT},
        "widget": {"refPath": OBJECT + ":WidgetTree.TitleText"},
        "bIsVariable": True,
    })
    print("  %s" % str(toggled)[:200])

    print()
    print("=== 6. the tree must still hold three widgets after the toggle ===")
    # The children were lost once before, between being added and being saved, and the
    # loss was silent. This asserts the count again after every mutating step so a
    # regression here cannot pass unnoticed.
    tree_again = session.call(UMG, "GetWidgets", {"widgetBlueprint": {"refPath": OBJECT}})
    if isinstance(tree_again, dict):
        count_again = tree_again.get("info", {}).get("widgetCount")
        check("still three widgets after the toggle", count_again == 3,
              "count=%s" % count_again)
    else:
        check("the tree could be read again", False, "got %s" % type(tree_again))

    print()
    print("=== 6. save, and prove it from the asset's own flag ===")
    saved = session.call(EDIT_TOOLSET, "EditTurboMCPSave", {"assets": PACKAGE})
    print(saved)
    if "0 failed" not in saved:
        print("\nFAIL  the save reported a failure.")
        return 1

    after = session.call(READ_TOOLSET, "ReadTurboMCPAssetDescribe",
                         {"assets": OBJECT})
    print()
    print(after)
    check("the asset is on disk", "on disk: yes" in after)
    check("it has no unsaved changes", "unsaved changes: no" in after)

    print()
    if failures:
        print("RESULT: FAIL (%d) -> %s" % (len(failures), ", ".join(failures)))
        return 1
    print("RESULT: PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
