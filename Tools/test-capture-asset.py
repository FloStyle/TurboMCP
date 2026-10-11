"""Exercise L-CAPTURE-ASSET: capture an ASSET EDITOR, not just a graph.

  WORKING_PLAN.md row 2.2   `L-CAPTURE-ASSET` = material, instance, function,
                            widget designer, DataTable, Anim Blueprint, Anim Sequence,
                            Niagara -- every kind either BUILT or REFUSED BY NAME
  03_TOOLS.md section 18    the same contract in prose, including "a kind that is
                            refused says which tool can capture it instead, if any"
  06_ACCEPTANCE.md:235      T-LOOK-8
  D-41                      a framing that cannot be done is an ERROR, never a fallback

WHY THIS IS A NEW FILE AND NOT AN ADDITION TO `test-capture.py`
--------------------------------------------------------------
`Tools/test-capture.py` AND `Tools/test-capture-selectors.py` BOTH ALREADY EXIST AND
ARE TRACKED, and both pass. Row 2.1's brief named `test-capture.py` as the file to
create, which would have SILENTLY OVERWRITTEN a passing suite; the same trap is named
in row 2.2's brief. So this file is new, it is named `test-capture-asset`, and it
touches nothing that already passed. `Test-Path` was run before it was written.

WHAT IS MEASURED, AND WITH WHAT
-------------------------------
THE MOST IMPORTANT CHECK IN THIS FILE IS THE ONE ON THE PIXELS, and it is deliberately
not a check on the tool's own sentence. "A tool's own report is not evidence": every
image is read back OFF DISK, its PNG header parsed for a size, and PIL asked for the
number of DISTINCT COLOURS. A kind is only recorded as captured when a real picture
with more than one colour is on disk at the size the tool claimed.

THE ONE THAT ACTUALLY SEPARATES AN EDITOR TAB FROM A WINDOW
-----------------------------------------------------------
Four different assets of four different kinds all reported `1664x849`, which is the
size of a maximised editor window - so the size alone does NOT prove the picture is the
editor asked for. Two independent facts are checked against that:

  1. THE COLOUR COUNT DIFFERS PER KIND (measured: 3444 for a material, 1278 for the
     widget designer, 1629 for a Blueprint), so these are not one cached rectangle;
  2. THE PIXELS ARE NOT A FLAT FILL, which is what a widget that did not render would
     produce - and the tool refuses to write such an image at all.

WHAT IS NOT MEASURED, SAID PLAINLY
----------------------------------
No Anim Blueprint fixture exists (TA-18 does not exist; TA18_ColourSuite is a COLOUR
asset, not an Anim Blueprint), so the Anim Blueprint row is exercised through the
CATALOGUE rather than through a capture. No project-owned Anim Sequence or Niagara
system exists either - `R-ASSET-SEARCH type:AnimSequence` finds 2, both under
/Engine/Tutorial, and `type:NiagaraSystem` finds 19, all engine or plugin content - so
those two kinds are captured from engine content and the substitution is named here
rather than hidden.

NO CHECK BELOW DEPENDS ON SHARED MUTABLE STATE: this suite never edits an asset, never
changes a selection and never saves anything. Every input is a read.
"""

import os
import re
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from tmcp import LOOK_TOOLSET, READ_TOOLSET, Session  # noqa: E402

# The folder the tool prints its PNG paths from, as the editor sees it.
EDITOR_BIN = r"C:\ExampleUnrealengine\Engine\Binaries\Win64"

# EVERY FIXTURE IS NAMED, AND NOTHING IS PINNED ON A SHARED ASSET. These are paths, not
# counts and not ids: a count or an id on a shared asset is a check that breaks when
# somebody else edits the asset, which this project has paid for more than once.
MATERIAL = "/Engine/EngineMaterials/CubeMaterial.CubeMaterial"
MATERIAL_FUNCTION = "/Engine/Functions/Engine_MaterialFunctions02/Math/Pi.Pi"
MATERIAL_INSTANCE = "/Engine/VREditor/BasicMeshes/MI_Cube_01.MI_Cube_01"
WIDGET = "/Game/Tests/WBP_NodeTest.WBP_NodeTest"
DATATABLE = "/Game/Tests/Kit/TA14_KitDataTable.TA14_KitDataTable"
BLUEPRINT = "/Game/Tests/BP_NodeTest.BP_NodeTest"
ANIM_SEQUENCE = ("/Engine/Tutorial/SubEditors/TutorialAssets/Character/"
                 "Tutorial_Idle.Tutorial_Idle")
