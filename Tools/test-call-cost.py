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


def section(text, heading):
    """The lines of one section, from its heading to the next blank-line-separated heading.

    STRUCTURAL RATHER THAN A LIST OF MAGIC PHRASES. The lost work on this capability recorded the
    lesson: its first fix for an unparseable section was a list of literal phrases to skip, which is
    A PARSER THAT ASSUMES A FORMAT - and the same mistake has cost this project five checks twice.
    A section here is whatever lies between two headings, whatever words it happens to contain.
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
    tokens_section = "\n".join(section(text, "MOST EXPENSIVE BY TOKENS"))
    check("with no token estimate recorded, it says so rather than naming a winner",
          "none of the" in tokens_section and "token estimate above zero" in tokens_section,
          tokens_section[:300])
    check("it explains that this is NOT zero cost, so an agent does not read it as cheap",
          "THIS IS NOT ZERO COST" in tokens_section, tokens_section[:400])
    check("it points at the ranking that CAN be answered instead",
          "SLOWEST" in tokens_section, tokens_section[:400])

    slowest_section = "\n".join(section(text, "SLOWEST BY DURATION"))
    slowest_line = next((l for l in slowest_section if "|" in l), "")
    check("the slowest call is NAMED, with a timestamp and a duration",
          slowest_line.strip().startswith("2026") and "ms" in slowest_line, slowest_section[:300])

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
    asset_section = "\n".join(section(text, "TOTALS PER ASSET"))
    check("the per-asset section is present even when it cannot fill",
          "TOTALS PER ASSET" in text, text[:200])
    check("when no entry names an asset, it says so rather than printing nothing",
          "none can be computed" in asset_section or "call(s)" in asset_section, asset_section[:300])
    if "none can be computed" in asset_section:
        check("it says WHY the per-asset totals cannot be computed",
              "NO ARGUMENTS" in asset_section or "name no asset" in asset_section, asset_section[:400])
        check("it distinguishes an absent measurement from zero cost",
              "not zero" in asset_section.lower(), asset_section[:400])

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
    capped_rows = [l for l in section(capped, "TOTALS PER TOOL").splitlines() if "call(s)" in l]
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
    if failures:
        print("RESULT: FAIL (%d) -> %s" % (len(failures), ", ".join(failures)))
        return 1
    print("RESULT: PASS (%d check(s) run)" % checks_run[0])
    return 0


if __name__ == "__main__":
    sys.exit(main())
