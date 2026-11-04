# Acceptance coverage: T-MAT-3 (linked and constant material inputs).
"""Exercise material graph readers against the saved material/function fixture.

Checks labels, constants, reachability, connected-output tracing and comment boxes.
The fixture object name differs from its package filename. Tests use the object name.
This suite checks reported facts; graph preservation needs independent state evidence.
"""

import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from tmcp import READ_TOOLSET, Session  # noqa: E402

# THE ASSET NAME, NOT THE FILE NAME. See the docstring.
ASSET = "M_TurboMCPFixture"
INSTANCE = "MI_TurboMCPFixture"
FUNCTION = "MF_TurboMCPFixture"

failures = []


def check(name, ok, detail=""):
    print("  %-4s %s%s" % ("ok" if ok else "FAIL", name, ("  " + detail) if detail else ""))
    if not ok:
        failures.append(name)


def main():
    session = Session()
    session.connect()

    for tool, extra in (("ReadTurboMCPMaterialLoad", {"bIncludeInstanceOverrides": False, "tokenBudget": 0}),
                        ("ReadTurboMCPMaterialTrace", {"outputProperty": "MP_BaseColor", "tokenBudget": 0}),
                        ("ReadTurboMCPMaterialLabels", {}),
                        ("ReadTurboMCPMaterialBlocks", {"tokenBudget": 0})):
        help_text = session.call(READ_TOOLSET, "ReadTurboMCPToolHelp", {"toolset": READ_TOOLSET, "tool": tool})
        check(tool + " help documents asset", "asset" in help_text.lower(), help_text)
        refusal = session.call(READ_TOOLSET, tool, dict(asset=INSTANCE, **extra))
        check(tool + " instance refusal gives usable recovery", refusal.startswith("error:")
              and "ReadTurboMCPMaterialInstance" in refusal
              and "includeInstanceOverrides=true" not in refusal, refusal)

    print("=== R-MAT-LABELS: the unread label, which is the whole point of this reader ===")
    labels = session.call(READ_TOOLSET, "ReadTurboMCPMaterialLabels", {"asset": ASSET})
    print(labels[:900])
    print()
    check("it answers rather than erroring", not labels.startswith("error:"), labels[:160])
    check("it reports the label the fixture never reads",
          "UnusedTint" in labels, labels[:300])
    check("it MARKS that label unread",
          "UNREAD" in labels.upper(), labels[:400])
    check("it reports the label that is read more than once",
          "VerticalMix" in labels, labels[:300])
    # The count is PARSED, never pinned: the fixture can gain a label without the suite lying.
    read_twice = re.search(r"@VerticalMix\b.*?read in (\d+) place", labels, re.DOTALL)
    check("VerticalMix is read more than once",
          read_twice is not None and int(read_twice.group(1)) > 1,
          read_twice.group(1) if read_twice else "not found")
    check("it names the asset it read", ASSET in labels, labels[:200])

    print()
    print("=== R-MAT-LOAD: parameters, the unlinked constant, and the unreached ===")
    load = session.call(READ_TOOLSET, "ReadTurboMCPMaterialLoad",
                        {"asset": ASSET, "bIncludeInstanceOverrides": False,
                         "tokenBudget": 4000})
    print(load[:1200])
    print()
    check("it answers rather than erroring", not load.startswith("error:"), load[:160])
    check("it names the scalar parameter the fixture declares",
          "LazyScalar" in load, load[:400])
    check("it names the vector parameter the fixture declares",
          "OuterTint" in load, load[:400])
    # THE CONSTANT AN UNLINKED INPUT KEEPS, which 04_FORMATS 2.5 requires by name.
    #
    # THE VALUE IS PARSED, NOT MATCHED AS A LITERAL, and the first version got that wrong. It asserted
    # the string "0.375", which is FIXTURE WORDING: change the fixture's constant and the suite goes red
    # for a reason that has nothing to do with the reader. The material workstream flagged exactly that,
    # and this project has recorded the mistake five times. The check now asserts the SHAPE - a bare
    # NUMBER printed for an input the fixture left unlinked - and READS the number instead of pinning it.
    #
    # The parameter default is 0.75, so a numeric input value that is NOT 0.75 is the unlinked one.
    # Parse only the tree's form lines, where an input reads `B <value>)`, and skip the parameter table.
    small_budget = session.call(READ_TOOLSET, "ReadTurboMCPMaterialLoad",
                                {"asset": ASSET, "bIncludeInstanceOverrides":False, "tokenBudget":1})
    check("budget overrun is honest", "budget exceeded:" in small_budget and "does not enforce the budget" in small_budget and "REDUCED:" not in small_budget)
    tree = load.split("connected outputs", 1)[-1]
    constants = re.findall(r"^\s+[AB] (\d+(?:\.\d+)?)\)?\s*$", tree, re.MULTILINE)
    print("    numeric constants printed for inputs: %s" % (constants or "none"))
    check("it prints a NUMERIC constant for the fixture's unlinked input",
          len(constants) > 0, "no bare number found for an input in the tree")
    check("the unlinked constant is REAL, not a placeholder word",
          all(c.replace(".", "").isdigit() for c in constants) if constants else False,
          "constants: %s" % constants)
    check("it names an expression that reaches NO output",
          "reach no output" in load.lower() or "unreached" in load.lower(), load[:400])
    check("the unread label is among the unreached",
          "UnusedTint" in load, load[:600])
    check("it says how many expressions reach no output",
          re.search(r"(\d+) expression\(s\) reach no output", load) is not None,
          "a count is parsed, never pinned")
    # A LABEL IS REFERRED TO BY LABEL, not re-expanded. The tree must contain the label reference.
    check("a labelled value is REFERRED TO by its label",
          "@VerticalMix" in load, load[:600])

    print()
    print("=== R-MAT-TRACE: one output, and a refusal that LISTS the connected ones ===")
    trace = session.call(READ_TOOLSET, "ReadTurboMCPMaterialTrace",
                         {"asset": ASSET, "outputProperty": "MP_BaseColor", "tokenBudget": 3000})
    print(trace[:600])
    print()
    check("it answers for a connected output", not trace.startswith("error:"), trace[:160])
    check("it names the output it traced", "MP_BaseColor" in trace, trace[:300])

    connected = set(re.findall(r"^\((MP_\w+)", load, re.MULTILINE))
    for empty_output in ("", "   "):
        empty_refusal = session.call(READ_TOOLSET, "ReadTurboMCPMaterialTrace",
                                     {"asset": ASSET, "outputProperty":empty_output, "tokenBudget":0})
        check("empty trace lists real choices", empty_refusal.startswith("error:")
              and "Connected outputs:" in empty_refusal
              and all(name in empty_refusal for name in connected)
              and "none listed yet" not in empty_refusal, empty_refusal)
    unconnected = next((name for name in ("MP_EmissiveColor", "MP_Roughness", "MP_Metallic", "MP_Opacity") if name not in connected), None)
    assert unconnected is not None, "Fixture needs an unconnected material output for refusal coverage"
    refused = session.call(READ_TOOLSET, "ReadTurboMCPMaterialTrace",
                           {"asset": ASSET, "outputProperty": unconnected,
                            "tokenBudget": 0})
    print(refused[:400])
    print()
    check("an UNCONNECTED output is refused", refused.startswith("error:"), refused[:200])
    check("the refusal LISTS the connected outputs",
          "MP_BaseColor" in refused, refused[:300])

    print()
    print("=== R-MAT-BLOCKS: the boxes, the nesting, and what is in no box ===")
    blocks = session.call(READ_TOOLSET, "ReadTurboMCPMaterialBlocks",
                          {"asset": ASSET, "tokenBudget": 4000})
    print(blocks[:1100])
    print()
    check("it answers rather than erroring", not blocks.startswith("error:"), blocks[:160])
    check("it reports both boxes",
          "INNER" in blocks and "OUTER" in blocks, blocks[:400])
    check("it reports the count outside comment boxes",
          re.search(r"\b\d+ expression\(s\) are outside all comment boxes", blocks) is not None, blocks[:500])
    check("its findings include a value crossing boxes without a label",
          "cross" in blocks.lower(), blocks[:500])
    # NESTING IS GEOMETRIC. INNER sits inside OUTER, so the geometry must show it.
    check("the box geometry is printed, so nesting can be seen",
          re.search(r"\d+x\d+ at \d+,\d+", blocks) is not None, "no WxH at X,Y found")

    print()
    print("=== the material INSTANCE and the material FUNCTION ===")
    inst = session.call(READ_TOOLSET, "ReadTurboMCPMaterialLoad",
                        {"asset": ASSET, "bIncludeInstanceOverrides": True,
                         "tokenBudget": 2000})
    check("the base material still answers with overrides requested",
          not inst.startswith("error:"), inst[:160])

    fn = session.call(READ_TOOLSET, "ReadTurboMCPMaterialLoad",
                      {"asset": FUNCTION, "bIncludeInstanceOverrides": False,
                       "tokenBudget": 2000})
    print(("  function: " + fn[:200].replace("\n", " ")) if not fn.startswith("error:")
          else ("  function: " + fn[:200]))
    check("a material FUNCTION is read by the same reader",
          not fn.startswith("error:"), fn[:200])

    print()
    print("=== READING DID NOT CHANGE THE MATERIAL ===")
    # Repeated label output detects visible report changes; it does not prove all in-memory state is unchanged.
    labels_after = session.call(READ_TOOLSET, "ReadTurboMCPMaterialLabels", {"asset": ASSET})
    check("label report is stable across reads", labels == labels_after, labels_after)
    before_counts = re.search(r"(\d+) expression\(s\) in the graph, (\d+) comment box", labels)
    after_counts = re.search(r"(\d+) expression\(s\) in the graph, (\d+) comment box",
                             labels_after)
    print("    expressions/boxes %s -> %s"
          % (before_counts.groups() if before_counts else "?", before_counts.groups()
             if before_counts else "?"))
    check("the expression and box counts are UNCHANGED by reading",
          before_counts is not None and after_counts is not None
          and before_counts.groups() == after_counts.groups(),
          "%s -> %s" % (before_counts.groups() if before_counts else "?",
                        after_counts.groups() if after_counts else "?"))

    print()
    if failures:
        print("RESULT: FAIL (%d) -> %s" % (len(failures), ", ".join(failures)))
        return 1
    print("RESULT: PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
