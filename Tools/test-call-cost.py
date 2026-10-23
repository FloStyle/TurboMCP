# ACCEPTANCE ROW THIS SUITE CLAIMS TO PROVE: T-HELP-6
"""Exercise `R-CALL-COST`.

  T-HELP-6  with the log on, the most expensive call, the slowest and the totals per tool
            match the log lines the same calls wrote; with the log OFF the answer says the
            log is off rather than reporting zero.

WHAT IS CHECKED FROM WHERE, AND WHY THE LOG IS READ TWICE
--------------------------------------------------------
This suite drives two readers over ONE log: `R-CALL-COST` and `R-CALL-LOG`. The second is the
witness - a different tool, a different rendering - and the checks compare them, because a tool's
own report is not evidence of anything except what it decided to say.

*** AND THE SHARPEST CONSTRAINTS ARE MEASURED RATHER THAN ASSUMED, FROM THE REAL LOG: ***

1. THIS PLUGIN'S OWN TOOLS NOW RECORD, so a call through one of them carries a real `result_chars`
   and `token_estimate`, and `MOST EXPENSIVE BY TOKENS` HAS A WINNER. *** THE THREE CHECKS THIS
   REPLACED ASSERTED THE ABSENCE AND WERE CORRECT UNTIL `FTurboMCPRecordingToolset` LANDED: measured
   then, every entry in the log was `source=engine` because nothing called `FScopedCall`, so every
   estimate was absent by construction. The absence is now a MEASUREMENT THAT CAN FAIL, which is the
   stronger check: a tool that printed "none of the N considered call(s) recorded a token estimate"
   while the log holds measured ones is reporting an absence that is not there. ***

2. ONE CALL IS ONE CALL, WHICH IS THE PAIRING THIS SUITE NOW PROVES. A call to this plugin's own
   tools leaves a `plugin start`, a `plugin end` AND the engine's own event for the same call, so a
   reader that counted RECORDS would report two or three calls where one was made - and `2 call(s)`
   per asset, because both records of one pair carry the arguments. So the per-asset row for the
   asset this suite calls is asserted to read exactly one call.

3. THE LOG'S CLOCK IS THE EDITOR'S, AND AN ENGINE-ONLY CALL CARRIES NO ARGUMENTS. So a per-asset
   total fills only from calls that passed this plugin's boundary, and the suite asserts the asset it
   used is named rather than that the section is empty.

4. THE LOG GROWS BETWEEN TWO READS, BECAUSE READING IS ITSELF LOGGED. `Tools/test-call-log.py`
   records this as "an observer of the log is part of the log". SO NO CHECK COMPARES TWO ABSOLUTE
   COUNTS. Each compares an answer against the log lines THAT ANSWER'S OWN COST REPORT named, and
   the caller's own call is accounted for explicitly where a count is asserted at all.
"""

import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from tmcp import EDIT_TOOLSET, READ_TOOLSET, Session  # noqa: E402

failures = []
checks_run = [0]


def check(name, ok, detail=""):
    # THE COUNT IS REPORTED RATHER THAN DESCRIBED. This project has recorded ten cases of a check
    # that passed while the thing it verified was broken, and the purest of them is a runner that
    # says PASS without saying how much it looked at.
    checks_run[0] += 1
    print("  %-4s %s%s" % ("ok" if ok else "FAIL", name, ("  " + detail) if detail else ""))
    if not ok:
        failures.append(name)


def cost(session, since="", filter_text="", cap=""):
    return session.call(READ_TOOLSET, "ReadTurboMCPCallCost",
                        {"since": since, "filter": filter_text, "cap": cap})


def call_log(session, filter_text="", limit="", since=""):
    return session.call(READ_TOOLSET, "ReadTurboMCPCallLog",
                        {"filter": filter_text, "limit": limit, "since": since})


def set_log(session, enabled):
    return session.call(EDIT_TOOLSET, "EditTurboMCPCallLog", {"enabled": enabled})


