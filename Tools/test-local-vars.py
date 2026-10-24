"""Check LocalVars graph selection, empty-graph responses and input refusals.

This suite does not create a repeated-calculation candidate and does not certify
promotion or the temporary candidate-refusal safeguard. See todo/67f00521.md.
Run through the supervisor, which restores the protected fixture files.
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from tmcp import EDIT_TOOLSET, PREVIEW_TOOLSET, Session  # noqa: E402

ASSET = "BP_NodeTest"

# A function graph this suite owns, so the asset is left as it was found. Its name is chosen so a
# leftover from an unfinished run is recognisable.
OWNED = "TLAY13Probe"

failures = []


def check(name, ok, detail=""):
    print("  %-4s %s%s" % ("ok" if ok else "FAIL", name, ("  " + detail) if detail else ""))
    if not ok:
        failures.append(name)


def preview(session, graph):
    return session.call(PREVIEW_TOOLSET, "PreviewTurboMCPLocalVars",
                        {"asset": ASSET, "graphName": graph})


def main():
    session = Session()
    session.connect()

    print("=== a graph that CANNOT hold locals is refused with that reason ===")
    # THE CHECK THAT SEPARATES A REAL ANSWER FROM A LAZY ONE. The event graph has no local variables,
    # and the difference between "this graph cannot" and "nothing found" is a reason a caller acts on.
    event = preview(session, "EventGraph")
    print(event)
    print()
    check("the event graph is refused", event.startswith("error:"), event[:80])
    check("IT NAMES THE REASON: an event graph has no local variables",
          "event graph has no local variables" in event, event[:240])
    check("it lists the functions that DO exist, so a caller can pick one",
          "Its functions are:" in event, event[:240])
    # A refusal that named no alternative would leave the caller stuck, which P7 forbids.
    check("the refusal is not just 'wrong graph'",
          "not a function graph" in event, event[:160])

    print()
    print("=== a function graph is ACCEPTED, which is the other half of the same check ===")
    # Inspect before creating: a name collision is a refusal, not proof of reuse.
    existing = preview(session, OWNED)
    if existing.startswith("preview:") and OWNED in existing.splitlines()[0]:
        check("the probe function exists and is selected explicitly", True)
    else:
        made = session.call(EDIT_TOOLSET, "EditTurboMCPFunctionAdd",
                            {"asset": ASSET, "name": OWNED,
                             "inputs": "", "outputs": ""})
        print(made)
        check("a function graph was made to test with", made.startswith("changed:"), made[:240])
        if not made.startswith("changed:"):
            print("RESULT: FAIL (fixture setup)")
            return 1

    accepted = preview(session, OWNED)
    print(accepted)
    print()
    check("a function graph is NOT refused", not accepted.startswith("error:"), accepted[:80])
    check("it names the graph it examined", OWNED in accepted, accepted[:160])

    print()
    print("=== a function with no candidate says WHY, rather than being blank ===")
    # `P1`. "Nothing to promote" is an answer; an empty result is not.
    check("it reports a count of candidates",
          "0 value(s) used more than once" in accepted, accepted[:200])
    check("it explains that a value read once gains nothing",
          "read once gains nothing" in accepted, accepted[:300])
    check("it does not call the absence a failure",
          "error:" not in accepted, accepted[:120])

    print()
    print("=== the preview changes nothing ===")
    before = session.call("TurboMCP.TurboMCPReadToolset", "ReadTurboMCPBlueprintLoad",
                          {"assets": ASSET, "detail": "manifest", "graphFilter": "",
                           "format": "", "tokenBudget": ""})
    preview(session, OWNED)
    after = session.call("TurboMCP.TurboMCPReadToolset", "ReadTurboMCPBlueprintLoad",
                         {"assets": ASSET, "detail": "manifest", "graphFilter": "",
                          "format": "", "tokenBudget": ""})
    check("the graph is unchanged by a preview", before == after,
          "the manifest differs after a preview")

    print()
    print("=== the edit refuses with unchanged:, not a change of zero ===")
    edited = session.call(EDIT_TOOLSET, "EditTurboMCPLocalVars",
                          {"asset": ASSET, "graphName": OWNED})
    print(edited)
    print()
    # `unchanged:` rather than `changed: promoted 0`, because a caller must be able to tell a no-op
    # from a change (`P4`).
    check("it opens with unchanged:", edited.startswith("unchanged:"), edited[:70])
    check("it says nothing was promoted", "was promoted" in edited, edited[:200])
    check("it points at the preview for the reasons", "preview" in edited.lower(),
          edited[:240])

    print()
    print("=== the same refusal comes from the EDIT, not only the preview ===")
    # `D-02`: a preview that offered a plan the edit refuses is the disagreement the rule prevents. So
    # the event graph must be refused by BOTH.
    edited_event = session.call(EDIT_TOOLSET, "EditTurboMCPLocalVars",
                                {"asset": ASSET, "graphName": "EventGraph"})
    check("the edit refuses the event graph too",
          edited_event.startswith("error:"), edited_event[:80])
    check("with the SAME reason the preview gave",
          "event graph has no local variables" in edited_event, edited_event[:240])

    print()
    print("=== the request is validated ===")
    for label, args, expect in (
        ("no asset", {"asset": "", "graphName": OWNED}, "no asset given"),
        ("a non-Blueprint", {"asset": "TA18_ColourSuite", "graphName": OWNED},
         "not a Blueprint"),
        ("a graph that does not exist", {"asset": ASSET, "graphName": "NoSuchFunction"},
         "is not a function graph"),
    ):
        result = session.call(PREVIEW_TOOLSET, "PreviewTurboMCPLocalVars", args)
        check("refuses %s" % label,
              result.startswith("error:") and expect in result, result[:130])

    print()
    print("=== the default graph is a function, and it is named in the answer ===")
    defaulted = preview(session, "")
    print(defaulted.splitlines()[0] if defaulted else "")
    check("an omitted graph name still names a graph", ":" in defaulted, defaulted[:160])
    check("it did not fall back to the event graph",
          "event graph has no local variables" not in defaulted, defaulted[:200])

    print()
    print("=== the suite LEAVES the probe function behind, on purpose ===")
    # Removing it would leave the package holding the name, so the next run could not add it back —
    # which is exactly how the first version broke its own re-runnability. Leaving an empty function
    # is the smaller cost, and it is named so it is recognisable.
    listed = preview(session, OWNED)
    check("the probe function is still there for the next run",
          not listed.startswith("error:"), listed[:120])
    print("    %s stays in %s; it is empty and named for this suite."
          % (OWNED, ASSET))

    print()
    print(session.call(EDIT_TOOLSET, "EditTurboMCPSave",
                       {"assets": ASSET})[:60].replace("\n", " "))

    print()
    if failures:
        print("RESULT: FAIL (%d) -> %s" % (len(failures), ", ".join(failures)))
        return 1
    print("RESULT: PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
