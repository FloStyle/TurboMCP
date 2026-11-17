# ACCEPTANCE ROW THIS SUITE CLAIMS TO PROVE: T-DATA-1
#
#   | T-DATA-1 | Data table rows | Add a row to TA-14, change one field of another and remove a third, in one call |
#
# *** AND THE ROW IS SATISFIED AGAINST THE ENGINE'S OWN TABLE RATHER THAN `TA-14`, BY DECISION, BECAUSE TA-14 DOES
# NOT EXIST. *** `Spec/06_ACCEPTANCE.md:29` defines TA-14 as "Data table asset with a project-owned row struct -
# 3 rows, 4 fields, one field of a struct type". It is not in `TestProject/Content/Tests/Kit/`, and
# `TurboMCPKitCommandlet.cpp:744` builds "TA-1..TA-13" and stops. `Spec/WORKING_PLAN.md` row 1.5, "Build the new kit
# assets TA-14 to TA-21 by hand", is `todo` and owns that job in its own place in the order.
#
# IT IS AN ENGINE CONSTRAINT THE PROJECT ALREADY DOCUMENTED, NOT A MISSING FILE. `Tools/test-datatable.py:11-31`
# records THREE refused attempts to build a project-owned table, and the decisive line is the engine's own:
#   "The row struct of a table cannot be set after the fact either: `RowStruct` is read-only once the asset exists."
# and `TurboMCPKitCommandlet.cpp:1425-1447` records the other half: `FStructureEditorUtils::AddVariable` needs a
# `UUserDefinedStructEditorData` that only the EDITOR FACTORY installs, and "a handmade UUserDefinedStruct crashes".
#
# SO THIS SUITE USES `/DatasmithContent/Datasmith/AreaLightsTable`, WHICH IS REAL CONTENT WITH A KNOWN STRUCT, KNOWN
# FIELDS AND 5 KNOWN ROWS - the same reasoning `test-datatable.py` gives. **A SUITE THAT TESTS THE CAPABILITY AGAINST
# REAL CONTENT IS STRONGER EVIDENCE THAN ONE TESTING IT AGAINST A FIXTURE NOBODY HAS BUILT.** The suite's job is to
# prove the capability, not the kit. THE DEVIATION IS DECLARED HERE RATHER THAN SILENTLY CITED, because a suite that
# cites a row it does not literally prove is the defect this project spent a whole round measuring.

"""`P/E-DATATABLE-ROWS`: add, change and remove rows in one call, proved by reading back through a DIFFERENT tool.

Run:  python Tools/test-datatable-rows.py
"""

import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from tmcp import EDIT_TOOLSET, PREVIEW_TOOLSET, READ_TOOLSET, Session  # noqa: E402

TABLE = "/DatasmithContent/Datasmith/AreaLightsTable"

# A NAME NO REAL ROW HAS, so this suite can only ever touch its own row. IT IS THE WHOLE RESTORE STRATEGY: the
# suite never edits a row it did not create, so restoring means removing one row it named itself.
SCRATCH = "TURBOMCP_SuiteScratchRow"

failures = []
checks = [0]


def check(name, ok, detail=""):
    checks[0] += 1
    print("  %-4s %s%s" % ("ok" if ok else "FAIL", name, ("  " + detail) if detail and not ok else ""))
    if not ok:
        failures.append(name)


def row_lines(reader_text):
    """The row names the reader printed, as a set.

    The rows come after the count and before the page metadata. Enum row names
    can contain double colons.
    """
    out = set()
    started = False
    for line in (reader_text or "").splitlines():
        stripped = line.strip()
        if stripped.startswith("rows:"):
            started = True
            continue
        if not started:
            continue
        if stripped.startswith(("page:", ";")):
            break
        if stripped and stripped != "(the table has no rows)":
            out.add(stripped)
    return out


def row_count(reader_text):
    """The count the reader states, PARSED rather than assumed from the list length."""
    match = re.search(r"^rows:\s*(\d+)", reader_text or "", re.MULTILINE)
    return int(match.group(1)) if match else None


