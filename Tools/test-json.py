"""Check that R-BP-LOAD's JSON carries the same facts as its text.

`04_FORMATS.md` warns that most bugs in this project came from two tools disagreeing about
one format. The JSON is a second WRITER over the same accessors, and this suite is what
keeps the two from drifting: it reads the same asset both ways and requires the facts to
agree.

  same facts    every graph, variable and component named in the text is in the JSON
  same counts   the counts agree with the lists, not just with each other
  P2            a level that is not implemented is refused as parseable JSON
  P1            an empty list is an empty array, not a missing key

WHAT THIS SUITE IS CAREFUL ABOUT
--------------------------------
Checking that the JSON parses proves almost nothing: `{}` parses. The checks below compare
the JSON against the TEXT for the same asset and the same level, which is the only way to
catch one rendering losing a fact the other has.

The suite would pass vacuously if the asset had no graphs, variables or components, so it
first asserts the asset actually has some.
"""

import json
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from tmcp import READ_TOOLSET, Session  # noqa: E402

ASSET = "/Game/Tests/BP_NodeTest"
WIDGET = "/Game/Tests/WBP_NodeTest.WBP_NodeTest"

failures = []


def check(name, ok, detail=""):
    print("  %-4s %s%s" % ("ok" if ok else "FAIL", name, ("  " + detail) if detail else ""))
    if not ok:
        failures.append(name)


def load(session, asset, detail="manifest", fmt="", budget="", graph=""):
    return session.call(READ_TOOLSET, "ReadTurboMCPBlueprintLoad", {
        "assets": asset, "detail": detail, "graphFilter": graph,
        "format": fmt, "tokenBudget": budget,
    })


def json_body(text):
    """The first complete JSON document in the tool's output, or None.

    `json.loads` is NOT used on the whole text, because the tool appends its own footer
    after the document — a count of refused assets, and the next step. That footer is not
    JSON, so a strict parse of everything found after the first brace fails on a document
    that is perfectly well formed. `raw_decode` reads one document and reports where it
    ended, which is exactly right when the surrounding text belongs to the wrapper.

    The first version used `json.loads` and reported the REFUSAL path as unparseable for
    that reason alone. The refusal was valid JSON the whole time.
    """
    start = text.find("{")
    if start < 0:
        return None
    try:
        document, _end = json.JSONDecoder().raw_decode(text[start:])
        return document
    except ValueError:
        return None


def text_names(text, section):
    """The names under a `section: count` line in the text output.

    Two earlier versions of this were wrong, and both failed in ways that looked like the
    tool disagreeing with itself rather than the reader being wrong:

    1. It treated the text after the header's colon as a comma-separated name list, so
       `events: 3` contributed the name `3`.
    2. It ended the section only at a line ending in a colon, so it ran through
       `functions: 1` and the whole relation layer.

    INDENTATION is what actually delimits these sections, and it is unambiguous: a header
    sits at some indent, its items are deeper, and the next header is at the same indent or
    shallower. The header's own indent is recorded and any later line at that depth or less
    ends the section.
    """
    lines = text.splitlines()
    names = []
    header_indent = None
    for line in lines:
        if not line.strip():
            continue
        indent = len(line) - len(line.lstrip())
        stripped = line.strip()

        if header_indent is None:
            if stripped.startswith(section + ":"):
                header_indent = indent
            continue

        # A line at the header's own depth or shallower starts something else.
        if indent <= header_indent:
            break

        if stripped.startswith("("):
            # A cap line such as "(12 more not shown)", not a name.
            continue
        names.append(stripped)
    return names


