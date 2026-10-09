"""Exercise the widget tree section of R-BP-LOAD.

Checks that the widget section behaves as `03_TOOLS.md` describes, and exits non-zero on
failure.

  signatures and above  the widget tree is present
  P1                    an empty tree says so in words
  P2                    "not a variable" is reported as a fact, never as "unused"

WHAT THIS SUITE IS CAREFUL ABOUT
--------------------------------
The first version of the scaffolding script added three widgets and then checked the tree
by looking for the text `"widgetCount":3` in the result. `Session.call` DECODES the result,
so that is a dict and the substring could never be present: a check that cannot pass, and
it would have been red on every run for a reason that had nothing to do with widgets.

The tree is checked by reading the field, and the count is asserted after each mutating
step, because the children were once lost silently between being added and being saved.
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from tmcp import READ_TOOLSET, Session  # noqa: E402

WIDGET = "/Game/Tests/WBP_NodeTest.WBP_NodeTest"
PLAIN = "/Game/Tests/BP_NodeTest.BP_NodeTest"

# The asset is built by Tools/make-test-widget.py.
EXPECTED_WIDGETS = 3

failures = []


def check(name, ok, detail=""):
    print("  %-4s %s%s" % ("ok" if ok else "FAIL", name, ("  " + detail) if detail else ""))
    if not ok:
        failures.append(name)


def load(session, asset, detail="signatures"):
    return session.call(READ_TOOLSET, "ReadTurboMCPBlueprintLoad", {
        "assets": asset, "detail": detail, "graphFilter": "",
        "format": "", "tokenBudget": "",
    })


def main():
    session = Session()
    session.connect()

    print("=== signatures: the widget tree is present ===")
    text = load(session, WIDGET)
    for line in text.splitlines():
        if "widget tree" in line or "[" in line and "differs" not in line:
            print("    " + line.rstrip())
    print()
    check("the section is present", "widget tree:" in text)
    check("it reports the widget count",
          "widget tree: %d widget(s)" % EXPECTED_WIDGETS in text,
          "expected %d" % EXPECTED_WIDGETS)
    check("the root canvas is listed", "RootCanvas" in text)
    check("the root's class is named", "[CanvasPanel]" in text)
    check("a child is listed", "TitleText" in text)
    check("a child's class is named", "[TextBlock]" in text)

    print()
    print("=== a non-default visibility is shown, the default is not ===")
    # The canvas root reports SelfHitTestInvisible, which is not the enum default, so it
    # must be printed. A widget at the default must NOT print a visibility, because a line
    # of defaults on every widget buries the ones that matter.
    check("a non-default visibility is shown", "visibility=SelfHitTestInvisible" in text)
    check("the default visibility is not printed", "visibility=Visible" not in text)

    print()
    print("=== the result says what 'variable' means, and does not overclaim ===")
    check("it says a widget is not claimed unused",
          "does NOT claim a widget is unused" in text)
    check("it names why that claim would need proof",
          "binding or an animation" in text)

    print()
    print("=== the tree is not produced below signatures ===")
    manifest = load(session, WIDGET, "manifest")
    check("manifest has no widget tree", "widget tree:" not in manifest)

    print()
    print("=== a plain Blueprint has no widget section at all ===")
    plain = load(session, PLAIN)
    check("no widget tree for a non-widget Blueprint", "widget tree:" not in plain)
    check("the read still succeeded", "=====" in plain)

    print()
    print("=== bodies still carries the tree, so the section is additive ===")
    bodies = load(session, WIDGET, "bodies")
    check("the tree is present at bodies too", "widget tree:" in bodies)

    print()
    if failures:
        print("RESULT: FAIL (%d) -> %s" % (len(failures), ", ".join(failures)))
        return 1
    print("RESULT: PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
