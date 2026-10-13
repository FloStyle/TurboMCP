# ACCEPTANCE ROW THIS SUITE CLAIMS TO PROVE: T-DATA-5
# Copyright (c) 2026 FloStyle
# SPDX-License-Identifier: MPL-2.0
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.
"""TA-17 built and loadable, TA-18 and TA-19 refused by name.

WHY THIS SUITE EXISTS. `Spec/06_ACCEPTANCE.md` names three assets that no fixture
carried: `TA-17` ("Curve float and a curve table", line 32, read by `T-DATA-5` at
line 213), `TA-18` ("Animation Blueprint and Anim Sequence", line 33) and `TA-19`
("Niagara system", line 34). The kit commandlet now builds the first and DECLINES
the other two, each with the file and line that decides it. A build whose own
report says `OK` is not evidence, so this suite does three separate things:

  Phase 1  REBUILDS the kit and reads THE COMMANDLET OWN LINES. This is also the
           reproducibility claim, run rather than asserted: the two `.uasset`
           files are written by the committed C++ on every invocation, so a
           fixture that stopped being produced from source fails here.
  Phase 2  READS THE ASSETS BACK THROUGH A DIFFERENT TOOL - the engine Python
           surface (`PythonScriptCommandlet`), which reaches
           `UCurveFloat::GetFloatValue` and `UDataTableFunctionLibrary::CurveTableToJson`,
           neither of which this project wrote. A builder reporting on itself is
           the defect this project records most often.
  Phase 3  READS THE RAW FILES, because "loadable" and "on disk" are two claims.

*** AND THE TANGENT CHECK IS BEHAVIOURAL, WHICH IS NOT A STYLE CHOICE. ***
`UCurveFloat::FloatCurve` is `UPROPERTY()` with no specifiers
(`Curves/CurveFloat.h:35-36`), so the Python property bridge REFUSES it
("Failed to find property 'float_curve'") and the keys cannot be read out as
properties. What IS reachable is the engine own evaluator. It is evaluated at
t=0.125, the midpoint of the first segment, where the two answers differ by
construction:

    zero tangents   cubic Hermite reduces to 0.5*p0 + 0.5*p1 = 1.0
    the tangents written      0.5*p0 + 0.5*p1 + 0.015625 + 0.046875 = 1.0625

with h10*m0*dt = 0.125 * 0.5 * 0.25 and h11*m1*dt = -0.125 * -1.5 * 0.25. So a
fixture whose tangents were silently reset to zero by `AutoSetTangents` answers
1.0 and FAILS here, which is exactly the row clause "a curve with 5 keys AND
non-zero tangents".

THE EDITOR IS STOPPED FIRST, AND THAT IS MEASURED RATHER THAN TIDY.
`Tools/test-graph-fix-backward.py` records it: a `-run=` commandlet that rebuilds
a package the running editor already holds in memory produces a file that is NOT
the one it reported writing. The stop goes through `Tools/tmcp_editor.py`, which
owns the lifecycle for the suites, and the editor is started again at the end so
a standalone run leaves the machine as it found it.
"""

import tmcp_paths  # noqa: E402  (after the sys.path line above)
import json
import os
import re
import subprocess
import sys
import tempfile
import time

import tmcp_editor

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TOOLS = os.path.join(REPO, "Tools")

EDITOR_CMD = os.path.join(tmcp_paths.find_engine(), "Engine", "Binaries", "Win64", "UnrealEditor-Cmd.exe")
PROJECT = os.path.join(REPO, "TestProject", "TurboMCPTest.uproject")
KIT_DIR = os.path.join(REPO, "TestProject", "Content", "Tests", "Kit")

CURVE_ASSET = "TA17_KitCurveFloat"
TABLE_ASSET = "TA17_KitCurveTable"
CURVE_PACKAGE = "/Game/Tests/Kit/" + CURVE_ASSET
TABLE_PACKAGE = "/Game/Tests/Kit/" + TABLE_ASSET

# WHAT THE BUILDER WRITES, COPIED HERE BY HAND SO THIS SUITE CANNOT AGREE WITH A BUG IN IT. If the two
# ever disagree the suite fails, which is the point of writing them twice.
EXPECTED_ROWS = {
    "Row_A": {0.0: 1.0, 0.25: 2.0, 0.5: 3.0},
    "Row_B": {0.0: 5.0, 0.25: 6.0},
    "Row_C": {0.0: 9.0, 0.25: 10.0, 0.5: 11.0, 0.75: 12.0},
}

