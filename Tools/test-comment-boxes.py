"""Exercise P-COMMENT and E-COMMENT against the running editor.

  T-LAY-12  comment boxes

THE CHECKS THAT MATTER
----------------------
1. DELETE NEVER TOUCHES THE NODES. That is the one thing this tool says twice, because getting it
   wrong destroys a caller's work. The suite counts the NODES the box held before and after, by
   their own keys, rather than trusting the result's sentence.

2. MEMBERSHIP IS COMPUTED, NOT READ FROM THE ENGINE'S CACHE, and that is not an implementation
   detail — it is the difference between working and not. `GetNodesUnderComment` returns a set that
   `SGraphNodeComment::HandleSelection` fills from RENDERED widgets, so an asset read without its
   graph editor open reports NOTHING inside every box. Measured before the fix: a box drawn around
   two nodes reported `holds 0 node(s)`. The suite therefore checks the counts, which would have
   caught it.

3. THE BOX KEY IDENTIFIES. The first version left every box created in one session with the key
   `000000`, because `AddNode` is what makes the GUID and the key was read back straight afterwards.
   Two boxes were made and BOTH reported `000000`, so `set` would have hit whichever came first. The
   suite makes two boxes and requires different keys.

The suite removes its own boxes and leaves the nodes alone, so it is re-runnable.
"""

import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from tmcp import EDIT_TOOLSET, PREVIEW_TOOLSET, READ_TOOLSET, Session  # noqa: E402

ASSET = "BP_NodeTest"
GRAPH = "EventGraph"

# Two nodes that exist in the built test graph. Taken from the graph itself at run time rather than
# pinned, because a pinned key would break when the graph is rebuilt — which is the order-dependent
# mistake this project has had to fix five times.
BOX_TEXT = "T-LAY-12 probe"

# A SENTINEL ABOVE THE KEY SPACE. A box key is six hex digits, as the create path prints
# (`refer to it later as 5acf55`), so a longer key cannot name a box the tool made. It is
# passed to `delete` to make the tool NAME THE REAL KEYS in its refusal - see `box_keys`.
MISSING_KEY = "ffffffff"

failures = []


def check(name, ok, detail=""):
    print("  %-4s %s%s" % ("ok" if ok else "FAIL", name, ("  " + detail) if detail else ""))
    if not ok:
        failures.append(name)


def node_keys(session, want=2):
    """Node keys from the graph, taken from the ids the reader prints."""
    text = session.call(READ_TOOLSET, "ReadTurboMCPGraphLoad",
                        {"asset": ASSET, "graphName": GRAPH, "detail": "full",
                         "format": "", "tokenBudget": ""})
    keys = []
    for match in re.finditer(r"%s:(\w{6})\s*;" % re.escape(GRAPH), text):
        key = match.group(1)
        if key not in keys:
            keys.append(key)
    return keys[:want], text


def comment(session, action, **kwargs):
    args = {"asset": ASSET, "graphName": GRAPH, "action": action,
            "nodes": kwargs.get("nodes", ""), "box": kwargs.get("box", ""),
            "text": kwargs.get("text", ""), "color": kwargs.get("color", ""),
            "size": kwargs.get("size", "")}
    return session.call(PREVIEW_TOOLSET, "PreviewTurboMCPComment", args)


def comment_edit(session, action, **kwargs):
    args = {"asset": ASSET, "graphName": GRAPH, "action": action,
            "nodes": kwargs.get("nodes", ""), "box": kwargs.get("box", ""),
            "text": kwargs.get("text", ""), "color": kwargs.get("color", ""),
            "size": kwargs.get("size", "")}
    return session.call(EDIT_TOOLSET, "EditTurboMCPComment", args)


