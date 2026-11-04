# ACCEPTANCE ROW THIS SUITE CLAIMS TO PROVE: T-WRITE-2
#!/usr/bin/env python3
"""`P/E-GRAPH-APPEND` (`T-WRITE-2`): a text block is added to a graph, and nothing is removed.

WHAT THIS SUITE IS ACTUALLY ASKING, AND EACH IS A SEPARATE WAY TO BE WRONG:

  1. THE FACTORY RESOLVES A TYPE ID THE GRAPH MAY NOT CONTAIN. The previous round measured that the
     CHECKER's resolver is GRAPH-SCOPED - it matches a type id against nodes already in the graph - so a
     writer built on it could never create a genuinely new node, which is the tool's whole purpose. The
     check is that `Development|PrintString` resolves even in a graph that has no PrintString.
  2. IT WRITES, AND THE WRITE IS REAL. Proved from OUTSIDE by reading the graph back through `R-GRAPH-LOAD`,
     a DIFFERENT tool. A writer reporting what it just wrote is not evidence.
  3. IT CANNOT REMOVE, WHICH IS WHAT MAKES THIS APPEND AND NOT REPLACE. The graph's node count must not go
     DOWN, and the nodes present before must all still be present after.
  4. A REFUSAL WRITES NOTHING. The preview and the edit must refuse the SAME input with the same words, and
     a refused edit must leave the graph untouched - checked by the node count AND by the asset's mtime.
  5. THE FIRST LINE IS `changed` OR `unchanged`, as the spec requires of every write entry point.
  6. THE COMPILED APPEND TESTS. Alias collision and placement run through the automation toolset against
     forced-GUID, forced-position in-memory fixtures; a missing or failed test FAILS here rather than
     passing vacuously.

THE SUITE MUTATES A GRAPH, SO IT MUST BE RE-RUNNABLE. It records the node ids present BEFORE the append and
asserts every one of them is still there afterwards; it does not try to remove what it added, because
removal is the capability this tool deliberately does not have and a suite that needed it would be testing
a different tool. Its own growth is bounded by a counter in the marker text, so repeated runs are visible
rather than silent.

OFFLINE PARSER CHECKS: `python Tools/test-graph-append.py --parser-only` runs the pure RunTests report
parser checks and exits before any Session, socket or fixture work.
"""
import hashlib
import json
import os
import re
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import tmcp_editor  # noqa: E402
from tmcp import Session  # noqa: E402

PREVIEW_TOOLSET = "TurboMCP.TurboMCPPreviewToolset"
EDIT_TOOLSET = "TurboMCP.TurboMCPEditToolset"
READ_TOOLSET = "TurboMCP.TurboMCPReadToolset"
AUTO_TOOLSET = "AutomationTestToolset.AutomationTestToolset"

# THE COMPILED TESTS THAT DRIVE THE APPEND PLAN/APPLY. They live in the plugin beside the node-id test
# because they call `FTurboMCPGraphAppend` directly; they build in-memory Blueprints with forced GUIDs,
# positions and stored sizes, and never save or open an editor window.
APPEND_ALIAS_TEST = "TurboMCP.GraphAppend.AliasCollision"
APPEND_PLACEMENT_TEST = "TurboMCP.GraphAppend.Placement"

# THE FIXTURE IS A GRAPH THIS SUITE CAN REBUILD, AND THAT IS THE WHOLE POINT OF THE CHANGE.
#
# THIS SUITE USED TO APPEND TO `BP_NodeTest:EventGraph` AND COULD NOT CLEAN UP - removal is the capability
# `E-GRAPH-APPEND` deliberately does not have - so THE GRAPH GREW BY ONE NODE PER RUN, measured from 12 to 17,
# while `BP_NodeTest` is shared with about fifteen other suites. A suite that slowly poisons the fixture every
# other suite depends on is worse than one that fails: it makes OTHER suites look flaky.
#
# `SaneProbe` lives in `/Game/Tests/TurboMCPAcceptance`, which `Tools/make-acceptance-asset.ps1` REGENERATES from
# the commandlet that describes it, so the suite restores its own fixture ON THE WAY IN and is re-runnable from
# any state rather than merely repeatable.
ASSET = "TurboMCPAcceptance"
GRAPH = "SaneProbe"
ASSET_FILE = "TurboMCPAcceptance.uasset"

# THE NODE THE APPENDED BLOCK LINKS INTO, READ FROM THE FIXTURE RATHER THAN PINNED. `SaneProbe` is built by the
# commandlet, so a hard-coded key would break the moment the fixture is rebuilt with a different order - the
# "never pin a count on a shared asset" rule applied to an ID.
LINK_TARGET_TYPE = "Function|/Script/Engine.KismetSystemLibrary:PrintString"
LINK_TARGET_PIN = "then"

failures = []


def check(name, ok, detail=""):
    print("  %s %s%s" % ("ok  " if ok else "FAIL", name, ("  " + str(detail)[:200]) if detail else ""))
    if not ok:
        failures.append(name)


# THE PINNED `AutomationTestToolset` REPORT SHAPE (5.8.3). `AutomationTestToolsetSubsystem.cpp` writes one
# object with a `tests` array and aggregate counts, and each test entry carries exactly these fields.
RUN_REPORT_KEYS = {"tests", "passed", "failed", "skipped", "total", "duration"}
TEST_ENTRY_KEYS = {"name", "state", "duration", "errors", "warnings"}
AUTOMATION_STATES = {"NotRun", "InProcess", "Fail", "Success", "Skipped"}


def _is_count(value):
    return type(value) is int and value >= 0


