# Acceptance coverage: T-MAT-8.
"""Check Material Blocks preview/edit against native and saved fixtures.

Native cases cover output selection, channel masks, reroute chains, material-output
consumers, stale-plan refusal and repeatability. The saved fixture covers duplicate
usage merging. Passing compiler checks do not establish visual equivalence.
"""

import os
import runpy
import re
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import tmcp_editor
from tmcp import (  # noqa: E402
    EDIT_TOOLSET,
    PREVIEW_TOOLSET,
    READ_TOOLSET,
    Session,
)

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TOOLS = os.path.join(REPO, "Tools")

# THE ASSET NAME, NOT THE FILE NAME. See the docstring.
ASSET = "M_TurboMCPFixture"

PREVIEW = "PreviewTurboMCPMaterialBlocks"
EDIT = "EditTurboMCPMaterialBlocks"

BUILT_BOX_COUNT = 2

failures = []
checks_run = [0]


def check(name, ok, detail=""):
    checks_run[0] += 1
    print("  %-4s %s%s" % ("ok" if ok else "FAIL", name,
                           ("  " + detail) if detail and not ok else ""))
    if not ok:
        failures.append(name)


def number_before(pattern, text, default=-1):
    """A COUNT PARSED OUT OF A SENTENCE. This project's rule: NEVER match a sentence beside a number,
    because a reworded sentence then silently stops matching and the check passes for the wrong
    reason."""
    match = re.search(pattern, text)
    return int(match.group(1)) if match else default


def key_set(text):
    """
    Every expression key the READER prints for this material.

    THE KEYS COME FROM THE READER RATHER THAN FROM THE TOOL'S OWN REPORT, which is the rule this
    project states for every writer: a tool's own report is not evidence. A material expression's key
    is six lowercase hex characters followed by the class name on the same line, which is the shape
    `R-MAT-LOAD` and `R-MAT-BLOCKS` both print.
    """
    return set(re.findall(r"\b([0-9a-f]{6})\s+MaterialExpression", text))


def rebuild_fixture(restore=True):
    """
    Regenerates the shared fixture so the suite is re-runnable from ANY state.

    THIS STOPS THE EDITOR, AND THAT IS WHY THE SESSION IS MADE AFTERWARDS RATHER THAN BEFORE.
    `make-material-asset.ps1` stops the editor itself, because the running editor holds the .uasset
    open and a save against a held handle is lost. MEASURED in this project: rebuilding with a live
    session first produced `URLError: [WinError 10061] no connection could be made` - the session was
    talking to an editor that had just been shut down.
    """
    if os.environ.get("TURBOMCP_RUNNER_OWNS_FIXTURES") == "1":
        if not restore:
            return True
        ok, detail = tmcp_editor.restore_runner_fixture()
        print("    runner fixture restore: %s" % detail)
        return bool(ok)
    completed = subprocess.run(
        ["pwsh", "-NoProfile", "-File", os.path.join(TOOLS, "make-material-asset.ps1")],
        capture_output=True, text=True, cwd=REPO)
    output = (completed.stdout or "") + (completed.stderr or "")
    if "PASS" not in output:
        print("THE FIXTURE COULD NOT BE REBUILT, so the checks below cannot run on a known state.")
        print(output[-1500:])
        return False
    return True


def connect():
    """
    Starts the editor and returns a connected session, or None.

    `subprocess.call` RATHER THAN `run(capture_output=True)`, AND THIS IS A MEASURED FACT RATHER THAN A
    PREFERENCE, recorded in this project before: THE EDITOR DOES NOT COME UP WHEN ITS OUTPUT IS
    CAPTURED, because the launcher leaves a pipe open. So the output goes to the console.
    """
    if os.environ.get("TURBOMCP_RUNNER_OWNS_FIXTURES") == "1":
        ok, detail = tmcp_editor.ensure_editor()
        print("    runner editor: %s" % detail)
        if not ok:
            return None
    else:
        subprocess.call(
            ["pwsh", "-NoProfile", "-File", os.path.join(TOOLS, "start-editor.ps1")], cwd=REPO)

    deadline = time.time() + 180
    while time.time() < deadline:
        try:
            session = Session()
            session.connect()
            return session
        except Exception:  # noqa: BLE001 - not up yet is the expected case
            time.sleep(3)
    return None


