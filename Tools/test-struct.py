"""Exercise P/E-STRUCT-NEW and P/E-STRUCT-FIELDS against the running editor.

  T-DATA-2    create a struct with 4 fields including one struct-typed field
  T-DATA-3    add, reorder and remove a field; a removal with a loaded reader is
              refused, names the reader and the count, and applies with force
  D-02        the Preview and the Edit share one code path, so both refuse the
              same input
  D-45        every field is re-resolved BY NAME at apply time

=== THE MEASUREMENT THIS SUITE EXISTS TO SETTLE ===

`Spec/03_TOOLS.md:910-914` and `D-51` say these tools "cannot be done from a
commandlet" because "`AddVariable` needs a `UUserDefinedStructEditorData` that
only the editor factory installs". The measured crash behind that sentence is
real and is recorded at `TurboMCPKitCommandlet.cpp:1420-1451`:

    CastChecked<UUserDefinedStructEditorData, UObject>()
    FMemberVariableNameHelper::Generate()   StructureEditorUtils.cpp:256
    FStructureEditorUtils::AddVariable()    StructureEditorUtils.cpp:304

But `FStructureEditorUtils::CreateUserDefinedStruct()`
(`StructureEditorUtils.cpp:42-64`) is a PUBLIC STATIC that installs that very
object at its line 49 and then calls `AddVariable` itself at line 59. Nothing in
`CreateUserDefinedStruct`, `AddVariable`, `RemoveVariable`, `MoveVariable` or
`CompileStructure` reads `GEditor` or an open asset editor. So the question the
brief asks - does this need an OPEN EDITOR or merely an EDITOR PROCESS - is
answered by the first check below: if a struct can be created at all from this
process, it needed no open editor.

THE FIRST CHECK IS THEREFORE NOT DECORATION. It is the measurement.

=== FIXTURE NOTE, AND IT IS A PRE-FLIGHT FINDING ===

`T-DATA-3` says "Add, reorder and remove a field of TA-21". **TA-21 DOES NOT
EXIST**: there is no `TA21_*` asset under `TestProject/Content`, and no
`UserDefinedStruct` asset anywhere in the repository. That is the fourth
acceptance row in this project found to name a fixture that does not exist, after
`T-MAT-10`->`TA-6`, `T-LEVEL-1/2`->`TA-15`/`TA-16` and `T-WIDGET-4`->`TA-8`.

This suite therefore does not depend on TA-21. It BUILDS the struct it then
edits, through its own new tool, which is what `T-DATA-2` does anyway. The
absence is recorded rather than papered over.

=== WHY THE READ BACK IS NOT THIS TOOL'S OWN REPORT ===

`E-STRUCT-NEW` prints "fields added: N" and then "fields read back: N". The
second number comes from the struct, the first from the call. Neither is evidence
on its own, because both come from the same code. So this suite:

  1. creates the struct through the tool,
  2. SAVES it, because an unsaved asset dies with the editor,
  3. checks it is found by `R-TYPE-FIND`, which is a DIFFERENT tool,
  4. opens the saved `.uasset` and looks for the field names in it.

Step 4 is the one that cannot be faked. The `.uasset` is a serialised archive, so
a name that was never written is not in it.
"""

import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from tmcp import EDIT_TOOLSET, PREVIEW_TOOLSET, READ_TOOLSET, Session  # noqa: E402

TESTS = os.path.join(
    os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
    "TestProject", "Content", "Tests")

# WHY EVERY PACKAGE THIS SUITE TOUCHES IS UNIQUE PER RUN, AND WHY THAT IS NOT A
# COSMETIC CHOICE.
#
# `E-STRUCT-NEW` REFUSES AN EXISTING PATH - correctly, and that refusal is one of
# the things this suite tests. But a package that exists in the RUNNING EDITOR is
# not removed by deleting its `.uasset`: the tool checks `FindObject` as well as
# the file, so a second run in the same editor session meets its own previous
# asset and is refused. A suite whose fixture collides with itself on the second
# run fails for the wrong reason, once, and then looks like a flake.
#
# `_RunId` is the process id, which is unique among live processes and gives a
# name that is a legal package name. The result is that every assertion below is
# about THIS run's asset and never about a previous one's.
_RUN_ID = "%d" % (os.getpid() % 100000)

