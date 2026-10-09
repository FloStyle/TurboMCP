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


def load(session, asset, detail="manifest", fmt=""):
    return session.call(READ_TOOLSET, "ReadTurboMCPBlueprintLoad", {
        "assets": asset, "detail": detail, "graphFilter": "",
        "format": fmt, "tokenBudget": "",
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
    """The indented names under a `section:` line in the text output."""
    lines = text.splitlines()
    names = []
    inside = False
    for line in lines:
        stripped = line.strip()
        if stripped.startswith(section + ":"):
            inside = True
            rest = stripped[len(section) + 1:].strip()
            if rest and rest != "none":
                names.extend(part.strip() for part in rest.split(","))
            continue
        if inside:
            if stripped.endswith(":") or stripped.startswith("counts:"):
                break
            if stripped.startswith("("):
                # A cap line, not a name.
                continue
            if stripped:
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
    print("=== a level that is not implemented is refused as PARSEABLE JSON (P2) ===")
    sig = load(session, ASSET, detail="signatures", fmt="json")
    sig_doc = json_body(sig)
    print(sig[:400])
    print()
    check("the refusal still parses", sig_doc is not None)
    if sig_doc is not None:
        check("it reports failure", sig_doc.get("ok") is False)
        check("it names the level asked for",
              "signatures" in str(sig_doc.get("error", "")))
        check("it does not return a partial document",
              "counts" not in sig_doc and "graphs" not in sig_doc)

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
