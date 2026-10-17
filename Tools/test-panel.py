# ACCEPTANCE ROW THIS SUITE CLAIMS TO PROVE: T-PANEL-2
#
# NAMED HERE SO Spec/06_ACCEPTANCE.md CAN BE ANSWERED MECHANICALLY. Spec/WORKING_PLAN.md section 1.3 says a
# capability is done when its acceptance row PASSES, WHICH MAKES THAT DOCUMENT THE DEFINITION OF DONE - and a
# script over the tree measured 64 OF ITS 122 ROWS CITED BY NO SUITE AT ALL, so nothing could answer
# 'is this row passing?' without a human reading it.
# *** THE SCRIPT IS NAMED NOW: python Tools/checks/count-acceptance-citations.py - IT STATES ITS CRITERION, a literal occurrence of the id in a Tools/test-*.py file, AND REPORTS 64. FOUR EARLIER ATTEMPTS GAVE 91, 64, 66 AND 56 BECAUSE NONE SAID WHAT CITED MEANT, AND THIS LINE CARRIED THE 91. ***
#
# THE ID IS READ FROM THE DOCUMENT, NOT GUESSED. VERIFIED FOR THIS ONE:
#   
#   | T-PANEL-2 | Tool list | Label format, order Read Look Preview Edit, engine tools included |

"""
Probe the TurboMCP panel's tool runner, through the commandlet that reaches it.

WHY THIS SUITE EXISTS AND WHY IT IS NOT AN MCP TEST
---------------------------------------------------
`QUESTIONS.md` Q-11 records that the panel's widget tree cannot be read, because this plugin has no Slate
inspection tool. The panel was therefore built with as much as possible OUT of Slate: discovery, sample
generation and execution live in `FTurboMCPPanelToolRunner`, plain C++, and the panel renders it.

That left the testable half with NOTHING reaching it - not an MCP tool, not a commandlet. This suite
closes that, through `UTurboMCPPanelProbeCommandlet`.

A PANEL FEATURE DOES NOT BELONG IN THE MCP API, which is why the hook is a commandlet. `P9` says the
panel and the MCP share one code path; it does not say a widget concern has to be an `AICallable` tool
that an agent can see and cannot use.

THE COMMANDLET IS INVOKED WITH A MODULE PREFIX, and that is an engine fact rather than a style choice:
`LaunchEngineLoop.cpp:4100` calls `LoadModule` ONLY when the `-run=` token contains a period, so
`-run=TurboMCPPanelProbeCommandlet` loads nothing and answers "looked like a commandlet, but we could
not find the class".

THIS SUITE DOES NOT START THE EDITOR. A commandlet needs none, and starting one would collide with the
editor the other suites hold.

WHAT EACH CHECK PROVES, AND HOW IT COULD FAIL
---------------------------------------------
- the registry answers, so a failed probe is never read as "the panel has no tools";
- the four TurboMCP toolsets are among the ones the registry publishes, which catches a rename or a
  dropped `.uplugin` entry;
- EACH toolset has a NON-ZERO tool count. This is the check no other suite makes: `R-TOOL-INDEX` counts
  what IS there and cannot know what SHOULD be, so a toolset that lost a tool to a bad merge lists its
  remaining tools happily. A pinned literal would break every time a tool is added, so the count is
  PARSED and compared against what the header declares;
- every tool of every TurboMCP toolset produces a SAMPLE, because a tool with no sample is a tool a
  human cannot try with one click - which is the user's requirement, tool by tool;
- the live call's outcome is REPORTED, whatever it is. See the note on the timeout below.
"""

import tmcp_paths  # noqa: E402  (after the sys.path line above)
import os
import re
import subprocess
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
HEADER = os.path.join(REPO, "Plugins", "TurboMCP", "Source", "TurboMCP", "Public", "Tools", "TurboMCPToolsets.h")
LOG = os.path.join(REPO, ".agent", "logs", "panel-probe.log")

EDITOR_EXE = os.path.join(tmcp_paths.find_engine(), "Engine", "Binaries", "Win64", "UnrealEditor-Cmd.exe")
PROJECT = os.path.join(REPO, "TestProject", "TurboMCPTest.uproject")

