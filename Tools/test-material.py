"""Drive the four material readers against the fixture built for them.

WHY THIS SUITE EXISTS
---------------------
`Tools/test-batch-tools.py` proves the four material readers REFUSE a non-material asset, and says in
its own docstring that this is not a test of the READING. This suite is that test: the fixture at
`/Game/Tests/TurboMCPMaterialFixture` carries the structures each reader exists to find.

THE ASSET'S NAME IS NOT THE FILE'S NAME, and every check below depends on knowing that.
`Tools/make-material-asset.ps1` writes `TurboMCPMaterialFixture.uasset`, and the asset INSIDE it is
`M_TurboMCPFixture`. A reader pointed at `TurboMCPMaterialFixture` answers "no asset named ..." while the
file sits on disk, which cost a round of confusion before the search output showed
`/Game/Tests/TurboMCPMaterialFixture.M_TurboMCPFixture [Material]`.

WHAT EACH CHECK PROVES, AND HOW IT COULD FAIL
---------------------------------------------
- `R-MAT-LABELS` reports the UNREAD label BY NAME. The fixture declares `UnusedTint` and reads it zero
  times. Fails if the unread rule stops firing, which is the single finding this reader exists for;
- `R-MAT-LABELS` reports `VerticalMix` as read MORE THAN ONCE. Fails if the reader counts declarations
  as reads;
- `R-MAT-LOAD` names both parameters with their group and prints the constant an UNLINKED input keeps.
  The fixture leaves `0.375` on a Multiply's B input. Fails if the reflection constant path breaks,
  which is what `04_FORMATS.md` 2.5 requires by name and which a class-name list would silently miss;
- `R-MAT-LOAD` flags expressions that reach NO output. The fixture has `LazyScalar` and `UnusedTint`
  among them. Fails if the reachability walk stops reporting;
- `R-MAT-TRACE` REFUSES an output that is not connected, and LISTS the ones that are. Fails if the
  refusal becomes a silent empty tree;
- `R-MAT-BLOCKS` reports the NESTING, and the fixture has `INNER` inside `OUTER`. It must also say how
  many expressions are inside NO box, because a block view that dropped them would look complete;
- and READING MUST NOT CHANGE THE MATERIAL, checked by comparing the label count before and after.

The suite does NOT check the exact expression text. That would pin it to the fixture's layout and break
on any change to the reader's wording, which is the "pinned literal" mistake this project has recorded
five times. It checks the FACTS the readers are supposed to surface.
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
    # THE CONSTANT AN UNLINKED INPUT KEEPS, which 04_FORMATS 2.5 requires by name. The fixture puts
    # 0.375 on a Multiply's B input and leaves it unlinked.
    check("it prints the constant an UNLINKED input keeps",
          "0.375" in load, "the fixture leaves 0.375 on a Multiply B input")
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

    refused = session.call(READ_TOOLSET, "ReadTurboMCPMaterialTrace",
                           {"asset": ASSET, "outputProperty": "MP_EmissiveColor",
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
    check("it reports how many expressions are inside NO box",
          "inside no comment box" in blocks, blocks[:500])
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
    # The readers must not add a probe node or move anything. The label report is taken again and
    # compared, because a reader that wrote would change what the next read sees.
    labels_after = session.call(READ_TOOLSET, "ReadTurboMCPMaterialLabels", {"asset": ASSET})
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
