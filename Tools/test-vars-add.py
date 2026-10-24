# ACCEPTANCE ROW THIS SUITE CLAIMS TO PROVE: T-STRUCT-1
"""Exercise the P/E-VARS-ADD pair against the running editor.

  T-STRUCT-1  add variables in bulk
  P4          the Edit opens with `changed:` or `unchanged:`
  D-02        the Preview and the Edit share one code path

THE CHECK THAT MATTERS
----------------------
`E-VARS-ADD` reports how many variables it added AND how many it read back. Both numbers come
from the same code, so neither is evidence on its own. The suite therefore reads the result
back through a DIFFERENT tool — `R-BP-LOAD` — after a SAVE, which is the only way to show the
variables are really on the Blueprint rather than in the writer's report.

The flags and the category are checked the same way, because a flag that was set in memory and
lost by the save is exactly the failure this project keeps finding.
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from tmcp import EDIT_TOOLSET, PREVIEW_TOOLSET, READ_TOOLSET, Session  # noqa: E402

ASSET = "BP_NodeTest"
WIDGET = "/Game/Tests/WBP_NodeTest.WBP_NodeTest"

# Names of their own, so a rerun meets the refusal path rather than a clash with itself.
VARS = ("Speed: float default=250 category=Movement, "
        "bIsArmed: bool default=false readonly=true, "
        "Label: string default=Hello")

failures = []


def check(name, ok, detail=""):
    print("  %-4s %s%s" % ("ok" if ok else "FAIL", name, ("  " + detail) if detail else ""))
    if not ok:
        failures.append(name)


def main():
    session = Session()
    session.connect()

    print("=== the preview parses and prints the plan without writing ===")
    preview = session.call(PREVIEW_TOOLSET, "PreviewTurboMCPVarsAdd",
                           {"asset": ASSET, "variables": VARS})
    print(preview)
    print()
    check("the preview names the asset", ASSET in preview)
    check("preview states target validation limits", "not the target Blueprint or existing names" in preview)
    check("preview warns about partial changes", "can leave a partial list" in preview)
    check("preview distinguishes full compilation", "does not perform a full compile or save" in preview)

    check("the preview counts the variables", "add 3 variable(s)" in preview)
    check("it prints each type", all(t in preview for t in ("float", "bool", "string")))
    check("it prints the default", "default 250" in preview)
    check("it prints the category", "category Movement" in preview)
    check("it prints the flags", "read only" in preview)

    print()
    print("=== the edit adds them, or refuses if they are already there ===")
    edit = session.call(EDIT_TOOLSET, "EditTurboMCPVarsAdd",
                        {"asset": ASSET, "variables": VARS})
    print(edit)
    print()
    check("the edit opens with changed: or unchanged:, per P4",
          edit.startswith("changed:") or edit.startswith("unchanged:"), edit[:70])
    check("the edit reads the variables back",
          "read back from the Blueprint: 3 of 3" in edit
          or edit.startswith("unchanged:"), edit[:120])

    print()
    print("=== a second call with the same names is refused, adding nothing ===")
    again = session.call(EDIT_TOOLSET, "EditTurboMCPVarsAdd",
                         {"asset": ASSET, "variables": VARS})
    print(again)
    print()
    check("the second call is refused", again.startswith("unchanged:"), again[:80])
    check("it names what already exists", all(name in again for name in ("Speed", "bIsArmed", "Label"))
          and "nothing was added" in again)

    print()
    print("=== save, then read it back through a DIFFERENT tool ===")
    # The point of a different tool is that it cannot share the writer's belief about what it
    # did. R-BP-LOAD reads the Blueprint's own variable list.
    print(session.call(EDIT_TOOLSET, "EditTurboMCPSave", {"assets": ASSET})[:90].replace("\n", " "))
    described = session.call(READ_TOOLSET, "ReadTurboMCPBlueprintLoad",
                             {"assets": ASSET, "detail": "signatures",
                              "graphFilter": "", "format": "", "tokenBudget": ""})
    for line in described.splitlines():
        if any(n in line for n in ("Speed", "bIsArmed", "Label", "category:", "flags:")):
            print("    " + line.rstrip())
    print()

    for name, type_text in (("Speed", "Float"), ("bIsArmed", "Boolean"), ("Label", "String")):
        check("%s is on the Blueprint, read by R-BP-LOAD" % name,
              "%s : %s" % (name, type_text) in described,
              "%s : %s" % (name, type_text))

    # The category is the field most likely to be set in memory and lost, so it is checked
    # by name rather than by counting lines.
    check("the category survived the save", "category: Movement" in described)
    # `CPF_BlueprintReadOnly` is how the engine expresses read-only, and the reader prints it.
    check("the read-only flag survived the save",
          "BlueprintReadOnly" in described)

    print()
    print("=== the plan refuses what it cannot do, naming the reason ===")
    for label, args, expect in (
        ("an unknown type",
         {"asset": ASSET, "variables": "X: banana"}, "is not a type"),
        ("an unknown field",
         {"asset": ASSET, "variables": "X: float wat=1"}, "unknown field"),
        ("a duplicate name",
         {"asset": ASSET, "variables": "X: float, X: bool"}, "given twice"),
        ("a name with a space",
         {"asset": ASSET, "variables": "Has Space: float"}, "cannot be a variable name"),
        ("no colon",
         {"asset": ASSET, "variables": "JustAName"}, "has no colon"),
        ("an unbalanced type",
         {"asset": ASSET, "variables": "X: Array<float"}, "never closed"),
        ("an empty list", {"asset": ASSET, "variables": ""}, "no variables given"),
        ("no asset", {"asset": "", "variables": "X: float"}, "no asset given"),
        ("a container of an unknown type",
         {"asset": ASSET, "variables": "X: Array<banana>"}, "is not a type"),
    ):
        out = session.call(PREVIEW_TOOLSET, "PreviewTurboMCPVarsAdd", args)
        check("refuses %s" % label,
              out.startswith("error:") and expect in out, out[:100])

    print()
    print("=== a container type parses as ONE variable, comma and all ===")
    # `Map<String, Int>` must not be split into two items. `Array<float>` must not be split
    # either, and this is the reason the splitter counts depth rather than splitting commas.
    container = session.call(PREVIEW_TOOLSET, "PreviewTurboMCPVarsAdd",
                             {"asset": ASSET, "variables": "Lookup: Map<String, int>"})
    print(container)
    print()
    check("a map is one variable", "add 1 variable(s)" in container, container[:90])
    check("the map type is printed whole", "Map<String, int>" in container)

    print()
    if failures:
        print("RESULT: FAIL (%d) -> %s" % (len(failures), ", ".join(failures)))
        return 1
    print("RESULT: PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
