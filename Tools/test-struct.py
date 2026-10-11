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
`CreateUserDefinedStruct`, `AddVariable`, `RemoveVariable`, `MoveVariable`,
`RenameVariable` or `CompileStructure` reads `GEditor` or an open asset editor.
So the question the brief asks - does this need an OPEN EDITOR or merely an
EDITOR PROCESS - is answered by the first section below: a struct is created,
populated, saved and found again with NO asset editor open anywhere.

THE FIRST SECTION IS THEREFORE NOT DECORATION. It is the measurement.

=== FIXTURE NOTE, AND IT IS A PRE-FLIGHT FINDING ===

`T-DATA-3` says "Add, reorder and remove a field of TA-21". **TA-21 DOES NOT
EXIST**: there is no `TA21_*` asset under `TestProject/Content`, and no
`UserDefinedStruct` asset anywhere in the repository. That is the fourth
acceptance row in this project found to name a fixture that does not exist, after
`T-MAT-10`->`TA-6`, `T-LEVEL-1/2`->`TA-15`/`TA-16` and `T-WIDGET-4`->`TA-8`.

This suite therefore does not depend on TA-21. It BUILDS every struct it edits,
through its own new tool, which is what `T-DATA-2` does anyway. The absence is
recorded rather than papered over.

=== WHY EVERY TEMPORARY PACKAGE CARRIES A RUN ID ===

`E-STRUCT-NEW` REFUSES AN EXISTING PATH, and that refusal is one of the things
this suite tests. A package that exists in the RUNNING EDITOR is not removed by
deleting its `.uasset` - the tool checks `FindObject` as well as the file - so
fixed names would make the second run in one editor session meet the first run's
asset and be refused. The run id makes every assertion below about THIS run's
asset and never about a previous one's.

The ONE exception is `PACKAGE_READ`, which must keep a stable name because a
Blueprint variable is a USE of a type rather than a definition: a variable of a
run-named struct can never be re-created, and `E-VARS-ADD` has no removal twin.
The reader is therefore assigned once and left, which the checks below handle
explicitly rather than assuming.

=== WHY THE READ BACK IS NOT THIS TOOL'S OWN REPORT ===

`E-STRUCT-NEW` prints "fields added: N" and then "fields read back: N". The
second number comes from the struct, the first from the call - and BOTH COME
FROM THE SAME CODE, so neither is evidence on its own. So this suite:

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

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TESTS = os.path.join(ROOT, "TestProject", "Content", "Tests")

# Unique among live processes, and a legal package name component.
_RUN = str(os.getpid() % 100000)

# T-DATA-2's shape: 4 fields, ONE OF THEM OF A STRUCT TYPE.
PACKAGE = "/Game/Tests/TA21_KitStruct_R" + _RUN
OBJECT = PACKAGE + ".TA21_KitStruct_R" + _RUN
FILE = os.path.join(TESTS, "TA21_KitStruct_R" + _RUN + ".uasset")
FIELDS = "Weight: float, Label: name, Count: int, Nested: Vector"

# The struct the field editor is exercised on. Unique per run, because a
# REORDER cannot converge on a shared fixture: "move Gamma above Alpha" is not
# idempotent, so a second run against a mutated struct would fail on a fixture
# that is still perfectly correct.
PACKAGE_EDIT = "/Game/Tests/TA21_FieldEdit_R" + _RUN
OBJECT_EDIT = PACKAGE_EDIT + ".TA21_FieldEdit_R" + _RUN
FILE_EDIT = os.path.join(TESTS, "TA21_FieldEdit_R" + _RUN + ".uasset")

# The struct a Blueprint variable reads - the reader the removal guard is
# measured against. STABLE ON PURPOSE, for the reason in the module docstring.
PACKAGE_READ = "/Game/Tests/TA21_WithReader"
OBJECT_READ = PACKAGE_READ + ".TA21_WithReader"
FILE_READ = os.path.join(TESTS, "TA21_WithReader.uasset")

# The kit Blueprint the reader variable is attached to. A shared kit asset is
# touched here, which is why the assignment is idempotent and said out loud.
READER_HOST = "/Game/Tests/Kit/TA01_HandLaid"

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