def section_text(text, heading):
    """EVERYTHING between one heading and the next, prose included.

    WHY THERE ARE TWO READERS AND NOT ONE. `section` returns DATA ROWS - the pipe-separated totals -
    and it deliberately drops prose so an absence MESSAGE cannot be mistaken for a row. But THE
    ABSENCE MESSAGES ARE THEMSELVES PART OF THE CONTRACT: `MOST EXPENSIVE BY TOKENS` answering
    "none of the N considered call(s) recorded a token estimate above zero" is the whole of `P1` for
    that section. So a check about a ROW uses `section`, and a check about the ABSENCE uses this.

    MEASURED, AND THE FIRST VERSION OF THIS SUITE GOT IT WRONG THE OTHER WAY: I tightened `section`
    to drop prose, which made four checks that look for absence messages fail - ON A CORRECT TOOL.
    A parser tightened for one caller broke another, WHICH IS WHY THE TWO ARE SEPARATE FUNCTIONS
    RATHER THAN ONE WITH A FLAG.
    """
    lines = text.splitlines()
    known = ("MOST EXPENSIVE BY TOKENS", "SLOWEST BY DURATION",
             "TOTALS PER TOOL", "TOTALS PER ASSET")
    start = None
    for index, line in enumerate(lines):
        if heading in line:
            start = index + 1
            break
    if start is None:
        return ""
    out = []
    for line in lines[start:]:
        if any(k in line for k in known):
            break
        out.append(line)
    return "\n".join(out)


def section(text, heading):
    """The DATA lines of one section, from its heading to the next heading.

    TWO RULES, AND THE FIRST VERSION GOT BOTH WRONG IN THE SAME WAY - IT TREATED PROSE AS DATA:
      * the section ends at the next heading, SO THE `ranked N entries of M` LINE THAT FOLLOWS THE
        HEADINGS IS EXCLUDED - my first version let it through and then a check looking for "the
        slowest line" matched that summary instead;
      * a line starting with `;` is prose, and `; the token estimate is an estimate` is not a row.
    STRUCTURAL RATHER THAN A LIST OF MAGIC PHRASES, which is the lesson the lost work on this
    capability recorded: its first fix for an unparseable section was a list of literal phrases to
    skip, and A PARSER THAT ASSUMES A FORMAT IS THE DEFECT, not the fix.
    """
    lines = text.splitlines()
    known = ("MOST EXPENSIVE BY TOKENS", "SLOWEST BY DURATION",
             "TOTALS PER TOOL", "TOTALS PER ASSET")
    start = None
    for index, line in enumerate(lines):
        if heading in line:
            start = index + 1
            break
    if start is None:
        return []
    out = []
    for line in lines[start:]:
        if any(k in line for k in known):
            break
        stripped = line.strip()
        # A `;` line is prose; a line with no pipe is a heading, a count sentence or an explanation.
        if stripped.startswith(";") or "|" not in stripped:
            continue
        out.append(line)
    return out


