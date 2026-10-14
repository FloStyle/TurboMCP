# ACCEPTANCE ROW THIS SUITE CLAIMS TO PROVE: T-STRUCT-8, and `T03`'s `done when` 3 and 4.
"""`E-SAVE`'s thumbnail halves: the result SAYS the draw runs widget code, and the option that skips
it - plus the NEGATIVE check that was missing when the first version of that option was withdrawn.

=== WHY THIS SUITE EXISTS, AND IT IS A REVERT ===

`bb6467b` reverted `6b4ce69`. That commit had added

    const FString& WithoutThumbnail = FString()

to `EditTurboMCPSave`, on the assumption that a C++ default makes a schema parameter optional. THE
ENGINE'S OWN ERROR SETTLED IT:

    !! isError: Function "EditTurboMCPSave", input param "withoutThumbnail" is required by the
    function input schema Json, but is missing from the incoming request

So the option REMOVED the ability to call the tool without it, and `test-struct` and `test-vcs-write`
went red doing nothing but passing `assets`.

*** AND THE LESSON IS THIS SUITE'S FIRST CHECK. `test-save-option-reachable` asserted that the schema
LISTED an argument mentioning the thumbnail; it said nothing about whether that argument is
REQUIRED. *** `T03`'s own card puts it in one line: REACHABLE IS NOT OPTIONAL.

So the declaration is checked directly. `R-TOOL-SCHEMA` is asked for `EditTurboMCPSave`, and its
signature must be EXACTLY `EditTurboMCPSave(assets)` with that one parameter marked REQUIRED. A later
change that adds a parameter to that tool - defaulted or not - fails here, whatever else is green.

=== THE REQUIREMENT IS AN OR, AND THAT IS WHY THERE ARE TWO HALVES BELOW ===

`Feedbacks/2026-10-13_S2_TurboMCP_graph-edit-tools.md:107-109`:

    SV-03: No protection against a thumbnail crash (medium)
      Observed: The engine crash on the second save happened while the editor drew the widget thumbnail.
      Design:   Add an option to save without a new thumbnail, OR SAY IN THE RESULT THAT THE THUMBNAIL
                DRAW RUNS WIDGET CODE.

Both halves are checked here, because either one discharges `SV-03` and a reader should be able to see
which of them is standing.

=== WHAT IS MEASURED HERE ===

  - the schema of `EditTurboMCPSave`: one parameter, `assets`, REQUIRED. THE CHECK THAT WAS MISSING.
  - `EditTurboMCPSave` called with ONLY `assets`, on a genuinely dirty asset: it writes (`1 saved`) -
    the regression that caused the withdrawal, performed rather than described.
  - `SV-03`'s SECOND BRANCH: a Widget Blueprint saved by the editor's save says so, and says that a
    thumbnail draw instantiates the widget and runs its design-time code.
  - `SV-03`'s FIRST BRANCH: `EditTurboMCPSaveWithoutThumbnail`, called with ONLY `assets`, writes the
    package and SAYS the thumbnail draw was skipped, so the two mechanisms cannot be told apart only
    by their side effect.
  - the option is registered: the Edit toolset's tool list names it.
  - `done when` 4: after the no-thumbnail save, `R-DIRTY-LIST` does NOT name the asset - asked again
    after the recorded re-dirty window, because the reported symptom was "unsaved about ten seconds
    later".

=== WHAT IS NOT MEASURED HERE, SAID PLAINLY ===

  - **THAT A THUMBNAIL DRAW ACTUALLY RAN.** The engine writes its thumbnail lines to the SAVE'S OWN
    output device and not to `GLog` (`FileHelpers.cpp:1332` passes `SaveOutput.GetOutputDevice()` to
    `GEngine->Exec`), so no log capture can see them. MEASURED: the strings "Generating thumbnails"
    and "Rendered thumbnail" appear in no log in this repository. What is checked is WHICH MECHANISM
    ran, which is the part a tool can honestly know, plus the engine's own rule for when a draw
    happens (`UnrealEdSrv.cpp:439-446`).
  - the no-thumbnail path names the file itself, so a package OUTSIDE the project's `/Game` mount is
    refused by name rather than written to a guessed path. THAT REFUSAL IS NOT EXERCISED HERE: this
    project's only dirty packages live under `/Game`, so the branch is unreachable from the MCP
    surface. It is named rather than pretended.

=== THE FIXTURES, AND THE RESIDUE THIS SUITE LEAVES ===

Two tracked fixtures are used: `BP_NodeTest` and the widget `WBP_NodeTest`. Each is dirtied with a
variable whose name carries a per-run token, because a FIXED name is refused on the second run and
leaves the asset clean - and a clean asset never reaches the save at all. The residue is the price of
a re-runnable check; `Tools/run-suites.ps1` restores the fixture with `git checkout` between suites,
which is where it goes.
"""

