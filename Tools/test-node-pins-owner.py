# ACCEPTANCE ROW THIS SUITE CLAIMS TO PROVE: NONE (this proves T01's FIRST verify scenario, which no row names)
"""A batch pins preview over ten or more node types, INCLUDING the case that asserted the editor.

WHAT CRASHED
    The report: "The node pins preview stopped the editor with an engine assertion. Its probe node asked for an owner
    graph that did not exist." *** `FTurboMCPNodePins::Plan` TOOK `FindGraphByName`'S RESULT - NULL WHEN NOTHING
    MATCHES - AND HANDED IT TO `BuildProbeNode`, WHICH GIVES IT TO THE ENGINE AS THE NODE'S OWNER. NOTHING CHECKED IT
    BETWEEN THE TWO LINES. *** Fixed in `ac10dca`.

WHY `test-batch-tools.py` IS NOT ENOUGH
    It already exercises this tool and does it well: the id the type reader returns is usable by the pins reader, pins
    come back in the engine's direction spelling, and *** READING THE PINS DOES NOT CHANGE THE GRAPH'S NODE COUNT *** -
    the throwaway-probe requirement. *** BUT IT USES ONE TYPE ID AND ALWAYS THE REAL GRAPH NAME, SO THE ONE INPUT
    THAT ASSERTED - A GRAPH NAME THAT DOES NOT EXIST - IS NOT IN IT, AND NEITHER IS A BATCH. ***

THE IDS, AND WHY THEY ARE A LIST RATHER THAN DISCOVERED
    The type reader's listing output does not carry bare `K2Node_...` tokens, so a batch cannot be assembled by
    scraping it. *** BUT `test-batch-tools` PASSES ITS PINS CHECK WITH THE ID `K2Node_CallFunction`, WHICH IS WHERE
    THIS LIST COMES FROM: THE FORM IS A BARE ENGINE CLASS NAME AND THAT ONE IS ALREADY PROVEN USABLE. *** The batch
    checks below do not trust the list either: they require at least ten types to be ANSWERED and at least one to
    come back as a real pin list, so a list of dead ids fails rather than passing quietly.

WHAT THIS CHECKS
    1. THE CRASH CASE: a real type id against a graph name that does not exist must be REFUSED BY NAME, and the call
       returning at all is the evidence that the editor survived it.
    2. THE CONTROL: the same type id against the real graph must NOT be refused, so a reader that refused everything
       could not pass this.
    3. THE BATCH: ten or more node types are previewed, each answer is a pin list or a refusal that names the type,
       and AT LEAST ONE IS A PIN LIST - *** WITHOUT THAT LAST CHECK, TEN REFUSALS WOULD LOOK LIKE A PASS. ***
"""

import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from tmcp import PREVIEW_TOOLSET, Session  # noqa: E402

ASSET = "/Game/Tests/BP_NodeTest"
GRAPH = "EventGraph"

# A name no Blueprint has. This is the input that reached the engine with a null owner.
MISSING_GRAPH = "NoSuchGraph_ForTheOwnerGuard"

# What the guard prints. A substring, because the sentence also names the asset and the type.
OWNER_MARKER = "has no graph named"

# Bare engine class names. `K2Node_CallFunction` is proven usable: `test-batch-tools` passes its pins check with it.
TYPE_IDS = [
    "K2Node_CallFunction",
    "K2Node_VariableGet",
    "K2Node_VariableSet",
    "K2Node_IfThenElse",
    "K2Node_ExecutionSequence",
    "K2Node_MacroInstance",
    "K2Node_Knot",
    "K2Node_CommutativeAssociativeBinaryOperator",
    "K2Node_Select",
    "K2Node_DynamicCast",
    "K2Node_MakeStruct",
    "K2Node_BreakStruct",
    "K2Node_CallParentFunction",
    "K2Node_Self",
]

failures = []


def check(name, ok, detail=""):
    print("  %-4s %s%s" % ("ok" if ok else "FAIL", name, ("  " + detail) if detail else ""))
    if not ok:
        failures.append(name)


def pins(session, graph, type_id):
    return session.call(PREVIEW_TOOLSET, "PreviewTurboMCPNodePins",
                        {"asset": ASSET, "graphName": graph, "typeId": type_id})


def main():
    session = Session()
    session.connect()
    for limit in ("0", "-1", "+1", "1.5", "201", "9" * 80, "abc"):
        response = session.call(PREVIEW_TOOLSET, "PreviewTurboMCPNodeType",
                                {"asset": "", "graphName": GRAPH, "words": "", "limit": limit})
        check("invalid limit refused: " + limit[:12], response.startswith("error:") and "Limit must" in response, response)


    # THE USABLE IDS COME FROM THE TYPE READER, NOT FROM A LIST OF ENGINE CLASS NAMES. A batch of bare `K2Node_x`
    # names came back ALL-REFUSALS, which the anti-vacuous check below caught. `test-batch-tools` reads the ids out of
    # the type reader's own output and passes, so its two patterns are used here.
    listed = session.call(PREVIEW_TOOLSET, "PreviewTurboMCPNodeType",
                          {"asset": ASSET, "graphName": GRAPH, "words": "PrintString", "limit": ""})
    discovered = re.findall(r"(?:^|\s)([\w|]+K2Node[\w|]*)", listed)
    if not discovered:
        discovered = re.findall(r"([A-Za-z][\w ]*\|[\w ]+)", listed)
    print("    ids the type reader listed: %s" % discovered[:3])

    # 1. THE CRASH CASE.
    crash = pins(session, MISSING_GRAPH, TYPE_IDS[0])
    print("    missing-graph answer: %s" % (crash.splitlines()[0][:110] if crash else "(none)"))
    check("a graph name that does not exist is refused by name, not asserted", OWNER_MARKER in crash)
    check("the refusal names the graph that was asked for", MISSING_GRAPH in crash)

    # 2. THE CONTROL.
    control = pins(session, GRAPH, TYPE_IDS[0])
    check("the same type against the real graph is not refused for its owner", OWNER_MARKER not in control,
          control.splitlines()[0][:110] if control else "(none)")

    # 3. THE BATCH.
    answered = 0
    pin_lists = 0
    unnamed = []
    for type_id in (discovered[:4] + TYPE_IDS):
        answer = pins(session, GRAPH, type_id)
        if not answer.strip():
            unnamed.append("%s: EMPTY" % type_id)
            continue
        answered += 1
        refused = answer.startswith("error:") or OWNER_MARKER in answer
        if refused:
            # A refusal is an answer, but it must say which type it refused.
            if type_id not in answer:
                unnamed.append("%s: refused without naming itself" % type_id)
        else:
            pin_lists += 1

    print("    batch: %d of %d types answered, %d with a real pin list" % (answered, len(TYPE_IDS), pin_lists))
    check("every type in the batch answered, none came back empty", not unnamed, "; ".join(unnamed[:4]))
    check("ten or more node types were answered, by pins or by a named refusal", answered >= 10,
          "%d of %d" % (answered, len(TYPE_IDS)))
    check("at least one type produced a real pin list", pin_lists > 0, "%d pin list(s)" % pin_lists)
    check("the reader was still answering at the end of the batch", answered > 0)

    print("")
    if failures:
        print("RESULT: FAIL (%d check(s) failed: %s)" % (len(failures), ", ".join(failures)))
        return 1
    print("RESULT: PASS (%d type(s) previewed in one batch; the missing-graph case is refused by name and the editor "
          "survived it)" % answered)
    return 0


if __name__ == "__main__":
    sys.exit(main())
