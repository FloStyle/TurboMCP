# ACCEPTANCE ROW THIS SUITE CLAIMS TO PROVE: T-STRUCT-5
"""Exercise E-FUNCTION-ADD and E-FUNCTION-REMOVE against the running editor.

  T-STRUCT-5  add and remove a function
  P4          the Edit opens with `changed:` or `unchanged:`
  P8          a refusal changes nothing

THE TWO CHECKS THAT MATTER
--------------------------
1. The THREE REFUSALS are told apart. A name taken on this Blueprint, a name the parent declares,
   and a name that is not an identifier are different mistakes and each must name its own reason.
   The parent one must point at P/E-OVERRIDE-ADD.

2. THE SILENT RENAME IS CAUGHT. `AddFunctionGraph` does not use the name it is given: it uniques it
   against the objects still in the package. So asking for a name something still holds creates
   `Name_0` and the call reports SUCCESS. The first version of this tool reported
   "added function 'ComputeScore'" while the graph was called `ComputeScore_0`, and nothing in its
   own report showed it. It was found by removing a function, re-adding the same name, and reading
   the list back.

This suite runs its own function name and removes it at the end, so the shared asset is left as it
was found and the suite is re-runnable.
"""

import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from tmcp import EDIT_TOOLSET, READ_TOOLSET, Session  # noqa: E402

ASSET = "BP_NodeTest"

# A name of this suite's own, so it does not collide with the asset's real functions and can be
# removed again.
OWNED = "TStructFiveProbe"

# A name the parent class declares. From R-OVERRIDES, so it is a real one.
PARENT = "ReceiveDestroyed"

failures = []


def check(name, ok, detail=""):
    print("  %-4s %s%s" % ("ok" if ok else "FAIL", name, ("  " + detail) if detail else ""))
    if not ok:
        failures.append(name)


def graphs(session):
    text = session.call(READ_TOOLSET, "ReadTurboMCPBlueprintLoad",
                        {"assets": ASSET, "detail": "manifest",
                         "graphFilter": "", "format": "", "tokenBudget": ""})
    names = []
    collecting = False
    for line in text.splitlines():
        stripped = line.strip()
        if stripped.startswith("graphs:"):
            collecting = True
            continue
        if collecting:
            if stripped.startswith("variables:") or stripped.startswith("counts:"):
                break
            if stripped:
                names.append(stripped)
    return names, text