PACKAGE = "/Game/Tests/TA21_KitStruct_Run" + _RUN_ID
OBJECT = PACKAGE + ".TA21_KitStruct_Run" + _RUN_ID
FILE = os.path.join(TESTS, os.path.basename(OBJECT) + ".uasset")

# T-DATA-2's shape: 4 fields, ONE OF THEM OF A STRUCT TYPE.
FIELDS = "Weight: float, Label: name, Count: int, Nested: Vector"

# A second struct, for the field editing half.
PACKAGE_EDIT = "/Game/Tests/TA21_FieldEdit_Run" + _RUN_ID
OBJECT_EDIT = PACKAGE_EDIT + ".TA21_FieldEdit_Run" + _RUN_ID
FILE_EDIT = os.path.join(TESTS, os.path.basename(OBJECT_EDIT) + ".uasset")

# A third struct, which a Blueprint variable reads. That reader is what the
# removal guard is measured against.
PACKAGE_READ = "/Game/Tests/TA21_WithReader_Run" + _RUN_ID
OBJECT_READ = PACKAGE_READ + ".TA21_WithReader_Run" + _RUN_ID
FILE_READ = os.path.join(TESTS, os.path.basename(OBJECT_READ) + ".uasset")

failures = []


def check(name, ok, detail=""):
    print("  %-4s %s%s" % ("ok" if ok else "FAIL", name, ("  " + detail) if detail else ""))
    if not ok:
        failures.append(name)


def field_names(text):
    """The field names in a tool's own read-back list.

    PARSED, NOT MATCHED AS A SENTENCE. The read back prints lines like
    `  Weight: float`, so the name is what precedes the first colon on a
    two-space-indented line. A line that does not have that shape is skipped
    rather than guessed at.
    """
    names = []
    for line in text.splitlines():
        if not line.startswith("  ") or line.startswith("   "):
            continue
        body = line.strip()
        if ":" not in body:
            continue
        names.append(body.split(":", 1)[0].strip())
    return names


def reader_counts(text):
    """Every `(N loaded reader(s))` count in a preview, as INTEGERS.

    A NUMBER IS PARSED RATHER THAN A SENTENCE BESIDE IT. `force=` needs the
    count, and a suite that matched the wording instead of the number would pass
    against a tool that printed the wrong figure.
    """
    return [int(n) for n in re.findall(r"\((\d+) loaded reader", text)]


def remove_files():
    """Delete this run's assets and any this suite left behind on an earlier run.

    The files are only half the state - the running editor caches the objects -
    which is exactly why the package names carry a run id above. This is belt and
    braces for the file check at the end.
    """
    for path in (FILE, FILE_EDIT, FILE_READ):
        if os.path.isfile(path):
            os.remove(path)
            print("    (removed a leftover %s)" % os.path.basename(path))
    for name in os.listdir(TESTS) if os.path.isdir(TESTS) else []:
        if name.startswith("TA21_") and name.endswith(".uasset"):
            os.remove(os.path.join(TESTS, name))
            print("    (removed a previous run's %s)" % name)