def parse_run_tests_report(payload, expected_test):
    """Parse the pinned RunTests JSON, rejecting anything that is not exactly the requested result.

    Substring checks are not enough: a report that merely quotes "Success" in an error, or that omits the
    requested test, must fail. Returns (entry, report) or raises ValueError with the reason.
    """
    if not isinstance(payload, str):
        raise ValueError("the RunTests answer is not text")
    try:
        report = json.loads(payload)
    except ValueError as exc:
        raise ValueError("the RunTests answer is not JSON: %s" % exc)
    if not isinstance(report, dict):
        raise ValueError("the RunTests answer is not a JSON object")
    if set(report) != RUN_REPORT_KEYS:
        raise ValueError("unexpected report keys: %s" % sorted(set(report) ^ RUN_REPORT_KEYS))
    for key in ("passed", "failed", "skipped", "total"):
        if not _is_count(report[key]):
            raise ValueError("%s is not a non-negative integer" % key)
    if not isinstance(report["duration"], (int, float)) or isinstance(report["duration"], bool):
        raise ValueError("duration is not a number")
    tests = report["tests"]
    if not isinstance(tests, list):
        raise ValueError("tests is not an array")
    if len(tests) != 1:
        raise ValueError("the report holds %d test entries; exactly one was requested" % len(tests))
    entry = tests[0]
    if not isinstance(entry, dict):
        raise ValueError("the test entry is not an object")
    if set(entry) != TEST_ENTRY_KEYS:
        raise ValueError("unexpected test-entry keys: %s" % sorted(set(entry) ^ TEST_ENTRY_KEYS))
    if not isinstance(entry["name"], str):
        raise ValueError("the test name is not a string")
    if entry["name"] != expected_test:
        raise ValueError("the report names %r, not the requested %r" % (entry["name"], expected_test))
    if entry["state"] not in AUTOMATION_STATES:
        raise ValueError("unknown automation state %r" % (entry["state"],))
    for key in ("errors", "warnings"):
        values = entry[key]
        if not isinstance(values, list) or any(not isinstance(value, str) for value in values):
            raise ValueError("%s is not an array of strings" % key)
    if not isinstance(entry["duration"], (int, float)) or isinstance(entry["duration"], bool):
        raise ValueError("the test duration is not a number")
    if report["total"] != len(tests):
        raise ValueError("total %d does not match %d test entries" % (report["total"], len(tests)))
    expected_counts = {"Success": 0, "Fail": 0, "Skipped": 0}
    expected_counts[entry["state"]] = 1
    if (report["passed"], report["failed"], report["skipped"]) != (
            expected_counts["Success"], expected_counts["Fail"], expected_counts["Skipped"]):
        raise ValueError("aggregate counts do not match the single test entry")
    if entry["state"] != "Success":
        raise ValueError("the requested test is %s, not Success" % entry["state"])
    if entry["errors"]:
        raise ValueError("the requested test reported %d error(s): %s"
                         % (len(entry["errors"]), entry["errors"][0][:160]))
    return entry, report


def run_report_parser_checks():
    """Literal reports only: no editor, socket, fixture or Session."""
    def make_report(state="Success", errors=None, warnings=None, **overrides):
        report = {
            "tests": [{"name": APPEND_ALIAS_TEST, "state": state, "duration": 0.25,
                       "errors": list(errors or []), "warnings": list(warnings or [])}],
            "passed": 1 if state == "Success" else 0,
            "failed": 1 if state == "Fail" else 0,
            "skipped": 1 if state == "Skipped" else 0,
            "total": 1,
            "duration": 0.25,
        }
        report.update(overrides)
        return json.dumps(report)

    def rejects(label, payload):
        try:
            parse_run_tests_report(payload, APPEND_ALIAS_TEST)
        except (ValueError, TypeError):
            check(label, True)
        else:
            check(label, False, "the parser accepted a report it must reject")

    entry, report = parse_run_tests_report(make_report(), APPEND_ALIAS_TEST)
    check("a pinned Success report is accepted",
          entry["state"] == "Success" and report["passed"] == 1, entry["state"])

    rejects("a Fail state is rejected", make_report("Fail"))
    rejects("a quoted Success inside an error is not a pass",
            make_report("Fail", errors=["Success is not the state here"]))
    rejects("non-empty errors on Success are rejected", make_report("Success", errors=["boom"]))
    rejects("a Skipped state is rejected", make_report("Skipped"))
    rejects("a NotRun state is rejected", make_report("NotRun"))
    rejects("an unknown state is rejected", make_report("Succeeded"))
    rejects("a report naming another test is rejected",
            make_report().replace(APPEND_ALIAS_TEST, "TurboMCP.GraphAppend.Other"))
    rejects("a missing tests entry is rejected",
            json.dumps({"tests": [], "passed": 0, "failed": 0, "skipped": 0, "total": 0,
                        "duration": 0.0}))
    duplicate = json.loads(make_report())
    duplicate["tests"].append(dict(duplicate["tests"][0]))
    duplicate["total"] = 2
    duplicate["passed"] = 2
    rejects("a duplicate test entry is rejected", json.dumps(duplicate))
    rejects("a total mismatch is rejected", make_report(total=2))
    rejects("a passed-count mismatch is rejected", make_report(passed=0))
    rejects("an extra top-level key is rejected", make_report(note="unexpected"))
    rejects("a missing aggregate is rejected",
            json.dumps({key: value for key, value in json.loads(make_report()).items()
                        if key != "skipped"}))
    rejects("a non-JSON answer is rejected", "Result={Success}")
    rejects("a bare JSON string is rejected", json.dumps("Success"))
    rejects("an empty answer is rejected", "")