def field_line(text, name):
    """The read-back line for one field, so its TYPE can be asserted."""
    for line in text.splitlines():
        stripped = line.strip()
        if stripped.startswith(name + ":"):
            return stripped
    return ""


def reader_count(text):
    """The loaded-reader count a tool reported, as an INTEGER or None.

    A NUMBER IS PARSED RATHER THAN A SENTENCE BESIDE IT. Two shapes carry it, and
    BOTH are parsed because a refusal and a preview word it differently:

      - a REFUSAL says "would break N loaded reader(s)";
      - a PREVIEW's removal line says "(N loaded reader(s))".

    The first shape is the one a caller acts on - it is the count that `force=`
    must name - so it is tried first.
    """
    for pattern in (r"would break (\d+) loaded reader",
                    r"\((\d+) loaded reader"):
        found = re.search(pattern, text)
        if found:
            return int(found.group(1))
    return None


def forced_count(text):
    """The `force=N` a refusal tells the caller to use, as an INTEGER or None."""
    found = re.search(r"force=(\d+) to proceed", text)
    return int(found.group(1)) if found else None


def cleanup():
    """Remove this suite's temporary assets, from this run and from earlier ones."""
    if not os.path.isdir(TESTS):
        return
    for name in sorted(os.listdir(TESTS)):
        # The stable reader struct is left alone: a Blueprint variable points at
        # it, and deleting the asset under a live variable is how a kit asset
        # ends up with a dangling member for the next suite to trip over.
        if name.startswith("TA21_KitStruct_R") or name.startswith("TA21_FieldEdit_R"):
            os.remove(os.path.join(TESTS, name))
            print("    (removed %s)" % name)