def text_bodies(text):
    """The graph bodies in the text output, as {graph name: body text}.

    The text writer separates them with `; ---- Name ----` markers, which are part of the
    text format's own comment syntax and therefore the reliable delimiter.

    THE DOCUMENT'S OWN TRAILER IS NOT A BODY. After the last graph the text writer appends
    a count of items it could not print in full. That trailer has no marker, so a first
    version of this attach it to the LAST graph and reported a divergence on that graph
    alone — 318 characters against 145 — when the bodies themselves were identical. The
    trailer is cut off at its own header and returned separately, and the JSON's
    `body_notes` is the equivalent of it.
    """
    bodies = {}
    current = None
    collected = []
    trailer = []
    for line in text.splitlines():
        stripped = line.strip()

        # The trailer header ends the LAST graph as well as starting the trailer, so the
        # graph in progress is stored before the trailer takes over. A first version
        # switched straight to `__trailer__` and lost that graph entirely.
        if re.match(r"^;\s*\d+\s+item\(s\) could not be printed in full", stripped):
            if current is not None and current != "__trailer__":
                bodies[current] = "\n".join(collected)
            current = "__trailer__"
            # The header line itself is not a note, so it is not collected.
            continue

        match = re.match(r"^;\s*----\s*(.+?)\s*----\s*$", stripped)
        if match:
            if current is not None and current != "__trailer__":
                bodies[current] = "\n".join(collected)
            current = match.group(1)
            collected = []
            continue

        if current == "__trailer__":
            if stripped:
                trailer.append(stripped)
        elif current is not None:
            collected.append(line)

    if current is not None and current != "__trailer__":
        bodies[current] = "\n".join(collected)
    return {name: body.strip() for name, body in bodies.items()}, trailer