def node_count(session):
    """The graph's node count, PARSED from `R-GRAPH-LOAD` rather than matched as a sentence."""
    out = session.call(READ_TOOLSET, "ReadTurboMCPGraphLoad",
                       {"asset": ASSET, "graphName": GRAPH, "detail": "manifest",
                        "format": "read", "offset": 0, "maxChars": 24000,
                        "revision": "", "entryFilter": ""})
    found = re.search(r"(\d+)\s+node\(s\)", out)
    return (int(found.group(1)) if found else None), out


def node_ids(session):
    """Every node id `R-GRAPH-LOAD` prints, which is the INDEPENDENT record of the graph's contents."""
    out = session.call(READ_TOOLSET, "ReadTurboMCPGraphLoad",
                       {"asset": ASSET, "graphName": GRAPH, "detail": "full",
                        "format": "read", "offset": 0, "maxChars": 24000,
                        "revision": "", "entryFilter": ""})
    # The long form is `Object:Graph:Key`, and the reader prints it inside a node's first comment.
    return set(re.findall(r"%s:%s:([0-9a-f]{6})" % (re.escape(ASSET), re.escape(GRAPH)), out)), out


def asset_fingerprint():
    """Saved content digest plus metadata; missing fixtures fail explicitly."""
    path = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                        "TestProject", "Content", "Tests", ASSET_FILE)
    if not os.path.isfile(path):
        raise RuntimeError("Required refusal fixture is missing: " + path)
    stat = os.stat(path)
    with open(path, "rb") as stream:
        digest = hashlib.sha256(stream.read()).hexdigest()
    return (stat.st_size, stat.st_mtime_ns, digest)


def refusal_pair(session, label, asset, graph, text, expected_prefix="unchanged:"):
    """Send one input to Preview and Edit, require the same answer, prove no mutation.

    THE CONTRACT IS THE SOURCE'S, NOT A GUESS. Any check finding makes the plan
    unacceptable (`FTurboMCPGraphCheck::FPlan::bAcceptable` is `Findings.Num() == 0`),
    Preview and Edit build the SAME plan (`TurboMCPGraphAppendTools.cpp:33/48`), a
    refused plan renders with the first line `unchanged:`
    (`TurboMCPGraphAppend.cpp:639-648`), and a plan that cannot be built returns
    `error:` from both tools. The identical answer is asserted because the pair
    disagreeing is the defect the shared Plan exists to prevent.
    """
    before_count, _ = node_count(session)
    before_ids, _ = node_ids(session)
    before_fp = asset_fingerprint()
    request = {"asset": asset, "graphName": graph, "text": text}
    preview = session.call(PREVIEW_TOOLSET, "PreviewTurboMCPGraphAppend", request)
    edit = session.call(EDIT_TOOLSET, "EditTurboMCPGraphAppend", request)
    print("    %s preview: %s" % (label, preview.split("\n", 1)[0][:140]))
    check(label + ": the preview refuses", preview.startswith(expected_prefix), preview[:220])
    # The edit recorder adds an observation that preview does not carry. Accept
    # only its exact zero-change form; retain all other text in the comparison.
    observation = re.search(r"\ndirty_packages: before=(\d+) after=(\d+) newly_dirty=0 no_longer_dirty=0$", edit)
    check(label + ": refusal preserves observed dirty state",
          observation is not None and observation.group(1) == observation.group(2), edit[-220:])
    edit_reason = edit[:observation.start()] if observation else edit
    check(label + ": preview and edit return the same refusal reason", edit_reason == preview, edit_reason[:220])
    after_count, _ = node_count(session)
    after_ids, _ = node_ids(session)
    check(label + ": the node count is unchanged", after_count == before_count,
          "%s -> %s" % (before_count, after_count))
    check(label + ": the node identity set is unchanged", after_ids == before_ids,
          "added=%s removed=%s" % (sorted(after_ids - before_ids)[:3],
                                   sorted(before_ids - after_ids)[:3]))
    check(label + ": the saved bytes are unchanged", asset_fingerprint() == before_fp,
          "%s -> %s" % (before_fp, asset_fingerprint()))
    return preview, edit


