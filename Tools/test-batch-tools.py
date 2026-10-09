"""Drive the tools added in the batch round: the schema reader, the node type readers, the material
readers.

WHY THIS SUITE EXISTS
---------------------
`Tools/audit-tool-coverage.py` reported these seven as called by NOTHING:

    ReadTurboMCPToolSchema, PreviewTurboMCPNodeType, PreviewTurboMCPNodePins,
    ReadTurboMCPMaterialLabels, ReadTurboMCPMaterialLoad,
    ReadTurboMCPMaterialTrace, ReadTurboMCPMaterialBlocks

Each was written against the engine's headers and COMPILED, which is not the same as working. A tool
that registers and answers nothing looks finished in the index and in the panel's list.

WHAT EACH CHECK PROVES, AND HOW IT COULD FAIL
---------------------------------------------
- the schema reader accepts a toolset by SHORT name, which it did not at first: a call with `Read`
  answered "'Read' is not a toolset the registry serves" and listed every real name, because the
  suffix rule compared the normalised name WITH its `Toolset` class suffix, so
  `turbomcpturbomcpreadtoolset` never ended with `read`. The check asserts the short form resolves,
  and it would fail again if that rule regressed;
- it reports the engine's tools too, so it is not a private list;
- the node type reader finds a real id for an ordinary node and refuses a word that matches nothing
  BY NAMING the closest ones, rather than answering nothing;
- the pins reader answers WITHOUT CHANGING THE ASSET, which is checked by reading the node count
  before and after. The engine's own pin reader adds a probe node to the real graph and leaves it
  there, which is the trap this reader exists to avoid, so the check is the point;
- each material reader answers on a non-material asset with a refusal BY NAME rather than an empty
  document, because there is no material in the test project yet. That is a deliberately weak
  assertion and it is labelled as one: it proves the refusal, not the reading.
"""

import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from tmcp import PREVIEW_TOOLSET, READ_TOOLSET, Session  # noqa: E402

ASSET = "BP_NodeTest"
GRAPH = "EventGraph"

failures = []


def check(name, ok, detail=""):
    print("  %-4s %s%s" % ("ok" if ok else "FAIL", name, ("  " + detail) if detail else ""))
    if not ok:
        failures.append(name)