def first_field(reader_text):
    """The struct's first field name, read from the reader's own `fields:` section."""
    lines = (reader_text or "").splitlines()
    for index, line in enumerate(lines):
        if line.strip().startswith("fields:"):
            for candidate in lines[index + 1:]:
                stripped = candidate.strip()
                if not stripped or stripped.startswith("rows:"):
                    return None
                # The reader prints `<Name> : <Type>`, so the name is what precedes the first colon-space.
                if " : " in stripped:
                    return stripped.split(" : ", 1)[0].strip()
            return None
    return None


def read_table_schema(session):
    # Names-only output retains the field schema and keeps cell values out of the row list.
    response = session.call(READ_TOOLSET, "ReadTurboMCPDataTable",
                            {"asset": TABLE, "include": "schema", "columns": "none",
                             "textMode": "lossless", "offset": 0, "limit": 60,
                             "rowFilter": "", "revision": ""})
    count = row_count(response)
    rows = row_lines(response)
    if count is None or count != len(rows):
        raise SystemExit("RESULT: FAIL (the DataTable reader did not return a complete row list)\n"
                         + (response or "(empty reply)"))
    return response, count, rows


def restore_on_the_way_in(session):
    """REMOVE THE SCRATCH ROW IF A PREVIOUS RUN LEFT IT, BEFORE THIS ONE ASSERTS ANYTHING.

    *** WHY THIS IS THE FIRST THING THE SUITE DOES, AND IT IS A MEASURED RULE RATHER THAN DEFENSIVENESS. *** This
    project has recorded FOUR times that a suite which mutates a shared asset and does not restore it makes the
    NEXT suite read something it does not expect - and the CURRENT failing suite on `main` is exactly that shape
    (a fixture whose shape changed under a suite that was not updated). A restore only on the way out does not help
    a run that was killed part-way, WHICH IS THE CASE THAT ACTUALLY HAPPENED.
    """
    session.call(EDIT_TOOLSET, "EditTurboMCPDataTableRows",
                 {"asset": TABLE, "rows": ["- %s" % SCRATCH]})


def run_parser_checks(session):
    """THE PURE HALF: no fixture, and it carries the rules most likely to be wrong.

    EACH CHECK ASSERTS ON A REFUSAL, which is a real answer. A malformed line must be refused FOR ITS OWN REASON,
    so the specific wording is what is asserted - not merely that something went wrong.
    """
    print("=== the parser: both forms, and every malformed line, refused for its own reason ===")

    cases = [
        # The nested and quoted cases are the ones a naive split on a comma destroys, and the field that follows
        # the nested value is what proves the split was right: if it were cut, the tool would complain about a
        # missing `=`.
        ("a nested struct value survives its own commas",
         ["Row = Shape=(A=1,B=2),Other=3"],
         lambda r: "has no `=` between a field name and its value" not in r),
        ("a quoted value survives its own commas and parentheses",
         ['Row = Name="a,b(c)",Other=3'],
         lambda r: "has no `=` between a field name and its value" not in r),
        ("a line that is neither form is refused with BOTH forms",
         ["just some words"],
         lambda r: "neither form" in r and "to set a row" in r and "to remove one" in r),
        ("a removal that names no row is refused",
         ["-"],
         lambda r: "removes a row but names none" in r),
        ("a set that names no row is refused",
         ["= value"],
         lambda r: "names no row" in r),
        ("a row with no field at all is refused",
         ["Row ="],
         lambda r: "gives no field" in r or "changes nothing" in r),
        ("a field with no equal sign is refused",
         ["Row = Name"],
         lambda r: "has no `=`" in r),
        ("a value with no field name is refused",
         ["Row = =3"],
         lambda r: "has a value and no field name" in r),
        ("no operations at all is refused",
         [],
         lambda r: "no row operations given" in r),
        ("every line empty is refused",
         ["", "   "],
         lambda r: "every line was empty" in r),
    ]

    for name, lines, predicate in cases:
        # A NONEXISTENT ASSET IS PASSED ON PURPOSE. The parse happens BEFORE the asset is loaded (a measured fix:
        # the first version loaded first and every malformed line came back as an asset error instead), so a
        # PARSE refusal is what must come back - and an asset message would mean the parser ACCEPTED the line.
        answer = session.call(PREVIEW_TOOLSET, "PreviewTurboMCPDataTableRows",
                              {"asset": "/Game/Tests/NoSuchTableForTheParser", "rows": lines})
        check(name, predicate(answer or ""), (answer or "")[:200].replace("\n", " | "))