def main():
    session = Session()
    session.connect()

    before, _ = graphs(session)
    print("=== starting state: %d graph(s): %s ===" % (len(before), ", ".join(before)))
    if OWNED in before:
        print("  %s is already there, left by an unfinished run; removing it first." % OWNED)
        session.call(EDIT_TOOLSET, "EditTurboMCPFunctionRemove",
                     {"asset": ASSET, "name": OWNED})

    print()
    print("=== E-FUNCTION-ADD creates a function of the name asked for ===")
    out = session.call(EDIT_TOOLSET, "EditTurboMCPFunctionAdd",
                       {"asset": ASSET, "name": OWNED})
    print(out)
    print()
    check("it opens with changed:", out.startswith("changed:"), out[:70])
    check("it names the function", "'%s'" % OWNED in out)
    # THE CREATED GRAPH NAME IS CHECKED AGAINST THE ONE ASKED FOR. This is the silent-rename
    # check seen from the outside: a graph called `TStructFiveProbe_0` would mean the tool
    # reported a name it did not create.
    check("the graph it created carries exactly the name asked for",
          "created: graph '%s'" % OWNED in out, out[:160])
    check("the editor reports it as implemented",
          "the editor now reports it as implemented: yes" in out)

    after_add, _ = graphs(session)
    check("the function is on the Blueprint, read by R-BP-LOAD",
          OWNED in after_add, ", ".join(after_add))

    print()
    print("=== a name already on this Blueprint is refused, and named ===")
    again = session.call(EDIT_TOOLSET, "EditTurboMCPFunctionAdd",
                         {"asset": ASSET, "name": OWNED})
    print(again)
    print()
    check("it opens with unchanged:", again.startswith("unchanged:"), again[:70])
    check("it says the name is already a function", "already a function" in again)
    check("it names this Blueprint as the holder", "on this Blueprint" in again)

    print()
    print("=== a name the PARENT declares is refused DIFFERENTLY ===")
    parent = session.call(EDIT_TOOLSET, "EditTurboMCPFunctionAdd",
                          {"asset": ASSET, "name": PARENT})
    print(parent)
    print()
    check("it opens with unchanged:", parent.startswith("unchanged:"), parent[:70])
    check("it names the parent class", "parent class" in parent)
    check("IT POINTS AT P/E-OVERRIDE-ADD",
          "P/E-OVERRIDE-ADD" in parent,
          "the spec requires the refusal to send the caller to the override tool")
    check("it is NOT reported as a plain clash",
          "already a function" not in parent,
          "the two refusals must be distinguishable")

    print()
    print("=== a name that is not an identifier is refused with the character ===")
    for label, bad, expect in (
        ("a space", "Has Space", "is not a letter, a digit or an underscore"),
        ("a leading digit", "1Bad", "does not start with a letter or underscore"),
        ("a dash", "Has-Dash", "is not a letter, a digit or an underscore"),
    ):
        result = session.call(EDIT_TOOLSET, "EditTurboMCPFunctionAdd",
                              {"asset": ASSET, "name": bad})
        check("refuses %s" % label,
              result.startswith("unchanged:") and expect in result, result[:100])

    print()
    print("=== E-FUNCTION-REMOVE takes it away and says the name is clear ===")
    removed = session.call(EDIT_TOOLSET, "EditTurboMCPFunctionRemove",
                           {"asset": ASSET, "name": OWNED})
    print(removed)
    print()
    check("it opens with changed:", removed.startswith("changed:"), removed[:70])
    check("it names the graph it removed", "removed: graph '%s'" % OWNED in removed)
    # The claim is deliberately NARROW: the tool cannot see the package, so it must not promise
    # more than that no function graph holds the name.
    check("it claims only what it can check",
          "no function graph holds that name now: yes" in removed, removed[:200])
    check("it does NOT overclaim that the package released the name",
          "does NOT guarantee" in removed,
          "the package can still hold the name; the next add may be renamed")

    after_remove, _ = graphs(session)
    check("the function is gone from the Blueprint", OWNED not in after_remove)
    check("the graph count is back where it started",
          len(after_remove) == len(before),
          "found %d, now %d" % (len(before), len(after_remove)))

    print()
    print("=== THE SILENT RENAME: re-adding the same name must NOT leave a numbered graph ===")
    # `AddFunctionGraph` uniques the name against the objects still in the package, so it can
    # create `TStructFiveProbe_0` and still return a graph. The tool must catch that, remove
    # what it made, and refuse rather than report a success under a name it did not create.
    readd = session.call(EDIT_TOOLSET, "EditTurboMCPFunctionAdd",
                         {"asset": ASSET, "name": OWNED})
    print(readd)
    print()
    check("the result is honest about what happened",
          readd.startswith("changed:") or readd.startswith("unchanged:"), readd[:70])

    if readd.startswith("unchanged:"):
        # The engine renamed it and the guard caught it.
        check("it says the engine would not use the name",
              "would not create a function named" in readd, readd[:200])
        check("it names what the engine made instead", "_0" in readd)
        check("it says the numbered graph was removed again",
              "removed again" in readd)
        numbered, _ = graphs(session)
        check("NO numbered graph was left behind",
              not any(g.startswith(OWNED + "_") for g in numbered),
              ", ".join(numbered))
    else:
        # The engine used the name, which is the good outcome: nothing to guard against.
        check("it created the exact name, so no rename happened",
              "created: graph '%s'" % OWNED in readd, readd[:160])
        check("no numbered graph exists either way",
              not any(g.startswith(OWNED + "_") for g in graphs(session)[0]))

    print()
    print("=== removing a name that is not there is refused with the real list ===")
    missing = session.call(EDIT_TOOLSET, "EditTurboMCPFunctionRemove",
                           {"asset": ASSET, "name": "NoSuchFunctionHere"})
    print(missing)
    print()
    check("it opens with unchanged:", missing.startswith("unchanged:"), missing[:70])
    check("it lists what the Blueprint does have",
          "UserConstructionScript" in missing, missing[:200])

    print()
    print("=== the request is validated before anything is created ===")
    for label, args, expect in (
        # An empty asset never reaches the name check: the shared resolver refuses it first, with
        # its own message. That is the right order, so the check accepts either wording.
        ("no asset", {"asset": "", "name": OWNED}, "no asset given"),
        ("no name", {"asset": ASSET, "name": ""}, "no function name given"),
        ("a non-Blueprint", {"asset": "TA18_ColourSuite", "name": OWNED},
         "not a Blueprint"),
    ):
        result = session.call(EDIT_TOOLSET, "EditTurboMCPFunctionAdd", args)
        check("refuses %s" % label,
              result.startswith("error:") and expect in result, result[:100])

    print()
    print("=== the suite leaves the asset as it found it ===")
    session.call(EDIT_TOOLSET, "EditTurboMCPFunctionRemove",
                 {"asset": ASSET, "name": OWNED})
    final, _ = graphs(session)
    check("the graph list matches the starting state",
          sorted(final) == sorted(before),
          "found %s, now %s" % (sorted(before), sorted(final)))
    print(session.call(EDIT_TOOLSET, "EditTurboMCPSave",
                       {"assets": ASSET})[:60].replace("\n", " "))

    print()
    if failures:
        print("RESULT: FAIL (%d) -> %s" % (len(failures), ", ".join(failures)))
        return 1
    print("RESULT: PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