# THE ENGINE OWN READ-BACK. It writes JSON to a file because `print` from
# `PythonScriptCommandlet` does not survive a PowerShell redirect on this machine
# (`Tools/test-vcs.py:85-87` measured that), and a read-back that vanishes reads
# as a pass.
READBACK_SCRIPT = '''"""Read the TA-17 fixtures through the engine Python surface."""
import json
import os

import unreal

OUT = os.environ["TMCP_KIT_READBACK"]
data = {}

curve = unreal.EditorAssetLibrary.load_asset("%(curve_package)s")
data["curve_loaded"] = curve is not None
data["curve_class"] = curve.get_class().get_name() if curve else None
if curve:
    # `GetFloatValue` is the engine own evaluator (`Curves/CurveFloat.h:43-44`), a different code path
    # from the C++ that wrote the keys.
    data["curve_eval_0_0"] = curve.get_float_value(0.0)
    data["curve_eval_0_125"] = curve.get_float_value(0.125)
    data["curve_eval_1_0"] = curve.get_float_value(1.0)
    try:
        data["float_curve_reachable"] = curve.get_editor_property("float_curve") is not None
    except Exception as error:
        data["float_curve_reachable"] = "REFUSED: %%s" %% (error,)

table = unreal.EditorAssetLibrary.load_asset("%(table_package)s")
data["table_loaded"] = table is not None
data["table_class"] = table.get_class().get_name() if table else None
if table:
    data["table_json"] = unreal.DataTableFunctionLibrary.curve_table_to_json(table)

# THE CENSUS. A CLASS QUERY IS THE WRONG INSTRUMENT HERE AND THAT WAS MEASURED: asking the registry for
# `AnimSequence` answered EMPTY even though the engine ships tutorial sequences, so a class query that
# returns nothing cannot report an absence. Everything under /Game is listed instead, WITH ITS CLASS, so
# the same call that proves the kit is visible is the one that reports the missing types.
registry = unreal.AssetRegistryHelpers.get_asset_registry()
census = []
for asset in registry.get_assets_by_path("/Game", True):
    try:
        kind = str(asset.asset_class_path.asset_name)
    except Exception:
        kind = str(asset.asset_class)
    census.append([str(asset.package_name), kind])
data["game_census"] = census

data["done"] = True
with open(OUT, "w") as handle:
    json.dump(data, handle, indent=1)
''' % {"curve_package": CURVE_PACKAGE, "table_package": TABLE_PACKAGE}

failures = []
checks_run = [0]


def check(name, ok, detail=""):
    checks_run[0] += 1
    print("  %-4s %s%s" % ("ok" if ok else "FAIL", name, ("  " + str(detail)) if detail and not ok else ""))
    if not ok:
        failures.append(name)


def number(text, pattern, group=1):
    """One captured NUMBER from `text`, or None. PARSED, never matched as a sentence."""
    match = re.search(pattern, text)
    if not match:
        return None
    try:
        return float(match.group(group))
    except (TypeError, ValueError):
        return None


def run_commandlet(argv, env=None):
    """Run one `-run=` commandlet and return (exit_code, combined_output)."""
    merged = dict(os.environ)
    merged.update(env or {})
    try:
        done = subprocess.run(argv, capture_output=True, text=True, cwd=REPO, env=merged, timeout=300)
    except (OSError, subprocess.TimeoutExpired) as error:
        return None, "the commandlet could not be run: %s" % error
    return done.returncode, (done.stdout or "") + (done.stderr or "")