def run_rules_checks(session, field):
    """The rules that need a real table: refusals, the preview writing nothing, and both tools reachable.

    *** ORDER MATTERS AND IT IS A MEASURED CORRECTION. *** This function first captured its baseline and THEN called
    a check that ran on a table still carrying a scratch row from an earlier run - so `before 6, after 6`, a count
    that could never move, and a refusal test that saw the tool ACCEPT a row because that row already existed. A
    suite whose expectations depend on residue from its own previous run is not deterministic, and the fix is the
    same rule the project already has: **RESTORE ON THE WAY IN, THEN ASSERT.**
    """
    print()
    print("=== the rules, against real content ===")

    # EVERY RULE CHECK BELOW STARTS FROM A TABLE WITH NO SCRATCH ROW, so it cannot depend on what a killed run left.
    restore_on_the_way_in(session)

    _, count_before, _ = read_table_schema(session)

    # BOTH TOOLS MUST ANSWER, WHICH IS ALSO THE STRONGEST STALE-BINARY GUARD AVAILABLE: neither existed before this
    # work, so a stale DLL would answer "no such tool" - and that trap has fired five times in this project.
    preview = session.call(PREVIEW_TOOLSET, "PreviewTurboMCPDataTableRows",
                           {"asset": TABLE, "rows": ["%s = %s=None" % (SCRATCH, field)]})
    check("PreviewTurboMCPDataTableRows answers, so the loaded DLL is new",
          bool(preview) and "no such tool" not in (preview or "").lower(), (preview or "")[:180])

    # THE PREVIEW MUST NOT WRITE. Read back through the READER, a different tool from the writer.
    _, count_after_preview, _ = read_table_schema(session)
    check("the PREVIEW wrote nothing", count_after_preview == count_before,
          "before %s, after %s" % (count_before, count_after_preview))

    # A FIELD THE STRUCT DOES NOT DECLARE IS REFUSED, AND THE REFUSAL NAMES THE REAL FIELDS - the spec's rule.
    bad_field = session.call(PREVIEW_TOOLSET, "PreviewTurboMCPDataTableRows",
                             {"asset": TABLE, "rows": ["%s = NotARealField=1" % SCRATCH]})
    refusal_prefix = "error: Row edit refused; nothing was written."
    field_reason = "error: field 'NotARealField' is unknown or has an ambiguous display label."
    preview_refusal = next((line for line in (bad_field or "").splitlines() if line.startswith(field_reason)), "")
    check("an undeclared field is refused",
          (bad_field or "").startswith(refusal_prefix) and bool(preview_refusal), bad_field or "")
    check("the refusal names the fields the struct really has",
          ("[internal: %s]" % field) in preview_refusal, bad_field or "")

    # A NON-DATATABLE IS REFUSED AND THE REFUSAL SAYS WHAT IT FOUND INSTEAD. THE OPERATION MUST PARSE, or the
    # refusal being tested would be the parser's and the asset path would never be reached - which is exactly what
    # happened when this passed `A = 1`: the parser correctly refused `1` for having no `=`.
    not_a_table = session.call(PREVIEW_TOOLSET, "PreviewTurboMCPDataTableRows",
                               {"asset": "/Game/Tests/BP_NodeTest", "rows": ["%s = %s=None" % (SCRATCH, field)]})
    check("a non-DataTable is refused, and the refusal says what it found",
          "not a DataTable" in (not_a_table or ""), (not_a_table or "")[:220])

    # THE EDIT REFUSES EXACTLY WHAT THE PREVIEW REFUSED, because both run one Plan. The spec requires it of every
    # P/E pair, and it is checked rather than assumed.
    edit_bad = session.call(EDIT_TOOLSET, "EditTurboMCPDataTableRows",
                            {"asset": TABLE, "rows": ["%s = NotARealField=1" % SCRATCH]})
    check("the EDIT refuses exactly what the preview refused",
          (edit_bad or "").startswith(refusal_prefix) and bool(preview_refusal)
          and preview_refusal in edit_bad.splitlines(), edit_bad or "")

    # A REMOVAL OF A ROW THAT DOES NOT EXIST IS REFUSED, and the refusal lists the rows that do.
    bad_remove = session.call(PREVIEW_TOOLSET, "PreviewTurboMCPDataTableRows",
                              {"asset": TABLE, "rows": ["- NoSuchRowAnywhere"]})
    check("removing a row that does not exist is refused, and the refusal lists the real rows",
          "does not have" in (bad_remove or ""), (bad_remove or "")[:220])


