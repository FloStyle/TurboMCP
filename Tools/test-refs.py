"""T-BP-11: Direct saved-registry references, identities and truthful limits."""

import json
import os
from pathlib import Path
from runpy import run_path
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from tmcp import READ_TOOLSET, Session  # noqa: E402

# A project asset with no content references, and an engine widget Blueprint with
# several. Both are needed: one proves a zero is honest, the other proves the walk
# works.
NO_REFS = "/Game/Tests/BP_NodeTest"
HAS_REFS = "/AudioWidgets/AudioFader/AudioFader"

failures = []


def check(name, ok, detail=""):
    print("  %-4s %s%s" % ("ok" if ok else "FAIL", name, ("  " + detail) if detail else ""))
    if not ok:
        failures.append(name)


def main():
    session = Session()
    session.connect()

    print("=== T-BP-11: repeat one reference query from a cold session ===")
    # This must be the first reference query in the Session. The original X-1
    # failure returned a temporary zero on the first query, then real edges on a
    # later identical query after the registry finished scanning. Repeating the
    # same asset and direction makes that state transition a test failure.
    repeated_out = []
    for attempt in range(5):
        text = session.call(READ_TOOLSET, "ReadTurboMCPRefs",
                            {"asset": HAS_REFS, "direction": "out"})
        repeated_out.append(text)
        print("--- attempt %d ---" % (attempt + 1))
        print(text)

    out = repeated_out[0]
    print()
    check("reports the outgoing direction", "uses (out):" in out)
    check("finds more than zero on the first query", "uses (out): 0" not in out)
    check(
        "no repeated query changes the real result to zero",
        all("uses (out): 0" not in text for text in repeated_out),
    )
    check(
        "the same query stays identical after registry readiness",
        len(set(repeated_out)) == 1,
        "%d distinct answer(s)" % len(set(repeated_out)),
    )
    check(
        "no repeated query reports an unavailable or incomplete scan",
        all(not text.lower().startswith("error: the asset registry")
            for text in repeated_out),
    )
    # The package is /AudioWidgets/AudioFader and the asset inside it is
    # AudioFader, so the object part repeats. Checking the full object path catches
    # a regression to the package path, which is what `Package.None` used to be.
    check(
        "prints a full object path, not the package",
        "/AudioWidgets/AudioFader/AudioFader_Inner.AudioFader_Inner" in out,
    )
    check("no path ends in .None", ".None" not in out)
    check("prints the class", "[Texture2D]" in out)

    first_page = session.call(READ_TOOLSET, "ReadTurboMCPRefs",
                              {"asset": HAS_REFS, "direction": "out", "pageSize": 1})
    first_meta = json.loads(first_page.split("\n; refs-page: ")[-1])
    check("first page returns one reference", first_meta["returned"] == 1)
    if first_meta["nextCursor"]:
        second_page = session.call(READ_TOOLSET, "ReadTurboMCPRefs",
                                   {"asset": HAS_REFS, "direction": "out", "pageSize": 1,
                                    "cursor": first_meta["nextCursor"]})
        second_meta = json.loads(second_page.split("\n; refs-page: ")[-1])
        check("legacy cursor advances with stable total",
              second_meta["offset"] == 1 and second_meta["total"] == first_meta["total"])
    else:
        check("legacy nextCursor is empty only on the final page", first_meta["total"] == 1)

    hard = session.call(READ_TOOLSET, "ReadTurboMCPRefs",
                        {"asset": HAS_REFS, "direction": "out", "kinds": "hard"})
    check("legacy kind filter excludes other kinds",
          not any(kind in hard for kind in ("(soft)", "(searchable-name)", "(management)")))
    check(
        "counts what it did not list",
        "registry entries omitted" in out or "uses (out): 0" in out,
    )
    check("says nothing about the incoming direction", "used by (in)" not in out)

    check("unrequested incoming direction is not a false zero", "in=not queried" in out, out)
    incoming = session.call(READ_TOOLSET, "ReadTurboMCPRefs", {"asset": "/AudioWidgets/AudioFader/AudioFader_Inner", "direction": "in"})
    print(incoming)
    check("inverse edge points back to the known dependency owner", "/AudioWidgets/AudioFader/AudioFader.AudioFader" in incoming, incoming)
    check("unrequested outgoing direction is not a false zero", "out=not queried" in incoming, incoming)
    check("limits do not imply deletion safety", "zero is not permission to delete" in out, out)
    help_text = session.call(READ_TOOLSET, "ReadTurboMCPToolHelp", {"toolsetName": READ_TOOLSET, "toolName": "ReadTurboMCPRefs"})
    print(help_text)
    check("live help includes required arguments and saved-registry scope", "asset:" in help_text and "direction:" in help_text and "registry" in help_text, help_text)
    print()
    print("=== an asset with no content references, and P1 ===")
    none = session.call(READ_TOOLSET, "ReadTurboMCPRefs",
                        {"asset": NO_REFS, "direction": ""})
    print(none)
    print()
    check("a zero is reported, not hidden", "uses (out): 0" in none)
    check("the zero explains itself", "none." in none)
    check("it suggests the next step", "ReadTurboMCPBlueprintLoad" in none)
    check("both directions are reported", "counts:" in none)

    print()
    print("=== the registry caveat is stated ===")
    check(
        "says the registry needs a saved package",
        "Unsaved or dynamic references may be missing" in none,
    )

    print()
    print("=== P2: a bad direction is refused by name ===")
    bad = session.call(READ_TOOLSET, "ReadTurboMCPRefs",
                       {"asset": NO_REFS, "direction": "banana"})
    print(bad)
    print()
    check("refuses", bad.startswith("error:"))
    check("names every accepted value", "out, in, both" in bad)

    print()
    print("=== P5: every path form gives the same answer ===")
    # A project asset is used here, not the engine one, because the short-name
    # search covers /Game only: `AudioFader` lives in a mounted plugin and cannot
    # be reached by its own name. That limitation has its own check below.
    forms = {
        "short name": "BP_NodeTest",
        "package path": NO_REFS,
        "object path": NO_REFS + ".BP_NodeTest",
    }
    answers = {}
    for label, value in forms.items():
        text = session.call(READ_TOOLSET, "ReadTurboMCPRefs",
                            {"asset": value, "direction": "out"})
        answers[label] = text
        print("--- %s: %s" % (label, text.splitlines()[0] if text else "(empty)"))

    # The status line carries the resolved path, which differs by form, so the
    # comparison starts after it.
    bodies = set()
    for text in answers.values():
        lines = text.splitlines()
        bodies.add("\n".join(lines[1:]) if len(lines) > 1 else text)

    check(
        "all project path forms give the same body",
        len(bodies) == 1,
        "%d distinct answer(s)" % len(bodies),
    )

    print()
    print("=== a short name outside /Game explains itself ===")
    outside = session.call(READ_TOOLSET, "ReadTurboMCPRefs",
                           {"asset": "AudioFader", "direction": "out"})
    print(outside)
    print()
    check("is refused", "error:" in outside)
    check(
        "says the search covers /Game only",
        "/Game only" in outside,
    )
    check("says to pass a full path", "full path" in outside)

    print()
    print("=== an unresolvable asset is refused ===")
    missing = session.call(READ_TOOLSET, "ReadTurboMCPRefs",
                           {"asset": "zzzznotathing", "direction": ""})
    print(missing)
    print()
    check("refuses rather than guessing", missing.startswith("error:") or "error:" in missing)

    print()
    # Keep the focused batch case runnable through the existing catalogued suite.
    run_batch = run_path(str(Path(__file__).with_name("test-refs-batch.py")))["main"]
    try:
        check("batch reference regression", run_batch() == 0)
    except (ValueError, KeyError, TypeError) as exc:
        check("batch reference regression", False, str(exc))

    if failures:
        print("RESULT: FAIL (%d) -> %s" % (len(failures), ", ".join(failures)))
        return 1
    print("RESULT: PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