def main():
    session = Session()
    session.connect()
    remove_files()

    print("=== THE MEASUREMENT: can this process create a user struct at all? ===")
    # If `FStructureEditorUtils` needed an OPEN asset editor, this call could not
    # work: no asset editor is open, and the tool refuses to open one. A struct
    # that exists afterwards is the measurement that it needed only an editor
    # PROCESS.
    preview = session.call(PREVIEW_TOOLSET, "PreviewTurboMCPStructNew",
                           {"path": PACKAGE, "fields": FIELDS})
    print(preview)
    print()
    check("the preview names the path", PACKAGE in preview)
    check("the preview counts the fields", "fields: 4" in preview)
    check("the preview lists each field",
          all(f in preview for f in ("Weight: float", "Label: name",
                                     "Count: int", "Nested: Vector")))
    check("the preview says whether the asset exists",
          "already exists" in preview or "does not exist yet" in preview)

    print()
    print("=== the edit creates it (T-DATA-2) ===")
    created = session.call(EDIT_TOOLSET, "EditTurboMCPStructNew",
                           {"path": PACKAGE, "fields": FIELDS})
    print(created)
    print()
    check("the edit reports changed", created.startswith("changed:"), created[:90])
    # THE TOOL'S OWN TWO NUMBERS, WHICH MUST AGREE. "asked for" is what the caller
    # sent and "read back" is what the struct holds; a tool whose loop added
    # three fields while reporting four would show up right here.
    check("it says 4 fields were asked for", "fields asked for: 4" in created)
    check("it reports 4 fields read back from the struct",
          "fields read back: 4" in created, created[:200])
    check("the read-back list has 4 names", len(field_names(created)) == 4,
          str(field_names(created)))
    check("the name-mangled engine field is NOT in the result, because it was removed",
          "MemberVar" not in created, created[:200])

    print()
    print("=== the engine's own field list is what reads back ===")
    # THIS IS WHERE A `UUserDefinedStruct` CREATED WITHOUT EDITOR DATA WOULD HAVE
    # CRASHED. Reaching this line at all is part of the measurement.
    described = session.call(READ_TOOLSET, "ReadTurboMCPAssetDescribe", {"assets": OBJECT})
    print(described)
    print()
    check("the asset is described as a UserDefinedStruct",
          "UserDefinedStruct" in described, described[:160])

    print()
    print("=== a second call on the same path is refused, not overwritten ===")
    again = session.call(EDIT_TOOLSET, "EditTurboMCPStructNew",
                         {"path": PACKAGE, "fields": "Other: int"})
    print(again)
    print()
    check("the second call is refused", again.startswith("unchanged:"), again[:90])
    check("it says the asset was not touched", "not touched" in again)

    print()
    print("=== saved, then read back through a DIFFERENT tool ===")
    saved = session.call(EDIT_TOOLSET, "EditTurboMCPSave", {"assets": PACKAGE})
    print(saved)
    print()
    check("the file is on disk", os.path.isfile(FILE), FILE)

    # `R-TYPE-FIND` is a different tool with a different code path: it searches the
    # object graph and the asset registry rather than asking the struct. If it
    # finds the struct, the struct is a real reflected type and not just a name
    # this tool remembers.
    found = session.call(READ_TOOLSET, "ReadTurboMCPTypeFind",
                         {"words": "TA21_KitStruct", "kind": "struct"})
    print(found)
    print()
    check("R-TYPE-FIND finds it as a struct", "TA21_KitStruct" in found, found[:300])
    check("R-TYPE-FIND classifies it as a struct", "struct" in found.lower())

    print()
    print("=== THE CHECK THAT CANNOT BE FAKED: the field names are in the SAVED FILE ===")
    if os.path.isfile(FILE):
        with open(FILE, "rb") as handle:
            raw = handle.read()
        text = raw.decode("latin-1")
        for field_name in ("Weight", "Label", "Count", "Nested"):
            check("%s is in the saved asset" % field_name, field_name in text)
        check("the asset name is in the saved asset", "TA21_KitStruct" in text)
    else:
        check("the saved file could be read", False, FILE)

    print()
    print("=== T-DATA-3: add, change, reorder and remove fields ===")
    # The struct is built by this suite rather than read from TA-21, which does
    # not exist. That is stated in the fixture note at the top of this file.
    built = session.call(EDIT_TOOLSET, "EditTurboMCPStructNew",
                         {"path": PACKAGE_EDIT, "fields": "Alpha: int, Beta: int, Gamma: int"})
    print(built)
    check("the editing fixture was created", built.startswith("changed:"), built[:90])
    before = field_names(built)
    check("it starts with 3 fields in order", before == ["Alpha", "Beta", "Gamma"], str(before))

    print()
    print("--- an ADD and a CHANGE, and a REORDER, in one call ---")
    # `Delta: float` is new, so it is an ADD. `Beta: string` already exists, so
    # the SAME syntax is a CHANGE - which is the spec's split between P-STRUCT-NEW
    # and P-STRUCT-FIELDS resolved by the struct rather than by the caller.
    edited = session.call(EDIT_TOOLSET, "EditTurboMCPStructFields",
                          {"struct": OBJECT_EDIT,
                           "changes": "Delta: float, Beta: string, move:Gamma, above=Alpha"})
    print(edited)
    print()
    check("the edit reports changed", edited.startswith("changed:"), edited[:90])
    check("it reports 1 added", "added: 1" in edited)
    check("it reports 1 changed", "changed: 1" in edited)
    check("it reports 1 moved", "moved: 1" in edited)
    now = field_names(edited)
    check("the read back has 4 fields", len(now) == 4, str(now))
    check("Delta was added", "Delta" in now, str(now))
    check("Gamma moved above Alpha", now.index("Gamma") < now.index("Alpha"), str(now))
    # `Beta: string` was an int and had to become a real string. The type is read
    # from the compiled property, so this is the engine's answer and not ours.
    beta_line = [l.strip() for l in edited.splitlines() if l.strip().startswith("Beta:")]
    check("Beta is a string now, read from its compiled property",
          bool(beta_line) and "String" in beta_line[0], str(beta_line))

    print()
    print("--- a REMOVAL is reported with its readers and refused without force ---")
    removed = session.call(EDIT_TOOLSET, "EditTurboMCPStructFields",
                           {"struct": OBJECT_EDIT, "changes": "remove:Delta"})
    print(removed)
    print()
    check("a removal with no loaded readers is allowed",
          removed.startswith("changed:") and "removed: 1" in removed, removed[:160])
    after = field_names(removed)
    check("Delta is gone", "Delta" not in after, str(after))
    check("the read back has 3 fields", len(after) == 3, str(after))

    print()
    print("=== the removal guard, measured against a real loaded reader ===")
    # A Blueprint variable of this struct's type IS a loaded reader: removing a
    # field changes the shape of a member that Blueprint holds. `E-VARS-ADD`
    # creates one, so the guard is exercised against a reader the engine built
    # rather than against a number this suite made up.
    built = session.call(EDIT_TOOLSET, "EditTurboMCPStructNew",
                         {"path": PACKAGE_READ, "fields": "Keep: int, Drop: int"})
    print(built)
    check("the reader fixture was created", built.startswith("changed:"), built[:120])
    check("the reader fixture holds exactly 2 fields",
          "fields read back: 2" in built, built[:200])

    # THE READER IS ASSIGNED AN IDEMPOTENT WAY ROUND. `E-VARS-ADD` has NO
    # removal twin, so a variable this suite adds cannot be taken away again.
    # That makes a name collide on the second run - and a `: Type` string is a
    # use of the type, not a definition, so a Blueprint that ALREADY carries a
    # variable of this struct's type is a fact this suite can measure instead of
    # fight: `E-VARS-ADD` is called once, and whether it reports `changed:` or
    # `unchanged:` the reader is there either way.
    reader = session.call(EDIT_TOOLSET, "EditTurboMCPVarsAdd",
                          {"asset": "/Game/Tests/Kit/TA01_HandLaid",
                           "variables": "StructHolder: " + OBJECT_READ})
    print(reader)
    print()
    check("the Blueprint now carries a variable of that struct's type",
          reader.startswith("changed:") or reader.startswith("unchanged:"),
          reader[:220])

    # The count is READ from the preview and then used. It is never pinned,
    # because a shared asset's contents are not this suite's to assume.
    preview = session.call(PREVIEW_TOOLSET, "PreviewTurboMCPStructFields",
                           {"struct": OBJECT_READ, "changes": "remove:Drop"})
    print(preview)
    print()
    counts = reader_counts(preview)
    check("the preview reports the removal's reader count", len(counts) >= 1,
          preview[:300])
    measured = counts[0] if counts else 0
    check("the measured reader count is at least 1", measured >= 1, str(measured))
    check("the preview names the reader by path, not only by count",
          "TA01_HandLaid" in preview, preview[:400])
    check("the preview REFUSES the removal without force",
          preview.startswith("error:"), preview[:160])
    check("the refusal names the count to pass",
          ("force=%d" % measured) in preview, preview[:300])

    print()
    print("--- the same input to the Edit is refused the SAME way (D-02) ---")
    refused = session.call(EDIT_TOOLSET, "EditTurboMCPStructFields",
                           {"struct": OBJECT_READ, "changes": "remove:Drop"})
    print(refused)
    print()
    check("the edit refuses it too, with no change made",
          refused.startswith("error:"), refused[:160])
    check("the edit names the reader as well", "TA01_HandLaid" in refused, refused[:400])

    print()
    print("--- a WRONG count is refused, so the guard cannot be satisfied by guessing ---")
    wrong = session.call(PREVIEW_TOOLSET, "PreviewTurboMCPStructFields",
                         {"struct": OBJECT_READ, "changes": "remove:Drop, force=0"})
    print(wrong)
    print()
    check("force=0 on a field with readers is refused", wrong.startswith("error:"),
          wrong[:160])

    print()
    print("--- the measured count authorises it ---")
    forced = session.call(EDIT_TOOLSET, "EditTurboMCPStructFields",
                          {"struct": OBJECT_READ,
                           "changes": "remove:Drop, force=%d" % measured})
    print(forced)
    print()
    check("the removal applies with the measured force",
          forced.startswith("changed:") and "removed: 1" in forced, forced[:200])
    remaining = field_names(forced)
    check("Drop is gone and Keep stays", "Drop" not in remaining and "Keep" in remaining,
          str(remaining))

    # TA-1 is left holding the reader deliberately. There is no E-VARS-REMOVE in
    # this plugin, so the alternative to leaving it is to change a shared kit
    # asset with no way back; and the variable points at a struct this suite
    # keeps, so nothing dangles. The rerun path above is what makes that safe.
    print()
    print("=== every refusal is decided in the PLAN, so the Preview and Edit agree ===")
    # EACH ENTRY NAMES THE INPUT THAT MAKES IT FAIL. A refused call must open with
    # `error:` - `unchanged:` would mean the request was fine and nothing needed
    # doing, which a caller should not retry.
    for label, toolset, tool, args, expect in (
        ("an empty path", PREVIEW_TOOLSET, "PreviewTurboMCPStructNew",
         {"path": "", "fields": FIELDS}, "no path given"),
        ("a relative path", PREVIEW_TOOLSET, "PreviewTurboMCPStructNew",
         {"path": "Tests/MyStruct", "fields": FIELDS}, "does not start with"),
        ("no fields", PREVIEW_TOOLSET, "PreviewTurboMCPStructNew",
         {"path": "/Game/Tests/ZZZ_NoFields", "fields": ""}, "no fields given"),
        ("a field with no type", PREVIEW_TOOLSET, "PreviewTurboMCPStructNew",
         {"path": "/Game/Tests/ZZZ_NoType", "fields": "Good: int, Bad:"}, "has no type"),
        ("a field with no colon", PREVIEW_TOOLSET, "PreviewTurboMCPStructNew",
         {"path": "/Game/Tests/ZZZ_NoColon", "fields": "JustAName"}, "has no ':'"),
        ("a field name with a space", PREVIEW_TOOLSET, "PreviewTurboMCPStructNew",
         {"path": "/Game/Tests/ZZZ_Space", "fields": "has space: int"},
         "cannot be a field name"),
        ("a field name starting with a digit", PREVIEW_TOOLSET, "PreviewTurboMCPStructNew",
         {"path": "/Game/Tests/ZZZ_Digit", "fields": "1Bad: int"},
         "cannot be a field name"),
        ("a duplicate field", PREVIEW_TOOLSET, "PreviewTurboMCPStructNew",
         {"path": "/Game/Tests/ZZZ_Dup", "fields": "Same: int, Same: int"},
         "is given twice"),
        ("an unknown option", PREVIEW_TOOLSET, "PreviewTurboMCPStructNew",
         {"path": "/Game/Tests/ZZZ_Opt", "fields": "Good: int, banana=3"},
         "not a field option"),
        ("a type that does not exist", PREVIEW_TOOLSET, "PreviewTurboMCPStructNew",
         {"path": "/Game/Tests/ZZZ_Type", "fields": "Good: banana"},
         "is not a type"),
        ("an unbalanced container", PREVIEW_TOOLSET, "PreviewTurboMCPStructNew",
         {"path": "/Game/Tests/ZZZ_Bal", "fields": "Good: Array<int"},
         "is not balanced"),
        ("a removal of a field that is not there", PREVIEW_TOOLSET,
         "PreviewTurboMCPStructFields",
         {"struct": OBJECT_EDIT, "changes": "remove:NoSuchField"},
         "is not a field of"),
        ("a reorder onto a field that is not there", PREVIEW_TOOLSET,
         "PreviewTurboMCPStructFields",
         {"struct": OBJECT_EDIT, "changes": "move:Alpha, above=NoSuchField"},
         "is not a field of"),
        ("a reorder relative to itself", PREVIEW_TOOLSET, "PreviewTurboMCPStructFields",
         {"struct": OBJECT_EDIT, "changes": "move:Alpha, above=Alpha"},
         "cannot be moved relative to itself"),
        ("a reorder with no anchor", PREVIEW_TOOLSET, "PreviewTurboMCPStructFields",
         {"struct": OBJECT_EDIT, "changes": "move:Alpha"}, "does not say where"),
        ("both above= and below=", PREVIEW_TOOLSET, "PreviewTurboMCPStructFields",
         {"struct": OBJECT_EDIT, "changes": "move:Alpha, above=Beta, below=Gamma"},
         "not both"),
        ("no changes at all", PREVIEW_TOOLSET, "PreviewTurboMCPStructFields",
         {"struct": OBJECT_EDIT, "changes": ""}, "no changes given"),
        ("a struct that cannot be loaded", PREVIEW_TOOLSET, "PreviewTurboMCPStructFields",
         {"struct": "/Game/Tests/DoesNotExist", "changes": "X: int"},
         "could not be loaded"),
        ("a NATIVE struct, which has no editable field list", PREVIEW_TOOLSET,
         "PreviewTurboMCPStructFields",
         {"struct": "/Script/CoreUObject.Vector", "changes": "X: int"},
         "not a user-defined struct"),
    ):
        out = session.call(toolset, tool, args)
        ok = out.startswith("error:") and expect in out
        check("refuses %s" % label, ok, out[:130])

    print()
    print("=== a refusal is an error, but an existing asset is not ===")
    check("a bad request opens with 'error:'",
          session.call(PREVIEW_TOOLSET, "PreviewTurboMCPStructNew",
                       {"path": "", "fields": FIELDS}).startswith("error:"))
    check("an existing asset opens with 'unchanged:'", again.startswith("unchanged:"))

    print()
    if failures:
        print("RESULT: FAIL (%d) -> %s" % (len(failures), ", ".join(failures)))
        return 1
    print("RESULT: PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