import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from tmcp import EDIT_TOOLSET, READ_TOOLSET, Session  # noqa: E402

# The Blueprint fixture, in the short form every suite uses for it.
BLUEPRINT = "BP_NodeTest"

# The widget fixture, in the two forms that are PROVEN for these two arguments:
#   `assets` for E-SAVE, as `test-widget-retarget.py:177` and `test-widgets-as-vars.py:125` pass it;
#   `asset` for the writers, as `test-widget-retarget.py:27` passes it.
WIDGET_SAVE = "/Game/Tests/WBP_NodeTest"
WIDGET_ASSET = "WBP_NodeTest"
WIDGET_PACKAGE = "/Game/Tests/WBP_NodeTest"

# The recorded window: the report was that the asset showed as unsaved "about ten seconds" after a
# successful save. The wait is deliberately LONGER than the report.
REDIRTY_WINDOW_SECONDS = 14

# Unique per run, so the suite is re-runnable without a fixture restore. See the header.
TOKEN = str(int(time.time()))[-6:]

failures = []


def check(name, ok, detail=""):
    print("  %-4s %s%s" % ("ok" if ok else "FAIL", name, ("  " + detail) if detail else ""))
    if not ok:
        failures.append(name)


def dirty(session, asset, suffix):
    """Make one asset genuinely dirty, and report which route did it.

    The name carries a per-run token on purpose: a fixed name is refused the second time, which leaves
    the asset clean and skips the code this suite exists to reach.
    """
    variable = "T03_%s%s: float default=1" % (TOKEN, suffix)
    out = session.call(EDIT_TOOLSET, "EditTurboMCPVarsAdd",
                       {"asset": asset, "variables": variable})
    first = out.strip().splitlines()[0][:110] if out.strip() else "(empty answer)"
    print("    dirty %-14s %s" % (asset, first))
    return out, variable