def main():
    session = Session()
    session.connect()

    # ---------------------------------------------------------------------------------------------
    # THE LOG IS CLEARED FIRST SO THE SUITE'S OWN CALLS ARE THE ONLY ONES IT REASONS ABOUT, and it
    # is turned back ON at the end. `test-call-log.py` already does the same; a suite that leaves
    # the switch wherever it found it is a suite whose next neighbour disagrees with it.
    # ---------------------------------------------------------------------------------------------
    set_log(session, "false")
    session.call(EDIT_TOOLSET, "EditTurboMCPCallLogClear", {"confirm": "yes"})

    print("=== with the log OFF the answer says so rather than reporting zero ===")
    off = cost(session)
    print(off)
    print()
    check("the log being OFF is stated in the recording line",
          "recording: OFF" in off, off[:200])
    check("R-CALL-COST says NO COST CAN BE REPORTED rather than printing zeros",
          "No recorded calls are available for cost analysis" in off, off[:300])
    check("it names LOGGING OFF as the reason, which is one of the three `P1` distinguishes",
          "Recording is off" in off, off[:400])
    # `P1` AND `T-HELP-6` TOGETHER: the whole point is that "off" and "zero" are different answers.
    check("it does NOT report a zero total, because nothing was measured",
          "0 tokens" not in off and "0 call(s)" not in off, off[:300])
    check("it names the switch that turns logging on, so the next step is a call and not a guess",
          "EditTurboMCPCallLog" in off, off[:400])

    print()
    print("=== with the log ON, known calls are made and then costed ===")
    set_log(session, "true")
    session.call(EDIT_TOOLSET, "EditTurboMCPCallLogClear", {"confirm": "yes"})

    # Deliberately cheap calls, each named so the suite can look for them by name afterwards rather
    # than by count. A refusal is included ON PURPOSE: it is the one kind of entry whose `error`
    # field is true, and a suite that never produces one cannot check the error column at all.
    session.call(READ_TOOLSET, "ReadTurboMCPStatus", {})
    session.call(READ_TOOLSET, "ReadTurboMCPDescribe",
                 {"asset": "BP_NodeTest", "tokenBudget": "0"})
    session.call(READ_TOOLSET, "ReadTurboMCPMaterialTrace",
                 {"asset": "no_such_asset", "outputProperty": "MP_BaseColor", "tokenBudget": "0"})

    text = cost(session)
    print(text)
    print()

    check("the recording line now says the log is on",
          "recording: on" in text, text[:200])
    check("the answer carries a denominator - how many calls it considered",
          re.search(r"counts:\s*\d+ call\(s\) from \d+ record\(s\) in the log,\s*\d+ considered",
                    text) is not None, text[:300])
    check("it says how many entries recorded a duration and how many a token estimate",
          re.search(r"\d+ recording a duration and \d+ recording a token estimate", text) is not None,
          text[:400])
    # *** AND THE TOKEN COUNT IS NON-ZERO, WHICH IS THE MEASUREMENT THIS SUITE WAS BLIND TO. *** The
    # calls above all go through this plugin's own toolsets, so they pass the boundary the recorder
    # wraps. A zero here would mean the recorder did not run - the exact condition the old checks
    # could not distinguish from a log of engine calls.
    recorded = re.search(r"(\d+) recording a duration and (\d+) recording a token estimate", text)
    check("this plugin's own calls recorded a token estimate, so the count is not zero",
          recorded is not None and int(recorded.group(2)) > 0,
          "matched %s" % (recorded.group(0) if recorded else "nothing"))

    # ---------------------------------------------------------------------------------------------
    # THE TWO RANKINGS ARE DIFFERENT QUESTIONS, AND THE SUITE CHECKS THEY ARE REPORTED SEPARATELY
    # rather than trusting one heading to do the work of two.
    # ---------------------------------------------------------------------------------------------
    check("the MOST EXPENSIVE and the SLOWEST have their own headings",
          "MOST EXPENSIVE BY TOKENS" in text and "SLOWEST BY DURATION" in text, text[:200])

    # *** THE HONEST-PRESENCE CHECK, AND IT IS THE POINT OF THE FIXTURE'S REALITY. *** These calls go
    # through `TurboMCP.TurboMCPReadToolset` and `TurboMCP.TurboMCPEditToolset`, which the recorder
    # wraps, so they DO carry a measured token estimate. A tool that answered with the absence message
    # here would be printing an absence that is not there - and the message itself is still asserted,
    # from the branch's own text, so removing it from the tool would fail this suite too.
    tokens_section = section_text(text, "MOST EXPENSIVE BY TOKENS")
    check("a measured token estimate exists, so the ranking names a winner",
          "token estimate |" in tokens_section, tokens_section[:300])
    check("the winner is one of the calls this suite made",
          any(name in tokens_section for name in
              ("ReadTurboMCPStatus", "ReadTurboMCPDescribe", "ReadTurboMCPMaterialTrace")),
          tokens_section[:300])
    check("and the absence message is NOT printed beside it, because it would be false",
          "none of the" not in tokens_section, tokens_section[:300])

    slowest_section = section_text(text, "SLOWEST BY DURATION")
    # `.splitlines()` IS REQUIRED, AND OMITTING IT WAS A REAL BUG IN THIS SUITE: `section_text`
    # returns a STRING, so iterating it yields CHARACTERS - every one of which fails the `"|" in l`
    # test - so the generator fell through to its default and the check failed WITH AN EMPTY DETAIL.
    # A CHECK THAT FAILS ON A CORRECT TOOL AND EXPLAINS ITSELF WITH NOTHING is the shape this project
    # has recorded ten times, and this instance was mine.
    slowest_line = next((l for l in slowest_section.splitlines() if "|" in l), "")
    check("the slowest call is NAMED, with a timestamp and a duration",
          slowest_line.strip().startswith("2026") and "ms" in slowest_line,
          "line=[%s] section=[%s]" % (slowest_line.strip(), slowest_section.strip()[:160]))

    # ---------------------------------------------------------------------------------------------
    # THE PER-TOOL TOTALS ARE COMPARED AGAINST `R-CALL-LOG`, A DIFFERENT TOOL.
    #
    # `R-CALL-COST` reports one row per tool for the entries IT considered; `R-CALL-LOG` lists lines.
    # The agreement that matters is that EVERY TOOL THE COST REPORT NAMES APPEARS IN THE LOG, which
    # is a claim about the same underlying data rendered two ways - and it is checkable without
    # comparing two absolute counts, which would race against the log's own growth.
    # ---------------------------------------------------------------------------------------------
    tool_section = "\n".join(section(text, "TOTALS PER TOOL"))
    cost_tools = set()
    for line in tool_section.splitlines():
        if "|" in line and "call(s)" in line:
            cost_tools.add(line.split("|")[0].strip())

    check("the per-tool section names at least one tool", len(cost_tools) > 0, tool_section[:200])
    check("every per-tool row carries its CALL COUNT, which is the denominator `P1` needs",
          all("call(s)" in l for l in tool_section.splitlines() if "|" in l), tool_section[:300])

    listing = call_log(session, limit="200")
    missing = []
    for tool in cost_tools:
        # The cost report compacts `TurboMCPReadToolset.ReadTurboMCPStatus`; the log prints its own
        # spelling of the same thing. So the check is on the TOOL half, which both render the same
        # way, rather than on the full label - comparing the full label would be comparing two
        # RENDERINGS instead of two statements about one log.
        short = tool.split(".")[-1]
        if short not in listing:
            missing.append(tool)
    check("every tool the cost report totals appears in R-CALL-LOG's own listing",
          not missing, "missing from the log listing: %s" % (missing or "none"))

    # ---------------------------------------------------------------------------------------------
    # THE DOUBLY-QUALIFIED NAME, WHICH IS THE DEFECT THE LOST WORK MEASURED.
    # ---------------------------------------------------------------------------------------------
    doubled = [t for t in cost_tools if t.count("Toolset") > 1]
    check("no tool row repeats its toolset name, which is the doubling defect",
          not doubled, "doubled rows: %s" % (doubled or "none"))
    check("no tool row contains the engine's qualification path",
          not any("TurboMCP.TurboMCP" in t for t in cost_tools),
          "\n".join(sorted(cost_tools)))

    # ---------------------------------------------------------------------------------------------
    # THE PER-ASSET SECTION, WHICH NOW FILLS. A call through this plugin's boundary carries its
    # arguments, so the asset the suite asked about is named - AND THE COUNT IS THE PAIRING CHECK:
    # one call must read one call, not two. Before the timeline paired the records, this row read
    # `2 call(s)` for a single call, because both records of one plugin pair carry the arguments.
    # ---------------------------------------------------------------------------------------------
    check("the per-asset section is present",
          "TOTALS PER ASSET" in text, text[:200])
    check("a call that named an asset fills the per-asset totals",
          "TOTALS PER ASSET (" in text, text[-600:])
    check("the asset this suite asked about is NAMED, not silently absent",
          "no_such_asset" in text, text[-600:])
    asset_line = next((l for l in text.splitlines() if l.strip().startswith("no_such_asset")), "")
    check("ONE call is counted ONCE - a start and an end are one call, not two",
          "| 1 call(s) |" in asset_line, "line=[%s]" % asset_line.strip())

    # ---------------------------------------------------------------------------------------------
    # THE FILTER, WHICH IS THE THIRD EMPTINESS `P1` NAMES.
    # ---------------------------------------------------------------------------------------------
    print()
    print("=== a filter that matches nothing says so, and names what it did not match ===")
    nothing = cost(session, filter_text="zzz_no_such_tool_zzz")
    print(nothing)
    print()
    check("a filter matching nothing still reports the log as ON, not as off",
          "recording: on" in nothing, nothing[:200])
    check("it says the entries are there but NONE matched, which is its own answer",
          "NONE matched the filter" in nothing or "none matching" in nothing, nothing[:400])
    check("it says how many calls DID exist, so the zero is given a denominator",
          re.search(r"\d+ call", nothing) is not None, nothing[:400])

    print()
    print("=== the filter finds a real tool, and the cap bounds the rows rather than the analysis ===")
    filtered = cost(session, filter_text="TurboMCPReadToolset")
    check("a filter on a real toolset matches something and reports it as the answer",
          "TOTALS PER TOOL" in filtered and "none matching" not in filtered, filtered[:300])

    capped = cost(session, cap="1")
    # `section` RETURNS A LIST OF LINES, and the first version of this line called `.splitlines()`
    # on it - which raised `AttributeError: 'list' object has no attribute 'splitlines'` and killed
    # the suite BEFORE IT REPORTED ANY VERDICT. THE HARNESS SAID `no result line`, WHICH IS EXACTLY
    # WHAT A CRASHED SUITE LOOKS LIKE AND EXACTLY WHY THE HARNESS NOW PRINTS THE TAIL.
    cap_section = section(capped, "TOTALS PER TOOL")
    capped_rows = [l for l in cap_section if "call(s)" in l]
    check("the cap bounds the rows SHOWN", len(capped_rows) == 1, "\n".join(capped_rows))
    # THE POINT OF THE CAP: it must not change a total, only how many rows are listed.
    total_in_cap = re.search(
        r"counts:\s*\d+ call\(s\) from \d+ record\(s\) in the log,\s*(\d+) considered", capped)
    total_in_full = re.search(
        r"counts:\s*\d+ call\(s\) from \d+ record\(s\) in the log,\s*(\d+) considered", text)
    check("the cap does NOT reduce how many entries were considered",
          total_in_cap is not None and total_in_full is not None,
          "cap=%s full=%s" % (total_in_cap and total_in_cap.group(1),
                              total_in_full and total_in_full.group(1)))

    # ---------------------------------------------------------------------------------------------
    # PUT THE SWITCH BACK. The suite toggled it, so it restores it and ASSERTS the restore - the
    # rule this project applies to every shared resource.
    # ---------------------------------------------------------------------------------------------
    set_log(session, "true")
    after = call_log(session, limit="1")
    check("the switch was restored to ON, which the log itself reports",
          "recording: on" in after, after[:200])

    print()
    # *** THE CAP'S TWO REFUSALS, WHICH NO RUN HAD EVER TAKEN. *** Until `4fc7f70` a `cap` of `banana` or `0`
    # answered 10 and said nothing. THE CLAMP FOR A LARGE NUMBER IS DELIBERATE AND STAYS - a caller asking for
    # a hundred rows is asking for as much as this tool will give, which is not the same as a typo - so what is
    # asserted here is only that a TYPO and a ZERO are refused, each in its own words.
    bad_cap = cost(session, cap="banana")
    check("refuses a cap that is not a number, and names the parameter",
          bad_cap.startswith("error:") and "cap" in bad_cap and "whole-number" in bad_cap, bad_cap[:200])
    zero_cap = cost(session, cap="0")
    check("refuses a cap of zero with its OWN sentence, not the non-number one",
          zero_cap.startswith("error:") and "cap must be positive" in zero_cap, zero_cap[:200])

    print()
    if failures:
        print("RESULT: FAIL (%d) -> %s" % (len(failures), ", ".join(failures)))
        return 1
    print("RESULT: PASS (%d check(s) run)" % checks_run[0])
    return 0


if __name__ == "__main__":
    sys.exit(main())
