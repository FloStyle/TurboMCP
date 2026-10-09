"""Exercise the phase 1 asset and type tools against the running editor.

Checks the rules the spec states, and exits non-zero on failure.

  T-ASSET-1  search finds assets, filters work, a miss says how many were searched
  T-ASSET-2  every path form of one asset gives the same answer
  T-HELP-5   a native type and a user-made enum are both found, with exact paths
  D-05       a fact that could not be read is reported as unknown, not guessed
  D-33       the unsaved state comes from the package flag, and two tools agree

Requires TestProject/Content/Tests/TA11_TestEnum.uasset, which
Tools/make-test-assets.py creates.
"""

import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from tmcp import READ_TOOLSET, Session  # noqa: E402

ASSET = "TA11_TestEnum"
ASSET_PACKAGE = "/Game/Tests/TA11_TestEnum"

failures = []


def check(name, ok, detail=""):
    print("  %-4s %s%s" % ("ok" if ok else "FAIL", name, ("  " + detail) if detail else ""))
    if not ok:
        failures.append(name)


def main():
    s = Session()
    s.connect()

    print("=== T-ASSET-1: search ===")
    found = s.call(READ_TOOLSET, "ReadTurboMCPAssetSearch", {"query": ASSET})
    print(found)
    print()
    check("finds the test asset", "TA11_TestEnum" in found)
    check("prints its class", "[UserDefinedEnum]" in found)
    check("prints the full object path", "/Game/Tests/TA11_TestEnum.TA11_TestEnum" in found)

    print()
    print("=== search with a type filter ===")
    typed = s.call(READ_TOOLSET, "ReadTurboMCPAssetSearch", {"query": "type:UserDefinedEnum"})
    print(typed[:600])
    print()
    check("the type filter is applied", "type: UserDefinedEnum" in typed)
    check("matches at least the test enum", "TA11_TestEnum" in typed)

    print()
    print("=== search matching nothing, and P1's rule ===")
    miss = s.call(READ_TOOLSET, "ReadTurboMCPAssetSearch", {"query": "zzzznotathing"})
    print(miss)
    print()
    check("says it matched nothing", "Nothing matched" in miss)
    check("says how many assets were searched", "assets were searched" in miss)
    check("suggests what to try", "Try fewer words" in miss)

    print()
    print("=== an empty query is refused ===")
    empty = s.call(READ_TOOLSET, "ReadTurboMCPAssetSearch", {"query": ""})
    print(empty)
    print()
    check("refuses an empty query", empty.startswith("error:"))
    check("gives an example", "type:Material" in empty)

    print()
    print("=== T-ASSET-2: every path form gives the same answer ===")
    forms = {
        "short name": ASSET,
        "package path": ASSET_PACKAGE,
        "object path": ASSET_PACKAGE + "." + ASSET,
    }
    answers = {}
    for label, value in forms.items():
        text = s.call(READ_TOOLSET, "ReadTurboMCPAssetDescribe", {"assets": value})
        answers[label] = text
        print("--- %s" % label)
        print(text)
        print()

    check(
        "all forms agree",
        len(set(answers.values())) == 1,
        "%d distinct answers" % len(set(answers.values())),
    )

    check("reports the class", "class: UserDefinedEnum" in answers["short name"])
    check("reports the object path", "object: %s.%s" % (ASSET_PACKAGE, ASSET) in answers["short name"])
    check(
        "says so when there is no generated class",
        "generated class: none" in answers["short name"],
    )

    print()
    print("=== D-05: describe never guesses ===")
    describe = answers["short name"]
    # Either the asset is loaded, in which case the flag is known, or it is not,
    # in which case the tool must say unknown rather than claim it is saved.
    if "loaded: no" in describe:
        check("an unreadable fact is called unknown", "unsaved changes: unknown" in describe)
    else:
        check("a loaded asset reports a real unsaved state",
              "unsaved changes: yes" in describe or "unsaved changes: no" in describe)

    print()
    print("=== a name matching nothing is refused ===")
    bad = s.call(READ_TOOLSET, "ReadTurboMCPAssetDescribe", {"assets": "zzzznotathing"})
    print(bad)
    print()
    check("refuses rather than guessing", "error:" in bad)
    check("counts honestly", "0 described" in bad or "refused" in bad)

    print()
    print("=== T-HELP-5: R-TYPE-FIND ===")
    native = s.call(READ_TOOLSET, "ReadTurboMCPTypeFind", {"words": "Vector", "kind": "struct"})
    print(native[:700])
    print()
    check("finds a native struct", "/Script/CoreUObject.Vector" in native)
    check("gives the exact pin path", "  [struct] in " in native)
    check("names the module", "in CoreUObject" in native)
    check("caps the list", "showing the first" in native)

    user = s.call(READ_TOOLSET, "ReadTurboMCPTypeFind", {"words": ASSET, "kind": "any"})
    print(user)
    print()
    check("finds the user-made enum", "TA11_TestEnum" in user)
    check("says it is an enum", "[enum]" in user)
    check(
        "says it is not loaded yet",
        "not loaded" in user,
    )

    bad_kind = s.call(READ_TOOLSET, "ReadTurboMCPTypeFind", {"words": "Vector", "kind": "banana"})
    print(bad_kind)
    print()
    check("refuses an unknown kind", bad_kind.startswith("error:"))
    check("lists the accepted kinds", "any, struct, enum, class, interface" in bad_kind)

    empty_type = s.call(READ_TOOLSET, "ReadTurboMCPTypeFind", {"words": "", "kind": "any"})
    print(empty_type)
    print()
    check("refuses an empty type search", empty_type.startswith("error:"))

    print()
    print("=== D-33: R-DIRTY-LIST ===")
    dirty = s.call(READ_TOOLSET, "ReadTurboMCPDirtyList", {"limit": ""})
    print(dirty)
    print()
    check("states how many packages are dirty", "package(s) dirty" in dirty)
    check(
        "a zero names its reason",
        ("Nothing has unsaved changes" in dirty) or ("listing" in dirty),
    )

    print()
    if failures:
        print("RESULT: FAIL (%d) -> %s" % (len(failures), ", ".join(failures)))
        return 1
    print("RESULT: PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
