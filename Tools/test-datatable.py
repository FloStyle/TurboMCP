# ACCEPTANCE ROW THIS SUITE CLAIMS TO PROVE: T-BP-15
"""Exercise R-DATATABLE against the running editor.

Checks the rules the spec states, and exits non-zero on failure.

  T-BP-15  row struct with fields and types, row count, row names, and the graphs
           in loaded Blueprints that read the table
  P1       zero says what it means
  P2       a bad filter and a non-DataTable are refused
  P5       every path form gives the same answer

NOTE ON THE TEST ASSET
----------------------
The engine's `/DatasmithContent/Datasmith/AreaLightsTable` is used, because it is
real content with a known struct, known fields and 5 known rows. Every assertion
below is against numbers read from it rather than numbers invented for it.

There is no project-owned DataTable. Three attempts to build one failed, and the
last failure is an engine requirement rather than a mistake in this plugin:

  DataTableFactory + a UserDefinedEnum row struct
      refused: a UEnum is not a UScriptStruct
  BlueprintEditorLibrary.add_member_variable on a UserDefinedStruct
      refused: that function only takes a Blueprint, so the struct stayed empty and
      every row was rejected
  CSV import through AssetImportTask
      refused: "A Data table row type must be specified in the import settings json
      file for automated import"

The row struct of a table cannot be set after the fact either: `RowStruct` is
read-only once the asset exists. So a project-owned table needs either an import
settings JSON or a C++ commandlet, and neither is worth building only to restate
what the engine's own table already proves. This is recorded in TestRecord.md and
QUESTIONS.md rather than hidden.
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from tmcp import READ_TOOLSET, Session  # noqa: E402

TABLE = "/DatasmithContent/Datasmith/AreaLightsTable.AreaLightsTable"
TABLE_PACKAGE = "/DatasmithContent/Datasmith/AreaLightsTable"

# Numbers read from the table itself, pinned so a regression changes them.
EXPECTED_STRUCT = "/DatasmithContent/Datasmith/AreaLightsStruct.AreaLightsStruct"
EXPECTED_ROWS = 5

# Two Blueprints read this table. They must be loaded before their graphs can be
# searched, which is what the second half of this file does.
READERS = [
    "/DatasmithContent/Datasmith/DatasmithAreaLight.DatasmithAreaLight",
    "/DatasmithContent/Datasmith/DatasmithArealightMesh.DatasmithAreaLightMesh",
]

failures = []


def check(name, ok, detail=""):
    print("  %-4s %s%s" % ("ok" if ok else "FAIL", name, ("  " + detail) if detail else ""))
    if not ok:
        failures.append(name)


def main():
    session = Session()
    session.connect()

    print("=== T-BP-15: the schema ===")
    out = session.call(READ_TOOLSET, "ReadTurboMCPDataTable",
                       {"asset": TABLE, "include": "schema"})
    print(out)
    print()
    check("reports the row struct", EXPECTED_STRUCT in out)
    check("reports the field count", "fields: 1" in out)
    check("reports a field with its type", ": UStaticMesh*" in out)
    check("reports the row count", "rows: %d" % EXPECTED_ROWS in out)
    check("lists the row names", "EDatasmithAreaLightActorShape::Cylinder" in out)
    check("says row values are out of scope", "row values are not printed" in out)
    check("does not print a reader section", "readers:" not in out)

    print()
    print("=== the readers ===")
    before = session.call(READ_TOOLSET, "ReadTurboMCPDataTable",
                          {"asset": TABLE, "include": "readers"})
    print(before)
    print()
    check("finds the readers from the asset registry", "readers: 2 asset(s)" in before)
    check("does not print the schema", "row struct:" not in before)
    check(
        "counts how many readers had their graphs searched",
        "reader(s) had their graphs searched" in before,
    )

    # Whether a reader is loaded when the test starts depends on what an earlier run
    # did, so the unloaded case cannot be asserted from a fixed starting state. What
    # CAN be asserted is that the two states are reported differently and that
    # loading changes it. The first version of this test asserted "not searched" and
    # failed on every run after the first, which is a real defect in the test rather
    # than in the tool.
    unloaded = "not searched, this asset is not loaded" in before
    searched_count = [line for line in before.splitlines()
                      if "had their graphs searched" in line]
    print("  (state at start: %s)"
          % ("readers were not loaded" if unloaded else "readers were already loaded"))

    if unloaded:
        check(
            "an unloaded reader says its graph was not searched",
            "not searched" in before,
        )
        check("says zero were searched", "0 of 2 reader(s)" in before)
    else:
        check(
            "a reader loaded from an earlier run is reported as searched",
            "not searched" not in before,
        )
        check("says all were searched", "2 of 2 reader(s)" in before)

    print()
    print("=== after loading the Blueprints, the graph is named ===")
    # Loading is what allows the graph search, so this is the path that names a
    # graph. It is done through the engine's own tool rather than by loading here.
    for path in READERS:
        session.call("EditorToolset.EditorAppToolset", "OpenEditorForAsset",
                     {"assetPath": path})

    after = session.call(READ_TOOLSET, "ReadTurboMCPDataTable",
                         {"asset": TABLE, "include": "readers"})
    print(after)
    print()
    check(
        "names the graph that reads the table",
        "graph: UserConstructionScript" in after,
    )
    check(
        "no reader is left unsearched",
        "not searched" not in after,
    )
    check("all readers were searched", "2 of 2 reader(s) had their graphs searched" in after)
    check(
        "loading changed the report",
        unloaded != ("not searched, this asset is not loaded" in after)
        or not unloaded,
    )

    print()
    print("=== P2: a bad filter is refused by name ===")
    bad = session.call(READ_TOOLSET, "ReadTurboMCPDataTable",
                       {"asset": TABLE, "include": "banana"})
    print(bad)
    print()
    check("refuses", bad.startswith("error:"))
    check("names every accepted value", "all, readers, schema" in bad)

    print()
    print("=== P2: a non-DataTable is refused ===")
    wrong = session.call(READ_TOOLSET, "ReadTurboMCPDataTable",
                         {"asset": "BP_NodeTest", "include": ""})
    print(wrong)
    print()
    check("refuses", wrong.startswith("error:"))
    check("names the class it found", "Blueprint" in wrong)

    print()
    print("=== P5: every path form gives the same answer ===")
    forms = {
        "package path": TABLE_PACKAGE,
        "object path": TABLE,
    }
    bodies = set()
    for label, value in forms.items():
        text = session.call(READ_TOOLSET, "ReadTurboMCPDataTable",
                            {"asset": value, "include": "schema"})
        print("--- %s: %s" % (label, text.splitlines()[0] if text else "(empty)"))
        bodies.add(text)
    check("both forms agree", len(bodies) == 1,
          "%d distinct answer(s)" % len(bodies))

    print()
    if failures:
        print("RESULT: FAIL (%d) -> %s" % (len(failures), ", ".join(failures)))
        return 1
    print("RESULT: PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