def main():
    if not os.path.exists(EDITOR_CMD):
        check("the editor commandlet binary exists", False, EDITOR_CMD)
        print("\nRESULT: FAIL (%d) -> %s" % (len(failures), ", ".join(failures)))
        return 1
    check("the editor commandlet binary exists", True)

    # -------------------------------------------------------------------------------------------
    # PHASE 1: rebuild the kit and read the commandlet own report.
    # -------------------------------------------------------------------------------------------
    print()
    print("=== phase 1: the kit commandlet, rebuilt from the committed source ===")
    tmcp_editor.stop_editor()
    started = time.time()
    exit_code, log = run_commandlet([
        EDITOR_CMD, PROJECT, "-run=TurboMCP.TurboMCPKitCommandlet",
        "-unattended", "-nullrhi", "-culture=en",
    ])
    print("    the kit commandlet returned %s after %ds" % (exit_code, int(time.time() - started)))

    summary = re.search(r"kit-summary assets=(\d+) built=(\d+) measured=(\d+) missing=(\d+) short=(\d+)", log)
    check("the commandlet printed its summary line", summary is not None,
          (log or "")[-400:])
    if summary is None:
        print("\nRESULT: FAIL (%d) -> %s" % (len(failures), ", ".join(failures)))
        return 1

    # THE EXIT CODE IS DELIBERATELY NOT THE GATE, AND THE REASON IS RECORDED IN `make-kit-assets.ps1:101-120`:
    # the engine exits 1 because its WIDGET compiler raises ensures while TA-7 is created, and repairs them
    # itself, so the asset on disk is correct. What replaces the exit code here is the pair of checks below,
    # because a commandlet that did nothing at all would otherwise satisfy every later check vacuously.
    assets = number(log, r"kit-summary assets=(\d+)")
    measured = number(log, r"kit-summary assets=\d+ built=\d+ measured=(\d+)")
    check("the run attempted the kit rather than nothing, and reported on every entry it attempted",
          assets is not None and assets >= 8 and measured == assets,
          "assets=%s measured=%s" % (assets, measured))

    written = re.findall(r"kit TA-17 wrote (\S+)", log)
    check("both TA-17 packages were WRITTEN by this run, so the fixtures are reproduced from source",
          len(written) == 2 and any(CURVE_ASSET in path for path in written)
          and any(TABLE_ASSET in path for path in written),
          written)

    curve_keys = number(log, r"kit TA-17 OK curve float: keys=(\d+)")
    curve_tangents = number(log, r"kit TA-17 OK curve float: keys=\d+ \(needs 5\) nonZeroTangentKeys=(\d+)")
    curve_interp = re.search(r"kit TA-17 OK curve float: .* firstKeyInterp=(\w+)", log)
    check("TA-17 curve float: 5 keys", curve_keys == 5, curve_keys)
    check("TA-17 curve float: a non-zero tangent on every key", curve_tangents == 5, curve_tangents)
    check("TA-17 curve float: the stored interpolation is cubic, so T-DATA-5 can print one",
          curve_interp is not None and curve_interp.group(1) == "RCIM_Cubic",
          curve_interp.group(1) if curve_interp else None)

    table_rows = number(log, r"kit TA-17 OK curve table: rows=(\d+)")
    table_rich = number(log, r"kit TA-17 OK curve table: rows=\d+ \(needs 3\) richRows=(\d+)")
    table_mode = re.search(r"kit TA-17 OK curve table: .* mode=(\w+)", log)
    check("TA-17 curve table: 3 rows", table_rows == 3, table_rows)
    check("TA-17 curve table: all 3 rows are reachable as rich curves", table_rich == 3, table_rich)
    check("TA-17 curve table: CurveTableMode is RichCurves and not merely non-empty",
          table_mode is not None and table_mode.group(1) == "RichCurves",
          table_mode.group(1) if table_mode else None)

    check("the run reports nothing SHORT, so every asset that was built carries its structure",
          number(log, r"kit-summary .* short=(\d+)") == 0.0,
          summary.group(0))
    check("no TA-17 entry is reported MISSING",
          not re.search(r"kit TA-17 MISSING", log),
          re.findall(r"kit TA-17 \w+", log))

    # -------------------------------------------------------------------------------------------
    # TA-18 AND TA-19 ARE REFUSALS, AND A REFUSAL BY NAME IS THE OUTCOME THAT IS CHECKED HERE.
    # -------------------------------------------------------------------------------------------
    for row, module in (("TA-18", "AnimGraph"), ("TA-19", "Niagara")):
        body = re.search(r"kit %s \w+ NOT BUILT: (.+)" % row, log)
        why = re.search(r"kit %s why: (.+)" % row, log)
        check("%s is reported NOT BUILT rather than left silent" % row, body is not None, (log or "")[-200:])
        check("%s names the missing module (%s) in its own why line" % (row, module),
              why is not None and module in why.group(1),
              why.group(1)[:200] if why else None)

    # -------------------------------------------------------------------------------------------
    # PHASE 2: a DIFFERENT tool reads the assets back.
    # -------------------------------------------------------------------------------------------
    print()
    print("=== phase 2: the same assets, read by the engine Python surface ===")
    script = os.path.join(tempfile.gettempdir(), "tmcp-kit-ta17-readback.py")
    result = os.path.join(tempfile.gettempdir(), "tmcp-kit-ta17-readback.json")
    with open(script, "w") as handle:
        handle.write(READBACK_SCRIPT)
    if os.path.exists(result):
        os.remove(result)

    probe_exit, probe_log = run_commandlet(
        [EDITOR_CMD, PROJECT, "-run=PythonScriptCommandlet", "-Script=" + script.replace("\\", "/"),
         "-unattended", "-nullrhi", "-culture=en"],
        env={"TMCP_KIT_READBACK": result})
    check("the read-back commandlet wrote its answer", os.path.exists(result),
          "exit %s; last lines: %s" % (probe_exit, (probe_log or "")[-300:]))
    if not os.path.exists(result):
        print("\nRESULT: FAIL (%d) -> %s" % (len(failures), ", ".join(failures)))
        return 1

    with open(result) as handle:
        data = json.load(handle)
    check("the read-back ran to completion", data.get("done") is True, data.get("done"))

    check("the curve float LOADS from disk and is a CurveFloat",
          data.get("curve_loaded") is True and data.get("curve_class") == "CurveFloat",
          (data.get("curve_loaded"), data.get("curve_class")))

    # THE BEHAVIOURAL TANGENT CHECK. See the module docstring for the arithmetic: 1.0 means the tangents
    # were zeroed, 1.0625 is what the written tangents evaluate to.
    mid = data.get("curve_eval_0_125")
    check("the curve evaluates through the engine own evaluator at the midpoint of its first segment",
          isinstance(mid, (int, float)) and abs(float(mid) - 1.0625) < 1e-6, mid)
    check("the midpoint answer is NOT the zero-tangent answer, which is what proves non-zero tangents",
          isinstance(mid, (int, float)) and abs(float(mid) - 1.0) > 1e-6, mid)
    check("the curve ends on its last key value",
          isinstance(data.get("curve_eval_1_0"), (int, float))
          and abs(float(data["curve_eval_1_0"]) - 0.5) < 1e-6, data.get("curve_eval_1_0"))
    check("the curve starts on its first key value",
          isinstance(data.get("curve_eval_0_0"), (int, float))
          and abs(float(data["curve_eval_0_0"]) - 0.0) < 1e-6, data.get("curve_eval_0_0"))

    check("the curve table LOADS from disk and is a CurveTable",
          data.get("table_loaded") is True and data.get("table_class") == "CurveTable",
          (data.get("table_loaded"), data.get("table_class")))

    rows = None
    raw_json = data.get("table_json")
    if isinstance(raw_json, str):
        try:
            rows = json.loads(raw_json)
        except ValueError as error:
            print("    the table JSON did not parse: %s" % error)
    check("the engine own CurveTableToJson exported the table", isinstance(rows, list), repr(raw_json)[:200])
    if isinstance(rows, list):
        names = [entry.get("Name") for entry in rows]
        check("the table carries exactly the 3 row names the row asks for",
              names == sorted(EXPECTED_ROWS), names)
        shape = {}
        for entry in rows:
            keys = {float(time_text): float(value) for time_text, value in entry.items() if time_text != "Name"}
            shape[entry.get("Name")] = keys
        check("every row keys, times and values read back exactly as the builder wrote them",
              shape == EXPECTED_ROWS,
              json.dumps({k: {str(t): v for t, v in row.items()} for k, row in shape.items()}, sort_keys=True))

    # THE MISSING-FIXTURE MEASUREMENT, with its own guard against a census that cannot see anything.
    census = data.get("game_census") or []
    kinds = sorted({kind for _, kind in census})
    check("the census can see the kit, so an absence it reports is a real absence",
          len(census) >= 8 and "CurveFloat" in kinds and "CurveTable" in kinds,
          "%d asset(s), kinds=%s" % (len(census), kinds))
    for missing_kind in ("AnimSequence", "AnimBlueprint", "NiagaraSystem", "Skeleton"):
        offenders = [package for package, kind in census if kind == missing_kind]
        check("no %s asset exists under /Game, which is why TA-18/TA-19 name a missing fixture"
              % missing_kind, not offenders, offenders)

    # -------------------------------------------------------------------------------------------
    # PHASE 3: the two files, on disk, as bytes.
    # -------------------------------------------------------------------------------------------
    print()
    print("=== phase 3: the files themselves ===")
    blobs = {}
    for asset in (CURVE_ASSET, TABLE_ASSET):
        path = os.path.join(KIT_DIR, asset + ".uasset")
        exists = os.path.isfile(path)
        size = os.path.getsize(path) if exists else 0
        check("%s.uasset exists on disk and is not empty" % asset, exists and size > 0,
              "%s (%d byte(s))" % (path, size))
        if exists:
            with open(path, "rb") as handle:
                blobs[asset] = handle.read()

    names = {asset: set(re.findall(rb"[ -~]{4,}", blob)) for asset, blob in blobs.items()}
    check("the curve table file records RICH keys, so the table is not a simple-curve table",
          b"RichCurveKey" in names.get(TABLE_ASSET, set()),
          sorted(names.get(TABLE_ASSET, set()))[:20])
    for row_name in EXPECTED_ROWS:
        check("the curve table file carries the row name %s in its own name table" % row_name,
              row_name.encode() in names.get(TABLE_ASSET, set()))
    check("the curve float file names its curve member, so the keys have somewhere to live",
          b"FloatCurve" in names.get(CURVE_ASSET, set()),
          sorted(names.get(CURVE_ASSET, set()))[:20])

    # The editor is started again so the harness - and a human - find the machine as this suite found it.
    ok, detail = tmcp_editor.ensure_editor()
    check("the editor answers again after the rebuild", ok, detail)

    print()
    if failures:
        print("RESULT: FAIL (%d) -> %s" % (len(failures), ", ".join(failures)))
        return 1
    print("RESULT: PASS (%d check(s), none skipped)" % checks_run[0])
    return 0


if __name__ == "__main__":
    sys.exit(main())
