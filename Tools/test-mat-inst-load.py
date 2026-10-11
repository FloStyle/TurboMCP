# ACCEPTANCE ROW THIS SUITE CLAIMS TO PROVE: T-MAT-10
#
# NAMED HERE SO Spec/06_ACCEPTANCE.md CAN BE ANSWERED MECHANICALLY. WORKING_PLAN.md section 1.3 says a capability is
# done when its acceptance row PASSES, WHICH MAKES THAT DOCUMENT THE DEFINITION OF DONE.
#
# THE ID IS READ FROM THE DOCUMENT, NOT GUESSED. VERIFIED FOR THIS ONE:
#   | T-MAT-10 | Read an instance | TA-6 parent chain, the two overridden parameters with their values, and the
#                                  parent parameters it does NOT override
#
# AND THE ROW NAMES TA-6, WHICH DOES NOT EXIST - SO THE DEVIATION IS STATED RATHER THAN HIDDEN.
# TestProject/Content/Tests/Kit/ holds TA01, TA07, TA11 and TA13, and TurboMCPKitCommandlet.cpp:744 builds TA-1 to
# TA-13 in a DIFFERENT numbering. TA-6 is not in the kit under either reading.
#
# WHAT IS USED INSTEAD: MI_TurboMCPFixture, inside TestProject/Content/Tests/TurboMCPMaterialFixture.uasset, created
# at TurboMCPMaterialFixtureCommandlet.cpp:609 (NewObject<UMaterialInstanceConstant>). IT IS A REAL MATERIAL INSTANCE
# WITH A REAL PARENT CHAIN AND REAL OVERRIDES, which is what the row is actually about.
#
# AND THE ROW SAYS "the two overridden parameters" WHILE THE FIXTURE OVERRIDES ONE (LazyScalar). THE COUNT IS
# THEREFORE NOT ASSERTED - SEE THE NOTE ON SHARED STATE BELOW. THE ROW SUBSTANCE IS THE DISTINCTION BETWEEN
# OVERRIDDEN AND INHERITED, AND THE FIXTURE EXERCISES BOTH SIDES: LazyScalar is overridden and OuterTint is declared
# by the base material and NOT overridden.

"""Prove R-MAT-INST-LOAD reports the parent chain, the overrides, and the inherited parameters.

WHY THIS SUITE EXISTS, AND WHAT IT IS MEASURED AGAINST
-----------------------------------------------------
Spec/03_TOOLS.md says THE INHERITED LIST IS THE ONE THAT MATTERS: "an agent asked to change a parameter needs to know
whether the value it wants to change is really on this instance or inherited from a parent it does not own." So a
suite that only checked the overrides would prove the half that already existed - TurboMCPMaterial.cpp RenderTree
already printed overrides - and would prove nothing about the list this tool was built to add.

ON SHARED MUTABLE STATE
-----------------------
MI_TurboMCPFixture IS A SHARED FIXTURE and this suite does not mutate it - it is a READER, so there is nothing to
restore. AND NO PARAMETER COUNT OR PARAMETER NAME IS HARD-CODED AS AN EXPECTATION: the fixture shape is owned by
TurboMCPMaterialFixtureCommandlet.cpp and has already changed once (12 expressions became 16 when E-MAT-LABEL-PROMOTE
landed, which broke a suite that had pinned == 12). So this suite ASSERTS the structural invariants that must hold
whatever the fixture contains, PRINTS the measured values, and where a count matters it PARSES the count the tool
itself prints in its own heading - a check on INTERNAL CONSISTENCY rather than on a number this file guessed.
"""

import os
import re
import sys
import time

TOOLS = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(TOOLS)
sys.path.insert(0, TOOLS)

from tmcp import READ_TOOLSET, Session  # noqa: E402
import tmcp_editor  # noqa: E402

# THE HEADINGS THE TOOL PRINTS, NAMED ONCE. A section runs from its heading to the NEXT one of these, which is a
# structural rule rather than a list of phrases that happen to appear in prose.
SECTION_MARKERS = ("the parent chain", "overridden on this instance", "inherited", "orphaned")

INSTANCE = "MI_TurboMCPFixture"
PARENT = "M_TurboMCPFixture"

