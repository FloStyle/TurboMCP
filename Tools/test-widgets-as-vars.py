"""Exercise E-WIDGETS-AS-VARS against the running editor.

  T-STRUCT-3  mark widgets as variables
  P4          the Edit opens with `changed:` or `unchanged:`
  P8          a refusal leaves the tree untouched

EDIT ONLY, ON PURPOSE. `03_TOOLS.md` says no Preview twin is needed, because it is a flag change
and the Edit reports old and new state. This suite is the check on that claim: it verifies the
result really does carry the OLD value beside the new one, which is what a preview would have
shown.

WHAT REPLACES TRUSTING THE REPORT
---------------------------------
The tool reports what it changed. That report comes from the same code that changed it, so the
suite reads the result back through R-BP-LOAD, which prints a widget as `variable` from the
dashboard's own flag.
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from tmcp import EDIT_TOOLSET, READ_TOOLSET, Session  # noqa: E402

WIDGET = "WBP_NodeTest"
TWO = "TitleText, TitleText2"

failures = []


def check(name, ok, detail=""):
    print("  %-4s %s%s" % ("ok" if ok else "FAIL", name, ("  " + detail) if detail else ""))
    if not ok:
        failures.append(name)


def widgets(session):
    text = session.call(READ_TOOLSET, "ReadTurboMCPBlueprintLoad",
                        {"assets": WIDGET, "detail": "signatures",
                         "graphFilter": "", "format": "", "tokenBudget": ""})
    marked = set()
    for line in text.splitlines():
        stripped = line.strip()
        for name in ("TitleText", "TitleText2"):
            if stripped.startswith(name + " ") and "variable" in stripped:
                marked.add(name)
    return marked, text


def main():
    session = Session()
    session.connect()

    # Start from a known state: unmark first, so the suite is not order-dependent.
    session.call(EDIT_TOOLSET, "EditTurboMCPWidgetsAsVars",
                 {"asset": WIDGET, "widgets": TWO, "bIsVariable": False})
    before, _ = widgets(session)
    print("=== starting state: %s ===" % (sorted(before) or "neither marked"))

    print()
    print("=== marking two widgets reports the OLD value beside the new one ===")
    out = session.call(EDIT_TOOLSET, "EditTurboMCPWidgetsAsVars",
                       {"asset": WIDGET, "widgets": TWO, "bIsVariable": True})
    print(out)
    print()
    check("the result opens with changed:", out.startswith("changed:"), out[:70])
    check("it counts what it changed", "changed: 2 of 2 widget(s)" in out)
    check("it says what the flag was set to", "set to: variable" in out)
    check("it names the OLD value of the flag", "was not a variable" in out,
          "the report must carry what it was, not only what it is")
    check("it names the NEW value of the flag", "now variable" in out)
    check("it says there is no Preview twin, and why",
          "no Preview twin" in out and "flag change" in out)
    check("it says the asset is not saved", "NOT saved" in out)

    print()
    print("=== the second call is idempotent and says so rather than claiming a change ===")
    again = session.call(EDIT_TOOLSET, "EditTurboMCPWidgetsAsVars",
                         {"asset": WIDGET, "widgets": TWO, "bIsVariable": True})
    print(again)
    print()
    check("the second call opens with unchanged:", again.startswith("unchanged:"),
          again[:70])
    check("it counts zero changed", "unchanged: 0 of 2" in again)
    check("it says how many were already in that state",
          "already in that state: 2" in again)
    check("it names each widget as not touched", again.count("not touched") == 2)

    print()
    print("=== a misspelled name refuses the WHOLE list and leaves the tree untouched ===")
    # The two good names must NOT have been unmarked. That is what makes the refusal atomic
    # rather than partly applied, which is what P8 requires.
    mixed = session.call(EDIT_TOOLSET, "EditTurboMCPWidgetsAsVars",
                         {"asset": WIDGET, "widgets": "TitleText, NoSuchWidget",
                          "bIsVariable": False})
    print(mixed)
    print()
    check("it opens with unchanged:", mixed.startswith("unchanged:"), mixed[:70])
    check("it names the name that does not exist", "NoSuchWidget" in mixed)
    check("it lists the real widget names",
          all(n in mixed for n in ("RootCanvas", "TitleText", "TitleText2")))

    after_refusal, _ = widgets(session)
    check("the good name was NOT touched by the refused call",
          "TitleText" in after_refusal,
          "the refusal must be atomic: %s" % sorted(after_refusal))

    print()
    print("=== unmarking works, and reports the direction ===")
    off = session.call(EDIT_TOOLSET, "EditTurboMCPWidgetsAsVars",
                       {"asset": WIDGET, "widgets": TWO, "bIsVariable": False})
    print(off)
    print()
    check("it opens with changed:", off.startswith("changed:"), off[:70])
    check("it says the flag was set to not a variable", "set to: not a variable" in off)

    print()
    print("=== save, then read the flag back through a DIFFERENT tool ===")
    # The tool reports what it changed; R-BP-LOAD reads the flag from the asset.
    session.call(EDIT_TOOLSET, "EditTurboMCPWidgetsAsVars",
                 {"asset": WIDGET, "widgets": TWO, "bIsVariable": True})
    print(session.call(EDIT_TOOLSET, "EditTurboMCPSave",
                       {"assets": "/Game/Tests/WBP_NodeTest"})[:70].replace("\n", " "))
    marked, text = widgets(session)
    for line in text.splitlines():
        if "TitleText" in line:
            print("    " + line.rstrip())
    print()
    check("TitleText reads as a variable after the save", "TitleText" in marked)
    check("TitleText2 reads as a variable after the save", "TitleText2" in marked)
    check("the root canvas is NOT a variable", "RootCanvas" not in marked)

    print()
    print("=== the request is validated before anything is touched ===")
    for label, args, expect in (
        ("no asset", {"asset": "", "widgets": TWO, "bIsVariable": True}, "no asset given"),
        ("no widgets", {"asset": WIDGET, "widgets": "", "bIsVariable": True},
         "no widgets given"),
        ("a non-widget asset", {"asset": "BP_NodeTest", "widgets": TWO,
                                "bIsVariable": True}, "only a Widget Blueprint"),
    ):
        out = session.call(EDIT_TOOLSET, "EditTurboMCPWidgetsAsVars", args)
        check("refuses %s" % label, out.startswith("error:") and expect in out,
              out[:90])

    print()
    if failures:
        print("RESULT: FAIL (%d) -> %s" % (len(failures), ", ".join(failures)))
        return 1
    print("RESULT: PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
