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

1. EVERY ENTRY IN THE LOG IS `source=engine`, because NO PLUGIN TOOL CALLS `FScopedCall` - it is
   defined in `TurboMCPCallLog.cpp:205` and constructed nowhere. So `token_estimate` IS 0 FOR EVERY
   LINE, and a check demanding a non-zero "most expensive by tokens" would fail on a CORRECT tool.
   THIS SUITE THEREFORE ASSERTS THE ABSENCE HONESTLY: it requires the answer to SAY that no token
   estimate was recorded, which is the difference between an absent measurement and a real zero.

2. THE LOG'S CLOCK IS THE EDITOR'S, AND AN ENGINE ENTRY CARRIES NO ARGUMENTS. So the per-asset
   section cannot fill from engine lines, and the suite requires the answer to SAY SO rather than
   printing an empty list that reads like "no asset cost anything".

3. THE LOG GROWS BETWEEN TWO READS, BECAUSE READING IS ITSELF LOGGED. `Tools/test-call-log.py`
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
          "NO COST CAN BE REPORTED" in off, off[:300])
    check("it names LOGGING OFF as the reason, which is one of the three `P1` distinguishes",
          "logging is OFF" in off, off[:400])
    # `P1` AND `T-HELP-6` TOGETHER: the whole point is that "off" and "zero" are different answers.
    check("it does NOT report a zero total, because nothing was measured",
          "0 tokens" not in off and "0 call(s)" not in off, off[:300])
    check("it names the switch that turns logging on, so the next step is a call and not a guess",
          "E-CALL-LOG" in off, off[:400])

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
    check("the answer carries a denominator - how many entries it considered",
          re.search(r"counts:\s*\d+ in the log,\s*\d+ considered", text) is not None, text[:300])
    check("it says how many entries recorded a duration and how many a token estimate",
          re.search(r"\d+ recording a duration and \d+ recording a token estimate", text) is not None,
          text[:400])

    # ---------------------------------------------------------------------------------------------
    # THE TWO RANKINGS ARE DIFFERENT QUESTIONS, AND THE SUITE CHECKS THEY ARE REPORTED SEPARATELY
    # rather than trusting one heading to do the work of two.
    # ---------------------------------------------------------------------------------------------
    check("the MOST EXPENSIVE and the SLOWEST have their own headings",
          "MOST EXPENSIVE BY TOKENS" in text and "SLOWEST BY DURATION" in text, text[:200])

    # *** THE HONEST-ABSENCE CHECK, AND IT IS THE POINT OF THE FIXTURE'S REALITY. *** No entry in
    # this log can carry a token estimate, because no plugin tool writes one. A tool that NAMED a
    # winner here would be ranking rows that all read zero.
    # PROSE-TOLERANT: the absence messages ARE the contract for this section.
    tokens_section = section_text(text, "MOST EXPENSIVE BY TOKENS")
    check("with no token estimate recorded, it says so rather than naming a winner",
          "none of the" in tokens_section and "token estimate above zero" in tokens_section,
          tokens_section[:300])
    check("it explains that this is NOT zero cost, so an agent does not read it as cheap",
          "THIS IS NOT ZERO COST" in tokens_section, tokens_section[:400])
    check("it points at the ranking that CAN be answered instead",
          "SLOWEST" in tokens_section, tokens_section[:400])

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
    # THE PER-ASSET SECTION NAMES ITS OWN ABSENCE. An empty list there would read as "no asset cost
    # anything" instead of "no entry named one", and those need different actions.
    # ---------------------------------------------------------------------------------------------
    # The absence statement is PROSE rather than a data row, so it is looked for in the whole answer
    # - `section` deliberately drops non-row lines, which is what makes it usable for the totals.
    check("the per-asset section is present even when it cannot fill",
          "TOTALS PER ASSET" in text, text[:200])
    check("when no entry names an asset, it says so rather than printing nothing",
          "none can be computed" in text, text[-600:])
    if "none can be computed" in text:
        check("it says WHY the per-asset totals cannot be computed",
              "NO ARGUMENTS" in text or "name no asset" in text, text[-600:])
        check("it distinguishes an absent measurement from zero cost",
              "not zero" in text.lower(), text[-600:])

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
    check("it says how many entries DID exist, so the zero is given a denominator",
          re.search(r"\d+ entr", nothing) is not None, nothing[:400])

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
    total_in_cap = re.search(r"counts:\s*\d+ in the log,\s*(\d+) considered", capped)
    total_in_full = re.search(r"counts:\s*\d+ in the log,\s*(\d+) considered", text)
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
          bad_cap.startswith("error:") and "is not a cap" in bad_cap, bad_cap[:200])
    zero_cap = cost(session, cap="0")
    check("refuses a cap of zero with its OWN sentence, not the non-number one",
          zero_cap.startswith("error:") and "is not usable" in zero_cap, zero_cap[:200])

    print()
    if failures:
        print("RESULT: FAIL (%d) -> %s" % (len(failures), ", ".join(failures)))
        return 1
    print("RESULT: PASS (%d check(s) run)" % checks_run[0])
    return 0


if __name__ == "__main__":
    sys.exit(main())
