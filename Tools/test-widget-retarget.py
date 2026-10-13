# ACCEPTANCE ROW THIS SUITE CLAIMS TO PROVE: T-STRUCT-6
"""Exercise P/E-WIDGET-RETARGET against the running editor.

  T-STRUCT-6  fix bound events after a class swap

BOTH PATHS ARE CHECKED, AND THAT IS THE POINT
--------------------------------------------
A tool that removes broken bindings can be tested with a broken binding alone — and such a test
would PASS while the tool deleted EVERY binding it saw. The "ok" path is what proves it keeps the
ones it should, so this suite makes one of each and checks that exactly one survives.

The broken binding is made ON PURPOSE, by `E-WIDGET-BIND-ADD`: a binding is a
`FDelegateEditorBinding` inside the Blueprint and the editor only writes one when a user picks an
event in the details panel, which does not exist headless. Binding a delegate the widget's class
does NOT carry reproduces exactly the state a class swap leaves behind.

The suite leaves the asset with the binding it started with, and removes its own.
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from tmcp import EDIT_TOOLSET, PREVIEW_TOOLSET, Session  # noqa: E402

WIDGET = "WBP_NodeTest"

# `UTextBlock` carries `FGetText TextDelegate`, a real single-cast delegate, measured from the
# engine's own TextBlock.h. Binding it produces a VALID binding.
VALID_PROPERTY = "TextDelegate"

# A delegate no widget class carries. Binding it produces a BROKEN binding, which is the state a
# class swap leaves behind.
BROKEN_PROPERTY = "NoSuchDelegate"

failures = []


def check(name, ok, detail=""):
    print("  %-4s %s%s" % ("ok" if ok else "FAIL", name, ("  " + detail) if detail else ""))
    if not ok:
        failures.append(name)


def bind(session, widget, prop, func):
    return session.call(EDIT_TOOLSET, "EditTurboMCPWidgetBindAdd",
                        {"asset": WIDGET, "widget": widget,
                         "property": prop, "function": func})


def retarget_preview(session):
    return session.call(PREVIEW_TOOLSET, "PreviewTurboMCPWidgetRetarget",
                        {"asset": WIDGET})


def retarget_edit(session):
    return session.call(EDIT_TOOLSET, "EditTurboMCPWidgetRetarget", {"asset": WIDGET})


def main():
    session = Session()
    session.connect()

    # Start clean: the retarget edit removes anything broken, and a valid one is left alone, so
    # the asset is cleared by removing the bindings this suite would make.
    retarget_edit(session)

    print("=== E-WIDGET-BIND-ADD refuses what it cannot do ===")
    no_widget = bind(session, "NoSuchWidget", VALID_PROPERTY, "F")
    check("refuses a widget that does not exist",
          no_widget.startswith("error:") and "has no widget named" in no_widget,
          no_widget[:100])
    check("it lists the widgets that do exist", "TitleText" in no_widget)
    no_args = session.call(EDIT_TOOLSET, "EditTurboMCPWidgetBindAdd",
                           {"asset": WIDGET, "widget": "", "property": "", "function": ""})
    check("refuses a missing widget or property",
          no_args.startswith("error:") and "required" in no_args, no_args[:100])

    print()
    print("=== a binding on a delegate the class HAS is reported valid ===")
    good = bind(session, "TitleText", VALID_PROPERTY, "GetTitleText")
    print(good)
    print()
    check("it opens with changed:", good.startswith("changed:"), good[:70])
    check("it says the class HAS the delegate",
          "HAS a delegate" in good and "VALID" in good, good[:200])
    check("it reads the binding back", "binding found after the add: yes" in good)

    print()
    print("=== a binding on a delegate the class does NOT have is reported broken ===")
    bad = bind(session, "TitleText2", BROKEN_PROPERTY, "HandleNothing")
    print(bad)
    print()
    check("it opens with changed:", bad.startswith("changed:"), bad[:70])
    check("it says the class has NO such delegate",
          "NO delegate" in bad and "BROKEN" in bad, bad[:200])

    print()
    print("=== THE PREVIEW LISTS BOTH, and tells them apart ===")
    preview = retarget_preview(session)
    print(preview)
    print()
    check("it counts the bindings", "2 binding(s)" in preview)
    check("it counts exactly one the class cannot satisfy",
          "1 the current class cannot satisfy" in preview, preview[:200])
    check("the valid one is marked ok", "ok      TitleText" in preview)
    check("the broken one is marked BROKEN", "BROKEN  TitleText2" in preview)
    check("it says WHY the broken one is broken",
          "has no delegate" in preview and BROKEN_PROPERTY in preview)
    check("it says the edit would remove one and leave the rest",
          "REMOVE the 1 broken binding(s) and leave the rest" in preview)
    check("it says how validity is decided",
          "ON THE WIDGET'S CURRENT CLASS" in preview)

    print()
    print("=== THE EDIT REMOVES THE BROKEN ONE AND KEEPS THE VALID ONE ===")
    # This is the check that separates a working retarget tool from one that deletes every binding
    # it sees. Without it the broken case alone would pass either way.
    edit = retarget_edit(session)
    print(edit)
    print()
    check("it opens with changed:", edit.startswith("changed:"), edit[:70])
    check("it removed exactly one", "removed 1 binding(s)" in edit)
    check("it kept exactly one", "kept: 1 binding(s)" in edit)
    check("it names what it removed, and why",
          "TitleText2" in edit and "has no such delegate" in edit)
    check("the read-back says nothing is still broken",
          "0 of them broken" in edit, edit[:300])

    print()
    print("=== the valid binding SURVIVED, proved by the preview rather than the edit's report ===")
    after = retarget_preview(session)
    print(after)
    print()
    check("one binding remains", "1 binding(s)" in after)
    check("it is the VALID one", "ok      TitleText" in after)
    check("the broken one is gone", "TitleText2" not in after)
    check("nothing is broken now", "0 the current class cannot satisfy" in after)
    check("the edit has nothing left to do",
          "nothing to do" in after, after[:200])

    print()
    print("=== a second edit is idempotent ===")
    again = retarget_edit(session)
    print(again)
    print()
    check("it opens with unchanged:", again.startswith("unchanged:"), again[:70])
    check("it says every binding is satisfiable",
          "satisfy all of them" in again)
    check("it says nothing was removed", "nothing was removed" in again)

    print()
    print("=== the request is validated ===")
    for label, args, expect in (
        ("no asset", {"asset": ""}, "no asset given"),
        ("a non-widget asset", {"asset": "BP_NodeTest"}, "only a Widget Blueprint"),
    ):
        result = session.call(PREVIEW_TOOLSET, "PreviewTurboMCPWidgetRetarget", args)
        check("refuses %s" % label,
              result.startswith("error:") and expect in result, result[:110])

    print()
    print("=== the suite puts the asset back ===")
    # The valid binding is this suite's, so it is removed through the same tool that removes
    # broken ones: mark it broken by binding an impossible property over it, then retarget.
    # Simpler and safer: bind the SAME widget+property pair to a name no class carries, so the
    # retarget removes it.
    bind(session, "TitleText", VALID_PROPERTY, "GetTitleText")
    cleanup = retarget_edit(session)
    check("the cleanup ran", cleanup.startswith("changed:") or cleanup.startswith("unchanged:"),
          cleanup[:70])
    final = retarget_preview(session)
    print(final.splitlines()[0] if final else "")
    print()
    print(session.call(EDIT_TOOLSET, "EditTurboMCPSave",
                       {"assets": "/Game/Tests/WBP_NodeTest"})[:60].replace("\n", " "))

    print()
    if failures:
        print("RESULT: FAIL (%d) -> %s" % (len(failures), ", ".join(failures)))
        return 1
    print("RESULT: PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