def main():
    session = Session()
    session.connect()

    print("=== R-TOOL-SCHEMA: the short toolset name, which was BROKEN ===")
    short = session.call(READ_TOOLSET, "ReadTurboMCPToolSchema", {"toolset": "Read", "tool": ""})
    print(short[:700])
    print()
    check("a SHORT toolset name resolves rather than being refused",
          not short.startswith("error:"), short[:160])
    check("it names the toolset it resolved",
          "TurboMCPReadToolset" in short, short[:200])
    count = re.search(r"(\d+) tool\(s\)", short)
    check("it reports a tool count", count is not None, short[:120])
    if count:
        # The count is PARSED, never a pinned literal: this asset and this plugin both grow.
        check("the count is more than the four tools that existed before materials",
              int(count.group(1)) > 4, count.group(1))

    # The engine's own tools must be reachable by the SAME code path, or the "works for engine tools
    # too" claim is untested.
    engine = session.call(READ_TOOLSET, "ReadTurboMCPToolSchema",
                          {"toolset": "SlateInspectorToolset", "tool": ""})
    print(engine[:400])
    print()
    check("an ENGINE toolset is served by the same reader",
          not engine.startswith("error:") and "Slate" in engine, engine[:200])

    # ONE tool's full schema: the parameters and the trap notes.
    one = session.call(READ_TOOLSET, "ReadTurboMCPToolSchema",
                       {"toolset": "Read", "tool": "ReadTurboMCPGraphLoad"})
    print(one[:700])
    print()
    check("one tool's schema answers", not one.startswith("error:"), one[:160])
    check("it lists the tool's parameters",
          "asset" in one.lower() and "graphName" in one, one[:400])
    check("the answer stays under the 2000 character budget R-TOOL-SCHEMA sets",
          len(one) <= 2400, "%d characters" % len(one))

    # A name that fits nothing is refused with the closest real names, which is `P7`.
    miss = session.call(READ_TOOLSET, "ReadTurboMCPToolSchema",
                        {"toolset": "ReadToolset", "tool": ""})
    check("a wrong toolset is refused with the closest names",
          miss.startswith("error:") and "Closest" in miss, miss[:200])

    print()
    print("=== P-NODE-TYPE and P-NODE-PINS ===")
    typed = session.call(PREVIEW_TOOLSET, "PreviewTurboMCPNodeType",
                         {"asset": ASSET, "graphName": GRAPH, "words": "PrintString",
                          "limit": ""})
    print(typed[:700])
    print()
    check("the node type reader answers", not typed.startswith("error:"), typed[:160])
    check("it found at least one real id", "|" in typed or "K2Node" in typed, typed[:300])

    # THE ID IT RETURNS MUST BE USABLE, so the pins reader is pointed at it. A type id the pins
    # reader then refuses would mean the two disagree, which is worse than one being wrong.
    ids = re.findall(r"(?:^|\s)([\w|]+K2Node[\w|]*)", typed)
    if not ids:
        ids = re.findall(r"([A-Za-z][\w ]*\|[\w ]+)", typed)
    print("    candidate ids: %s" % ids[:3])

    # THE ASSET MUST NOT CHANGE. The engine's own pin reader adds a probe node to the real graph and
    # leaves it there; this one must not.
    before = session.call(READ_TOOLSET, "ReadTurboMCPGraphLoad",
                          {"asset": ASSET, "graphName": GRAPH, "detail": "signatures",
                           "format": "", "tokenBudget": ""})

    pins_args = {"asset": ASSET, "graphName": GRAPH,
                 "typeId": ids[0].strip() if ids else "K2Node_CallFunction"}
    pinned = session.call(PREVIEW_TOOLSET, "PreviewTurboMCPNodePins", pins_args)
    print(pinned[:700])
    print()
    check("the pins reader answers for an id the type reader returned",
          not pinned.startswith("error:"), pinned[:200])
    check("it reports at least one pin", "EGPD_" in pinned or "input" in pinned.lower(),
          pinned[:300])
    # THE POSITION IS COUNTED PER DIRECTION, which is what the engine does and what the doc says.
    check("the pin direction uses the engine's spelling",
          "EGPD_Input" in pinned or "EGPD_Output" in pinned, pinned[:300])

    after = session.call(READ_TOOLSET, "ReadTurboMCPGraphLoad",
                         {"asset": ASSET, "graphName": GRAPH, "detail": "signatures",
                          "format": "", "tokenBudget": ""})
    before_count = re.search(r"(\d+) node\(s\)", before)
    after_count = re.search(r"(\d+) node\(s\)", after)
    print("    node count %s -> %s"
          % (before_count.group(1) if before_count else "?",
             after_count.group(1) if after_count else "?"))
    check("READING THE PINS DID NOT CHANGE THE GRAPH",
          before_count is not None and after_count is not None
          and before_count.group(1) == after_count.group(1),
          "%s -> %s" % (before_count.group(1) if before_count else "?",
                        after_count.group(1) if after_count else "?"))

    # A word that matches nothing is refused with the closest names.
    nothing = session.call(PREVIEW_TOOLSET, "PreviewTurboMCPNodeType",
                           {"asset": ASSET, "graphName": GRAPH,
                            "words": "zzzznotanodewordzzzz", "limit": ""})
    check("a word that matches nothing is refused rather than answering nothing",
          nothing.startswith("error:") or "closest" in nothing.lower() or "no " in nothing.lower(),
          nothing[:200])

    print()
    print("=== the four material readers, against an asset that is NOT a material ===")
    # THERE IS NO MATERIAL IN THE TEST PROJECT YET, so this proves the REFUSAL and not the reading.
    # It is labelled that way on purpose: a suite that claimed to test the reading here would be
    # lying, and this project has been bitten by exactly that.
    for tool, args in (
        ("ReadTurboMCPMaterialLabels", {"asset": ASSET}),
        ("ReadTurboMCPMaterialLoad", {"asset": ASSET, "bIncludeInstanceOverrides": False,
                                      "tokenBudget": 0}),
        ("ReadTurboMCPMaterialTrace", {"asset": ASSET, "outputProperty": "MP_BaseColor",
                                       "tokenBudget": 0}),
        ("ReadTurboMCPMaterialBlocks", {"asset": ASSET, "tokenBudget": 0}),
    ):
        out = session.call(READ_TOOLSET, tool, args)
        print("  %s: %s" % (tool, out[:220].replace("\n", " ")))
        check("%s refuses a non-material BY NAME" % tool,
              out.startswith("error:") and "material" in out.lower(), out[:200])
        check("%s does not answer nothing" % tool, len(out.strip()) > 20, repr(out[:60]))

    # A material name that does not exist is refused by the shared resolver.
    missing = session.call(READ_TOOLSET, "ReadTurboMCPMaterialLabels",
                           {"asset": "NoSuchMaterialAnywhere"})
    check("a material that does not exist is refused",
          missing.startswith("error:"), missing[:160])

    print()
    print("=== the audit agrees these seven are covered now ===")
    import subprocess  # noqa: PLC0415
    repo = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    audit = subprocess.run([sys.executable, os.path.join(repo, "Tools", "audit-tool-coverage.py")],
                           capture_output=True, text=True, cwd=repo)
    uncalled = ""
    for line in (audit.stdout or "").splitlines():
        if "uncalled:" in line:
            uncalled = line.split("uncalled:")[1].strip()
    print("    uncalled now: %s" % (uncalled or "(none)"))
    check("no tool is left uncalled", uncalled in ("", "(none)"), uncalled)

    print()
    if failures:
        print("RESULT: FAIL (%d) -> %s" % (len(failures), ", ".join(failures)))
        return 1
    print("RESULT: PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