def box_keys(session):
    """Every box key in the graph.

    *** THE FIRST VERSION OF THIS FUNCTION COULD NEVER RETURN A KEY, AND ITS CHECK
    THEREFORE COULD NEVER FAIL. *** It asked the `fit` action for its listing and
    matched `^\\s+(\\w{6})\\s+"`. MEASURED: `fit` DOES NOT PRINT KEYS AT ALL - its
    whole answer is three lines,

        preview: fit in /Game/Tests/BP_NodeTest.BP_NodeTest : EventGraph
        1 comment box(es) in the graph
        would fit 1 box(es) to their contents, leaving 40 units of padding

    so the regex matched nothing, `box_keys` returned `[]` ALWAYS, and the suite's
    closing check `len(final) <= 0` was TRUE ON EVERY RUN INCLUDING ONE THAT LEFT
    THREE BOXES BEHIND. That is the vacuity this project has now recorded five times:
    a check over a relationship inherits the vacuity of its input.

    THE ENUMERATION COMES FROM THE TOOL'S OWN REFUSAL, WHICH IS THE ONLY PATH THAT
    NAMES KEYS. A `delete` for a key that does not exist answers with the real ones:

        error: 'ffffff' is not a comment box in EventGraph. Its boxes are: 5acf55

    The sentinel above the key space is used so this cannot collide with a real key,
    and the keys are parsed out of that one sentence.
    """
    missing = comment(session, "delete", box=MISSING_KEY)
    listed = re.search(r"Its boxes are: ([^.]*)", missing)
    if listed is None:
        return [], missing
    return re.findall(r"\b([0-9a-f]{6})\b", listed.group(1)), missing


def restore_fixture(session):
    """GIVE THE FIXTURE BACK ON THE WAY IN, AND ASSERT THAT IT WAS GIVEN BACK.

    `Spec/WORKING_PLAN.md` ROW 0.2. This suite removes every box it makes at the end,
    so it leaves the graph as it found it - ON A RUN THAT REACHES THE END. A run
    killed part-way does not, and `BP_NodeTest:EventGraph` IS BUILT WITH NO COMMENT
    BOXES AT ALL (`Spec/06_ACCEPTANCE.md` TA-1, measured: `0 comment box(es) in the
    graph`). So a leftover box is distinguishable from the built state, and the
    restore is "this graph holds no box".

    WHY HERE RATHER THAN AT THE END: restoring at the end cannot help the run that
    crashed, which is the run that caused the problem, and it kills the server the
    suite is talking to (`WinError 10061`, measured twice on the graph row). This is
    IN-PROCESS and needs no editor restart.

    RETURNS (ok, detail). `ok` IS FALSE ONLY WHEN A LEFTOVER WAS FOUND AND THE DELETE
    DID NOT TAKE - a claim about THIS SUITE's own behaviour, which is the only kind of
    claim a shared fixture permits. A graph that was already box-free is NOT a failure.
    """
    keys, _ = box_keys(session)
    if not keys:
        return True, "the graph already held no comment box, so nothing was given back"

    print("  the graph held %d comment box(es) this suite did not make: %s"
          % (len(keys), ", ".join(keys)))
    print("  a previous run was killed before its own cleanup, so they are removed now.")

    left = []
    for key in keys:
        result = comment_edit(session, "delete", box=key)
        print("  remove %s: %s" % (key, result.strip().splitlines()[0]))
        if not result.startswith("changed:"):
            left.append(key)

    # READ BACK THROUGH THE ENUMERATION, because the delete's own sentence is not evidence.
    remaining, _ = box_keys(session)
    if remaining or left:
        return False, ("the restore left %s behind" % ", ".join(left or remaining))

    # The deletes only change the in-memory asset. A repair that is not written back is
    # undone by the next editor restart, which is what makes residue durable - measured
    # on the override fixture, where a saved leftover survived a stop and a start.
    saved = session.call(EDIT_TOOLSET, "EditTurboMCPSave", {"assets": ASSET})
    if "1 saved" not in saved and "0 failed" not in saved:
        return False, "the repaired fixture could not be saved: %s" % saved.strip()[:120]

    return True, "the graph was given back with no comment box left in it"