def main():
    quality_session = Session()
    quality_session.connect()
    support = runpy.run_path(os.path.join(TOOLS, "test-widget-roundtrip.py"))
    test_name = "TurboMCP.Material.BlocksQuality"
    quality_session.call(support["AUTO_TOOLSET"], "DiscoverTests", {})
    raw = quality_session.call(support["AUTO_TOOLSET"], "RunTests", {"testNames": [test_name]})
    print(raw)
    entry, _ = support["parse_run_tests_report"](raw, test_name)
    assert not entry["warnings"], entry["warnings"]
    for toolset, tool in ((PREVIEW_TOOLSET, PREVIEW), (EDIT_TOOLSET, EDIT)):
        help_text = quality_session.call(READ_TOOLSET, "ReadTurboMCPToolHelp", {"toolset": toolset, "tool": tool})
        assert "asset" in help_text.lower() and "material functions" in help_text.lower(), help_text
        refusal = quality_session.call(toolset, tool, {"asset": "MF_TurboMCPFixture"})
        assert refusal.startswith("error:") and "material functions" in refusal, refusal
    print("=== checking whether the shared fixture needs rebuilding, WITHOUT stopping the editor ===")
    session = connect()
    if session is None:
        print()
        print("RESULT: FAIL (the editor never came up, so nothing could be checked)")
        return 1

    look = session.call(READ_TOOLSET, "ReadTurboMCPMaterialBlocks",
                        {"asset": ASSET, "tokenBudget": 6000})
    boxes_at_start = number_before(r"blocks: (\d+) comment box", look)
    print("    the fixture reports %s box(es); it is built with %d" % (boxes_at_start, BUILT_BOX_COUNT))

    if boxes_at_start != BUILT_BOX_COUNT:
        print("    it is NOT in its built state, so it is rebuilt - and that stops the editor, so the")
        print("    session is remade afterwards.")
        del session
        if not rebuild_fixture():
            print()
            print("RESULT: FAIL (the material fixture could not be rebuilt)")
            return 1
        session = connect()
        if session is None:
            print()
            print("RESULT: FAIL (the editor never came back after the rebuild)")
            return 1
        look = session.call(READ_TOOLSET, "ReadTurboMCPMaterialBlocks",
                            {"asset": ASSET, "tokenBudget": 6000})
        boxes_at_start = number_before(r"blocks: (\d+) comment box", look)
        print("    after the rebuild it reports %s box(es)" % boxes_at_start)
    else:
        print("    it is already in its built state, so the editor was NOT stopped")

    print()
    print("=== the PREVIEW writes nothing and lists what the edit would change ===")

    keys_before_preview = key_set(look)
    preview = session.call(PREVIEW_TOOLSET, PREVIEW, {"asset": ASSET})
    print()
    print(preview)
    print()

    check("the preview names the asset it looked at", ASSET in preview,
          "the preview did not mention %s" % ASSET)

    items = number_before(r"items the edit would change: (\d+)", preview)
    check("the preview states how many items the edit would change", items >= 0,
          "no 'items the edit would change' line was found")

    look_after_preview = session.call(READ_TOOLSET, "ReadTurboMCPMaterialBlocks",
                                      {"asset": ASSET, "tokenBudget": 6000})
    check("THE PREVIEW CHANGED NOTHING - the reader sees the same expression keys",
          key_set(look_after_preview) == keys_before_preview,
          "the preview altered the material, which it must never do")

    print("=== the two item kinds the fixture does NOT exercise, asserted absent ===")

    has_crossing = "LABELS TO ADD" in preview
    has_reroute = "IN-BLOCK REROUTES TO REMOVE" in preview
    has_merge = "USAGE NODES TO MERGE" in preview

    check("the fixture HAS a merge to exercise, so the positive case is real", has_merge,
          "no merge was listed, so this suite would be proving nothing at all")

    check("the pass invents NO crossing on a fixture that has none", not has_crossing,
          "a crossing was listed; the fixture has no value read from two different boxes, so this "
          "would be a rule firing on a material it does not describe")

    check("the pass invents NO in-block reroute on a fixture that has none", not has_reroute,
          "an in-block reroute was listed; the fixture builds no plain reroute at all, so this would "
          "be a rule firing on a material it does not describe")

    print()
    print("=== the EDIT changes exactly the listed items ===")

    # THE KEYS ARE TAKEN IMMEDIATELY BEFORE THE EDIT, THROUGH THE READER, so half one is measured
    # against the material as the edit actually found it rather than against an earlier read.
    look_before = session.call(READ_TOOLSET, "ReadTurboMCPMaterialBlocks",
                               {"asset": ASSET, "tokenBudget": 6000})
    keys_before_edit = key_set(look_before)

    listed_removals = set(re.findall(r"\n\s+- ([0-9a-f]{6})\s*$", preview, re.MULTILINE))

    report = session.call(EDIT_TOOLSET, EDIT, {"asset": ASSET})
    print()
    print(report)
    print()

    look_after = session.call(READ_TOOLSET, "ReadTurboMCPMaterialBlocks",
                              {"asset": ASSET, "tokenBudget": 6000})
    keys_after_edit = key_set(look_after)

    lost = keys_before_edit - keys_after_edit
    gained = keys_after_edit - keys_before_edit

    print("    keys before: %d, after: %d, lost: %d, gained: %d"
          % (len(keys_before_edit), len(keys_after_edit), len(lost), len(gained)))

    check("the edit LOST nothing that the Preview did not list",
          lost.issubset(listed_removals),
          "lost %s, of which %s was never listed in the Preview"
          % (sorted(lost), sorted(lost - listed_removals)))

    check("the Preview listed the removal the edit actually made",
          len(listed_removals) == len(lost),
          "the Preview listed %d removal(s) but %d node(s) went away"
          % (len(listed_removals), len(lost)))

    check("the edit added nothing to the material",
          len(gained) == 0,
          "gained %s; a merge must not create expressions" % sorted(gained))

    # THE EDIT MUST SAY `changed` WHEN THE PREVIEW LISTED SOMETHING, because a tool that lists work and
    # then reports having done none of it is the failure mode this half of the sentence is about.
    check("the edit reports that it changed something", report.startswith("changed:"),
          "the edit did not report a change on a material the Preview said had %d item(s)" % items)

    listed_in_report = set(re.findall(r"\nremoved ([0-9a-f]{6})", report))
    check("D-45: the edit LISTS each removal by key rather than counting them",
          listed_in_report == lost,
          "the report listed %s and the material lost %s"
          % (sorted(listed_in_report), sorted(lost)))

    check("the edit reports the COMPILED RESULT explicitly", "CHECK RESULTS (not proof of identical rendered output)" in report,
          "the edit made no compiled-result statement at all")

    check("the edit reports that no compile error is new",
          "none of them new" in report,
          "the compiled-result line did not contain the 'none of them new' claim")

    check("the engine reports ZERO errors after the edit",
          number_before(r"the engine reports (\d+) error", report, default=-1) == 0,
          "the material has compile errors after the edit")

    check("the edit REPORTS StateId rather than asserting on it",
          "StateId" in report and "reported rather than asserted" in report,
          "the compiled-result line carried no StateId, so the check cannot be inspected")

    # EVERY PRE-EXISTING EXPRESSION THAT WAS NOT LISTED AS A REMOVAL MUST STILL BE THERE - the set
    # comparison run in the direction that catches a silent drop, taken from the reader.
    check("every expression the edit did not remove is still present",
          len(lost - listed_removals) == 0,
          "the material lost expressions the Preview never listed")

    print()
    print("=== the label now follows D-17: one usage node per block ===")

    labels = session.call(READ_TOOLSET, "ReadTurboMCPMaterialLabels", {"asset": ASSET})
    print(labels)
    print()

    vertical = re.search(r"@VerticalMix\b[\s\S]{0,300}?read in (\d+) place", labels)
    check("D-17: @VerticalMix is now read through a SINGLE usage node",
          vertical is not None and int(vertical.group(1)) == 1,
          "the reader answered %s place(s) for @VerticalMix, and D-17 asks for one"
          % (vertical.group(1) if vertical else "no"))

    check("the label still exists and is still read rather than deleted",
          vertical is not None and int(vertical.group(1)) >= 1,
          "@VerticalMix is gone or unread, so the merge removed the label instead of merging usages")

    print()
    print("=== a SECOND run has nothing to do, which is what makes this a rule rather than a rewrite ===")

    preview_again = session.call(PREVIEW_TOOLSET, PREVIEW, {"asset": ASSET})
    items_again = number_before(r"items the edit would change: (\d+)", preview_again)
    check("a second Preview lists ZERO items on a material that now follows D-17",
          items_again == 0,
          "the second Preview still lists %s item(s), so the first edit did not settle the material"
          % items_again)

    check("the second Preview says the material already follows the block rules rather than saying nothing",
          "already follows the block rules" in preview_again,
          "the refusal did not explain why there is nothing to do")

    report_again = session.call(EDIT_TOOLSET, EDIT, {"asset": ASSET})
    check("a second EDIT answers `unchanged` rather than claiming a change",
          report_again.startswith("unchanged:"),
          "the second edit answered: %s" % report_again[:120])

    print()
    print("=== the refusal names the asset rather than answering emptily ===")

    missing = session.call(PREVIEW_TOOLSET, PREVIEW, {"asset": ""})
    check("an empty asset is refused by name rather than answered emptily",
          missing.startswith("error:"),
          "an empty asset answered: %s" % missing[:120])

    no_such = session.call(PREVIEW_TOOLSET, PREVIEW, {"asset": "NoSuchMaterialAtAll"})
    check("an asset that does not exist is refused rather than answered emptily",
          no_such.startswith("error:"),
          "a missing asset answered: %s" % no_such[:120])

    print()
    print("=== the shared fixture is given back ===")

    del session
    restored = rebuild_fixture(restore=os.environ.get("TURBOMCP_RUNNER_OWNS_FIXTURES") != "1")
    check("the shared fixture was GIVEN BACK, so the next suite reads what it expects", restored,
          "a suite that mutates a shared asset must give it back")

    print()
    if failures:
        print("RESULT: FAIL (%d) -> %s" % (len(failures), ", ".join(failures)))
        return 1

    print("RESULT: PASS (%d check(s) run)" % checks_run[0])
    print("; Scope: native connection and cleanup cases plus the saved merge fixture. "
          "Rendered equivalence and graph layout require separate visual evidence.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