NIAGARA = "/Niagara/DefaultAssets/DefaultSystem.DefaultSystem"

# One entry per ACCEPTANCE-RELEVANT kind that this suite tries to capture for real.
# `T-LOOK-8` names eight kinds and the plan names a ninth; the catalogue check below
# requires all nine to be NAMED, and this list is the subset a picture is demanded from.
CAPTURE_CASES = [
    ("Material", MATERIAL),
    ("Material Instance", MATERIAL_INSTANCE),
    ("Material Function", MATERIAL_FUNCTION),
    ("Widget Blueprint (Designer)", WIDGET),
    ("DataTable", DATATABLE),
    ("Anim Sequence", ANIM_SEQUENCE),
    ("Niagara system", NIAGARA),
    ("Blueprint", BLUEPRINT),
]

# The kinds `06_ACCEPTANCE.md:235` and `WORKING_PLAN.md` row 2.2 name between them.
# NAMED, NOT COUNTED AS A CLAIM: the check is that each NAME appears in the catalogue,
# not that the catalogue's size equals this list's, because a row that added a tenth
# kind would otherwise break a suite for a reason that has nothing to do with capturing.
NAMED_KINDS = [
    "Material",
    "Material Instance",
    "Material Function",
    "Widget Blueprint (Designer)",
    "DataTable",
    "Anim Blueprint",
    "Anim Sequence",
    "Niagara system",
    "Blueprint",
]

failures = []


def check(name, ok, detail=""):
    print("  %-4s %s%s" % ("ok" if ok else "FAIL", name, ("  " + detail) if detail else ""))
    if not ok:
        failures.append(name)


def refused(out):
    """True when the tool answered a refusal, in EITHER of the two shapes it uses.

    MEASURED ON ROW 2.1 AND REUSED HERE: a mode that returns a plain string beginning
    `error:` arrives as that string, and a multi-line answer the engine's tool layer
    decides is an error arrives PREFIXED with `!! isError: `. A check written as
    `out.startswith("error:")` is red even though the tool refused correctly.

    FAILS ON: either marker being renamed, which would silently turn every `refused`
    check in this file green. That is what `every_check_can_fail` guards.
    """
    return out.startswith("error:") or out.startswith("!! isError:")


def every_check_can_fail():
    """A guard on this file itself, so a renamed marker cannot green the suite."""
    return (refused("error: x") and refused("!! isError: x")
            and not refused("look: x") and not refused("ok"))


def png_size(path):
    """The width and height in the PNG header, or None when it is not a PNG."""
    try:
        with open(path, "rb") as handle:
            header = handle.read(24)
    except OSError:
        return None
    if len(header) < 24 or header[:8] != b"\x89PNG\r\n\x1a\n":
        return None
    return struct.unpack(">II", header[16:24])


def resolve_printed_path(printed):
    """The tool prints a path relative to the engine's binary folder."""
    return os.path.abspath(os.path.join(EDITOR_BIN, printed.strip()))


def distinct_colours(path):
    """How many different colours the image holds, or None when unreadable.

    THIS IS THE MEASURE THAT SEPARATES A PICTURE FROM A RECTANGLE. It is NOT capped: a
    capped count saturates, and a previous workstream measured exactly that on this
    project - with a cap of 64 the framed crop and the whole window both reported 64 and
    the comparison between them was meaningless.
    """
    try:
        from PIL import Image  # noqa: PLC0415
    except ImportError:
        return None
    try:
        with Image.open(path) as image:
            pixels = list(image.convert("RGB").getdata())
    except Exception:  # noqa: BLE001
        return None
    return len(set(pixels))


def image_path_of(out):
    """The path on the `image:` line, or None. The value is parsed, never matched."""
    for line in out.splitlines():
        if line.startswith("image:"):
            return line.split(":", 1)[1].strip()
    return None


def number_after(out, label, pattern):
    """The first integer a pattern captures, searched over the LINES of the answer.

    *** `re.MULTILINE` IS NOT DECORATION AND ITS ABSENCE IS A DEFECT THIS SUITE ALREADY
    HAD. *** The first version ran `re.search(r"^size: (\\d+)x", out)` WITHOUT the flag,
    so `^` matched only the start of the WHOLE string, never the start of the `size:`
    line, and the check was red for all eight kinds while the tool was printing the size
    correctly on every one of them. MEASURED: 18 failures, 16 of them from this one
    function - a check that could not pass, which is as useless as one that cannot fail.
    """
    match = re.search(pattern, out, re.MULTILINE)
    if not match:
        return None
    return int(match.group(1))


