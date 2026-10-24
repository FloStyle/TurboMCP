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
                names.append(re.sub(r" \[(?:event graph|function|macro)\]$", "", stripped))
    return names, text


def main():
    session = Session()
    session.connect()

    before, _ = graphs(session)
    print("=== starting state: %d graph(s): %s ===" % (len(before), ", ".join(before)))
    if OWNED in before:
        print("  %s is already there, left by an unfinished run; removing it first." % OWNED)
        session.call(EDIT_TOOLSET, "EditTurboMCPFunctionRemove",
                     {"asset": ASSET, "name": OWNED, "inputs": "", "outputs": ""})

    print()
    print("=== E-FUNCTION-ADD creates a function of the name asked for ===")
    out = session.call(EDIT_TOOLSET, "EditTurboMCPFunctionAdd",
                       {"asset": ASSET, "name": OWNED, "inputs": "", "outputs": ""})
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
                         {"asset": ASSET, "name": OWNED, "inputs": "", "outputs": ""})
    print(again)
    print()
    check("it opens with unchanged:", again.startswith("unchanged:"), again[:70])
    check("it says the name is already a function", "already a function" in again)
    check("it names this Blueprint as the holder", "on this Blueprint" in again)

    print()
    print("=== a name the PARENT declares is refused DIFFERENTLY ===")
    parent = session.call(EDIT_TOOLSET, "EditTurboMCPFunctionAdd",
                          {"asset": ASSET, "name": PARENT, "inputs": "", "outputs": ""})
    print(parent)
    print()
    check("it opens with unchanged:", parent.startswith("unchanged:"), parent[:70])
    check("it names the parent class", "parent class" in parent)
    check("IT POINTS AT P/E-OVERRIDE-ADD",
          "PreviewTurboMCPOverrideAdd" in parent and "EditTurboMCPOverrideAdd" in parent,
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
                              {"asset": ASSET, "name": bad, "inputs": "", "outputs": ""})
        check("refuses %s" % label,
              result.startswith("unchanged:") and expect in result, result[:100])

    print()
    print("=== E-FUNCTION-REMOVE takes it away and says the name is clear ===")
    removed = session.call(EDIT_TOOLSET, "EditTurboMCPFunctionRemove",
                           {"asset": ASSET, "name": OWNED, "inputs": "", "outputs": ""})
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
                         {"asset": ASSET, "name": OWNED, "inputs": "", "outputs": ""})
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
        ("no asset", {"asset": "", "name": OWNED, "inputs": "", "outputs": ""}, "no asset given"),
        ("no name", {"asset": ASSET, "name": "", "inputs": "", "outputs": ""}, "no function name given"),
        ("a non-Blueprint", {"asset": "TA18_ColourSuite", "name": OWNED, "inputs": "", "outputs": ""},
         "not a Blueprint"),
    ):
        result = session.call(EDIT_TOOLSET, "EditTurboMCPFunctionAdd", args)
        check("refuses %s" % label,
              result.startswith("error:") and expect in result, result[:100])

    # --------------------------------------------------------------------------------------------
    # THE TWO DEFAULTED PARAMETERS, `Inputs` AND `Outputs`, ARE EXERCISED HERE AND NOWHERE ELSE.
    #
    # Release finding R04 recorded that the coverage audit's parser reported a parameter literally named
    # `TEXT("")` for `const FString& Inputs = TEXT("")`. That was a parser false positive and it is fixed
    # (Tools/audit-tool-coverage.py, `split_parameters`). THE FIX EXPOSED A REAL GAP BENEATH THE FALSE
    # POSITIVE: with the names parsed correctly, the audit then reported that `Inputs` and `Outputs` appear
    # in NO sample anywhere, because every caller in this suite omitted them and took the empty defaults.
    # A tool whose signature parameters are never supplied is a tool a caller has to guess at.
    #
    # The signature syntax is `Name: Type`, comma-separated, from `ParseSignature` in
    # TurboMCPFunction.cpp. THE TYPES ARE ONES THE RESOLVER ACCEPTS, WHICH `FString` IS NOT - MEASURED:
    #   "input pin 'Label' has invalid type 'FString': 'FString' is not a type or a loadable class"
    # so this case asserts what the engine supports rather than a type name that merely reads well.
    # It supplies BOTH parameters, reads back the pin names the tool reports it created, and removes the
    # graph again, so the shared asset is left as it was found.
    #
    # *** EACH TRIAL USES ITS OWN NAME, AND THAT IS THE MEASURED FIX FOR THIS CASE. ***
    #
    # The first version drove every trial through ONE name and re-added it after removing it, and the engine
    # refused the second and later attempts:
    #
    #   unchanged: the engine would not create a function named 'TStructFiveProbeSigned' and made
    #              'TStructFiveProbeSigned_0' instead, because something in the package still holds that name.
    #
    # The tool was RIGHT to refuse and RIGHT to say so; the case was reusing a name the package had not
    # released. Distinct names remove the contention, so each trial measures the signature rather than the
    # package's name lifetime. The tool's own refusal of an unreleased name is asserted separately, below.
    # --------------------------------------------------------------------------------------------
    print()
    print("=== the Inputs and Outputs parameters are supplied, not defaulted ===")

    def signed_trial(suffix, inputs, outputs):
        """One trial with a name nothing else uses, then its own cleanup."""
        name = OWNED + "Signed" + suffix
        session.call(EDIT_TOOLSET, "EditTurboMCPFunctionRemove", {"asset": ASSET, "name": name})
        answer = session.call(EDIT_TOOLSET, "EditTurboMCPFunctionAdd",
                             {"asset": ASSET, "name": name, "inputs": inputs, "outputs": outputs})
        session.call(EDIT_TOOLSET, "EditTurboMCPFunctionRemove", {"asset": ASSET, "name": name})
        return name, answer

    signature_probe = OWNED + "SignedA"
    signature_probe, signed = signed_trial("A", "Score: float, Weight: int", "Total: float")
    print(signed)
    print()
    check("a signature is accepted with both parameters supplied",
          signed.startswith("changed:"), signed[:100])
    check("the input pins are reported as created",
          "created inputs: Score, Weight" in signed,
          str([line for line in signed.splitlines() if "created inputs" in line]))
    check("the output pin is reported as created",
          "created outputs: Total" in signed,
          str([line for line in signed.splitlines() if "created outputs" in line]))
    check("no input pin was lost to a rename", "lost links by pin name: (none)" in signed)
    check("the editor reports it as implemented", "reports it as implemented: yes" in signed)

    # A SECOND trial, with its own name, so the case proves the two parameters work twice rather than once.
    second_name, second = signed_trial("B", "Score: float, Count: int", "Sum: float")
    check("a second signature with different names also succeeds",
          second.startswith("changed:") and "created inputs: Score, Count" in second
          and "created outputs: Sum" in second, second[:200])

    remaining, _ = graphs(session)
    check("both signed probes were removed again",
          signature_probe not in remaining and second_name not in remaining,
          ", ".join(remaining))

    print()
    print("=== invalid signature text is refused before anything is created ===")
    for label, bad_inputs, why in (
        ("a pin without a type", "Score", "must be 'Name: Type'"),
        ("a type that does not resolve", "Score: NotARealTypeAtAll", "invalid type"),
        ("the same pin name twice", "Score: float, Score: float", "invalid or duplicated"),
    ):
        before_bad, _ = graphs(session)
        refused = session.call(EDIT_TOOLSET, "EditTurboMCPFunctionAdd",
                               {"asset": ASSET, "name": signature_probe,
                                "inputs": bad_inputs, "outputs": ""})
        after_bad, _ = graphs(session)
        check("refuses %s" % label,
              refused.startswith("unchanged:") and why in refused, refused[:120])
        check("%s created no graph" % label,
              sorted(after_bad) == sorted(before_bad),
              "before %s after %s" % (sorted(before_bad), sorted(after_bad)))

    print()
    print("=== the suite leaves the asset as it found it ===")
    session.call(EDIT_TOOLSET, "EditTurboMCPFunctionRemove",
                 {"asset": ASSET, "name": OWNED, "inputs": "", "outputs": ""})
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