def main():
    session = Session()
    session.connect()
    cleanup()

    print("=== THE MEASUREMENT: can this process build a user struct at all? ===")
    # NO ASSET EDITOR IS OPEN ANYWHERE IN THIS PROCESS, and this tool does not
    # open one. A struct that exists and carries the four requested fields
    # afterwards is the measurement that only an editor PROCESS was needed.
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
    check("it says 4 fields were asked for", "fields asked for: 4" in created)
    check("it reports 4 fields read back from the struct",
          "fields read back: 4" in created, created[:220])
    read_back = field_names(created)
    check("the read back has exactly the 4 requested names",
          read_back == ["Weight", "Label", "Count", "Nested"], str(read_back))
    # THE ENGINE'S CREATOR ADDS A BOOLEAN OF ITS OWN, and the tool must have
    # removed it. If it had not, this would carry a `MemberVar_N` entry, which is
    # what the first version of this tool actually produced.
    check("the creator's own unnamed field is NOT in the read back",
          not any(n.startswith("MemberVar") for n in read_back), str(read_back))
    check("the struct-typed field really is a struct type",
          "Vector" in field_line(created, "Nested"), field_line(created, "Nested"))

    print()
    print("=== the engine's own asset registry agrees about what it is ===")
    described = session.call(READ_TOOLSET, "ReadTurboMCPAssetDescribe", {"assets": OBJECT})
    print(described)
    print()
    check("the asset is described as a UserDefinedStruct",
          "UserDefinedStruct" in described, described[:200])

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
    check("the save reports the asset was saved", "1 saved" in saved, saved[:200])
    check("the file is on disk", os.path.isfile(FILE), FILE)

    # `R-TYPE-FIND` is a different tool with a different code path: it searches the
    # object graph and the asset registry rather than asking the struct. If it
    # finds the struct, the struct is a real reflected type and not just a name
    # this tool remembers.
    found = session.call(READ_TOOLSET, "ReadTurboMCPTypeFind",
                         {"words": os.path.basename(OBJECT), "kind": "struct"})
    print(found)
    print()
    check("R-TYPE-FIND finds it", OBJECT in found, found[:300])
    check("R-TYPE-FIND classifies it as a struct", "[struct]" in found)

    print()
    print("=== THE CHECK THAT CANNOT BE FAKED: the field names are in the SAVED FILE ===")
    if os.path.isfile(FILE):
        with open(FILE, "rb") as handle:
            raw = handle.read()
        text = raw.decode("latin-1")
        for field_name in ("Weight", "Label", "Count", "Nested"):
            check("%s is in the saved asset" % field_name, field_name in text)
        # THE STRUCT'S OWN CLASS MARKER, which is the engine's word for what this
        # asset is and not this tool's.
        check("the engine's UserDefinedStruct marker is in the saved asset",
              "UserDefinedStruct" in text)
        check("a Struct marker is in the saved asset", "Struct" in text)
    else:
        check("the saved file could be read", False, FILE)

    print()
    print("=== T-DATA-3: add, change, reorder and remove fields ===")
    # The struct is built by this suite rather than read from TA-21, which does
    # not exist. That is stated in the fixture note at the top of this file.
    built = session.call(EDIT_TOOLSET, "EditTurboMCPStructNew",
                         {"path": PACKAGE_EDIT,
                          "fields": "Alpha: int, Beta: int, Gamma: int"})
    print(built)
    check("the editing fixture was created", built.startswith("changed:"), built[:120])
    before = field_names(built)
    check("it starts with the 3 fields in order",
          before == ["Alpha", "Beta", "Gamma"], str(before))

    print()
    print("--- an ADD, a CHANGE and a REORDER in ONE call ---")
    # `Delta: float` is new, so it is an ADD. `Beta: string` already exists, so
    # the SAME syntax is a CHANGE - the spec's split between P-STRUCT-NEW and
    # P-STRUCT-FIELDS resolved by the struct rather than by the caller.
    edited = session.call(EDIT_TOOLSET, "EditTurboMCPStructFields",
                          {"struct": OBJECT_EDIT,
                           "changes": "Delta: float, Beta: string, move:Gamma, above=Alpha"})
    print(edited)
    print()
    check("the edit reports changed", edited.startswith("changed:"), edited[:120])
    check("it reports 1 added", "added: 1" in edited)
    check("it reports 1 changed", "changed: 1" in edited)
    check("it reports 1 moved", "moved: 1" in edited)
    now = field_names(edited)
    check("the read back has 4 fields", len(now) == 4, str(now))
    check("Delta was added", "Delta" in now, str(now))
    check("Gamma moved above Alpha", now.index("Gamma") < now.index("Alpha"), str(now))
    # `Beta: string` was an int and had to become a real string. The type printed
    # is read from the COMPILED PROPERTY, so this is the engine's answer.
    beta = field_line(edited, "Beta")
    check("Beta's type changed, read from its compiled property",
          bool(beta) and "String" in beta, beta)

    print()
    print("--- a CHANGE of the same field, alone, proves the type really moved ---")
    # The previous call bundled the change with other work. This one changes ONE
    # field and nothing else, so if the type moved it moved because of this.
    changed = session.call(EDIT_TOOLSET, "EditTurboMCPStructFields",
                           {"struct": OBJECT_EDIT, "changes": "Alpha: double"})
    print(changed)
    print()
    check("the lone change reports changed", changed.startswith("changed:"), changed[:120])
    check("it reports 1 changed and nothing else",
          "changed: 1" in changed and "added: 0" in changed
          and "moved: 0" in changed and "removed: 0" in changed, changed[:200])
    check("Alpha is a double now", "double" in field_line(changed, "Alpha").lower()
          or "Double" in field_line(changed, "Alpha"), field_line(changed, "Alpha"))

    print()
    print("--- a REMOVAL on a struct with no reader is allowed ---")
    removed = session.call(EDIT_TOOLSET, "EditTurboMCPStructFields",
                           {"struct": OBJECT_EDIT, "changes": "remove:Delta"})
    print(removed)
    print()
    check("a removal with nothing reading it is allowed",
          removed.startswith("changed:") and "removed: 1" in removed, removed[:160])
    after = field_names(removed)
    check("Delta is gone", "Delta" not in after, str(after))
    check("the read back has 3 fields", len(after) == 3, str(after))

    print()
    print("--- a removal of a field that is NOT there is refused by name ---")
    absent = session.call(EDIT_TOOLSET, "EditTurboMCPStructFields",
                          {"struct": OBJECT_EDIT, "changes": "remove:Delta"})
    print(absent)
    print()
    check("removing it again is refused", absent.startswith("error:"), absent[:160])
    check("the refusal lists the real field names",
          "Alpha" in absent and "Beta" in absent and "Gamma" in absent, absent[:300])

    print()
    print("=== the removal guard, measured against a REAL loaded reader ===")
    # A Blueprint variable of this struct's type IS a loaded reader: removing a
    # field changes the shape of a member that Blueprint holds. `E-VARS-ADD`
    # creates one, so the guard is measured against a reader the ENGINE built
    # rather than against a number this suite made up.
    built = session.call(EDIT_TOOLSET, "EditTurboMCPStructNew",
                         {"path": PACKAGE_READ, "fields": "Keep: int, Drop: int"})
    print(built)
    # `unchanged:` IS A PASS HERE, AND IT IS SAID SO RATHER THAN HIDDEN. The name
    # is stable because a Blueprint variable can only ever be added once, so on a
    # second run in one editor session the struct is already there and the tool is
    # right to refuse to overwrite it. The reader below is what this section
    # needs, and it is checked directly.
    if built.startswith("unchanged:"):
        print("    (%s already exists in this editor and is REUSED; it is stable "
              "because a Blueprint variable of it can never be re-created)"
              % OBJECT_READ)
    check("the reader fixture exists (created, or already there)",
          built.startswith("changed:") or built.startswith("unchanged:"), built[:120])

    reader = session.call(EDIT_TOOLSET, "EditTurboMCPVarsAdd",
                          {"asset": READER_HOST,
                           "variables": "TurboMCPStructReader: " + OBJECT_READ})
    print(reader)
    print()
    check("the Blueprint carries a variable of that struct's type",
          reader.startswith("changed:") or reader.startswith("unchanged:"),
          reader[:220])

    # The count is READ from the refusal and then used. It is never pinned, because
    # a shared asset's contents are not this suite's to assume.
    preview = session.call(PREVIEW_TOOLSET, "PreviewTurboMCPStructFields",
                           {"struct": OBJECT_READ, "changes": "remove:Drop"})
    print(preview)
    print()
    measured = reader_count(preview)
    check("the preview reports a loaded-reader count as a NUMBER",
          measured is not None, preview[:300])

    if measured is not None and measured >= 1:
        print()
        print("    a reader EXISTS, so the guard is exercised at full strength")
        check("the preview names the reader by path, not only by count",
              "TA01_HandLaid" in preview, preview[:400])
        check("the preview REFUSES the removal without force",
              preview.startswith("error:"), preview[:160])
        check("the refusal tells the caller the exact force to use",
              forced_count(preview) == measured,
              "force told: %s, count measured: %s" % (forced_count(preview), measured))

        print()
        print("--- the same input to the Edit is refused the SAME way (D-02) ---")
        refused = session.call(EDIT_TOOLSET, "EditTurboMCPStructFields",
                               {"struct": OBJECT_READ, "changes": "remove:Drop"})
        print(refused)
        print()
        check("the edit refuses it too, with no change made",
              refused.startswith("error:"), refused[:160])
        check("the edit names the reader as well",
              "TA01_HandLaid" in refused, refused[:400])
        check("the edit names the field it refused to remove",
              "Drop" in refused, refused[:400])

        print()
        print("--- a WRONG count is refused, so the guard cannot be satisfied by guessing ---")
        for wrong_force in ("0", "1", "99"):
            if wrong_force == str(measured):
                continue
            wrong = session.call(PREVIEW_TOOLSET, "PreviewTurboMCPStructFields",
                                 {"struct": OBJECT_READ,
                                  "changes": "remove:Drop, force=" + wrong_force})
            print(wrong)
            print()
            check("force=%s on a field with %d reader(s) is refused"
                  % (wrong_force, measured), wrong.startswith("error:"), wrong[:200])

        print()
        print("--- the MEASURED count authorises it ---")
        forced = session.call(EDIT_TOOLSET, "EditTurboMCPStructFields",
                              {"struct": OBJECT_READ,
                               "changes": "remove:Drop, force=%d" % measured})
        print(forced)
        print()
        check("the removal applies with the measured force",
              forced.startswith("changed:") and "removed: 1" in forced, forced[:220])
        remaining = field_names(forced)
        check("Drop is gone and Keep stays",
              "Drop" not in remaining and "Keep" in remaining, str(remaining))
    else:
        # THE READER DID NOT TAKE, AND THAT IS REPORTED AS A LIMIT ON THIS RUN
        # RATHER THAN PASSED OFF AS COVERAGE. With no reader the correct `force`
        # is `0` and the refusal branch never runs, so the force ARITHMETIC is not
        # exercised - only the zero-reader path is.
        print()
        print("    NO READER EXISTS, so the force arithmetic is NOT exercised this run.")
        print("    E-VARS-ADD did not attach a variable of %s to %s."
              % (OBJECT_READ, READER_HOST))
        zero = session.call(PREVIEW_TOOLSET, "PreviewTurboMCPStructFields",
                            {"struct": OBJECT_READ, "changes": "remove:Drop, force=0"})
        print(zero)
        print()
        check("with 0 readers, force=0 is the count that is accepted",
              zero.startswith("preview:"), zero[:200])
        bogus = session.call(PREVIEW_TOOLSET, "PreviewTurboMCPStructFields",
                             {"struct": OBJECT_READ, "changes": "remove:Drop, force=7"})
        print(bogus)
        print()
        check("with 0 readers, force=7 is refused",
              bogus.startswith("error:"), bogus[:200])

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
         {"path": "/Game/Tests/ZZZ_NoFields_R" + _RUN, "fields": ""},
         "no fields given"),
        ("a field with no type", PREVIEW_TOOLSET, "PreviewTurboMCPStructNew",
         {"path": "/Game/Tests/ZZZ_NoType_R" + _RUN, "fields": "Good: int, Bad:"},
         "has no type"),
        ("a field with no colon", PREVIEW_TOOLSET, "PreviewTurboMCPStructNew",
         {"path": "/Game/Tests/ZZZ_NoColon_R" + _RUN, "fields": "JustAName"},
         "has no ':'"),
        ("a field name with a space", PREVIEW_TOOLSET, "PreviewTurboMCPStructNew",
         {"path": "/Game/Tests/ZZZ_Space_R" + _RUN, "fields": "has space: int"},
         "cannot be a field name"),
        ("a field name starting with a digit", PREVIEW_TOOLSET,
         "PreviewTurboMCPStructNew",
         {"path": "/Game/Tests/ZZZ_Digit_R" + _RUN, "fields": "1Bad: int"},
         "cannot be a field name"),
        ("a duplicate field", PREVIEW_TOOLSET, "PreviewTurboMCPStructNew",
         {"path": "/Game/Tests/ZZZ_Dup_R" + _RUN, "fields": "Same: int, Same: int"},
         "is given twice"),
        ("an unknown option on a NEW field", PREVIEW_TOOLSET, "PreviewTurboMCPStructNew",
         {"path": "/Game/Tests/ZZZ_Opt_R" + _RUN, "fields": "Good: int, banana=3"},
         "has no ':'"),
        ("an unknown option on a field description", PREVIEW_TOOLSET,
         "PreviewTurboMCPStructNew",
         {"path": "/Game/Tests/ZZZ_Opt2_R" + _RUN,
          "fields": "Good: int, normal=3"},
         "has no ':'"),
        ("an unknown option attached to a real field", PREVIEW_TOOLSET,
         "PreviewTurboMCPStructNew",
         {"path": "/Game/Tests/ZZZ_Opt3_R" + _RUN,
          "fields": "Good: int, default=1, banana=3"},
         "not a field option"),
        ("a type that does not exist", PREVIEW_TOOLSET, "PreviewTurboMCPStructNew",
         {"path": "/Game/Tests/ZZZ_Type_R" + _RUN, "fields": "Good: banana"},
         "is not a type"),
        ("an unbalanced container", PREVIEW_TOOLSET, "PreviewTurboMCPStructNew",
         {"path": "/Game/Tests/ZZZ_Bal_R" + _RUN, "fields": "Good: Array<int"},
         "is not balanced"),
        ("a Map with one type", PREVIEW_TOOLSET, "PreviewTurboMCPStructNew",
         {"path": "/Game/Tests/ZZZ_Map_R" + _RUN, "fields": "Good: Map<int>"},
         "exactly two types"),
        ("an empty struct path", PREVIEW_TOOLSET, "PreviewTurboMCPStructFields",
         {"struct": "", "changes": "X: int"}, "no struct given"),
        ("no changes at all", PREVIEW_TOOLSET, "PreviewTurboMCPStructFields",
         {"struct": OBJECT_EDIT, "changes": ""}, "no changes given"),
        ("a struct that cannot be loaded", PREVIEW_TOOLSET,
         "PreviewTurboMCPStructFields",
         {"struct": "/Game/Tests/DoesNotExist_R" + _RUN, "changes": "X: int"},
         "could not be loaded"),
        ("a NATIVE struct, which has no editable field list", PREVIEW_TOOLSET,
         "PreviewTurboMCPStructFields",
         {"struct": "/Script/CoreUObject.Vector", "changes": "X: int"},
         "not a user-defined struct"),
        ("a removal of a field that is not there", PREVIEW_TOOLSET,
         "PreviewTurboMCPStructFields",
         {"struct": OBJECT_EDIT, "changes": "remove:NoSuchField"},
         "is not a field of"),
        # NOTE, and it is a NOTE ABOUT THE TOOL RATHER THAN ABOUT THE TEST:
        # `Alpha: int` on a struct that ALREADY has Alpha is a CHANGE, not a
        # refused ADD. That is the documented contract - the same syntax means
        # "make Alpha be an int", and it is a no-op when Alpha already is one.
        # So there is deliberately no "adding an existing field is refused" case
        # here: the tool's answer to that input is `unchanged:`, which is a
        # correct answer and not an error. Pretending otherwise would be an
        # assertion that cannot be satisfied by the truth.
        ("a reorder onto a field that is not there", PREVIEW_TOOLSET,
         "PreviewTurboMCPStructFields",
         {"struct": OBJECT_EDIT, "changes": "move:Alpha, above=NoSuchField"},
         "is not a field of"),
        ("a reorder relative to itself", PREVIEW_TOOLSET,
         "PreviewTurboMCPStructFields",
         {"struct": OBJECT_EDIT, "changes": "move:Alpha, above=Alpha"},
         "cannot be moved relative to itself"),
        ("a reorder with no anchor", PREVIEW_TOOLSET, "PreviewTurboMCPStructFields",
         {"struct": OBJECT_EDIT, "changes": "move:Alpha"}, "does not say where"),
        ("both above= and below=", PREVIEW_TOOLSET, "PreviewTurboMCPStructFields",
         {"struct": OBJECT_EDIT, "changes": "move:Alpha, above=Beta, below=Gamma"},
         "not both"),
        ("an unknown reorder option", PREVIEW_TOOLSET, "PreviewTurboMCPStructFields",
         {"struct": OBJECT_EDIT, "changes": "move:Alpha, sideways=Beta"},
         "not an option of a reorder"),
        ("a non-numeric force", PREVIEW_TOOLSET, "PreviewTurboMCPStructFields",
         {"struct": OBJECT_READ, "changes": "remove:Keep, force=lots"},
         "is not a count"),
        ("the same field changed twice", PREVIEW_TOOLSET, "PreviewTurboMCPStructFields",
         {"struct": OBJECT_EDIT, "changes": "Alpha: int, Alpha: double"},
         "changed twice"),
        ("'remove' with no field named", PREVIEW_TOOLSET, "PreviewTurboMCPStructFields",
         {"struct": OBJECT_EDIT, "changes": "remove:"}, "needs a field name"),
        ("'move' with no field named", PREVIEW_TOOLSET, "PreviewTurboMCPStructFields",
         {"struct": OBJECT_EDIT, "changes": "move:, above=Alpha"}, "needs a field name"),
        ("a removal carrying an option other than force", PREVIEW_TOOLSET,
         "PreviewTurboMCPStructFields",
         {"struct": OBJECT_EDIT, "changes": "remove:Alpha, banana=1"},
         "only option is force"),
    ):
        out = session.call(toolset, tool, args)
        ok = out.startswith("error:") and expect in out
        check("refuses %s" % label, ok, out[:150])

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