def main():
    session = Session()
    session.connect()

    # EVERY BOX THIS SUITE MAKES IS REMEMBERED BY KEY, AND REMOVED BY KEY AT THE END.
    #
    # The first version tried to recognise its own work by the TEXT it had set, and that failed: a
    # `set` that only resized a box left the text alone, so the cleanup's marker test did not match
    # and FIVE boxes were left behind. They then made `test-comments` go red, correctly — that suite
    # reads the boxes on the same asset and expects none. Cleaning up by a marker is a guess about
    # what was made; a key is the thing itself.
    created = []

    # THE FIXTURE IS GIVEN BACK BEFORE ANYTHING IS TESTED, AND THE GIVING BACK IS
    # ASSERTED - `Spec/WORKING_PLAN.md` row 0.2. See `restore_fixture` for why this
    # belongs on the way in and why the closing cleanup alone could never have fixed a
    # run that was killed part-way.
    restored, detail = restore_fixture(session)
    check("the fixture was restored on the way in", restored, detail)
    if not restored:
        print()
        print("RESULT: FAIL (the graph could not be given back, so nothing below "
              "could be tested on a known fixture)")
        return 1

    keys, text = node_keys(session)
    print("=== the graph has node keys: %s ===" % ", ".join(keys))
    if len(keys) < 2:
        print("RESULT: FAIL (the test graph has fewer than two nodes to box)")
        return 1

    print()
    print("=== create boxes a set of nodes, and the KEY IDENTIFIES it ===")
    out = comment_edit(session, "create", nodes=", ".join(keys), text=BOX_TEXT,
                       color="#3A6EA5")
    print(out)
    print()
    check("it opens with changed:", out.startswith("changed:"), out[:70])
    check("IT HOLDS THE NODES IT WAS PUT AROUND",
          "holds %d node(s)" % len(keys) in out,
          "measured before the membership fix: holds 0 node(s)")

    key_match = re.search(r"refer to it later as (\w{6})", out)
    first_key = key_match.group(1) if key_match else None
    if first_key:
        created.append(first_key)
    check("the box reports a key to refer to it by", first_key is not None, out[:200])
    # THE KEY MUST IDENTIFY. `000000` for every box would look like a key and be useless.
    check("the key is not the all-zero placeholder",
          first_key is not None and first_key != "000000", str(first_key))

    # A SECOND box, to prove two boxes get DIFFERENT keys.
    second = comment_edit(session, "create", nodes=keys[0], text=BOX_TEXT + " two")
    second_match = re.search(r"refer to it later as (\w{6})", second)
    second_key = second_match.group(1) if second_match else None
    if second_key:
        created.append(second_key)
    check("a second box gets its own key",
          second_key is not None and second_key != first_key,
          "%s vs %s" % (first_key, second_key))
    check("the second box holds its one node", "holds 1 node(s)" in second, second[:200])

    print()
    print("=== set reports the OLD value beside the new one ===")
    renamed = comment_edit(session, "set", box=first_key, text="T-LAY-12 renamed")
    print(renamed)
    print()
    check("it opens with changed:", renamed.startswith("changed:"), renamed[:70])
    check("it shows the old text and the new one",
          BOX_TEXT in renamed and "T-LAY-12 renamed" in renamed, renamed[:200])
    # THE SIZE IS NOT EXPECTED HERE, because this call did not ask for one. The first version
    # asserted `size:` on a TEXT-only set and went red on a tool that was right; a check must assert
    # what the call asked for, not what a different call would produce.

    same = comment_edit(session, "set", box=first_key, text="T-LAY-12 renamed")
    check("a set that changes nothing opens with unchanged:",
          same.startswith("unchanged:"), same[:70])

    resized = comment_edit(session, "set", box=first_key, size="600x400")
    check("a size-only set takes the size", "600x400" in resized, resized[:200])
    check("a size-only set reports the OLD size beside the new one",
          "size:" in resized and "->" in resized, resized[:200])

    print()
    print("=== fit resizes to the contents, and only to the contents ===")
    fitted = comment_edit(session, "fit")
    print(fitted)
    print()
    check("it opens with changed: or unchanged:",
          fitted.startswith("changed:") or fitted.startswith("unchanged:"), fitted[:70])
    check("it reports each box it touched or none", "->" in fitted or "unchanged" in fitted)
    # THE EMPTY-BOX SENTENCE IS REQUIRED ONLY WHEN THERE ARE EMPTY BOXES. The first version demanded
    # it unconditionally and went red on a graph where every box was full — a tool reporting
    # `fitted 3` and nothing else was right, and the CHECK was wrong.
    if "LEFT ALONE" in fitted or "hold NOTHING" in fitted:
        check("when an empty box is left alone, the reason is given",
              "no size to fit" in fitted or "hold nothing" in fitted.lower(),
              fitted[-260:])
    else:
        check("with no empty box, none is claimed",
              "LEFT ALONE" not in fitted, fitted[-200:])

    print()
    print("=== a box holding nothing is listed, and fit does not shrink it away ===")
    empty = comment_edit(session, "create", nodes="", text="")
    check("create with no nodes is refused",
          empty.startswith("error:") and "needs nodes" in empty, empty[:120])

    # An empty box is made by boxing a node then moving the box off it, which is what a caller does
    # by accident. The node is one that exists, so the box can be created at all.
    empty_box = comment_edit(session, "create", nodes=keys[0], text="T-LAY-12 empty",
                             size="40x40")
    empty_key = re.search(r"refer to it later as (\w{6})", empty_box)
    empty_key = empty_key.group(1) if empty_key else None
    if empty_key:
        created.append(empty_key)
    if empty_key:
        # The box is 40x40 at the node's corner, so it cannot contain it.
        listing = comment(session, "fit")
        check("a box too small to hold its node is reported as holding nothing",
              "hold NOTHING" in listing or "no box is empty" in listing, listing[-300:])
        if "hold NOTHING" in listing:
            check("the empty box is named by its key", empty_key in listing, listing[-200:])
        comment_edit(session, "delete", box=empty_key)

    print()
    print("=== DELETE removes the box and NEVER the nodes ===")
    # THE CHECK THAT MATTERS. The nodes are counted BY KEY before and after, which is the only way
    # to show the deletion did not take them.
    before_keys, before_text = node_keys(session, want=20)
    check("the nodes are in the graph before the delete",
          all(k in before_text for k in keys), ", ".join(keys))

    deleted = comment_edit(session, "delete", box=first_key)
    print(deleted)
    print()
    check("it opens with changed:", deleted.startswith("changed:"), deleted[:70])
    check("it says the nodes are untouched", "untouched" in deleted, deleted[:200])
    check("it re-counts the boxes, so the claim is checked",
          "boxes in the graph now" in deleted, deleted[:240])

    after_keys, after_text = node_keys(session, want=20)
    check("THE NODES SURVIVED THE DELETE",
          all(k in after_text for k in keys),
          "missing: %s" % [k for k in keys if k not in after_text])
    check("and no node was added either",
          len([k for k in after_keys if k not in before_keys]) == 0,
          "appeared: %s" % [k for k in after_keys if k not in before_keys])

    print()
    print("=== a key that names nothing is refused BY THE PREVIEW too ===")
    # The refusal lives in Plan, not Apply, so the preview refuses the same input the edit would
    # (`D-02`). A preview that offered a plan the edit refuses is the disagreement that rule exists
    # to prevent, and this is the fourth capability where the suite checks the PREVIEW's refusal.
    missing = comment(session, "delete", box="ffffff")
    print(missing)
    print()
    check("the preview refuses it", missing.startswith("error:"), missing[:80])
    check("it lists the boxes that do exist", "Its boxes are:" in missing, missing[:200])

    print()
    print("=== the request is validated ===")
    for label, kwargs, expect in (
        ("no action", {"action": ""}, "no action given"),
        ("an unknown action", {"action": "banana"}, "is not an action"),
        ("no asset", {"asset": "", "action": "fit"}, "no asset given"),
        ("a bad colour", {"action": "create", "nodes": keys[0], "color": "#ZZZ"},
         "not a colour"),
        ("a bad size", {"action": "create", "nodes": keys[0], "size": "big"},
         "not a size"),
        ("a bad graph", {"graphName": "NoSuchGraph", "action": "fit"}, "is not a graph"),
        ("set with nothing to set", {"action": "set", "box": second_key or "aaa",
                                     "text": ""}, "nothing to change"),
    ):
        args = {"asset": ASSET, "graphName": GRAPH, "action": kwargs.get("action", "fit"),
                "nodes": kwargs.get("nodes", ""), "box": kwargs.get("box", ""),
                "text": "", "color": kwargs.get("color", ""),
                "size": kwargs.get("size", "")}
        if "asset" in kwargs:
            args["asset"] = kwargs["asset"]
        if "graphName" in kwargs:
            args["graphName"] = kwargs["graphName"]
        result = session.call(PREVIEW_TOOLSET, "PreviewTurboMCPComment", args)
        check("refuses %s" % label,
              result.startswith("error:") and expect in result, result[:120])

    print()
    print("=== the suite removes EVERY box it made, by key ===")
    for key in created:
        result = comment_edit(session, "delete", box=key)
        if result.startswith("changed:"):
            print("    removed %s" % key)
    final, final_listing = box_keys(session)
    print("    boxes left in the graph: %d" % len(final))
    check("no box this suite made is left behind",
          len(final) <= 0,
          "still there: %s" % ", ".join(final))

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