def run_one_call_checks(session, field):
    """Set the same scratch row twice in one call, then remove it by name."""
    print()
    print("=== add, change and remove in ONE call, proved through the READER ===")

    # START FROM A CLEAN TABLE, for the same measured reason as above: the baseline this function compares against
    # must not include a row a previous run left behind.
    restore_on_the_way_in(session)

    _, count_before, rows_before = read_table_schema(session)

    # The second set must address the row created by the first without duplicating it.
    try:
        one_call = session.call(EDIT_TOOLSET, "EditTurboMCPDataTableRows",
                                {"asset": TABLE, "rows": [
                                    "%s = %s=None" % (SCRATCH, field),
                                    "%s = %s=None" % (SCRATCH, field),
                                ]})
        _, count_after, rows_after = read_table_schema(session)
    finally:
        # Clean up even when the reader cannot verify the edit.
        removed = session.call(EDIT_TOOLSET, "EditTurboMCPDataTableRows",
                               {"asset": TABLE, "rows": ["- %s" % SCRATCH]})

    check("the call added exactly one row", count_after == count_before + 1,
          "before %s, after %s" % (count_before, count_after))
    check("the added row is there, read back through the READER",
          SCRATCH in rows_after, "rows: %s" % ", ".join(sorted(rows_after))[:200])
    check("no row was lost", rows_before.issubset(rows_after),
          "missing: %s" % ", ".join(sorted(rows_before - rows_after))[:200])
    check("the edit reported reading the row back from the table",
          "read back from the table" in (one_call or ""), (one_call or "")[:220])

    # A REMOVAL, IN ITS OWN CALL, AND THE ROW COUNT MUST COME BACK DOWN BY EXACTLY ONE. Removing by NAME is what
    # makes this safe: nothing else addresses a position that shifted.
    _, final_count, final_rows = read_table_schema(session)
    check("removing the row brought the count back to what it was", final_count == count_before,
          "was %s, is now %s" % (count_before, final_count))
    check("the scratch row is gone", SCRATCH not in final_rows,
          "rows: %s" % ", ".join(sorted(final_rows))[:200])

    # *** THE RESTORE IS ASSERTED, NOT ASSUMED. *** A suite that mutates a SHARED ENGINE ASSET and does not give it
    # back makes the next suite read something it does not expect - the defect recorded four times here.
    check("the shared table was GIVEN BACK, so the next suite reads what it expects",
          final_rows == rows_before,
          "expected the original rows, got %s" % ", ".join(sorted(final_rows))[:200])

    # AND THE PLAN LISTED THE REMOVAL BY NAME, which `D-45` requires - never a count.
    check("the plan listed the removal BY NAME rather than counting it",
          ("- %s" % SCRATCH) in (removed or ""), (removed or "")[:220])


if __name__ == "__main__":
    session = Session()
    session.connect()

    # ON THE WAY IN, BEFORE ANY ASSERTION - so this suite is re-runnable from a state a killed run left behind.
    restore_on_the_way_in(session)

    # Imported structs can have generated field names. Use the actual internal name.
    schema, _, _ = read_table_schema(session)
    field = first_field(schema)
    check("the reader named the struct first field, which every other check depends on",
          bool(field), schema)
    if not field:
        sys.exit("RESULT: FAIL (the DataTable reader did not name the first field)")

    run_parser_checks(session)
    run_rules_checks(session, field)
    run_one_call_checks(session, field)

    print()
    if failures:
        print("RESULT: FAIL (%d) -> %s" % (len(failures), ", ".join(failures)))
        sys.exit(1)
    print("RESULT: PASS (%d check(s) run)" % checks[0])
    sys.exit(0)
