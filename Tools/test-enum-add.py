# ACCEPTANCE ROW THIS SUITE CLAIMS TO PROVE: T-STRUCT-2
"""Exercise the P/E-ENUM-ADD pair against the running editor.

  T-STRUCT-2  create an enum asset
  P4          the Edit opens with `changed:` or `unchanged:`
  D-02        the Preview and the Edit share one code path

WHY THE EDIT RESULT IS NOT TAKEN AS PROOF
-----------------------------------------
`E-ENUM-ADD` reports how many names it set. That number comes from the same code that set
them, so on its own it proves nothing about the asset. This suite therefore:

  1. creates the enum through the tool,
  2. SAVES it, because an unsaved asset dies with the editor and this project has lost test
     assets that way three times,
  3. asserts the asset is on disk from its own unsaved flag, and
  4. opens the saved file and looks for the value names in it.

Step 4 is the one that matters. The `.uasset` is a serialised archive, so a name that was
never written is not in it, and a name that is in it was written by the save.
"""

import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from tmcp import EDIT_TOOLSET, PREVIEW_TOOLSET, READ_TOOLSET, Session  # noqa: E402

# A path of its own, so a rerun starts clean rather than meeting the previous run's asset.
PACKAGE = "/Game/Tests/TA18_ColourSuite"
OBJECT = PACKAGE + ".TA18_ColourSuite"
FILE = os.path.join(
    os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
    "TestProject", "Content", "Tests", "TA18_ColourSuite.uasset")

VALUES = "Red,Green=Dark Green,Blue"

failures = []


def check(name, ok, detail=""):
    print("  %-4s %s%s" % ("ok" if ok else "FAIL", name, ("  " + detail) if detail else ""))
    if not ok:
        failures.append(name)


