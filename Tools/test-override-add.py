# ACCEPTANCE ROW THIS SUITE CLAIMS TO PROVE: T-STRUCT-4
"""Exercise the P/E-OVERRIDE-ADD pair against the running editor.

  T-STRUCT-4  create an override on purpose
  P4          the Edit opens with `changed:` or `unchanged:`
  D-02        the Preview and the Edit share one code path

THE TWO CHECKS THAT MATTER
--------------------------
1. The Preview must print the exact SIGNATURE. That is the whole reason this capability exists
   rather than a thin wrapper, and it is checked against the real parameter list.
2. The PREVIEW must warn that a name is already overridden, not only the Edit. The engine's own
   AddFunctionOverride does not check, so it would quietly add a SECOND graph; putting the check in
   Plan rather than Apply is what makes the preview refuse too, and a preview that showed a plan the
   edit would not carry out is the disagreement D-02 exists to prevent.

The count of overridden functions is read back through R-OVERRIDES, a different tool, because the
Edit's own report of what it made is not evidence that the Blueprint agrees.
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from tmcp import EDIT_TOOLSET, PREVIEW_TOOLSET, READ_TOOLSET, Session  # noqa: E402

ASSET = "BP_NodeTest"

# A DEDICATED EVENT, AND THE TEST PUTS IT BACK.
#
# Earlier versions picked any event that happened to be free. That made the suite CONSUME A FINITE
# RESOURCE: each run overrode one more event out of the 23 BP_NodeTest can override, and after four
# runs there was nothing left and the suite failed on its own work. It also made the shared test
# asset drift further from what build-test-graph.py builds, which is what broke two OTHER suites
# that had pinned counts.
#
# So this suite owns one event. It overrides it, checks the behaviour, and then removes its own
# override so the asset is left as it was found. That is what makes it re-runnable and keeps the
# shared asset stable for every other suite.
#
# `TAKEN` is the override BP_NodeTest was BUILT with, so it is stable and is the already-overridden
# case.
OWNED = "ReceiveActorOnClicked"
TAKEN = "ReceiveActorBeginOverlap"

failures = []


def check(name, ok, detail=""):
    print("  %-4s %s%s" % ("ok" if ok else "FAIL", name, ("  " + detail) if detail else ""))
    if not ok:
        failures.append(name)


def override_counts(session):
    text = session.call(READ_TOOLSET, "ReadTurboMCPOverrides",
                        {"asset": ASSET, "filter": "", "include": "", "limit": ""})
    for line in text.splitlines():
        if line.strip().startswith("counts:"):
            # `counts: 23 overridable, 5 already overridden here, 18 still available`
            numbers = [int(tok) for tok in line.replace(",", " ").split() if tok.isdigit()]
            if len(numbers) >= 2:
                return numbers[1], text
    return None, text


def reads_as_overridden(session, name):
    """True when R-OVERRIDES reports `name` as overridden in THIS Blueprint.

    THE WITNESS IS A DIFFERENT TOOL FROM THE ONE THAT DID THE REMOVAL, which is this
    project's rule: a tool's own report of what it removed is not evidence.

    PARSED BY WALKING THE LINES, AND THAT SHAPE WAS MEASURED RATHER THAN ASSUMED. The
    first version split the answer on a blank line and looked for the name's own
    block. MEASURED: THE ANSWER HAS NO BLANK LINES AT ALL - it is 24 entries in a row,
    each a name line followed by indented `state:`/`from:` lines:

        ReceiveActorOnClicked : (FKey ButtonPressed)  [event]
            state: overridden here
            from: /Script/Engine.Actor
        ReceiveActorOnInputTouchBegin : (...)  [event]
            state: available

    so `split("\\n\\n")` returned ONE block, the name never matched a block start, and
    the check answered "not overridden" EVERY TIME - A CHECK THAT COULD NEVER FAIL in
    the direction that mattered. It reported the dirty fixture as clean and the suite
    then failed six checks later, on the tool rather than on the fixture.

    A NAME LINE IS ONE THAT DOES NOT START WITH WHITESPACE, so the indentation is what
    separates an entry from its own state - not a separator character this would have
    to guess.
    """
    text = session.call(READ_TOOLSET, "ReadTurboMCPOverrides",
                        {"asset": ASSET, "filter": "", "include": "", "limit": ""})
    in_block = False
    for line in text.splitlines():
        if line[:1] not in (" ", "\t", "") and not line.startswith("read:"):
            # A new entry. Only an exact name match counts, so `ReceiveActorOnClicked`
            # is never confused with a longer neighbour that starts the same way.
            in_block = line.split(":", 1)[0].strip() == name
            continue
        if in_block and line.strip() == "state: overridden here":
            return True
    return False


def restore_fixture(session):
    """GIVE THE FIXTURE BACK ON THE WAY IN, AND ASSERT THAT IT WAS GIVEN BACK.

    `Spec/WORKING_PLAN.md` ROW 0.2. This suite owns ONE event and removes its own
    override at the end, so it leaves the asset as it found it - ON A RUN THAT
    REACHES THE END. A run killed part-way does not, and the residue is DURABLE:
    MEASURED, an override added here and saved with E-SAVE was still present after
    the editor was stopped and started again (5 overridden -> 6).

    WHAT THE FIRST VERSION DID, AND WHY THAT IS NOT A RESTORE. It DETECTED the
    residue and then FAILED, with `RESULT: FAIL (ReceiveActorOnClicked was left
    overridden by an earlier run)`. That is a suite that REPORTS a dirty fixture, not
    one that RECOVERS from it - so a single interrupted run made every later run red
    until a human cleaned up by hand. MEASURED TWICE on the dirty fixture before this
    change, both runs failing on the same line.

    WHY THE RESTORE BELONGS HERE RATHER THAN AT THE END. Restoring at the end cannot
    help the run that crashed, which is the run that caused the problem; and it kills
    the server the suite is talking to (`WinError 10061`, measured twice on the graph
    row). This is IN-PROCESS and needs no editor restart: `E-OVERRIDE-REMOVE` is the
    suite's own tool and it works against the live editor.

    RETURNS (ok, detail). `ok` IS FALSE ONLY WHEN THE RESTORE WAS NEEDED AND DID NOT
    TAKE - a claim about THIS SUITE's own behaviour, which is the only kind of claim
    a shared fixture permits. A fixture that was already clean is NOT a failure.
    """
    if not reads_as_overridden(session, OWNED):
        return True, "%s was already free, so nothing had to be given back" % OWNED

    print("  %s is ALREADY overridden. A previous run was killed before its own" % OWNED)
    print("  cleanup, so this suite gives the fixture back BEFORE testing anything.")

    removal = session.call(EDIT_TOOLSET, "EditTurboMCPOverrideRemove",
                           {"asset": ASSET, "functionOrEvent": OWNED})
    print("  remove: %s" % removal.strip().splitlines()[0])

    # READ BACK THROUGH R-OVERRIDES, because the removal's own sentence is not evidence.
    if reads_as_overridden(session, OWNED):
        return False, ("E-OVERRIDE-REMOVE ran and R-OVERRIDES still reports %s as "
                       "overridden here" % OWNED)

    # The removal only changes the in-memory asset, and the residue this repairs was
    # SAVED - so the repair is saved too. A repair that is not written back is undone
    # by the next editor restart, which is precisely how the residue became durable.
    saved = session.call(EDIT_TOOLSET, "EditTurboMCPSave", {"assets": ASSET})
    if "1 saved" not in saved and "0 failed" not in saved:
        return False, "the repaired fixture could not be saved: %s" % saved.strip()[:120]

    return True, "%s was overridden by an earlier run and has been given back" % OWNED


def main():
    session = Session()
    session.connect()

    # The suite owns one event and gives it back. THE GIVING BACK HAPPENS HERE, ON THE
    # WAY IN, AND IT IS ASSERTED - see `restore_fixture` for why the first version's
    # detect-and-fail was a report rather than a restore.
    FREE = OWNED
    restored, detail = restore_fixture(session)
    check("the fixture was restored on the way in", restored, detail)
    if not restored:
        print()
        print("RESULT: FAIL (%s could not be given back, so nothing below could be "
              "tested on a known fixture)" % FREE)
        return 1

    print("=== the suite owns %s, and will remove its override at the end ===" % FREE)

    print()
    print("=== the preview prints the EXACT signature ===")
    out = session.call(PREVIEW_TOOLSET, "PreviewTurboMCPOverrideAdd",
                       {"asset": ASSET, "functionOrEvent": FREE})
    print(out)
    print()
    check("it names the asset", ASSET in out)
    check("it says the kind", "kind: event" in out or "kind: function" in out)
    check("it prints a signature", "signature:" in out)
    check("the signature carries the name", FREE in out)

    print()
    print("=== a name this Blueprint cannot override is refused with BOTH lists ===")
    unknown = session.call(PREVIEW_TOOLSET, "PreviewTurboMCPOverrideAdd",
                           {"asset": ASSET, "functionOrEvent": "NoSuchFunctionAtAll"})
    print(unknown[:300])
    print()
    check("it is refused", unknown.startswith("error:"))
    check("it lists events still available", "Events still available:" in unknown)
    check("it lists functions still available", "Functions still available:" in unknown)

    # THE LISTED NAMES ARE CHECKED AGAINST THE TOOL, not against a pinned list. An earlier
    # version named a specific event here, and that event was consumed by an earlier run of this
    # very suite, so the check failed on the suite's own work rather than on the tool.
    #
    # What the tool promises is that a name it lists as available IS available. So the first name
    # out of the refusal is taken and previewed: it must be accepted.
    import re as _re  # noqa: PLC0415
    listed = _re.search(r"Events still available: ([^.]+)\.", unknown)
    check("the event list is not empty",
          listed is not None and listed.group(1).strip() != "(none)", unknown[:160])
    if listed is not None:
        first = listed.group(1).split(",")[0].strip()
        print("    the refusal offers: %s" % first)
        accepted = session.call(PREVIEW_TOOLSET, "PreviewTurboMCPOverrideAdd",
                                {"asset": ASSET, "functionOrEvent": first})
        check("a name the refusal calls available is accepted by the preview",
              accepted.startswith("preview:"), accepted[:120])

    print()
    print("=== THE PREVIEW refuses an already-overridden name, not just the Edit ===")
    # The engine's AddFunctionOverride does not check, so this refusal lives in Plan. If it
    # lived in Apply the preview would show a plan the edit would refuse.
    taken_preview = session.call(PREVIEW_TOOLSET, "PreviewTurboMCPOverrideAdd",
                                 {"asset": ASSET, "functionOrEvent": TAKEN})
    print(taken_preview)
    print()
    check("the preview says it is already overridden",
          "ALREADY OVERRIDDEN" in taken_preview, taken_preview[:120])
    check("it says WHERE", "EventGraph" in taken_preview)
    check("it says the edit will refuse", "will REFUSE" in taken_preview)

    print()
    print("=== the edit creates it, and reads back what the editor now reports ===")
    before, _ = override_counts(session)
    print("    overridden before: %s" % before)

    edit = session.call(EDIT_TOOLSET, "EditTurboMCPOverrideAdd",
                        {"asset": ASSET, "functionOrEvent": FREE})
    print(edit)
    print()
    check("the edit opens with changed:", edit.startswith("changed:"), edit[:70])
    check("it names what it created", "created:" in edit)
    check("it names the graph or node", "EventGraph" in edit)
    check("it prints the signature again", "signature:" in edit)
    check("the editor reports it as implemented",
          "the editor now reports it as implemented: yes" in edit,
          "the read-back is the evidence, not the return value")

    print()
    print("=== a SECOND call is refused and creates nothing ===")
    second = session.call(EDIT_TOOLSET, "EditTurboMCPOverrideAdd",
                          {"asset": ASSET, "functionOrEvent": FREE})
    print(second)
    print()
    check("the second call is refused", second.startswith("unchanged:"), second[:70])
    check("it says where the override already is", "EventGraph" in second)
    check("it says nothing was created", "nothing was created" in second)

    print()
    print("=== the count changed by exactly ONE, read back by a DIFFERENT tool ===")
    # R-OVERRIDES counts from the Blueprint's own graphs. A second override would have made
    # this two, which is what the refusal above prevented.
    after, text = override_counts(session)
    print("    overridden after: %s" % after)
    check("the count went up by exactly one",
          before is not None and after == before + 1,
          "before=%s after=%s" % (before, after))
    check("the reader names the new override as overridden here",
          FREE in text and "overridden here" in text.split(FREE, 1)[-1][:200],
          "R-OVERRIDES must agree")

    print()
    print("=== the request is validated before anything is created ===")
    for label, args, expect in (
        ("no asset", {"asset": "", "functionOrEvent": FREE}, "no asset given"),
        ("no function", {"asset": ASSET, "functionOrEvent": ""}, "no function or event"),
        ("a non-Blueprint", {"asset": "TA18_ColourSuite", "functionOrEvent": FREE},
         "not a Blueprint"),
    ):
        result = session.call(PREVIEW_TOOLSET, "PreviewTurboMCPOverrideAdd", args)
        check("refuses %s" % label,
              result.startswith("error:") and expect in result, result[:100])

    print()
    print("=== save, and the override survives ===")
    print(session.call(EDIT_TOOLSET, "EditTurboMCPSave",
                       {"assets": ASSET})[:60].replace("\n", " "))
    saved_count, saved_text = override_counts(session)
    check("the override is still there after the save", saved_count == after,
          "%s vs %s" % (saved_count, after))

    print()
    print("=== the suite gives the event back ===")
    # TWO REASONS, AND BOTH MATTER. The suite is re-runnable only if it leaves the asset as it
    # found it, and the shared test asset must not drift from what build-test-graph.py builds,
    # because other suites count things on it. The first version of this suite consumed one
    # override per run, ran the asset out of free events, and broke two unrelated suites that had
    # pinned counts.
    #
    # There is no E-OVERRIDE-REMOVE yet, so the graph the Edit created is removed with the
    # editor's own RemoveFunctionGraph. The graph's name is the function's name for an event
    # override in the event graph, which is what the Edit reported as `created:`.
    import re as _re2  # noqa: PLC0415
    made = _re2.search(r"created: (\S+) in graph", edit)
    if made:
        removal = session.call(EDIT_TOOLSET, "EditTurboMCPOverrideRemove",
                               {"asset": ASSET, "functionOrEvent": FREE})
        print(removal[:220])
        check("the removal reports changed: or unchanged:",
              removal.startswith("changed:") or removal.startswith("unchanged:"),
              removal[:80])
        final_count, _ = override_counts(session)
        check("the count is back where the suite found it",
              final_count == before,
              "found %s, left %s" % (before, final_count))
    else:
        check("the edit reported what it created, so the suite can undo it", False,
              edit[:120])

    print()
    if failures:
        print("RESULT: FAIL (%d) -> %s" % (len(failures), ", ".join(failures)))
        return 1
    print("RESULT: PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