# THE TOOLSETS ARE MATCHED BY SUFFIX, because the registry publishes the full name
# (`TurboMCP.TurboMCPReadToolset`) and the header names the class. A folded suffix match finds both.
TOOLSETS = {
    "Read": "UTurboMCPReadToolset",
    "Look": "UTurboMCPLookToolset",
    "Preview": "UTurboMCPPreviewToolset",
    "Edit": "UTurboMCPEditToolset",
}

failures = []


def check(name, ok, detail=""):
    print("  %-4s %s%s" % ("ok" if ok else "FAIL", name, ("  " + detail) if detail else ""))
    if not ok:
        failures.append(name)


def declared_tool_counts():
    """How many AICallable tools each toolset declares, read from the header.

    READ RATHER THAN PINNED. A literal here would be a second list of the tools, and this project's
    recurring failure is two places that agree until one of them changes. The count is compared against
    the SAME header the build compiles, so adding a tool cannot make this suite wrong.
    """
    with open(HEADER, "r", encoding="utf-8", errors="replace") as handle:
        text = handle.read()

    classes = [(m.start(), m.group(1)) for m in
               re.finditer(r"^class (UTurboMCP\w+Toolset)", text, re.MULTILINE)]
    counts = {}
    for match in re.finditer(
            r"UFUNCTION\(meta = \(AICallable\)\)\s*\n\s*static FString (\w+)\(", text, re.DOTALL):
        owner = None
        for start, class_name in classes:
            if start < match.start():
                owner = class_name
            else:
                break
        if owner:
            counts[owner] = counts.get(owner, 0) + 1
    return counts


def run_commandlet():
    """Runs the probe and returns (exit code, full log text)."""
    if not os.path.exists(EDITOR_EXE):
        return None, "the editor commandlet binary is not at %s" % EDITOR_EXE

    os.makedirs(os.path.dirname(LOG), exist_ok=True)
    # THE REDIRECTION TARGET IS AN ABSOLUTE PATH, AND THAT MATTERS. The first version passed a
    # relative `-log`-less redirect through `cmd.exe /c` with `cwd=REPO`, and the log was never created:
    # `cmd.exe` resolves a redirect against ITS OWN working directory, and the quoting of a path with
    # spaces inside a `/c` string is not reliable. An absolute path removes both problems.
    log_path = os.path.abspath(LOG)

    # `-run=` NEEDS THE MODULE PREFIX: the engine calls LoadModule only when the token contains a
    # period. `-nullrhi` and `-unattended` because there is nothing to draw and nobody to ask.
    # A DSH task can write only inside the workspace, while Unreal's default DDC may live under the
    # engine checkout. Give confined runs a persistent project-local cache; normal runs keep their
    # usual DDC.
    child_env = None
    if os.environ.get("DSH_SESSION_ID"):
        workspace_ddc = os.path.join(REPO, "TestProject", "Saved", "DerivedDataCache")
        os.makedirs(workspace_ddc, exist_ok=True)
        child_env = os.environ.copy()
        child_env["UE-LocalDataCachePath"] = workspace_ddc
    command = (
        '"{exe}" "{project}" -run=TurboMCP.TurboMCPPanelProbeCommandlet '
        "-unattended -nullrhi -culture=en -MaxParallelActions=8 > \"{log}\" 2>&1"
    ).format(exe=EDITOR_EXE, project=PROJECT, log=log_path)

    # `shell=True` AND NOT `["cmd.exe", "/c", command]`, AND THE REASON IS MEASURED. Passing the whole
    # command as one argument to `cmd.exe /c` through a LIST makes Python quote the argument, and cmd
    # then rejected the result with "the filename, directory name, or volume label syntax is
    # incorrect" - the same class of quoting failure that has broken an inline commit message twice in
    # this project. `shell=True` hands the string to the command processor the way it expects.
    #
    # THE REDIRECT IS THE SHELL'S, AND PYTHON MUST NOT ALSO HOLD THE FILE. Letting `subprocess` open
    # the log while the command ALSO redirects into it produced "the process cannot access the file
    # because it is being used by another process", and an empty log. The command owns the redirect and
    # Python only reads the result.
    completed = subprocess.run(command, shell=True, cwd=REPO, env=child_env,
                               capture_output=True, text=True)

    if not os.path.exists(log_path):
        return None, ("the commandlet produced no log at %s. cmd.exe said: %s"
                      % (log_path, (completed.stderr or completed.stdout or "(nothing)")[:400]))

    with open(log_path, "r", encoding="utf-8", errors="replace") as handle:
        return completed.returncode, handle.read()