def main():
    session = Session()
    session.connect()

    text = load(session, ASSET)
    js = load(session, ASSET, fmt="json")

    print("=== the JSON parses and says it succeeded ===")
    doc = json_body(js)
    check("the document parses", doc is not None)
    if doc is None:
        print(js[:600])
        print("\nRESULT: FAIL (the JSON could not be parsed)")
        return 1
    check("it reports success", doc.get("ok") is True)
    check("it names the detail level", doc.get("detail") == "manifest")

    print()
    print("=== the asset really has facts, so the checks below cannot pass vacuously ===")
    counts = doc.get("counts", {})
    print("    counts: %s" % counts)
    check("the asset has graphs", counts.get("graphs", 0) > 0,
          "graphs=%s" % counts.get("graphs"))
    check("the asset has variables", counts.get("variables", 0) > 0,
          "variables=%s" % counts.get("variables"))
    check("the asset has components", counts.get("components", 0) > 0,
          "components=%s" % counts.get("components"))

    print()
    print("=== the counts agree with the lists they summarise ===")
    # A count that disagrees with its own list is worse than no count.
    check("the graph count matches the graph list",
          counts.get("graphs") == len(doc.get("graphs", [])),
          "%s vs %d" % (counts.get("graphs"), len(doc.get("graphs", []))))
    check("the variable count matches the variable list",
          counts.get("variables") == len(doc.get("variables", [])),
          "%s vs %d" % (counts.get("variables"), len(doc.get("variables", []))))
    check("the component count matches the component list",
          counts.get("components") == len(doc.get("components", [])),
          "%s vs %d" % (counts.get("components"), len(doc.get("components", []))))

    print()
    print("=== the SAME FACTS IN BOTH RENDERINGS: this is the check that matters ===")
    for section, key in (("graphs", "graphs"), ("variables", "variables"),
                         ("components", "components")):
        in_text = text_names(text, section)
        if key == "graphs":
            in_json = [g.get("name") for g in doc.get("graphs", [])]
        else:
            in_json = list(doc.get(key, []))

        print("    %-11s text=%s" % (section, in_text))
        print("    %-11s json=%s" % ("", in_json))

        missing = [name for name in in_text if name not in in_json]
        extra = [name for name in in_json if name not in in_text]
        check("%s: nothing in the text is missing from the JSON" % section,
              not missing, "missing %s" % missing)
        check("%s: nothing in the JSON is absent from the text" % section,
              not extra, "extra %s" % extra)

    print()
    print("=== the asset path and class agree ===")
    check("the asset path is in the JSON", doc.get("asset") == ASSET.replace("/Game", "/Game")
          or doc.get("asset", "").endswith("BP_NodeTest.BP_NodeTest"),
          str(doc.get("asset")))
    check("the asset path appears in the text too",
          "BP_NodeTest.BP_NodeTest" in text)
    check("the class is Blueprint", doc.get("class") == "Blueprint", str(doc.get("class")))
    check("the parent is named and absolute",
          str(doc.get("parent", "")).startswith("/Script/"))

    print()
    print("=== an empty list is an empty array, not a missing key (P1) ===")
    # BP_NodeTest implements no interfaces, so the key must be present and empty. A
    # caller testing a missing key and a caller testing an empty array behave very
    # differently, and only one of them is right.
    check("interfaces is present", "interfaces" in doc)
    check("interfaces is an empty list", doc.get("interfaces") == [],
          str(doc.get("interfaces")))
    check("the text says 'none' for the same fact", "interfaces: none" in text)

    print()
    print("=== signatures: the JSON carries the same facts as the text ===")
    sig_text = load(session, ASSET, detail="signatures")
    sig_json = load(session, ASSET, detail="signatures", fmt="json")
    sig = json_body(sig_json)
    check("the signatures document parses", sig is not None)

    if sig is not None:
        check("it reports the level it answered", sig.get("detail") == "signatures",
              str(sig.get("detail")))

        # Every variable named at manifest must have a detail entry, and the type and flag
        # text must appear in the text rendering. A type or a flag list spelled two ways is
        # the drift this suite exists to catch.
        details = {one.get("name"): one for one in sig.get("variable_details", [])}
        for variable in doc.get("variables", []):
            check("%s has a detail entry" % variable, variable in details)
        check("the details are not empty, so the checks below mean something",
              len(details) > 0, "%d entries" % len(details))
        for name, one in details.items():
            check("%s: the type text appears in the text rendering" % name,
                  one.get("type", "") in sig_text, "%r" % one.get("type"))
            check("%s: the flags appear in the text rendering" % name,
                  one.get("flags", "") in sig_text, "%r" % one.get("flags"))

        # Events and functions: the JSON list must match the text list IN ORDER. The text
        # may cap a long list, so the JSON is required to be a prefix rather than equal.
        for label, key in (("events", "events"), ("functions", "functions")):
            in_text = text_names(sig_text, label)
            in_json = list(sig.get(key, []))
            print("    %-9s text=%s" % (label, in_text))
            print("    %-9s json=%s" % ("", in_json))
            check("%s: the JSON matches the text in order" % label,
                  in_json == in_text[:len(in_json)],
                  "json=%s text=%s" % (in_json, in_text))
            check("%s: the JSON invents nothing" % label,
                  all(one in in_text for one in in_json))

        # Components gain their class at this level, in both renderings.
        comp = {one.get("name"): one for one in sig.get("component_details", [])}
        for name, one in comp.items():
            check("%s: the component class appears in the text" % name,
                  one.get("class", "") in sig_text, "%r" % one.get("class"))

    print()
    print("=== the budget is a REFUSAL, not a cut, so the document always parses ===")
    # A truncated JSON document does not parse at all, so cutting is worse than refusing:
    # a caller holding an unparseable document cannot even read why it failed.
    tight = load(session, ASSET, detail="signatures", fmt="json", budget="500")
    tight_doc = json_body(tight)
    print(tight[:330])
    print()
    check("the refusal parses", tight_doc is not None)
    if tight_doc is not None:
        check("it reports failure", tight_doc.get("ok") is False)
        check("it names the real size", "characters" in str(tight_doc.get("error", "")))
        check("it names the budget", "500" in str(tight_doc.get("error", "")))
        check("it says what to do instead",
              "lower detail level" in str(tight_doc.get("error", "")))
        check("it carries no partial document", "variable_details" not in tight_doc)

    # The same call with room must still succeed, so the refusal is about the budget and
    # not about the level.
    roomy = load(session, ASSET, detail="signatures", fmt="json", budget="100000")
    check("the same call with room succeeds", (json_body(roomy) or {}).get("ok") is True)

    print()
    print("=== bodies: each graph carries the SAME body text as the text rendering ===")
    # This is where the "bodies stay strings" decision pays off, and it is also where a
    # comparison is worth the most: a body is the largest thing either rendering produces,
    # so a divergence here would be the easiest to miss by eye and the most damaging.
    bodies_text = load(session, ASSET, detail="bodies")
    bodies_json = load(session, ASSET, detail="bodies", fmt="json")
    bodies_doc = json_body(bodies_json)
    check("the bodies document parses", bodies_doc is not None)

    if bodies_doc is not None:
        check("it reports the level it answered", bodies_doc.get("detail") == "bodies",
              str(bodies_doc.get("detail")))

        from_text, trailer = text_bodies(bodies_text)
        from_json = {g.get("name"): (g.get("body") or "").strip()
                     for g in bodies_doc.get("graphs", [])}
        notes_json = bodies_doc.get("body_notes", [])

        print("    graphs in the text: %s" % sorted(from_text))
        print("    graphs in the JSON: %s" % sorted(from_json))
        check("both renderings cover the same graphs",
              sorted(from_text) == sorted(from_json))
        check("the bodies are not empty, so the comparison means something",
              all(len(body) > 0 for body in from_json.values())
              and len(from_json) > 0,
              "%d graph(s)" % len(from_json))

        # EVERY body must be identical, character for character. Not a prefix, not a
        # summary: the two writers call the same graph text writer, so anything short of
        # equality is a defect worth failing on.
        for name in sorted(set(from_text) & set(from_json)):
            same = from_text[name] == from_json[name]
            check("%s: the body text is identical in both renderings" % name, same,
                  "" if same else "text %d chars, json %d chars"
                  % (len(from_text[name]), len(from_json[name])))

        # Spot-check a fact inside a body, so the equality above is not the only evidence.
        check("a body carries a node ID",
              any("BP_NodeTest:EventGraph:" in body or "BP_NodeTest:" in body
                  for body in from_json.values()))
        check("a body carries an S-expression",
              any("(event " in body for body in from_json.values()))

        # The notes trailer is the document's, not a body's, and the JSON carries the same
        # facts under `body_notes`. Both must agree on whether anything was unprintable.
        print("    notes in the text trailer: %d" % len(trailer))
        print("    notes in the JSON        : %d" % len(notes_json))
        check("both renderings agree that something was unprintable, or that nothing was",
              (len(trailer) > 0) == (len(notes_json) > 0),
              "text=%d json=%d" % (len(trailer), len(notes_json)))
        # A note names the node it is about, and that node ID must appear in the other
        # rendering too, or the two are describing different problems.
        for note in notes_json:
            check("the note names a node the text also mentions",
                  note.split(":")[0] in bodies_text or note[:40] in bodies_text,
                  note[:70])

    print()
    print("=== bodies respects the graph filter, in both renderings ===")
    filtered_text = load(session, ASSET, detail="bodies", graph="Construction")
    filtered_json = load(session, ASSET, detail="bodies", fmt="json",
                         graph="Construction")
    filtered_doc = json_body(filtered_json)
    print(filtered_json[:260])
    print()
    if filtered_doc is not None:
        names = [g.get("name") for g in filtered_doc.get("graphs", [])]
        check("the filter is applied in JSON", len(names) < 3, str(names))
        check("the filtered graph is the one asked for",
              all("Construction" in n for n in names), str(names))
        check("the text is filtered the same way",
              "EventGraph" not in filtered_text.split("bodies:")[-1][:400])

    print()
    print("=== a filter that matches nothing is refused, not answered empty ===")
    nomatch = load(session, ASSET, detail="bodies", fmt="json", graph="zzzznotagraph")
    nomatch_doc = json_body(nomatch)
    print(nomatch[:300])
    print()
    check("the refusal parses", nomatch_doc is not None)
    if nomatch_doc is not None:
        check("it reports failure", nomatch_doc.get("ok") is False)
        check("it lists the real graphs", "EventGraph" in str(nomatch_doc.get("error", "")))
        check("it returns no graphs", not nomatch_doc.get("graphs"))

    print()
    print("=== full: the editing details are present, and the bodies still match ===")
    # `full` is forwarded to the graph text writer, which already knows what it adds: each
    # node's C++ class, its linked outputs, the long-form node IDs, and a knot named on a
    # link instead of being followed through it. The first three are checked here.
    full_text = load(session, ASSET, detail="full")
    full_json = load(session, ASSET, detail="full", fmt="json")
    full_doc = json_body(full_json)
    check("the full document parses", full_doc is not None)

    if full_doc is not None:
        check("it reports the level it answered", full_doc.get("detail") == "full",
              str(full_doc.get("detail")))

        full_bodies = {g.get("name"): (g.get("body") or "").strip()
                       for g in full_doc.get("graphs", [])}
        full_from_text, _full_trailer = text_bodies(full_text)

        # The same EXACT equality as at bodies, because the same writer produces both.
        check("both renderings cover the same graphs",
              sorted(full_from_text) == sorted(full_bodies),
              "text=%s json=%s" % (sorted(full_from_text), sorted(full_bodies)))
        for name in sorted(set(full_from_text) & set(full_bodies)):
            same = full_from_text[name] == full_bodies[name]
            check("%s: the full body is identical in both renderings" % name, same,
                  "" if same else "text %d chars, json %d chars"
                  % (len(full_from_text[name]), len(full_bodies[name])))

        joined = "\n".join(full_bodies.values())
        bodies_joined = "\n".join(
            (g.get("body") or "") for g in bodies_doc.get("graphs", []))

        # The details the spec names that this asset can actually exercise.
        check("full adds each node's C++ class", "; class " in joined)
        check("bodies does NOT carry the C++ class", "; class " not in bodies_joined)
        check("full adds the linked outputs", "; out " in joined)
        check("full uses the long-form node IDs",
              "/Game/Tests/BP_NodeTest.BP_NodeTest:EventGraph:" in joined)
        check("bodies uses the short-form node IDs",
              "/Game/Tests/BP_NodeTest.BP_NodeTest:EventGraph:" not in bodies_joined)
        check("full is larger than bodies", len(joined) > len(bodies_joined),
              "%d vs %d" % (len(joined), len(bodies_joined)))

        # NOT VERIFIED HERE, and stated rather than assumed: the spec also asks full to name
        # a knot on a link, and BP_NodeTest has no knot, so that branch is exercised by no
        # test in this repository. Raised as Q-9.
        print("        note: the knot branch of 'full' is NOT exercised: this asset has"
              " no knot. See QUESTIONS Q-9.")

    print()
    print("=== an unknown detail NAME is refused by the reader, listing the four ===")
    # The JSON writer's own guard covers an out-of-range INT, which no caller can reach
    # through the tool because a detail arrives as a name. What a caller CAN reach is a
    # misspelled name, so that is what is checked here.
    bogus = load(session, ASSET, detail="banana", fmt="json")
    print(bogus[:220])
    print()
    check("the refusal names every accepted level",
          all(word in bogus for word in ("manifest", "signatures", "bodies", "full")))

    print()
    print("=== the text rendering still works, so JSON did not replace it ===")
    check("the text output is not JSON", text.lstrip().splitlines()[0].startswith("read:"))
    check("the text still lists the graphs", "EventGraph" in text)

    print()
    print("=== a Widget Blueprint renders as JSON too ===")
    wjson = load(session, WIDGET, fmt="json")
    wdoc = json_body(wjson)
    check("the widget's JSON parses", wdoc is not None)
    if wdoc is not None:
        check("it has a graph", len(wdoc.get("graphs", [])) > 0)

    print()
    if failures:
        print("RESULT: FAIL (%d) -> %s" % (len(failures), ", ".join(failures)))
        return 1
    print("RESULT: PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