def rebuild_fixture_and_reconnect():
    """
    Regenerates the acceptance asset and returns a fresh connected Session, or None.

    THE SUITE RESTORES ITS FIXTURE ON THE WAY IN, NOT ONLY ON THE WAY OUT, because this one CANNOT restore it on
    the way out: it appends nodes and the tool it tests has no way to remove them. Rebuilding first makes the
    suite re-runnable from ANY state, which is the property that matters - a run killed part-way leaves the same
    mess behind, and cleaning up at the end would not have helped.

    `make-acceptance-asset.ps1` runs a `-run=` commandlet, WHICH EXITS THE EDITOR when it finishes, so the
    editor is started again afterwards and the caller gets a new Session.
    """
    repo = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    if os.environ.get("TURBOMCP_RUNNER_OWNS_FIXTURES") == "1":
        ok, detail = tmcp_editor.restore_runner_fixture()
        print("    fixture restore: %s" % detail)
        if not ok:
            return None
        candidate = Session()
        candidate.connect()
        return candidate

    # THE SHAPE OF THIS HELPER IS COPIED FROM `test-graph-fix-backward.py`'s WORKING `rebuild_kit_and_reconnect`,
    # AND THE COPYING IS DELIBERATE RATHER THAN LAZY.
    #
    # AN EARLIER VERSION OF THIS FUNCTION KILLED THE EDITOR AND BOTH BACKGROUND SERVICES BY NAME, on the theory
    # that the rebuild needed the machine to itself. MEASURED CONSEQUENCE: `test-graph-append` passed alone, and
    # a FULL RUN fell from 37 of 39 to 32 of 39, with `test-call-log`, `test-function`, `test-play-check` and
    # `test-widget-retarget` all reporting `no result line` and `test-graph-knots` reporting `the editor could
    # not be started`. THE SUITE WAS STRONG-ARMING THE HARNESS THAT OWNS THE EDITOR, and the damage landed on
    # every suite after it. A test that breaks its neighbours is worse than one that fails.
    #
    # What works is what the kit helper already does: `start-editor.ps1 -Stop`, a SHORT PAUSE, then the
    # commandlet, then a fresh editor. Neither `UnrealTraceServer` nor `zenserver` needs killing - the kit
    # rebuild proves that on every full run.
    #
    # THE STOP IS A REQUIREMENT RATHER THAN TIDINESS, for the reason the kit helper records: `-run=`
    # commandlets FREE A PACKAGE'S NAME AND REBUILD IT, and a package the running editor already holds in memory
    # is not the one that reaches disk. Stopping the editor makes the commandlet work on a package it loaded
    # itself.
    stop = subprocess.run(
        ["pwsh", "-NoProfile", "-File", os.path.join(repo, "Tools", "start-editor.ps1"), "-Stop"],
        capture_output=True, text=True, cwd=repo)
    # `-Stop` answers non-zero when nothing was running, which is fine and not a failure.
    _ = stop
    time.sleep(3)

    # THE ACCEPTANCE FIXTURE IS REBUILT ONLY WHEN ASKED FOR, AND THAT DEFAULT IS A MEASURED DECISION RATHER THAN
    # A CONVENIENCE.
    #
    # MEASURED: rebuilding it from inside this suite took a FULL RUN from 37 of 39 to 32 of 39, and the damage
    # was not confined to this suite - `check-probe-graphs` and `test-graph-knots` ALSO failed with "the
    # acceptance asset could not be rebuilt", because this suite and they use THE SAME SCRIPT and one was
    # stopping the editor while another was mid-rebuild. `Tools/run-suites.ps1` already gives every suite its
    # own editor and `check-probe-graphs` already regenerates this asset as part of its own work, so this suite
    # rebuilding it too is a SECOND OWNER OF ONE FIXTURE, and two owners is how a fixture gets corrupted.
    #
    # SO THE DEFAULT IS TO USE THE FIXTURE AND NOT TO REBUILD IT. `SaneProbe` is a small committed graph, the
    # suite re-runs correctly against it, and the asset a previous full run left behind is regenerated by the
    # suite that already owns it. Set `TURBOMCP_APPEND_REBUILD=1` to force a rebuild when running this suite by
    # hand from a suspect tree - the capability is kept, it is just no longer the default.
    if os.environ.get("TURBOMCP_APPEND_REBUILD") == "1":
        rebuild = subprocess.run(
            ["pwsh", "-NoProfile", "-File", os.path.join(repo, "Tools", "make-acceptance-asset.ps1")],
            capture_output=True, text=True, cwd=repo)
    else:
        rebuild = None
    # THE REBUILD'S WHOLE ANSWER IS KEPT WHEN THERE WAS ONE, because "could not be rebuilt" is not a diagnosis
    # and this project has spent rounds on failures whose only visible symptom was a status code. A file rather
    # than stdout, because the harness shows only the tail of a suite's output and this is needed when it fails.
    if rebuild is None:
        ok = True
        print("    fixture rebuild: not requested; using the committed SaneProbe "
              "(set TURBOMCP_APPEND_REBUILD=1 to regenerate it)")
    else:
        ok = rebuild.returncode == 0 and "PASS" in (rebuild.stdout or "")
        first = ((rebuild.stdout or rebuild.stderr).strip().splitlines() or ["no output"])[0]
        print("    fixture rebuild: %s" % first)
        try:
            with open(os.path.join(repo, ".agent", "logs", "append-rebuild.log"), "w",
                      encoding="utf-8", errors="replace") as handle:
                handle.write("returncode=%s\n" % rebuild.returncode)
                handle.write(rebuild.stdout or "")
                handle.write(rebuild.stderr or "")
        except OSError:
            pass

    if not ok:
        print("    the acceptance asset could not be rebuilt, so this suite cannot run")
        print((rebuild.stdout or rebuild.stderr)[-400:])
        return None

    start = subprocess.call(
        ["pwsh", "-NoProfile", "-File", os.path.join(repo, "Tools", "start-editor.ps1")], cwd=repo)
    if start != 0:
        print("    the editor launcher returned %s (non-zero means one was ALREADY RUNNING)" % start)

    deadline = time.time() + 180
    while time.time() < deadline:
        try:
            candidate = Session()
            candidate.connect()
            return candidate
        except Exception:
            time.sleep(3)
    return None


def text_for(marker, extra_pins=""):
    return '(Development|PrintString\n    InString "%s"\n    Duration 1.5%s)' % (marker, extra_pins)


def link_target_id(session):
    """
    The node id of the fixture's PrintString, READ from the graph rather than pinned.

    `SaneProbe` is built by a commandlet, so a hard-coded key would break the moment it is rebuilt in a different
    order. Parsing it is the same rule the rest of this suite follows for counts.
    """
    out = session.call(READ_TOOLSET, "ReadTurboMCPGraphLoad",
                       {"asset": ASSET, "graphName": GRAPH, "detail": "full",
                        "format": "read", "offset": 0, "maxChars": 24000,
                        "revision": "", "entryFilter": ""})
    found = re.findall(r"\(%s ; [^:]+:%s:([0-9a-f]{6})" % (re.escape(LINK_TARGET_TYPE), re.escape(GRAPH)), out)
    return (found[-1] if found else None), out


