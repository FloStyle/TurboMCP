"""T-ASSET-3: Check direct dependency size provenance and bounded output.

Live registry checks use TA01_HandLaid. A synthetic native test covers
unknown sizes, truncation, default caps and partial-query output.
"""

import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from tmcp import READ_TOOLSET, Session  # noqa: E402

ASSET = "TA01_HandLaid"

failures = []
checks_run = [0]


def check(name, ok, detail=""):
    checks_run[0] += 1
    print("  %-4s %s%s" % ("ok" if ok else "FAIL", name, ("  " + detail) if detail and not ok else ""))
    if not ok:
        failures.append(name)


def section(text, heading):
    """Return lines between a section heading and the next known heading."""
    lines = text.splitlines()
    known = ("OWN SIZE", "DIRECT DEPENDENCIES", "SIZES THAT COULD NOT BE READ", "LARGEST CONTRIBUTORS")
    start = None
    for index, line in enumerate(lines):
        if heading in line:
            start = index + 1
            break
    if start is None:
        return []
    out = []
    for line in lines[start:]:
        if any(line.startswith(k) or (" " + k) in line for k in known):
            break
        out.append(line)
    return out


def main():
    session = Session()
    session.connect()
    import runpy
    from pathlib import Path
    support = runpy.run_path(str(Path(__file__).with_name("test-widget-roundtrip.py")))
    session.call(support["AUTO_TOOLSET"], "DiscoverTests", {})
    name = "TurboMCP.Assets.CostOutput"
    raw = session.call(support["AUTO_TOOLSET"], "RunTests", {"testNames": [name]})
    print(raw)
    entry, report = support["parse_run_tests_report"](raw, name)
    check("native output controls have no warnings", not entry["warnings"], repr(entry["warnings"]))


    text = session.call(READ_TOOLSET, "ReadTurboMCPAssetCost", {"asset": ASSET, "cap": 5})
    print(text[:900])
    print()
    print()

    check("it names the asset it answered about", ASSET in text, text[:200])

    check("it splits the answer by provenance, naming HARD references",
          re.search(r"hard references\s*:\s*\d+ package", text) is not None, text[:400])
    check("it names SOFT references as their own kind",
          re.search(r"soft references\s*:\s*\d+ package", text) is not None, text[:400])
    check("it names EDITOR-ONLY references as their own kind",
          re.search(r"editor-only references\s*:\s*\d+ package", text) is not None, text[:400])

    check("the split states how many dependency entries were examined",
          re.search(r"(\d+) dependency entr", text) is not None, text[:400])

    categories = re.findall(r"(?:hard|soft|editor-only) references\s*:\s*(\d+) package", text)
    examined = re.search(r"(\d+) dependency entr", text)
    check("category counts include every returned dependency entry",
          len(categories) == 3 and examined is not None and sum(map(int, categories)) == int(examined.group(1)), text[:700])
    check("size provenance and overlap are explicit",
          "recorded package disk sizes" in text and "overlap" in text and "not a recursive" in text, text[:1000])

    own = section(text, "OWN SIZE")
    own_text = "\n".join(own)
    own_ok = ("byte(s)" in text.split("DIRECT DEPENDENCIES")[0]) or ("unknown" in own_text.lower())
    check("the asset's own size is either a size or an explicit unknown",
          own_ok, own_text[:300])

    if re.search(r"OWN SIZE: unknown|\| size UNKNOWN \|", text):
        check("an unknown size comes WITH A REASON rather than alone",
              re.search(r"unknown\s*-\s*\S|size UNKNOWN \| .+", text) is not None, text[:400])
        check("it says explicitly that this is NOT ZERO",
              "unknown, not zero" in text or "AN ABSENT MEASUREMENT, NOT ZERO COST" in text, text[:600])
    else:
        print("    no unknown size on this asset, so the unknown path is NOT exercised here")
        print("    An unresolved-asset refusal below does not cover missing size measurements.")

    refused = session.call(READ_TOOLSET, "ReadTurboMCPAssetCost", {"asset": "NoSuchAsset_ForTheUnknownPath", "cap": 5})
    print(refused[:400])
    print()
    check("an asset that does not resolve is REFUSED rather than reported as costing nothing",
          refused.startswith("error:") or "did not resolve" in refused, refused[:300])
    zero_claim = re.search(r"OWN SIZE:\s*0 byte", refused)
    check("and the refusal does NOT claim a zero size, which would read as free",
          zero_claim is None, refused[:300])

    largest = section(text, "LARGEST CONTRIBUTORS")
    largest_text = "\n".join(largest)
    check("a LARGEST CONTRIBUTORS section exists", len(largest) > 0, text[-600:])

    rows = [l for l in largest if "|" in l]
    check("the contributors are capped at the requested number", len(rows) <= 5,
          "%d row(s) for a cap of 5" % len(rows))
    if rows:
        check("every contributor row carries WHY it is referenced, not only its size",
              all(("HARD" in r or "SOFT" in r or "EDITOR-ONLY" in r or "UNKNOWN" in r) for r in rows),
              "\n".join(rows[:4]))
        check("and every contributor row carries a size or an explicit unknown",
              all(("byte(s)" in r or "UNKNOWN" in r) for r in rows), "\n".join(rows[:4]))
    else:
        print("    no contributors on this asset, so the per-row assertions are NOT exercised")
        print("    THE TOOL SAYS SO IN ITS OWN OUTPUT rather than leaving the section empty and silent")

    print()
    if failures:
        print("RESULT: FAIL (%d) -> %s" % (len(failures), ", ".join(failures)))
        return 1
    print("RESULT: PASS (%d check(s) run)" % checks_run[0])
    return 0


if __name__ == "__main__":
    sys.exit(main())