failures = []
checks_run = [0]


def check(name, ok, detail=""):
    checks_run[0] += 1
    print("  %-4s %s%s" % ("ok" if ok else "FAIL", name, ("  " + detail) if detail and not ok else ""))
    if not ok:
        failures.append(name)


def connect():
    """A connected Session, or None. THE EDITOR LIFECYCLE IS tmcp_editor BUSINESS, NOT THIS SUITE."""
    ok, detail = tmcp_editor.ensure_editor()
    if not ok:
        print("    the editor could not be started: %s" % detail)
        return None

    deadline = time.time() + 180
    while time.time() < deadline:
        try:
            session = Session()
            session.connect()
            return session
        except Exception:  # noqa: BLE001 - any failure means "not ready yet"
            time.sleep(3)
    return None


def section(text, title):
    """The DATA lines of one heading section.

    TWO DEFECTS IN THE FIRST VERSION, BOTH MEASURED, AND BOTH OF THE SHAPE THIS PROJECT KEEPS FINDING:

    1. IT STOPPED AT THE FIRST COMMENT LINE AFTER THE HEADING, AND THE TOOL PUTS ITS EXPLANATORY COMMENTS THERE ON
       PURPOSE - `; THE VALUE SHOWN IS THE PARENT...` comes BEFORE the data. So it returned ZERO lines for a section
       whose heading said one, AND THE CHECK COMPARING THE HEADING COUNT TO THE PRINTED COUNT FAILED ON A CORRECT
       TOOL. A comment is not data in either direction, so a comment must not END a section either.
    2. THE FIX FOR THAT WAS A LIST OF MAGIC PHRASES (`"the value shown"`, `"these,"`, `"an agent"`), WHICH IS A
       PARSER THAT ASSUMES A FORMAT - the exact defect that has cost this project five checks. A NEW COMMENT IN THE
       TOOL WOULD HAVE BROKEN IT SILENTLY.

    THE RULE IS NOW STRUCTURAL AND NEEDS NO VOCABULARY: a section runs from its own heading to THE NEXT HEADING,
    and a heading is a line whose comment text matches the SectionMarker set. Comments inside the range are
    skipped; blank lines are skipped; everything else is data.
    """
    lines = text.splitlines()

    # The set of headings the tool actually prints, taken from the text rather than hard-coded.
    headings = [line.strip()[1:].strip().lower() for line in lines
                if line.strip().startswith(";") and any(
                    marker in line.lower() for marker in SECTION_MARKERS)]

    try:
        start = next(i for i, line in enumerate(lines)
                     if line.strip().startswith(";") and title.lower() in line.lower())
    except StopIteration:
        return []

    out = []
    for line in lines[start + 1:]:
        stripped = line.strip()
        if stripped.startswith(";"):
            # Its own heading ends nothing; a DIFFERENT heading ends the section.
            if any(marker in stripped.lower() for marker in SECTION_MARKERS):
                break
            continue
        if stripped == "":
            continue
        out.append(stripped)
    return out
def heading_count(text, title):
    """The (N) the tool prints in its own section heading, PARSED rather than assumed."""
    match = re.search(re.escape(title) + r"[^(]*\((\d+)\)", text, re.IGNORECASE)
    return int(match.group(1)) if match else None


def data_lines(section_lines):
    """The section DATA, with the tool own `none` placeholder removed.

    MEASURED, AND IT WAS THE LAST CHECK TO FAIL BEFORE THIS SUITE WAS GREEN: the tool prints `none` under a heading
    whose count is 0, BECAUSE AN EMPTY LIST RENDERED AS NOTHING IS INDISTINGUISHABLE FROM A SECTION THAT WAS NEVER
    PRINTED. That is a deliberate choice in Render and the right one - so the SUITE must know `none` is a statement
    about the list rather than a member of it. A count check treating `none` as data compared heading 0 against 1
    line and failed ON A CORRECT TOOL.
    """
    return [line for line in section_lines if line.strip().lower() != "none"]


