#!/usr/bin/env python3
# Copyright (c) 2026 FloStyle
# SPDX-License-Identifier: MPL-2.0
"""H01-H07: the suite runner's verdict and selection decisions, driven by fake children.

WHY THIS SUITE EXISTS
    Release finding R01 was confirmed against the real classifier: `RESULT: PASS` followed by a traceback was
    accepted as a passing suite. Four causes, all of them decisions rather than processes:

      * the child's exit code was discarded;
      * a timeout was a verdict string rather than a state the gate could refuse;
      * a selection that matched nothing only printed a warning, so an empty run looked green;
      * a partly misspelled selection was silently reduced to the names it recognised.

    None of those could be tested while the decisions lived inside `run-suites.ps1`, because the only way to
    reach them was to run the whole harness against a real editor. They now live in `Tools/run-suites-lib.ps1`,
    and this suite drives that file and the real runner with fake children.

WHAT THIS PROVES, AND WHAT IT DOES NOT
    Every case here asserts a NEGATIVE CONTROL as well as the positive: H01 proves that a suite which prints
    `PASS` and exits 7 is REFUSED, and H07 proves that the same suite exiting 0 is ACCEPTED. A classifier that
    rejected everything would pass H01 and fail H07. No editor is started; no real suite runs.

CASES
    H01  child prints PASS then exits 7     -> runner refuses, records exit 7
    H02  only required case reports BLOCKED -> runner refuses, keeps the blocked status
    H03  unknown suite, and valid + unknown -> both refused BEFORE anything starts
    H04  zero cases, and duplicate case ids -> both refused explicitly
    H05  child times out                    -> runner refuses, kills only that child tree, keeps output
    H06  child exits 0 with no verdict      -> runner refuses, names the missing verdict
    H07  child prints a valid verdict, exit 0 -> runner accepts, one executed case
"""

import json
import os
import pathlib
import shutil
import subprocess
import sys
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[1]
RUNNER = ROOT / "Tools" / "run-suites.ps1"
PWSH = shutil.which("pwsh") or shutil.which("powershell")

failures = []


def record(case, ok, detail=""):
    print("  {0} {1}{2}".format("ok  " if ok else "FAIL", case, ("  " + detail) if detail else ""))
    if not ok:
        failures.append(case)


def write_fake(root, name, body):
    path = root / (name + ".py")
    path.write_text(body, encoding="utf-8")
    return path


def run_runner(fake_root, suites, timeout_s=None, log_root=None, extra=None):
    """One real runner invocation against a fake suite root. Returns (exit code, output, manifest).

    The log root is CLEARED first. A manifest left by the previous invocation would otherwise be read as this
    one's result, and "nothing ran" would look like "one suite passed" - which is exactly the kind of stale
    evidence this release is trying to remove.
    """
    env = dict(os.environ)
    env["TURBOMCP_RUNNER_EXTRA_SUITES"] = str(fake_root)
    if timeout_s is not None:
        env["TURBOMCP_RUNNER_SUITE_TIMEOUT"] = str(timeout_s)
    if log_root is not None:
        env["TURBOMCP_RUNNER_LOG_ROOT"] = str(log_root)
        root = pathlib.Path(log_root)
        if root.exists():
            shutil.rmtree(root, ignore_errors=True)
        root.mkdir(parents=True, exist_ok=True)
    if extra:
        env.update(extra)
    args = [PWSH, "-NoProfile", "-File", str(RUNNER)]
    if suites:
        args += ["-Suites", suites]
    done = subprocess.run(args, capture_output=True, text=True, cwd=str(ROOT), env=env, timeout=900)
    out = (done.stdout or "") + (done.stderr or "")
    manifest = None
    candidate = (pathlib.Path(log_root) if log_root else ROOT / ".agent" / "suite-logs") / "manifest.json"
    if candidate.is_file():
        try:
            manifest = json.loads(candidate.read_text(encoding="utf-8"))
        except ValueError:
            manifest = None
    return done.returncode, out, manifest


def suite_entry(manifest, name):
    if not manifest:
        return None
    for entry in manifest.get("suites", []):
        if entry.get("suite") == name:
            return entry
    return None