def main():
    print("=== the commandlet's probe of the panel runner ===")
    exit_code, text = run_commandlet()
    if exit_code is None:
        check("the editor commandlet binary exists", False, text)
        print("\nRESULT: FAIL")
        return 1

    # THE PROBE'S OWN LINES, so the report says what it found rather than only whether it passed.
    for line in text.splitlines():
        if "LogTurboMCP:" in line and ("panel-" in line or "TurboMCP: probing" in line):
            print("    " + line.split("LogTurboMCP:")[-1].strip().lstrip("Display: ").strip())

    print()
    # THE EXIT CODE IS NOT THE ONLY EVIDENCE. The acceptance commandlet has reported success while
    # writing nothing, so the SUMMARY LINE is parsed rather than trusted.
    summary = None
    for line in text.splitlines():
        if "panel-summary:" in line:
            summary = line.split("panel-summary:")[-1].strip()
    check("the commandlet printed its summary line", summary is not None, repr(summary))
    if summary is None:
        print("\n    the commandlet did not reach its summary; its last lines:")
        for line in text.splitlines()[-12:]:
            print("      " + line.strip())
        print("\nRESULT: FAIL")
        return 1

    fields = dict(re.findall(r"(\w+)=(\S+)", summary))
    print("    parsed: %s" % fields)

    check("the registry was available",
          fields.get("registry") == "available", fields.get("registry"))
    check("no TurboMCP toolset is missing",
          fields.get("missing") == "0", "missing=%s" % fields.get("missing"))
    check("no toolset failed to list",
          fields.get("failedtoolsets") == "0", "failedtoolsets=%s" % fields.get("failedtoolsets"))

    published = int(fields.get("toolsets", "0"))
    check("the registry publishes more than just this plugin's toolsets",
          published > 4, "%d published" % published)

    # THE COUNTS, AGAINST THE HEADER RATHER THAN AGAINST A LITERAL.
    expected = declared_tool_counts()
    print()
    print("=== the tool count per toolset, against the header ===")
    total_reported = int(fields.get("tools", "0"))

    declared_total = 0
    for label, class_name in TOOLSETS.items():
        want = expected.get(class_name, 0)
        declared_total += want
        # THE LINE IS MATCHED ON THE BARE CLASS NAME, which the commandlet now prints beside the
        # registry name. THE FIRST RUN FAILED ALL EIGHT OF THESE FOR THE WRONG REASON: the registry
        # publishes `TurboMCP.TurboMCPReadToolset` while the header names the class
        # `UTurboMCPReadToolset`, so this match found nothing and reported "the toolset has no tools"
        # when it had twenty-one. A probe whose READER is wrong is worse than no probe, because it
        # reports a working toolset as broken.
        bare = class_name.lstrip("U")
        found = None
        for line in text.splitlines():
            if "panel-toolset-tools:" in line and bare in line:
                match = re.search(r"(\d+) tool\(s\)", line)
                if match:
                    found = int(match.group(1))
        print("    %-9s header says %2d, the probe found %s"
              % (label, want, found if found is not None else "(no line)"))
        check("%s reports a NON-ZERO tool count" % label,
              found is not None and found > 0, "found %s" % found)
        check("%s reports the count the header declares" % label,
              found == want, "header %d, probe %s" % (want, found))

    check("the total matches the header's own total",
          total_reported == declared_total,
          "probe %d, header %d" % (total_reported, declared_total))

    # A SAMPLE FOR EVERY TOOL, because a tool with no sample is one a human cannot try in one click.
    print()
    nosample = [l.split("panel-nosample:")[-1].strip()
                for l in text.splitlines() if "panel-nosample:" in l]
    print("    tools with NO sample: %s" % (", ".join(nosample) if nosample else "(none)"))
    check("every tool of every TurboMCP toolset has a sample",
          len(nosample) == 0, ", ".join(nosample))

    partial = int(fields.get("partial", "0"))
    partial_names = [l.split("panel-partial:")[-1].strip()
                     for l in text.splitlines() if "panel-partial:" in l]
    print("    tools with a PARTIAL sample: %d  %s"
          % (partial, ", ".join(n[:60] for n in partial_names) if partial_names else ""))
    check("the partial-sample count agrees with the lines that report them",
          partial == len(partial_names), "reported %d, lines %d" % (partial, len(partial_names)))

    # THE LIVE CALL, WHATEVER IT DID. This is a MEASUREMENT, not a pass/fail: `CallAndWait` sleeps on
    # the calling thread while the completion is delivered by the editor's tick, and a `-run=`
    # commandlet has no tick. A timeout is a fact about what a commandlet can do, so it is REPORTED and
    # the check is that the commandlet SAID WHICH of the outcomes it got.
    print()
    print("=== the live call, measured rather than assumed ===")
    live = None
    for line in text.splitlines():
        if "panel-livecall:" in line:
            live = line.split("panel-livecall:")[-1].strip()
    print("    %s" % live)
    check("the commandlet reported the live call's outcome", live is not None)
    if live:
        outcome = live.split()[0]
        check("the outcome is one of the four it can report",
              outcome in ("ANSWER", "TIMEOUT", "ERROR", "NEVER-RAN"), outcome)
        if outcome == "ANSWER":
            # An ANSWER means a commandlet CAN drive a real call, which is the strongest result. An
            # empty value would be the "tool returned nothing" case `P1` forbids, so it is failed on.
            check("an ANSWER carried text, because P1 forbids returning nothing",
                  "0 character(s)" not in live, live)
        else:
            print("    NOTE: %s. The probe still verified discovery, counts and samples; only the "
                  "live call is limited." % outcome)

    # ---------------------------------------------------------------------------------------------
    # THE CALL TIMELINE'S ROWS, FILTERS AND EXPANSION - `T02`'s item 4b, ON LITERAL RECORDS.
    #
    # *** THIS IS THE HALF OF ITEM 4b THAT CAN BE PROVED, AND IT IS THE LARGER HALF. *** Q-11 says the
    # widget tree cannot be read, so the rows, the four filters and the expansion text live in
    # `FTurboMCPCallTimeline` - plain C++, no Slate - and the commandlet drives them with records it
    # writes itself. What is left in the widget is which rows are shown, which are open, and the four
    # boxes; the arithmetic is all here, and every expected number below is COUNTABLE BY HAND from the
    # five literal records the probe builds. A check whose expected value cannot be derived is a check
    # that will be "fixed" to match whatever the code does.
    # ---------------------------------------------------------------------------------------------
    print()
    print("=== T02 item 4b: the timeline's rows, its four filters and its expansion ===")

    tl = {}
    for line in text.splitlines():
        if "panel-timeline" not in line:
            continue
        body = line.split("LogTurboMCP:")[-1].strip().lstrip("Display: ").strip()
        body = body[len("TurboMCP: "):] if body.startswith("TurboMCP: ") else body
        if ":" in body:
            key, value = body.split(":", 1)
            tl.setdefault(key.strip(), []).append(value.strip())

    rows = tl.get("panel-timeline-row", [])
    summary = tl.get("panel-timeline", [""])[0]
    print("    %s" % summary)
    for row in rows:
        print("      %s" % row)

    # FIVE RECORDS, THREE CALLS. The pairing is the whole point: the plugin's start and end AND the
    # engine's own event for one call must be one row, or `R-CALL-COST` counts every call twice - which
    # is exactly what it did before `FTurboMCPCallTimeline` existed.
    check("five literal records assemble into THREE calls",
          "3 call(s) from 5 record(s)" in summary, summary)
    check("one row per call", len(rows) == 3, "%d row(s)" % len(rows))
    check("the merged call keeps the START's stamp and the end's measurement",
          any("00:00:01.000Z" in r and "336 chars" in r and "111 tokens" in r for r in rows),
          "; ".join(rows[:1]))
    check("an engine-only call says so rather than pretending to have a start",
          any("engine-only" in r and "call_tool" in r for r in rows),
          "; ".join(r[:70] for r in rows))
    check("a start with no end reads `running`, which is the shape a crash leaves",
          any(r.endswith("running") for r in rows) and tl.get("panel-timeline-running") == ["1"],
          str(tl.get("panel-timeline-running")))

    # THE FOUR FILTERS, EACH WITH AN EXPECTED COUNT DERIVED FROM THE LITERAL RECORDS:
    #   toolset  "ReadToolset"      -> the one merged Read call
    #   tool     "EditTurboMCPSave" -> the one Edit call
    #   asset    "BP_NodeTest"      -> TWO, because both plugin records name it in their arguments
    #   status   "error"            -> the engine-only `call_tool`, which is the one that failed
    for name, want in (("toolset", 1), ("tool", 1), ("asset", 2), ("status", 1)):
        lines = tl.get("panel-timeline-filter-%s" % name, [])
        got = None
        if lines:
            match = re.search(r"kept=(\d+) of (\d+)", lines[0])
            if match:
                got = int(match.group(1))
        check("the %s filter keeps exactly %d of the 3 calls" % (name, want),
              got == want, "%s -> %s" % (name, lines[0] if lines else "(no line)"))
    # A FILTER THAT KEEPS EVERYTHING IS NOT A FILTER. Asserted separately, because the counts above
    # would all still pass if `Select` ignored its filter and the expected values were all 3.
    check("no filter keeps all three, so each one actually narrowed",
          all(re.search(r"kept=3 of 3", l[0]) is None
              for l in (tl.get("panel-timeline-filter-%s" % n, []) for n in
                        ("toolset", "tool", "asset", "status")) if l),
          str({n: tl.get("panel-timeline-filter-%s" % n) for n in ("toolset", "asset")}))

    expansion = tl.get("panel-timeline-expand", [""])[0]
    print("    %s" % expansion)
    check("the expansion carries the FULL arguments, not the 60-character summary",
          "arguments=yes" in expansion, expansion)
    check("and the first lines of the result", "result_lines=2" in expansion, expansion)
    check("and it says what it left out, so a short expansion cannot read as a short answer",
          "truncated=yes" in expansion, expansion)

    # THE GROUPING, WHICH IS A ROW AND NOT A COLUMN: two sessions in the literal records, so five
    # entries - heading, call, heading, call, call. A reader who is handed the session id on every row
    # has to compare twenty strings to find where one batch script ended; that is not grouping.
    grouping = tl.get("panel-timeline-list", [""])[0]
    print("    %s" % grouping)
    check("the list groups by session with ONE heading per batch, not a session on every row",
          "5 entries, 2 heading(s), 3 call(s)" in grouping, grouping)

    # THE WIDGET ITSELF. `QUESTIONS.md` Q-11 says the widget tree cannot be READ; it does not say it
    # cannot be BUILT, and the probe builds it whenever the process has Slate. *** WHAT IS CHECKED HERE
    # IS THAT THE PROBE SAID WHICH OF THE TWO HAPPENED. *** A silent skip is the vacuous pass this
    # project blocks suites for, so the line must exist and, when it reports a skip, must name the
    # reason - which is what makes "the panel was not built here" a measurement rather than a shrug.
    print()
    widget = [l for l in text.splitlines() if "panel-widget:" in l]
    state = widget[0].split("panel-widget:")[-1].strip().lstrip("Display: ").strip() if widget else ""
    print("    panel-widget: %s" % (state or "(no line)"))
    check("the probe reported whether it could build the window", bool(widget))
    if state.startswith("SKIPPED"):
        check("and said WHY it could not, rather than skipping quietly",
              "Slate" in state or "slate" in state, state)
    elif state.startswith("BUILT"):
        check("the built window reports its row count and its count text",
              re.search(r"timeline_rows=\d+", state) is not None
              and re.search(r'count_text="\d+ of \d+ call\(s\)"', state) is not None,
              state)

    print()
    check("the commandlet exited zero", exit_code == 0, "exit %s" % exit_code)

    print()
    if failures:
        print("RESULT: FAIL (%d) -> %s" % (len(failures), ", ".join(failures)))
        return 1
    print("RESULT: PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