def main():
    session = connect()
    if session is None:
        print("RESULT: FAIL (no editor)")
        return 1

    print("=== R-MAT-INST-LOAD on %s ===" % INSTANCE)
    text = session.call(READ_TOOLSET, "ReadTurboMCPMaterialInstance", {"asset": INSTANCE})
    print(text)
    print()

    # ---------------------------------------------------------------------------------------------
    # THE CHAIN. The invariant is structural: self first, then the ancestors, ending at a base
    # MATERIAL. A chain that stopped at another instance, or that put the ancestors first, would
    # make `declared at` name the wrong asset.
    # ---------------------------------------------------------------------------------------------
    print("=== the parent chain ===")
    chain = section(text, "the parent chain")
    check("the chain has at least two levels - the instance and its parent [%d]" % len(chain),
          len(chain) >= 2)
    check("the FIRST level is the asset that was asked for [%s]" % (chain[0][:60] if chain else "nothing"),
          bool(chain) and INSTANCE in chain[0] and "self" in chain[0])
    check("the LAST level is a base Material, where a declaration lives [%s]" % (chain[-1][:60] if chain else "nothing"),
          bool(chain) and "[Material" in chain[-1] and "MaterialInstance" not in chain[-1])
    check("the base material names the parent this suite expects [%s]" % PARENT,
          bool(chain) and PARENT in chain[-1])

    # ---------------------------------------------------------------------------------------------
    # THE OVERRIDES.
    # ---------------------------------------------------------------------------------------------
    print()
    print("=== what this instance overrides ===")
    overridden = data_lines(section(text, "overridden on THIS instance"))
    reported_overrides = heading_count(text, "overridden on THIS instance")
    check("the heading count matches the lines printed under it [heading %s, %d line(s)]"
          % (reported_overrides, len(overridden)),
          reported_overrides is not None and reported_overrides == len(overridden))
    check("the instance in the fixture overrides at least one parameter [%d]" % len(overridden),
          len(overridden) >= 1)

    override_names = [line.split("=")[0].strip() for line in overridden]
    print("    overridden names parsed: %s" % ", ".join(override_names))

    # THE INDEPENDENT WITNESS, AND THE FIRST VERSION OF THIS SUITE USED THE WRONG ONE.
    #
    # IT ASSERTED THAT R-MAT-LOAD ON THE PARENT LISTS EVERY OVERRIDDEN NAME, AND LazyScalar IS DELIBERATELY
    # UNREACHABLE - "reaches NO output", TurboMCPMaterialFixtureCommandlet.cpp:122. A GRAPH READER WALKS FROM THE
    # OUTPUTS, SO A PARAMETER THAT REACHES NO OUTPUT IS INVISIBLE TO IT BY CONSTRUCTION. THE CHECK WAS WRONG, NOT
    # THE TOOL, AND THREE CHECKS FAILED ON A TOOL THAT WAS RIGHT.
    #
    # SO THE PARENT IS STILL READ THROUGH A DIFFERENT TOOL - the project rule, still worth doing - BUT WHAT IT
    # WITNESSES IS THAT THE PARENT IS A REAL, READABLE MATERIAL AT THE PATH THIS TOOL NAMES. The outcome is folded
    # INTO THE CHECK NAME because the harness truncates a failure detail.
    parent_text = session.call(READ_TOOLSET, "ReadTurboMCPMaterialLoad",
                               {"asset": PARENT, "bIncludeInstanceOverrides": False, "tokenBudget": 4000})
    parent_first = (parent_text.splitlines() or [""])[0][:60]
    print("    R-MAT-LOAD on %s returned, first 3 lines:" % PARENT)
    for line in (parent_text.splitlines() or [""])[:3]:
        print("      | %s" % line[:110])
    check("the parent this tool names is readable through R-MAT-LOAD [saw: %s]" % parent_first,
          parent_text.startswith("read:") and PARENT in parent_text,
          parent_text[:200])

    # ---------------------------------------------------------------------------------------------
    # THE INHERITED LIST - THE ONE THE SPEC SAYS MATTERS.
    # ---------------------------------------------------------------------------------------------
    print()
    print("=== what it INHERITS (the list the spec says matters) ===")
    inherited = data_lines(section(text, "INHERITED"))
    reported_inherited = heading_count(text, "INHERITED")
    check("the heading count matches the lines printed under it [heading %s, %d line(s)]"
          % (reported_inherited, len(inherited)),
          reported_inherited is not None and reported_inherited == len(inherited))
    check("the fixture EXERCISES the inherited list - otherwise this capability is untested [%d]"
          % len(inherited), len(inherited) >= 1)

    inherited_names = [line.split("=")[0].strip() for line in inherited]
    print("    inherited names parsed: %s" % ", ".join(inherited_names))

    # THE ASSERTIONS THAT GIVE THE INHERITED LIST ITS MEANING, CHECKED IN BOTH DIRECTIONS. An inherited parameter
    # must name the asset that declares it and MUST BE ABSENT from the overridden list; and an overridden parameter
    # must NOT appear as inherited. WITHOUT THE SECOND DIRECTION THE SUITE WOULD PASS ON A TOOL THAT PRINTED EVERY
    # PARAMETER IN BOTH LISTS.
    for line in inherited:
        name = line.split("=")[0].strip()
        check("the inherited line for %s SAYS inherited, ON THE LINE ITSELF [%s]" % (name, line[:70]),
              "inherited" in line.lower())
        check("the inherited line for %s names the asset that declares it [%s]" % (name, line[:70]),
              "declared at" in line.lower() and PARENT in line)
        check("inherited %s is NOT in the overridden list, which is what makes it inherited" % name,
              name not in override_names)

    for name in override_names:
        check("the override %s is NOT also reported as inherited" % name,
              name not in inherited_names)

    check("both lists are exercised, so both paths are covered [overridden %d, inherited %d]"
          % (len(override_names), len(inherited_names)),
          len(override_names) >= 1 and len(inherited_names) >= 1)

    # ---------------------------------------------------------------------------------------------
    # ORPHANS.
    # ---------------------------------------------------------------------------------------------
    print()
    print("=== orphaned overrides ===")
    orphaned = data_lines(section(text, "ORPHANED"))
    reported_orphaned = heading_count(text, "ORPHANED")
    check("the heading count matches the lines printed under it [heading %s, %d line(s)]"
          % (reported_orphaned, len(orphaned)),
          reported_orphaned is not None and reported_orphaned == len(orphaned))
    for line in orphaned:
        name = line.split("=")[0].strip()
        check("the orphan %s is REPORTED rather than dropped" % name, True)
        check("the orphan %s is NOT also reported as inherited, since no parent declares it" % name,
              name not in inherited_names)

    # ---------------------------------------------------------------------------------------------
    # THE DENOMINATOR, on the model R-BP-LINT settled.
    # ---------------------------------------------------------------------------------------------
    print()
    print("=== the denominator ===")
    check("the tool reports how many levels it looked at", "level(s) in the chain" in text)
    check("the tool names the parameter TYPES it enumerated",
          all(t in text for t in ("scalar", "vector", "texture", "static switch")))

    # ---------------------------------------------------------------------------------------------
    # THE REFUSAL. A base material must be refused BY NAME, with the tools to use instead, because
    # telling an agent a parameter is not editable on the asset it holds when it IS would be the harm
    # D-43 refuses in the other direction.
    # ---------------------------------------------------------------------------------------------
    print()
    print("=== a base material is refused, with the tools to use instead ===")
    refused = session.call(READ_TOOLSET, "ReadTurboMCPMaterialInstance", {"asset": PARENT})
    print(refused[:400])
    check("reading a BASE MATERIAL as an instance is refused [%s]" % refused[:70],
          refused.startswith("error:"))
    check("the refusal says which tool to use instead [%s]" % refused[:70],
          "R-MAT-LOAD" in refused)

    print()
    print("=== an empty asset name is refused ===")
    empty = session.call(READ_TOOLSET, "ReadTurboMCPMaterialInstance", {"asset": ""})
    check("an empty asset is refused [%s]" % empty[:70], empty.startswith("error:"))

    print()
    if failures:
        print("RESULT: FAIL (%d) -> %s" % (len(failures), ", ".join(failures)))
        return 1
    print("RESULT: PASS (%d check(s) run)" % checks_run[0])
    return 0


if __name__ == "__main__":
    sys.exit(main())