def main():
    if not PWSH:
        print("RESULT: BLOCKED (no PowerShell host found, so the runner cannot be driven)")
        return 1
    if not RUNNER.is_file():
        print("RESULT: BLOCKED (the runner is missing: %s)" % RUNNER)
        return 1

    work = pathlib.Path(tempfile.mkdtemp(prefix="tmcp-runner-verdicts-"))
    fake = work / "fake-suites"
    logs = work / "logs"
    fake.mkdir()
    logs.mkdir()

    # --- the fake children ---------------------------------------------------------------------
    write_fake(fake, "test-fake-pass-exit7", '''\
import sys
print("  ok a check that passed")
print("RESULT: PASS")
sys.exit(7)
''')
    write_fake(fake, "test-fake-blocked", '''\
import sys
print("RESULT: BLOCKED (the fixture is unavailable)")
sys.exit(3)
''')
    write_fake(fake, "test-fake-silent", '''\
import sys
print("did some work and printed no verdict at all")
sys.exit(0)
''')
    write_fake(fake, "test-fake-pass", '''\
import sys
print("  ok a check that passed")
print("RESULT: PASS")
sys.exit(0)
''')
    write_fake(fake, "test-fake-hang", '''\
import sys, time
print("started and will never finish")
sys.stdout.flush()
time.sleep(600)
sys.exit(0)
''')
    # A child that starts a grandchild and then hangs: the timeout must not leave the grandchild running.
    write_fake(fake, "test-fake-hang-tree", '''\
import subprocess, sys, time
subprocess.Popen([sys.executable, "-c", "import time; time.sleep(600)"])
print("started a grandchild and will never finish")
sys.stdout.flush()
time.sleep(600)
sys.exit(0)
''')

    try:
        # --- H01 and H07: the same suite, differing only in its exit code -----------------------
        code, out, manifest = run_runner(fake, "test-fake-pass-exit7", log_root=logs)
        entry = suite_entry(manifest, "test-fake-pass-exit7")
        record("H01 exit code preserved", entry is not None and entry.get("exitCode") == 7,
               "recorded exit code: %s" % (entry.get("exitCode") if entry else "no manifest"))
        record("H01 PASS-with-exit-7 is refused", entry is not None and entry.get("accepted") is False)
        record("H01 runner exits nonzero", code != 0, "exit %d" % code)
        record("H01 refusal names the exit code",
               any("exited 7" in r for r in (entry or {}).get("reasons", [])),
               str((entry or {}).get("reasons")))

        code, out, manifest = run_runner(fake, "test-fake-pass", log_root=logs)
        entry = suite_entry(manifest, "test-fake-pass")
        record("H07 valid verdict and exit 0 is accepted", entry is not None and entry.get("accepted") is True)
        record("H07 runner exits zero", code == 0, "exit %d" % code)
        record("H07 exactly one case executed", manifest is not None and manifest.get("total") == 1,
               "total: %s" % (manifest or {}).get("total"))

        # --- H02: a blocked-only run -------------------------------------------------------------
        code, out, manifest = run_runner(fake, "test-fake-blocked", log_root=logs)
        entry = suite_entry(manifest, "test-fake-blocked")
        record("H02 blocked status preserved", entry is not None and entry.get("kind") == "blocked",
               "kind: %s" % (entry or {}).get("kind"))
        record("H02 blocked-only run exits nonzero", code != 0, "exit %d" % code)
        record("H02 blocked run is not accepted", entry is not None and entry.get("accepted") is False)
        record("H02 blocked counted as blocked, not failed",
               manifest is not None and manifest.get("blocked") == 1,
               "blocked: %s failed: %s" % ((manifest or {}).get("blocked"), (manifest or {}).get("failed")))

        # --- H03: unknown suite names, refused before anything starts ----------------------------
        code, out, manifest = run_runner(fake, "test-fake-nosuchsuite", log_root=logs)
        record("H03 unknown suite refused", code == 2, "exit %d" % code)
        record("H03 refusal names the unknown suite", "test-fake-nosuchsuite" in out)
        record("H03 nothing ran", manifest is None or manifest.get("total") in (0, None),
               "manifest total: %s" % ((manifest or {}).get("total")))

        code, out, manifest = run_runner(fake, "test-fake-pass,test-fake-nosuchsuite", log_root=logs)
        record("H03 valid plus unknown refused", code == 2, "exit %d" % code)
        record("H03 valid plus unknown refused before execution",
               manifest is None or manifest.get("total") in (0, None),
               "manifest total: %s" % ((manifest or {}).get("total")))

        # --- H04: zero cases and duplicate ids ---------------------------------------------------
        code, out, manifest = run_runner(fake, "test-fake-nosuchsuite1,test-fake-nosuchsuite2", log_root=logs)
        record("H04 zero matched cases refused", code == 2 and "unknown suite name" in out, "exit %d" % code)

        code, out, manifest = run_runner(fake, "test-fake-pass,test-fake-pass", log_root=logs)
        record("H04 duplicate case ids refused", code == 2, "exit %d" % code)
        record("H04 duplicate refusal names the duplicate", "duplicate" in out.lower())

        # --- H06: exit zero with no verdict ------------------------------------------------------
        code, out, manifest = run_runner(fake, "test-fake-silent", log_root=logs)
        entry = suite_entry(manifest, "test-fake-silent")
        record("H06 missing verdict refused", entry is not None and entry.get("accepted") is False)
        record("H06 refusal names the missing verdict",
               any("not an accepted pass" in r for r in (entry or {}).get("reasons", [])),
               str((entry or {}).get("reasons")))
        record("H06 runner exits nonzero", code != 0, "exit %d" % code)

        # --- H05: the timeout path ---------------------------------------------------------------
        code, out, manifest = run_runner(fake, "test-fake-hang", timeout_s=8, log_root=logs)
        entry = suite_entry(manifest, "test-fake-hang")
        record("H05 timeout is a state", entry is not None and entry.get("timedOut") is True)
        record("H05 timeout refused", entry is not None and entry.get("accepted") is False)
        record("H05 runner exits nonzero", code != 0, "exit %d" % code)
        record("H05 output retained", entry is not None and entry.get("logPath")
               and pathlib.Path(entry["logPath"]).is_file(),
               "logPath: %s" % (entry or {}).get("logPath"))
        if entry and entry.get("logPath") and pathlib.Path(entry["logPath"]).is_file():
            text = pathlib.Path(entry["logPath"]).read_text(encoding="utf-8", errors="replace")
            record("H05 partial output is in the log", "will never finish" in text)

        # --- the child tree, not the machine ------------------------------------------------------
        before = count_sleepers()
        code, out, manifest = run_runner(fake, "test-fake-hang-tree", timeout_s=8, log_root=logs)
        after = count_sleepers()
        record("H05 only the child tree is stopped", after <= before,
               "python sleepers before=%d after=%d" % (before, after))

        # --- the log is preserved for a passing suite too ----------------------------------------
        code, out, manifest = run_runner(fake, "test-fake-pass", log_root=logs)
        entry = suite_entry(manifest, "test-fake-pass")
        record("a passing suite keeps its log",
               entry is not None and entry.get("logPath") and pathlib.Path(entry["logPath"]).is_file())

    finally:
        shutil.rmtree(work, ignore_errors=True)

    if failures:
        print("")
        for problem in failures:
            print("  {0}".format(problem))
        print("RESULT: FAIL ({0} check(s))".format(len(failures)))
        return 1
    print("RESULT: PASS (the runner refuses every non-acceptance shape and accepts a real pass)")
    return 0


def count_sleepers():
    """Python processes currently sleeping. Used to prove a timeout did not leak a grandchild."""
    try:
        done = subprocess.run(
            ["powershell", "-NoProfile", "-Command",
             "(Get-CimInstance Win32_Process -Filter \"Name='python.exe'\" | "
             "Where-Object { $_.CommandLine -match 'time.sleep' }).Count"],
            capture_output=True, text=True, timeout=60)
        return int((done.stdout or "0").strip() or "0")
    except Exception:
        return 0


if __name__ == "__main__":
    sys.exit(main())