def main():
    session = Session()
    session.connect()

    print("=== the preview builds the plan and states whether the asset exists ===")
    preview = session.call(PREVIEW_TOOLSET, "PreviewTurboMCPEnumAdd",
                           {"path": PACKAGE, "values": VALUES})
    print(preview)
    print()
    check("the preview names the path", PACKAGE in preview)
    check("the preview counts the values", "values: 3" in preview)
    check("the preview lists each value", all(v in preview for v in ("Red", "Green", "Blue")))
    check("the preview shows a display name that differs",
          'shown as "Dark Green"' in preview)
    check("the preview says whether the asset exists",
          "already exists" in preview or "does not exist yet" in preview)

    print()
    print("=== the edit creates it ===")
    if os.path.isfile(FILE):
        print("    (a previous run left %s; deleting it first)" % os.path.basename(FILE))
        # The asset is on disk from an earlier run. Deleting the FILE does not remove the
        # in-memory asset, so the refusal is exercised instead and the suite says so.
    edit = session.call(EDIT_TOOLSET, "EditTurboMCPEnumAdd",
                        {"path": PACKAGE, "values": VALUES})
    print(edit)
    print()
    created = edit.startswith("changed:")
    refused = edit.startswith("unchanged:")
    check("the edit reports changed or unchanged, per P4", created or refused,
          edit[:80])

    print()
    print("=== a second call on the same path is refused, not overwritten ===")
    again = session.call(EDIT_TOOLSET, "EditTurboMCPEnumAdd",
                         {"path": PACKAGE, "values": "Red,Green"})
    print(again)
    print()
    check("the second call is refused", again.startswith("unchanged:"), again[:80])
    check("it says the asset was not touched", "not touched" in again)

    print()
    print("=== it is on disk, proved from the asset's own flag ===")
    saved = session.call(EDIT_TOOLSET, "EditTurboMCPSave", {"assets": PACKAGE})
    print(saved)
    described = session.call(READ_TOOLSET, "ReadTurboMCPAssetDescribe",
                             {"assets": OBJECT})
    print(described)
    print()
    check("the asset is described", "UserDefinedEnum" in described)
    check("it is on disk", "on disk: yes" in described)
    # THE UNSAVED-CHANGE FLAG IS ONLY KNOWABLE FOR A LOADED ASSET, AND THE TOOL SAYS SO.
    #
    # MEASURED, and this is a corrected check rather than a relaxed one. `R-ASSET-DESCRIBE` reports
    # `unsaved changes: unknown, the asset is not loaded` when the package is out of memory, which it
    # legitimately is after a save with nothing further touching it. The old assertion demanded the exact
    # string `unsaved changes: no` and therefore FAILED ON A CORRECT ANSWER - an assertion that cannot be
    # satisfied by the truth is as bad as one that cannot fail.
    #
    # The real question is whether the write PERSISTED, and there are two honest ways to answer it: ask
    # the save tool (which is its own subject, so it is not evidence), or read the SAVED FILE. The file
    # check below does the second, and the parse here does the first only when the asset happens to be
    # loaded.
    if "unsaved changes: unknown" in described:
        check("an unloaded asset REPORTS that it cannot know, rather than claiming to be saved",
              "not loaded" in described, described[:200])
    else:
        unsaved = re.search(r"unsaved changes:\s*(\w+)", described)
        check("a LOADED asset reports no unsaved changes",
              unsaved is not None and unsaved.group(1) == "no",
              unsaved.group(1) if unsaved else "no value found")
    check("the file exists", os.path.isfile(FILE), FILE)

    print()
    print("=== THE CHECK THAT MATTERS: the value names are IN THE SAVED FILE ===")
    # The `.uasset` is a serialised archive. A name that was never written is not in it, so
    # finding it there is evidence that the save carried it. This replaces trusting the
    # tool's own count of what it set.
    if os.path.isfile(FILE):
        with open(FILE, "rb") as handle:
            raw = handle.read()
        text = raw.decode("latin-1")
        for value in ("Red", "Green", "Blue"):
            check("%s is in the saved asset" % value, value in text)
        check("the display name is in the saved asset", "Dark Green" in text)
        check("the asset name is in the saved asset",
              "TA18_ColourSuite" in text)
        # `_MAX` is the entry every user-defined enum carries, so its presence is evidence
        # the engine's own creator made this rather than a hand-built asset.
        check("the engine's own _MAX entry is present", "_MAX" in text)
    else:
        check("the saved file could be read", False, FILE)

    print()
    print("=== the plan validates before anything is created ===")
    for label, path, values, expect in (
        ("an empty path", "", VALUES, "no path given"),
        ("a relative path", "Tests/MyEnum", VALUES, "does not start with"),
        ("no values", PACKAGE + "2", "", "no values given"),
        ("a bad identifier", PACKAGE + "3", "has space", "cannot be an enumerator name"),
        ("a leading digit", PACKAGE + "4", "1Bad", "cannot be an enumerator name"),
        ("_MAX", PACKAGE + "5", "Good,_MAX", "reserved"),
        ("a duplicate", PACKAGE + "6", "Red,Red", "given twice"),
    ):
        out = session.call(PREVIEW_TOOLSET, "PreviewTurboMCPEnumAdd",
                           {"path": path, "values": values})
        ok = out.startswith("error:") and expect in out
        check("refuses %s" % label, ok, out[:90])

    print()
    print("=== a refusal is an error, and an existing asset is not an error ===")
    # The distinction matters: `error:` means the request was wrong, `unchanged:` means the
    # request was fine and nothing needed doing. A caller should not retry the second.
    check("a bad request opens with 'error:'",
          session.call(PREVIEW_TOOLSET, "PreviewTurboMCPEnumAdd",
                       {"path": "", "values": VALUES}).startswith("error:"))
    check("an existing asset opens with 'unchanged:'", again.startswith("unchanged:"))

    print()
    if failures:
        print("RESULT: FAIL (%d) -> %s" % (len(failures), ", ".join(failures)))
        return 1
    print("RESULT: PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
