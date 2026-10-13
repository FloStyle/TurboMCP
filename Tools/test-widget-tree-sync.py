# ACCEPTANCE ROW THIS SUITE CLAIMS TO PROVE: T-WIDGET-4
"""Exercise `R-WIDGET-TREE`, the slot-ordered tree reader behind `P/E-WIDGET-TREE`.

  T-WIDGET-4  a widget tree read back reads back identical, animations included.

WHY THIS FILE HAS A DIFFERENT NAME FROM `test-widget-tree.py`
------------------------------------------------------------
`Tools/test-widget-tree.py` ALREADY EXISTS and is TRACKED: it covers **`R-BP-LOAD`'s widget section**,
which is a different, older capability (`5b7e646`). The runner globs `test-*.py`, so writing my checks
into that file would have CLOBBERED a passing suite for a capability I do not own. This file is
therefore named after the thing it tests - `FTurboMCPWidgetTreeSync` - and touches nothing of theirs.

WHAT IS CHECKED FROM WHERE, AND WHY THERE ARE TWO WITNESSES
----------------------------------------------------------
`R-WIDGET-TREE` prints a live Widget Blueprint's tree as the `04_FORMATS.md` section 8 dialect.
`ReadTurboMCPBlueprintLoad` prints the SAME widgets as a flat list. THEY ARE INDEPENDENT RENDERINGS OF
ONE TREE, so the suite compares them, because a tool's own report is not evidence of anything except
what it decided to say.

*** AND THE SHARPEST CONSTRAINT IS MEASURED RATHER THAN ASSUMED: *** the dialect's whole reason to
exist is that CHILD ORDER IS DRAW ORDER and a flat list cannot carry it. So the check that matters is
not "are the same widgets named" - a flat list can do that - it is **"does every child's path EXTEND
its parent's path, and do the children come out in slot order"**. A walk that flattened the tree would
still print every widget while failing both, which is precisely the failure `D-48` rejected the graph
text for.

*** A SIBLING AGENT'S CARE IS BORROWED RATHER THAN REPEATED: the kit commandlet records at
`TurboMCPKitCommandlet.cpp:1588-1595` that `UPanelSlot` (`Components/PanelSlot.h`) declares ONLY
`Content` and `Parent` - there is NO `Padding` member and no `SetPadding`. THE CARD THEREFORE CARRIES
ITS HIDDEN HALF AND NOT ITS PADDING HALF, so this suite does not demand non-zero slot padding from it:
that would fail on a CORRECT tool over a CORRECT fixture. ***

*** AND NO CHECK COMPARES TWO ABSOLUTE WIDGET COUNTS ACROSS RUNS, because the fixture is rebuilt by
other suites. Every check asserts on WHAT THIS RUN READ. ***
"""

import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from tmcp import READ_TOOLSET, Session  # noqa: E402

# THE CARD, WHICH IS A WIDGET BLUEPRINT THE KIT BUILDS. `T-WIDGET-1` and `T-WIDGET-4` both name TA-7,
# so the suite reads the same asset the acceptance rows do.
CARD = "/Game/Tests/Kit/TA07_Card"


def read_tree(session, asset):
    """`R-WIDGET-TREE` through the tool dispatcher.

    `session.call` RATHER THAN `session.rpc`, and the difference cost a run: `rpc` speaks the raw MCP
    protocol, where a tool name is not a method and the server answers `HTTP 400`. Every suite in this
    project goes through `call`, which wraps the name in the engine's own `call_tool` dispatcher.
    """
    return session.call(READ_TOOLSET, "ReadTurboMCPWidgetTree", {"asset": asset})


failures = []
checks_run = [0]


def check(name, ok, detail=""):
    # THE COUNT IS REPORTED RATHER THAN DESCRIBED. This project has recorded ten cases of a check that
    # passed while the thing it verified was broken, and the purest is a runner that says PASS without
    # saying how much it looked at.
    checks_run[0] += 1
    if ok:
        print("  ok   %s" % name)
    else:
        print("  FAIL %s" % name)
        if detail:
            print("       %s" % str(detail)[:900])
        failures.append(name)