def main():
    session = Session()
    session.connect()

    print("=== THE DECLARATION OF E-SAVE: ONE PARAMETER, REQUIRED (the check that was missing) ===")
    # THE TOOLSET NAME IS TAKEN FROM THE REGISTRY, not guessed. An empty toolset is refused with the
    # full list (P1), and reading the name from that list makes this suite independent of whether the
    # published name carries the class's leading `U`.
    listing = session.call(READ_TOOLSET, "ReadTurboMCPToolSchema", {"toolset": "", "tool": ""})
    names = sorted(set(re.findall(r"[A-Za-z0-9_.]*Toolset", listing)))
    edit_names = [n for n in names if n.endswith("EditToolset")]
    check("the registry's toolset list is reachable", len(names) > 3, "%d name(s)" % len(names))
    check("exactly one Edit toolset is published", len(edit_names) == 1, ", ".join(edit_names))
    if len(edit_names) != 1:
        print()
        print("RESULT: FAIL (the Edit toolset could not be identified, so nothing else can be asked)")
        return 1
    edit_toolset = edit_names[0]

    schema = session.call(READ_TOOLSET, "ReadTurboMCPToolSchema",
                          {"toolset": edit_toolset, "tool": "EditTurboMCPSave"})
    print(schema)
    print()
    check("the schema answers", not schema.startswith("error:"), schema[:160])
    check("THE SIGNATURE IS EXACTLY ONE PARAMETER, NAMED assets",
          "signature: EditTurboMCPSave(assets)" in schema, schema[:200])
    check("the schema counts one parameter", "parameters: 1" in schema, schema[:300])
    check("that parameter is REQUIRED, not merely reachable",
          re.search(r"assets\s*:\s*\w+\s+REQUIRED", schema) is not None)
    # THE CHECK THE WITHDRAWN SUITE DID NOT HAVE, in its narrowest form: the parameter that broke every
    # caller must not be back on this tool. A different new parameter would already fail `parameters: 1`
    # and the signature check above, so this one names the actual regression.
    check("the withdrawn `withoutThumbnail` parameter is NOT back on E-SAVE",
          "withoutthumbnail" not in schema.lower())

    print()
    print("=== the whole Edit toolset, so the option can be seen to be REGISTERED ===")
    toolset_list = session.call(READ_TOOLSET, "ReadTurboMCPToolSchema",
                               {"toolset": edit_toolset, "tool": ""})
    check("the toolset lists its tools", not toolset_list.startswith("error:"), toolset_list[:160])
    check("the option is in the list, so a caller can find it",
          "EditTurboMCPSaveWithoutThumbnail" in toolset_list)
    check("E-SAVE is still in the list, unchanged and callable",
          "EditTurboMCPSave " in toolset_list or "EditTurboMCPSave\n" in toolset_list)

    print()
    print("=== E-SAVE called with ONLY `assets`, on a genuinely dirty asset ===")
    made, _ = dirty(session, BLUEPRINT, "a")
    check("the Blueprint fixture was made dirty", "added" in made or "changed:" in made, made[:140])
    plain = session.call(EDIT_TOOLSET, "EditTurboMCPSave", {"assets": BLUEPRINT})
    print(plain)
    print()
    check("E-SAVE accepted a call with only `assets` - the withdrawal's regression",
          not plain.startswith("error:") and "unchanged: nothing was saved" not in plain, plain[:160])
    check("it wrote the asset", "1 saved" in plain, plain[:160])
    check("it names the mechanism that ran", "written by the editor's save" in plain, plain[:200])

    print()
    print("=== SV-03 SECOND BRANCH: a widget saved by the editor says what its thumbnail draw does ===")
    made, _ = dirty(session, WIDGET_ASSET, "b")
    check("the widget fixture was made dirty", "added" in made or "changed:" in made, made[:140])
    widget_saved = session.call(EDIT_TOOLSET, "EditTurboMCPSave", {"assets": WIDGET_SAVE})
    print(widget_saved)
    print()
    check("the widget saved", "1 saved" in widget_saved, widget_saved[:200])
    check("the answer SAYS a thumbnail draw runs the widget's design-time code",
          "design-time code" in widget_saved and "thumbnail draw" in widget_saved,
          widget_saved[:300])
    check("it names the engine's own condition for when the draw happens",
          "WidgetBlueprintThumbnailRenderer.cpp" in widget_saved)
    check("it names the tool that writes without that step, so the caller can act",
          "EditTurboMCPSaveWithoutThumbnail" in widget_saved)

    print()
    print("=== `done when` 4 on the EDITOR'S save, waited out over the whole re-dirty window ===")
    # *** THE CARD'S OWN CAUSE, PUT TO THE TEST RATHER THAN DESCRIBED. *** `T03` records that the asset came
    # back unsaved "about ten seconds" after a successful save and names the engine's post-save thumbnail
    # write as what re-dirtied it. `EditTurboMCPSave` IS that path, so the window is waited out on ITS
    # answer and not only on the direct write's.
    #
    # AND THE CHECK IS `done when` 4's OWN SENTENCE, BOTH CLAUSES: "the unsaved list does not show the
    # asset, OR IT SAYS WHAT CHANGED THE ASSET AGAIN." So a reappearance is not a failure by itself - a
    # reappearance the result did NOT explain is.
    time.sleep(REDIRTY_WINDOW_SECONDS)
    after_editor_save = session.call(READ_TOOLSET, "ReadTurboMCPDirtyList", {"limit": ""})
    print("    " + after_editor_save.strip().splitlines()[0][:120])
    print("    the widget %s in the unsaved list after the editor's save"
          % ("IS" if WIDGET_PACKAGE in after_editor_save else "is NOT"))
    check("`done when` 4 on the editor's save: not listed again, OR the result named what re-dirtied it",
          (WIDGET_PACKAGE not in after_editor_save) or ("thumbnail draw" in widget_saved),
          after_editor_save[:240])

    print()
    print("=== SV-03 FIRST BRANCH: the option, called with ONLY `assets` ===")
    made, _ = dirty(session, WIDGET_ASSET, "c")
    check("the widget was made dirty again for the second mechanism",
          "added" in made or "changed:" in made, made[:140])
    without = session.call(EDIT_TOOLSET, "EditTurboMCPSaveWithoutThumbnail",
                           {"assets": WIDGET_SAVE})
    print(without)
    print()
    check("the option accepted a call with only `assets`",
          not without.startswith("error:") and "unchanged: nothing was saved" not in without,
          without[:200])
    check("it wrote the asset", "1 saved" in without, without[:200])
    check("it SAYS the thumbnail draw was skipped",
          "without the thumbnail draw" in without, without[:240])
    check("and it says the design-time code did not run for a draw",
          "did not run" in without, without[:320])
    check("it does not claim the editor's save ran",
          "written by the editor's save" not in without)

    print()
    print("=== `done when` 4: the unsaved list must not name it again (%ds later) ===")
    immediate = session.call(READ_TOOLSET, "ReadTurboMCPDirtyList", {"limit": ""})
    check("the read names how many packages it looked at, so a zero is a reading",
          "package(s) dirty" in immediate, immediate[:140])
    check("the widget is not dirty straight after the no-thumbnail save",
          WIDGET_PACKAGE not in immediate, immediate[:300])

    time.sleep(REDIRTY_WINDOW_SECONDS)

    later = session.call(READ_TOOLSET, "ReadTurboMCPDirtyList", {"limit": ""})
    print(later[:400])
    print()
    check("and it is still not named %d seconds later" % REDIRTY_WINDOW_SECONDS,
          WIDGET_PACKAGE not in later, later[:400])

    print()
    if failures:
        print("RESULT: FAIL (%d) -> %s" % (len(failures), ", ".join(failures)))
        return 1
    print("RESULT: PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