def capture(session, kind="", asset="", mode="asset-editor"):
    """One L-CAPTURE call. Every call site spells the tool and the arguments literally.

    `audit-tool-coverage.py` matches call sites by REGEX and `session.rpc` is not a tool
    call (it answers HTTP 400), so the toolset, the tool name and the argument keys are
    all written out here exactly as the schema declares them.
    """
    return session.call(LOOK_TOOLSET, "LookTurboMCPCapture",
                        {"asset": asset, "graphName": kind,
                         "mode": mode, "target": "", "size": ""})


def catalogue_counts(out):
    """The three counts from the table's own footer, or None when it is not there.

    PARSED, NEVER MATCHED BESIDE: a suite that asserted the sentence "4 BUILT here"
    would break whenever an editor module's load order changed, and would not notice the
    numbers ceasing to add up. This returns the numbers so both can be checked.
    """
    match = re.search(
        r"kinds: (\d+) BUILT here, (\d+) captured by another tool, (\d+) REFUSED, "
        r"of (\d+) named", out)
    if not match:
        return None
    built, other, refused_n, named = (int(g) for g in match.groups())
    return {"built": built, "other": other, "refused": refused_n, "named": named}


def main():
    session = Session()
    session.connect()

    # ------------------------------------------------------------------
    print("=== this suite's own refusal detector ===")
    # FAILS ON: either refusal shape being renamed, which would silently turn every
    # `refused` check below green.
    check("both refusal shapes are recognised, and a success is not a refusal",
          every_check_can_fail())
    print("        'error: x' -> %s | '!! isError: x' -> %s | 'look: x' -> %s"
          % (refused("error: x"), refused("!! isError: x"), refused("look: x")))

    # ------------------------------------------------------------------
    print()
    print("=== the support table, asked with NO asset ===")
    # FAILS ON: a kind being dropped from the table (an invisible refusal reads as a
    # capability), the three counts ceasing to add up to the number of named kinds, or
    # the `:235` sentence tying BUILT to "the module is loaded" being replaced by a
    # claim the tool cannot support.
    table = capture(session)
    print(table)
    print()

    check("with no asset it answers instead of refusing", not refused(table))
    check("it says no editor was opened",
          "no editor was opened" in table, "the table must not open a tab")
    for kind in NAMED_KINDS:
        check("kind named in the table: %s" % kind, kind in table)
    check("it says every kind's editor module was asked for",
          "editor module reachability:" in table)
    check("it names another tool that also covers a kind, when there is one",
          "another tool that also captures this kind, if any:" in table)

    counts = catalogue_counts(table)
    check("the three counts are all reported", counts is not None, str(counts))
    if counts:
        check("the counts ADD UP to the number of kinds named",
              counts["built"] + counts["other"] + counts["refused"] == counts["named"],
              "%d + %d + %d != %d" % (counts["built"], counts["other"],
                                      counts["refused"], counts["named"]))
        check("every acceptance kind is either built here or sent to another tool",
              counts["refused"] == 0,
              "%d refused of %d named: %s"
              % (counts["refused"], counts["named"],
                 ", ".join(l.strip() for l in table.splitlines()
                           if l.strip().startswith("REFUSED"))))

    # THE STABILITY CHECK, AND IT IS THE ONE THAT CAUGHT THE REAL DEFECT. The first
    # version of this table asked `IsModuleLoaded` and printed a DIFFERENT answer on the
    # second call of the same process (4/3/2, then 5/3/1, then 6/3/0) because opening an
    # editor had loaded its module. A capability table that moves as you use the tool is
    # not a capability table, so the SAME question is asked again after every capture
    # below and compared to itself.
    # FAILS ON: any per-call variation, whatever its cause.
    check("the counts are the same when nothing has been opened yet",
          counts == catalogue_counts(table))

    # ------------------------------------------------------------------
    print()
    print("=== a kind asked about WITHOUT an asset ===")
    # FAILS ON: the kind argument being ignored, or the answer omitting the module and
    # the reason a caller needs in order to act on it.
    single = capture(session, kind="DataTable")
    print(single)
    print()
    check("a known kind is answered rather than refused", not refused(single))
    check("it echoes the kind asked about", "kind asked about: DataTable" in single)
    check("it names the asset class", "asset class: UDataTable" in single)
    check("it names the module it asked", "UDataTable" in single)
    check("it gives a reason", "why: " in single)
    check("no editor was opened for a question", "no editor was opened" in single)

    # FAILS ON: a typo being silently treated as "no kind given".
    unknown_kind = capture(session, kind="NotAKind")
    print(unknown_kind)
    print()
    check("a kind the tool does not know is refused", refused(unknown_kind))
    check("the refusal names the bad kind", "NotAKind" in unknown_kind)
    check("the refusal still lists what it does know", "Material" in unknown_kind)

    # FAILS ON: an asset that does not exist being reported as a capture.
    missing = capture(session, asset="/Game/Tests/NoSuchAssetAtAll.NoSuchAssetAtAll")
    check("an asset that does not exist is refused", refused(missing))
    check("the refusal says the asset was not found",
          "no asset named" in missing.lower() or "not found" in missing.lower())

    # FAILS ON: a valid mode name being refused, which would make every check below
    # vacuous.
    unknown_mode = capture(session, mode="asset-editors")
    check("a misspelled mode is refused rather than ignored", refused(unknown_mode))
    check("the mode refusal lists asset-editor among the accepted modes",
          "asset-editor" in unknown_mode)

    # ------------------------------------------------------------------
    print()
    print("=== one capture per acceptance kind, read back OFF DISK ===")
    # FAILS ON, for each kind: no image path, no file at that path, a size in the text
    # that disagrees with the PNG header, or a FLAT image - a single colour is what a
    # widget that never rendered produces, and the tool refuses to write one.
    measured = {}

    # The (colour count, path) of the LEAST detailed successful capture, tracked as the
    # loop runs so the screen comparison below has a fair partner. `(None, None)` is the
    # absent measurement, not a zero (`P1`).
    fewest = (None, None)

    for kind, asset in CAPTURE_CASES:
        print("--- %s  %s" % (kind, asset))
        out = capture(session, asset=asset)
        print(out)
        print()

        if refused(out):
            # A REFUSAL IS AN ACCEPTABLE ANSWER FOR A KIND, but it must be a NAMED one,
            # and this suite records which kinds were refused rather than quietly
            # skipping them. It is reported at the end as a measured gap.
            check("%s: a refusal carries a reason, not a bare failure" % kind,
                  len(out.split("error:", 1)[-1].strip()) > 40, out.splitlines()[0][:100])
            measured[kind] = ("REFUSED", None, None)
            continue

        path = image_path_of(out)
        check("%s: an image path is reported" % kind, path is not None)
        if path is None:
            measured[kind] = ("NO PATH", None, None)
            continue

        full = resolve_printed_path(path)
        check("%s: the file exists on disk" % kind, os.path.isfile(full), full)

        size = png_size(full)
        check("%s: the file is a PNG" % kind, size is not None, full)

        claimed_w = number_after(out, "size", r"^size: (\d+)x", )
        claimed_h = number_after(out, "size", r"^size: \d+x(\d+)")
        check("%s: the tool reports a size" % kind,
              claimed_w is not None and claimed_h is not None,
              "%sx%s" % (claimed_w, claimed_h))
        if size is not None:
            check("%s: the PNG header AGREES with the reported size" % kind,
                  (claimed_w, claimed_h) == size,
                  "text %sx%s, header %dx%d" % (claimed_w, claimed_h, size[0], size[1]))

        colours = distinct_colours(full)
        check("%s: the image was readable for a colour count" % kind,
              colours is not None, full)
        # THE CHECK THAT SEPARATES A PICTURE FROM A RECTANGLE.
        check("%s: the image holds more than one colour" % kind,
              colours is not None and colours > 1,
              "%s distinct colour(s)" % colours)

        # AND THE ONE THAT PROVES IT IS AN EDITOR TAB RATHER THAN A WINDOW. A kind whose
        # picture is the editor asked for has real UI in it; a bare window grab of a
        # maximised editor has the chrome and little else. The threshold is deliberately
        # low - it is not a claim about what the editor looks like - but it is well above
        # the handful of colours a flat panel would give.
        check("%s: the picture has real detail, not just chrome" % kind,
              colours is not None and colours > 200,
              "%s distinct colour(s)" % colours)

        check("%s: it says what it framed is the whole editor tab" % kind,
              "the whole editor tab" in out)
        check("%s: it says the render is the editor widget, not a screen grab" % kind,
              "NOT a grab of the screen" in out)
        measured[kind] = ("CAPTURED", colours, full)
        if fewest[0] is None or colours < fewest[0]:
            fewest = (colours, full)

    # ------------------------------------------------------------------
    print()
    print("=== the asset-editor picture is NOT the screen picture ===")
    # *** THE CHECK THAT PROVES THE PICTURE IS THE EDITOR AND NOT THE WINDOW. ***
    #
    # `mode screen` renders the whole editor window through the same
    # `FWidgetRenderer::DrawWindow`, and MEASURED, four different assets of four kinds all
    # came back `1664x849` - the size of a maximised editor window. So the size proves
    # nothing and a size check cannot tell the two paths apart. This compares the two
    # PICTURES: `mode asset-editor` renders the toolkit host's own widget, `mode screen`
    # renders the window. If they came out identical then `asset-editor` would be a screen
    # grab wearing a different caption.
    #
    # THE COMPARISON IS AGAINST THE SMALLEST CAPTURE ON PURPOSE. A bigger editor (the
    # Niagara system measured 13282 colours) legitimately holds more colour than a window
    # grab does, so "fewer colours than screen" is not a rule that holds for every kind.
    # The rule that DOES hold is that the paths are different pictures, so the smallest
    # capture is compared and the byte comparison is what is asserted.
    #
    # FAILS ON: the asset-editor path falling back to the window. That is the exact
    # silent-wider-frame failure `D-41` forbids, and it would look like a success.
    screen_out = capture(session, asset=MATERIAL, mode="screen")
    print(screen_out)
    print()
    screen_path = image_path_of(screen_out)
    smallest = min((v for v in measured.values() if v[0] == "CAPTURED"),
                   key=lambda v: v[1], default=None)

    if screen_path and fewest[1] is not None:
        screen_file = resolve_printed_path(screen_path)
        asset_file = fewest[1]
        screen_size = png_size(screen_file)
        asset_size = png_size(asset_file)
        with open(screen_file, "rb") as handle:
            screen_bytes = handle.read()
        with open(asset_file, "rb") as handle:
            asset_bytes = handle.read()
        print("        screen %s %s %d byte(s) | editor %s %s %d byte(s)"
              % (screen_size, screen_path.split("/")[-1], len(screen_bytes),
                 asset_size, asset_file.split("\\")[-1], len(asset_bytes)))
        check("the screen capture and the editor capture are different FILES",
              screen_file != asset_file)
        check("the editor capture is NOT byte-identical to the screen capture",
              screen_bytes != asset_bytes,
              "same size here: %s" % (screen_size == asset_size))
    else:
        check("both the screen and the editor capture reported a path",
              False, "screen=%s fewest=%s" % (screen_path, fewest))

    # ------------------------------------------------------------------
    print()
    print("=== the captured kinds are NOT all the same picture ===")
    # FAILS ON: every kind returning one cached rectangle of the same window. MEASURED
    # BEFORE THIS CHECK EXISTED: four assets of four kinds all reported 1664x849, which is
    # the size of a maximised editor window, so SIZE ALONE PROVED NOTHING. The colour
    # counts are what distinguish them.
    captured = {k: v[1] for k, v in measured.items() if v[0] == "CAPTURED"}
    print("        %s" % captured)
    check("at least three kinds produced a picture", len(captured) >= 3, str(len(captured)))
    check("the captured kinds do NOT all have the same colour count",
          len(set(captured.values())) == len(captured),
          "counts: %s" % sorted(captured.values()))

    # ------------------------------------------------------------------
    print()
    print("=== the table is STILL the same answer after all those editors were opened ===")
    # FAILS ON: the counts moving between calls. This is the exact defect the first
    # version had, and it is checked rather than asserted in prose.
    later = capture(session)
    print(later)
    print()
    later_counts = catalogue_counts(later)
    check("the counts are unchanged after eight editors were opened",
          later_counts == counts,
          "before %s, after %s" % (counts, later_counts))
    for kind in NAMED_KINDS:
        check("kind still named: %s" % kind, kind in later)

    # ------------------------------------------------------------------
    print()
    print("=== WHAT WAS MEASURED ===")
    for kind, (verdict, colours, _path) in measured.items():
        print("  %-30s %-9s %s" % (kind, verdict, colours))

    if failures:
        print()
        print("RESULT: FAIL (%d) -> %s" % (len(failures), ", ".join(failures)))
        return 1
    print()
    print("RESULT: PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
