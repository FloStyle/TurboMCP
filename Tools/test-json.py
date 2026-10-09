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


def load(session, asset, detail="manifest", fmt="", budget=""):
    return session.call(READ_TOOLSET, "ReadTurboMCPBlueprintLoad", {
        "assets": asset, "detail": detail, "graphFilter": "",
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
    print("=== a level that is not implemented is refused as PARSEABLE JSON (P2) ===")
    bodies = load(session, ASSET, detail="bodies", fmt="json")
    bodies_doc = json_body(bodies)
    print(bodies[:400])
    print()
    check("the refusal parses", bodies_doc is not None)
    if bodies_doc is not None:
        check("it reports failure", bodies_doc.get("ok") is False)
        check("it names the level asked for",
              "bodies" in str(bodies_doc.get("error", "")))
        check("it does not return a partial document",
              "counts" not in bodies_doc and "graphs" not in bodies_doc)

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