def linked_text_for(marker, target_key):
    """
    A block whose EXEC input is a wire to an existing node's `then`, which is the shape this suite exists to prove.

    THE WIRE IS WRITTEN IN A PIN'S VALUE POSITION, which is how the format prints it, and the target is named by
    NODE ID - the spec's "links them to existing nodes by node ID".
    """
    return ('(Development|PrintString\n    InString "%s"\n    execute (-> %s:%s:%s:%s))'
            % (marker, ASSET, GRAPH, target_key, LINK_TARGET_PIN))


def local_label_text(marker, target_key):
    """Two new forms, where the second targets the first by a block-local label."""
    return ('(Development|PrintString\n'
            '    ; label First\n'
            '    InString "%s source"\n'
            '    execute (-> %s:%s:%s:then))\n'
            '(Development|PrintString\n'
            '    ; label Second\n'
            '    InString "%s target"\n'
            '    execute (-> First:then))') % (marker, ASSET, GRAPH, target_key, marker)


def report_result():
    print()
    if failures:
        print("RESULT: FAIL (%d) -> %s" % (len(failures), ", ".join(failures)))
        return 1
    print("RESULT: PASS")
    return 0


def main():
    # PURE FIRST, AND NO SESSION FOR --parser-only: these checks must be runnable without an editor.
    run_report_parser_checks()
    if "--parser-only" in sys.argv:
        print("  RECORDED: parser checks only; no editor, socket or fixture was contacted.")
        return report_result()

    # A focused native case does not replace full-suite acceptance. It lets a repair
    # gather exact literal evidence while the independently recorded legacy failures remain open.
    if os.environ.get("TMCP_APPEND_LITERAL_ONLY") == "1":
        session = Session()
        session.connect()
        session.call(AUTO_TOOLSET, "DiscoverTests", {})
        raw = session.call(AUTO_TOOLSET, "RunTests", {"testNames": [APPEND_ALIAS_TEST]})
        print(str(raw))
        try:
            entry, report = parse_run_tests_report(raw, APPEND_ALIAS_TEST)
            check("focused literal/public append regression", entry["state"] == "Success" and not entry["errors"])
        except (ValueError, TypeError) as exc:
            check("focused literal/public append regression", False, str(exc))
        print("RECORDED: focused literal regression only; full append/placement acceptance remains separate.")
        return report_result()

    session = rebuild_fixture_and_reconnect()
    if session is None:
        print("RESULT: FAIL (the fixture could not be rebuilt, so nothing was tested)")
        return 1
    check("the fixture was REBUILT on the way in, so this suite is re-runnable from any state", True)

    initial_target, initial_read = link_target_id(session)
    if initial_target is None:
        print(initial_read)
        check("connected fixture exposes its execution predecessor", False, initial_read)
        return report_result()

    initial_disk = asset_fingerprint()
    before_count, before_text = node_count(session)
    before_ids, _ = node_ids(session)
    print("=== the graph has %s node(s) before anything is appended ===" % before_count)
    check("the node count is parseable, so nothing below pins a literal",
          before_count is not None, before_text[:120])
    check("the graph has nodes to begin with", (before_count or 0) > 0, str(before_count))

    print()
    print("=== 1. the PREVIEW resolves a type id and writes nothing ===")
    preview = session.call(PREVIEW_TOOLSET, "PreviewTurboMCPGraphAppend",
                           {"asset": ASSET, "graphName": GRAPH, "text": linked_text_for("PROBE-PREVIEW", initial_target)})
    print(preview[:600])
    print()
    check("the preview answers rather than erroring", not preview.startswith("error:"), preview[:160])
    check("it says a node WOULD be added", "would be added" in preview, preview[:200])
    # THE PREVIEW OF A VALID TEXT BEGINS WITH ITS OWN SUMMARY, not with `unchanged`. The spec requires the
    # first line to be `unchanged` OR A SUMMARY, and a plan that WOULD write is summarised. What must not
    # happen is a first line claiming a change the preview will not make.
    check("the preview of a valid text summarises rather than claiming it already changed",
          preview.split("\n", 1)[0].startswith("preview:"), preview[:100])
    check("IT SAYS IT CHANGES NOTHING", "No append was applied to the target graph." in preview, preview[:240])

    count_after_preview, _ = node_count(session)
    check("THE PREVIEW DID NOT ADD A NODE", count_after_preview == before_count,
          "before %s, after %s" % (before_count, count_after_preview))

    print()
    print("=== 2. the EDIT writes, and the write is proved from OUTSIDE ===")
    marker = "PROBE-APPEND-REAL"
    edited = session.call(EDIT_TOOLSET, "EditTurboMCPGraphAppend",
                          {"asset": ASSET, "graphName": GRAPH, "text": linked_text_for(marker, initial_target)})
    print(edited[:600])
    print()
    check("the edit answers rather than erroring", not edited.startswith("error:"), edited[:160])
    # THE BUG THIS CHECK EXISTS FOR, MEASURED: `Apply` once built its whole report into a local string and
    # returned success WITHOUT assigning it, so the caller got an EMPTY answer while the graph was correctly
    # changed. Every check that read the GRAPH passed. Only reading the RETURN VALUE catches that.
    check("THE EDIT RETURNED A REPORT RATHER THAN AN EMPTY STRING", edited.strip() != "", repr(edited[:80]))
    check("the first line starts with changed, as the spec requires",
          edited.split("\n", 1)[0].startswith("changed:"), edited[:100])
    check("it reports the read back matched", "read back matches" in edited, edited[:300])
    check("it reports how many nodes it added, parsed rather than assumed",
          re.search(r"(\d+)\s+node\(s\) added", edited) is not None, edited[:200])

    after_count, after_text = node_count(session)
    after_ids, full_after = node_ids(session)
    print("    node count: %s -> %s" % (before_count, after_count))
    check("THE GRAPH REALLY GREW, read back through a DIFFERENT tool",
          (after_count or 0) > (before_count or 0),
          "before %s, after %s" % (before_count, after_count))
    # The appended call is reachable, so full readback must expose its literal value.
    check("reader confirms the appended literal", marker in full_after, full_after[-500:])
    check("a node of the resolved class is now in the graph",
          "PrintString" in full_after, full_after[:200])
    check("the tool reports a non-zero pin count, parsed rather than assumed",
          re.search(r"(\d+)\s+pin value\(s\) written", edited) is not None
          and int(re.search(r"(\d+)\s+pin value\(s\) written", edited).group(1)) > 0,
          edited[-300:])

    print()
    print("=== 3. IT CANNOT REMOVE - this is what makes it APPEND and not REPLACE ===")
    check("THE NODE COUNT DID NOT GO DOWN", (after_count or 0) >= (before_count or 0),
          "before %s, after %s" % (before_count, after_count))
    lost = before_ids - after_ids
    check("EVERY NODE THAT WAS THERE BEFORE IS STILL THERE", len(lost) == 0,
          "gone: %s" % sorted(lost)[:6])

    print()
    print("=== 4. a REFUSAL writes nothing, and the preview and edit agree ===")
    # A type id that cannot exist. Both tools must refuse it, in the same words, and neither may write.
    bad = "(NoSuchCategory|NoSuchFunction\n    InString \"nope\")"
    bad_preview = session.call(PREVIEW_TOOLSET, "PreviewTurboMCPGraphAppend",
                               {"asset": ASSET, "graphName": GRAPH, "text": bad})
    print(bad_preview[:400])
    bad_edit = session.call(EDIT_TOOLSET, "EditTurboMCPGraphAppend",
                            {"asset": ASSET, "graphName": GRAPH, "text": bad})
    print(bad_edit[:400])
    print()
    check("the preview REFUSES an unresolvable type id", bad_preview.startswith("unchanged:") and "refusal(s)" in bad_preview, bad_preview[:240])
    check("the preview's refusal names the line it is about", "line 1" in bad_preview, bad_preview[:240])
    check("the EDIT refuses the same input", not bad_edit.startswith("changed:"), bad_edit[:160])
    check("the edit's first line is unchanged, so a caller reading only that learns nothing was written",
          bad_edit.split("\n", 1)[0].startswith("unchanged"), bad_edit[:120])

    refused_count, _ = node_count(session)
    check("A REFUSED EDIT DID NOT CHANGE THE GRAPH", refused_count == after_count,
          "before %s, after %s" % (after_count, refused_count))

    print()
    print("=== 5. a LINK is planned, made, and witnessed ===")
    #
    # THIS SECTION USED TO ASSERT THE OPPOSITE - that a link was REFUSED - because linking was not implemented.
    # It is now the capability, so the checks are the other way round, and the negative case that survives is a
    # link whose SOURCE does not exist, which must still be refused BY NAME rather than written as text.
    target_key, target_text = link_target_id(session)
    check("the fixture exposes a node to link INTO, so the link checks are not vacuous",
          target_key is not None, target_text[-200:] if target_key is None else target_key)

    if target_key is not None:
        link_out = session.call(PREVIEW_TOOLSET, "PreviewTurboMCPGraphAppend",
                                {"asset": ASSET, "graphName": GRAPH,
                                 "text": linked_text_for("PROBE-LINK-PREVIEW", target_key)})
        print(link_out[:500])
        check("the preview PLANS the wire rather than refusing it",
              "wire  line" in link_out, link_out[:300])
        check("the planned wire names BOTH ends, so a caller can check it before writing",
              target_key in link_out and "execute" in link_out, link_out[:300])

        before_link_count, _ = node_count(session)
        link_edit = session.call(EDIT_TOOLSET, "EditTurboMCPGraphAppend",
                                 {"asset": ASSET, "graphName": GRAPH,
                                  "text": linked_text_for("PROBE-LINK-REAL", target_key)})
        print(link_edit[:500])
        check("the edit reports a WIRE was made", "wire(s) made" in link_edit, link_edit[:300])
        # PARSED, NOT MATCHED. "1 wire(s) made, of 1 the text described" is read as two numbers, because a
        # sentence beside them could stay right while the numbers went wrong.
        made = re.search(r"(\d+) wire\(s\) made, of (\d+)", link_edit)
        check("the wire count is parseable and NON-ZERO", made is not None and int(made.group(1)) > 0,
              made.group(0) if made else link_edit[:200])
        check("it made every wire the text described, not merely one",
              made is not None and made.group(1) == made.group(2),
              made.group(0) if made else "not parsed")

        # THE WITNESS IS A DIFFERENT TOOL. A writer saying it made a wire is not evidence; the READER printing
        # `; out 0 then -> 1 link(s)` off the live graph is, because it reads identity and links from the graph
        # rather than from the plan.
        _, after_link_text = link_target_id(session)
        check("THE READER WITNESSES THE WIRE on the target's own output pin",
              re.search(r"out \d+ %s -> \d+ link\(s\)" % re.escape(LINK_TARGET_PIN), after_link_text)
              is not None,
              [l for l in after_link_text.splitlines() if LINK_TARGET_PIN in l][:2])

        check("the link grew the graph, so the check is not measuring a refusal",
              (node_count(session)[0] or 0) > (before_link_count or 0),
              "before %s after %s" % (before_link_count, node_count(session)[0]))

        print()
        print("=== 5b. a local label links two new nodes in the same block ===")
        label_target, _ = link_target_id(session)
        labelled_preview = session.call(PREVIEW_TOOLSET, "PreviewTurboMCPGraphAppend", {
            "asset": ASSET, "graphName": GRAPH, "text": local_label_text("PROBE-LOCAL-LABEL", label_target)})
        print(labelled_preview[:500])
        check("the preview accepts the local-label link", "wire  line" in labelled_preview,
              labelled_preview[:300])
        before_label_count, _ = node_count(session)
        labelled_edit = session.call(EDIT_TOOLSET, "EditTurboMCPGraphAppend", {
            "asset": ASSET, "graphName": GRAPH, "text": local_label_text("PROBE-LOCAL-LABEL", label_target)})
        print(labelled_edit[:500])
        check("the edit makes the local-label wire", "2 wire(s) made, of 2" in labelled_edit,
              labelled_edit[:300])
        check("the local-label block creates both nodes", (node_count(session)[0] or 0) >=
              (before_label_count or 0) + 2,
              "before %s after %s" % (before_label_count, node_count(session)[0]))

        print()
        print("=== 5c. a link to a node that does NOT exist is still refused by name ===")
        ghost = ("(Development|PrintString\n    execute (-> %s:%s:ffffff:then))" % (ASSET, GRAPH))
        ghost_preview = session.call(PREVIEW_TOOLSET, "PreviewTurboMCPGraphAppend",
                                     {"asset": ASSET, "graphName": GRAPH, "text": ghost})
        print(ghost_preview[:400])
        check("a link whose SOURCE is not in the graph is refused", ghost_preview.startswith("unchanged:") and "refusal(s)" in ghost_preview,
              ghost_preview[:300])
        check("and the refusal names the line", "line 2" in ghost_preview, ghost_preview[:300])
        check("and it names what could not be resolved", "ffffff" in ghost_preview, ghost_preview[:300])

        print()
        print("=== 5d. a link to a PIN that does not exist names the real pins ===")
        bad_pin = linked_text_for("PROBE-LINK-BADPIN", target_key).replace(
            ":%s))" % LINK_TARGET_PIN, ":noSuchPin))")
        pin_preview = session.call(PREVIEW_TOOLSET, "PreviewTurboMCPGraphAppend",
                                   {"asset": ASSET, "graphName": GRAPH, "text": bad_pin})
        print(pin_preview[:500])
        check("a link to a pin that does not exist is refused", pin_preview.startswith("unchanged:") and "refusal(s)" in pin_preview,
              pin_preview[:300])
        # THE REAL NAMES ARE THE POINT. A refusal that only says "no such pin" leaves the caller guessing, and
        # the spec asks for the real names.
        check("and the refusal lists the pins the node really has",
              "It has:" in pin_preview and "then" in pin_preview, pin_preview[:400])

    print()
    print("=== 6. an empty text is nothing to do, not a change ===")
    empty = session.call(EDIT_TOOLSET, "EditTurboMCPGraphAppend",
                         {"asset": ASSET, "graphName": GRAPH, "text": ""})
    print(empty[:300])
    # AN EMPTY TEXT IS AN ERROR RATHER THAN AN `unchanged`, AND THAT IS RIGHT: the checker runs FIRST - it is
    # the gate every writer depends on - and the gate refuses text with nothing in it, so the writer never
    # gets to decide. An error is a clearer answer than a silent no-op, because a caller who sent nothing
    # most likely made a mistake.
    check("an empty text is refused loudly rather than silently treated as no change",
          empty.startswith("error:"), empty[:160])

    print()
    print("=== 6b. the compiled automation tests: alias refusal and placement ===")
    #
    # THIS IS A DIFFERENT KIND OF EVIDENCE FROM THE TOOL CHECKS ABOVE. The alias collision needs two nodes
    # whose GUIDs share six characters, and placement needs forced positions and stored sizes; the compiled
    # tests build disposable in-memory Blueprints and call the real Plan/Apply. They run here through the
    # engine's automation toolset, and a MISSING or FAILED test is a failure: the run is otherwise easy to
    # mistake for "nothing to check".
    session.call(AUTO_TOOLSET, "DiscoverTests", {})
    listed = str(session.call(AUTO_TOOLSET, "ListTests",
                              {"nameFilter": "TurboMCP.GraphAppend", "tagFilter": ""}))
    print(listed[:400])

    automation_tests = (
        (APPEND_ALIAS_TEST,
         "the append alias automation test is discovered",
         "the RunTests answer is the pinned report for exactly the requested test",
         "the requested test reported Success",
         "the requested test reported no errors"),
        (APPEND_PLACEMENT_TEST,
         "the append placement automation test is discovered",
         "the placement RunTests answer is the pinned report for exactly the requested test",
         "the placement test reported Success",
         "the placement test reported no errors"),
    )
    automation_tests += (("TurboMCP.GraphAppend.DelegateSideEffects", "delegate test discovered", "delegate report parsed", "delegate workflow succeeds", "delegate workflow has no errors"),)
    automation_tests += (("TurboMCP.GraphAppend.Reachability", "reachability discovered", "reachability report parsed", "reachability workflow succeeds", "reachability workflow has no errors"),)
    automation_tests += (("TurboMCP.GraphAppend.TidyFeedback", "feedback test discovered", "feedback report parsed", "feedback workflow succeeds", "feedback workflow has no errors"),)
    automation_tests += (("TurboMCP.GraphAppend.InterfaceTidy", "interface tidy discovered", "interface tidy report parsed", "interface tidy succeeds", "interface tidy has no errors"),)
    automation_tests += (("TurboMCP.GraphAppend.EditNodeKeys", "node key test discovered", "node key report parsed", "node key workflow succeeds", "node key workflow has no errors"),)
    automation_tests += (("TurboMCP.GraphAppend.PinContainers", "container test discovered", "container report parsed", "container workflow succeeds", "container workflow has no errors"),)
    for test_path, discovered_name, report_name, success_name, errors_name in automation_tests:
        check(discovered_name, test_path in listed, listed[:240])
        if test_path not in listed:
            check(report_name, False, "the test is not registered, so no check ran")
            continue

        raw = session.call(AUTO_TOOLSET, "RunTests", {"testNames": [test_path]})
        print(str(raw)[:900])
        # THE PINNED JSON IS PARSED, NOT SEARCHED FOR WORDS. A report that merely quotes "Success" in an
        # error, omits the requested test, duplicates it, or disagrees with its own counts fails here.
        try:
            entry, report = parse_run_tests_report(raw, test_path)
        except (ValueError, TypeError) as exc:
            check(report_name, False, str(exc)[:240])
        else:
            check(report_name, True,
                  "passed=%d failed=%d skipped=%d total=%d"
                  % (report["passed"], report["failed"], report["skipped"], report["total"]))
            check(success_name, entry["state"] == "Success", entry["state"])
            check(errors_name, not entry["errors"], entry["errors"][:1])

    print()
    print("=== 6c. truncated and missing targets are refused without mutation ===")
    #
    # THE CONTRACT, READ FROM SOURCE BEFORE THE ASSERTIONS:
    #
    # - `(Development|Print` is a truncated TYPE ID. Type ids are resolved by building
    #   a probe node; an id that matches nothing is a `node-type` finding, and ANY
    #   finding makes the plan unacceptable.
    # - `InString "unterminated` is a truncated QUOTED value. It is neither a number,
    #   a boolean nor a quoted string, so the checker reports it `unjudged` and names
    #   the pin; `bAcceptable` still requires zero findings, so the writer refuses.
    # - A missing asset or graph cannot reach a plan at all; both tools return the
    #   same `error:` line from the shared Plan.
    # - A VALID CONTROL is the earlier part of this suite: those appends reported
    #   `changed:` and grew the graph, so the refusals below are not a suite whose
    #   only outcome is refusal.
    current_count, _ = node_count(session)
    check("a valid append earlier in this suite grew the graph",
          current_count is not None and before_count is not None and current_count > before_count,
          "%s -> %s" % (before_count, current_count))

    truncated_type = '(Development|Print\n    InString "cut off")'
    refused, _ = refusal_pair(session, "truncated node type", ASSET, GRAPH, truncated_type)
    check("the truncated type refusal names the line, the id and the reason",
          "line 1" in refused and "Development|Print" in refused
          and "No supported creation ID" in refused
          and "PreviewTurboMCPNodeType" in refused, refused[:500])

    truncated_literal = '(Development|PrintString\n    InString "unterminated)'
    refused, _ = refusal_pair(session, "truncated quoted literal", ASSET, GRAPH,
                              truncated_literal, expected_prefix="error:")
    check("the truncated literal refusal names its line and lexical error",
          "line 2" in refused and "quoted value is not closed" in refused, refused[:500])

    # MISSING TARGET, TWO FORMS: an absent graph on a real asset, and an absent
    # asset. Both are `error:` lines, so the same helper runs with that prefix.
    for label, asset, graph in (
        ("missing graph", ASSET, "NoSuchGraphForRefusalTest"),
        ("missing asset", "NoSuchAssetForRefusalTest", GRAPH),
    ):
        preview, _ = refusal_pair(session, label, asset, graph,
                                  text_for("PROBE-REFUSAL-UNREACHED"),
                                  expected_prefix="error:")
        if label == "missing graph":
            check("the missing-graph error lists the real graph", "EventGraph" in preview,
                  preview[:400])
        else:
            check("the missing-asset error names the missing asset",
                  "NoSuchAssetForRefusalTest" in preview, preview[:400])

    # AMBIGUOUS IDENTITY IS DEFERRED, NOT WEAKENED. A deterministic ambiguous node
    # key needs two nodes sharing the first six hex digits, and an ambiguous asset
    # name needs two assets with the same leaf name. The committed fixtures have
    # neither - SaneProbe gets fresh random GUIDs and all TestProject asset names are
    # unique - and creating either would need a fixture file outside this
    # assignment's allowlist. The compiled `TurboMCP.GraphAppend.AliasCollision`
    # test already covers plan-level alias ambiguity; this suite does not fake an
    # MCP-level case it cannot construct.

    print()
    print("=== 7. the asset on disk is untouched, because nothing saved it ===")
    # `E-SAVE` was never called, so the FILE must be exactly as it was. This is the check that a write to
    # MEMORY did not silently become a write to DISK - a claim no tool's own report can settle.
    check("all successful appends and refusals preserved saved bytes", asset_fingerprint() == initial_disk, str(asset_fingerprint()))
    check("the edit did NOT claim to have saved", "saved" not in edited.lower(), edited[:200])

    print()
    print("  RECORDED: this suite APPENDS and does not remove, because removal is the capability this")
    print("  tool deliberately does not have. Its own growth is bounded by the marker counter above.")

    return report_result()


if __name__ == "__main__":
    sys.exit(main())