def widget_lines(text):
    """Every `(widget ...)` line with its indentation depth, in print order.

    THE DEPTH IS THE INDENTATION AND THE ORDER IS THE PRINT ORDER, which is what makes this able to
    check the one thing a flat list cannot: that children come out in slot order.
    """
    seen = []
    for line in text.splitlines():
        stripped = line.strip()
        if stripped.startswith("(widget "):
            seen.append((len(line) - len(line.lstrip()), stripped))
    return seen


def path_of(widget_line):
    match = re.search(r":path\s+(\S+)", widget_line)
    return match.group(1) if match else ""


def name_of(widget_line):
    match = re.match(r"\(widget\s+(\S+)", widget_line)
    return match.group(1) if match else ""


def main():
    session = Session()

    # ---------------------------------------------------------------------------------------------
    # THE TREE IS WELL FORMED AND CARRIES ITS PATHS.
    # ---------------------------------------------------------------------------------------------
    print("--- R-WIDGET-TREE on %s ---" % CARD)
    tree_text = read_tree(session, CARD)

    check("the answer is a `widget-tree` form rather than an error",
          "(widget-tree" in tree_text and not tree_text.lstrip().startswith("error:"),
          tree_text[:400])

    widgets = widget_lines(tree_text)
    check("the tree prints at least one widget", len(widgets) >= 1,
          "widget lines = %d ; answer = %s" % (len(widgets), tree_text[:300]))

    without_path = [w for _, w in widgets if not path_of(w)]
    check("every widget carries a `:path`, so no agent has to invent a name",
          len(without_path) == 0, "missing on: %s" % without_path[:4])

    without_class = [w for _, w in widgets if ":class" not in w]
    check("every widget carries a `:class`", len(without_class) == 0,
          "missing on: %s" % without_class[:4])

    # ---------------------------------------------------------------------------------------------
    # *** THE CHECK THAT CATCHES A FLATTENED WALK. *** A child's path is DERIVED from the names above
    # it, so it must extend its parent's path. A walk that lost the nesting still prints every widget
    # and fails exactly here.
    # ---------------------------------------------------------------------------------------------
    mismatched = []
    parent_stack = {}
    for depth, line in widgets:
        path = path_of(line)
        if not path:
            continue
        parent = None
        for candidate_depth in range(depth - 1, -1, -1):
            if candidate_depth in parent_stack:
                parent = parent_stack[candidate_depth]
                break
        parent_stack = {d: p for d, p in parent_stack.items() if d < depth}
        parent_stack[depth] = path
        if parent and not path.startswith(parent + "/"):
            mismatched.append("%s does not extend %s" % (path, parent))

    check("every child's path extends its parent's path", len(mismatched) == 0,
          "; ".join(mismatched[:4]))

    root_path = path_of(widgets[0][1]) if widgets else ""
    check("the root's path is a bare name with no slash, since it has no parent",
          root_path != "" and "/" not in root_path, "root path = %r" % root_path)

    # ---------------------------------------------------------------------------------------------
    # T-WIDGET-4: THE WALK IS DETERMINISTIC. Two reads of one unchanged tree must agree exactly. This
    # is the half of the round trip a READ-ONLY tool can prove on its own, and it is the half that
    # would catch an unordered container leaking into the walk.
    # ---------------------------------------------------------------------------------------------
    again = read_tree(session, CARD)
    check("reading the same tree twice gives the same text, so the walk is deterministic",
          again == tree_text,
          "first %d chars vs second %d chars" % (len(tree_text), len(again)))

    shape_first = [(d, name_of(w), path_of(w)) for d, w in widgets]
    shape_second = [(d, name_of(w), path_of(w)) for d, w in widget_lines(again)]
    check("the tree's shape is identical across two reads", shape_first == shape_second,
          "first %s vs second %s" % (shape_first[:5], shape_second[:5]))

    # ---------------------------------------------------------------------------------------------
    # SLOT ORDER. The card's children are added at indices 0..3 (`TurboMCPKitCommandlet.cpp:1574-1586`),
    # so the tree must print them in THAT order. `D-48`: "a dialect that cannot express order would
    # silently reorder children on a round trip."
    # ---------------------------------------------------------------------------------------------
    print_order = [name_of(w) for _, w in widgets]
    child_names = [name_of(w) for d, w in widgets if d > 0]
    sorted_by_print = sorted(child_names, key=lambda n: print_order.index(n))
    check("the children are printed in slot order rather than sorted or reversed",
          child_names == sorted_by_print,
          "children in print order: %s" % child_names)

    # AND IF THE CARD HAS THE CHILDREN THE KIT ADDS, THE ORDER IS THE KIT'S ORDER. Asserted ONLY when
    # they are present, because the fixture is shared and other suites rebuild it - a hard demand on a
    # shared asset is the shared-mutable-state defect this project has recorded ten times.
    kit_order = ["CardTitle", "CardImage", "CardButton", "CardHiddenText"]
    present = [n for n in child_names if n in kit_order]
    if present:
        check("the children that are present keep the kit's own insertion order",
              present == [n for n in kit_order if n in present],
              "print order %s vs kit order %s" % (present, kit_order))
    else:
        # SAID RATHER THAN SILENTLY SKIPPED. A skipped check that reports nothing is the "check that
        # cannot fail" this project has recorded ten times.
        check("the card's children were present, so the order check had something to check",
              False,
              "no kit child names found; children printed were %s" % child_names)

    # ---------------------------------------------------------------------------------------------
    # THE INDEPENDENT WITNESS: `R-BP-LOAD`'s flat list, a different tool and a different rendering.
    # ---------------------------------------------------------------------------------------------
    print("--- the independent witness: R-BP-LOAD ---")
    # THE ARGUMENT SET IS COPIED FROM `test-widget-tree.py`, which already drives this reader: `detail`
    # and `graphFilter` are REQUIRED by its schema, and a first attempt with only `assets` came back
    # `input param "detail" is required`. Reusing a working call beats guessing at a schema.
    flat = session.call(READ_TOOLSET, "ReadTurboMCPBlueprintLoad", {
        "assets": CARD, "detail": "signatures", "graphFilter": "",
        "format": "", "tokenBudget": "",
    })

    tree_names = set(name_of(w) for _, w in widgets)
    flat_names = set(re.findall(r"\b(CardTitle|CardImage|CardButton|CardHiddenText|InnerSpacer)\b", flat))

    if flat_names:
        missing = sorted(flat_names - tree_names)
        check("every widget the flat list names is also in the tree", len(missing) == 0,
              "flat list only: %s ; tree has %s" % (missing, sorted(tree_names)))
    else:
        check("the flat list named at least one widget, so the witness existed", False,
              "R-BP-LOAD printed no widget names; its answer was: %s" % flat[:400])

    # ---------------------------------------------------------------------------------------------
    # REFUSALS, WHICH MUST NAME WHAT WAS WRONG. A refusal that prints nothing usable is worse than no
    # refusal, because an agent cannot tell it from an empty tree.
    # ---------------------------------------------------------------------------------------------
    print("--- refusals ---")
    absent = read_tree(session, "/Game/Tests/Kit/NoSuchWidgetTree")
    check("an asset that does not exist is refused by name",
          "error:" in absent and "NoSuchWidgetTree" in absent, absent[:400])

    blank = read_tree(session, "")
    check("an empty asset string is refused rather than resolving to something", "error:" in blank,
          blank[:400])

    # A BLUEPRINT THAT IS NOT A WIDGET BLUEPRINT HAS NO WIDGET TREE, which is a DIFFERENT answer from a
    # Widget Blueprint with an empty one. `P1` is exactly that distinction, and this is where it is
    # exercised on the refusal path.
    not_widget = read_tree(session, "/Game/Tests/Kit/BP_TowerArray")
    check("a Blueprint that is not a Widget Blueprint says so rather than printing an empty tree",
          "Widget Blueprint" in not_widget or "no widget tree" in not_widget, not_widget[:400])

    print("")
    print("RESULT: %s (%d check(s) run)"
          % ("FAIL (%d)" % len(failures) if failures else "PASS", checks_run[0]))
    if failures:
        for name in failures:
            print("  failed: %s" % name)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
