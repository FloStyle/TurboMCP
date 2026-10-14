# Copyright (c) 2026 FloStyle
# SPDX-License-Identifier: MPL-2.0
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.
"""One owner of the editor lifecycle.

WHY THIS MODULE EXISTS, AND IT IS `Spec/WORKING_PLAN.md` ROW 0.1. `08_ROADMAP.md` section 3 states the defect as
work rather than as a symptom, in two sentences:

    "the harness OWNS THE EDITOR ONLY BETWEEN SUITES AND NEVER INSIDE ONE"
    "ONE OWNER OF THE EDITOR LIFECYCLE is the fix, and THE HYBRID WHERE A SUITE AND THE HARNESS BOTH DRIVE THE
     SAME EDITOR IS WHAT KEEPS PRODUCING THIS."

MEASURED BEFORE WRITING IT: **ELEVEN SUITES DRIVE THE EDITOR THEMSELVES** - `test-graph-append`,
`test-graph-dead`, `test-graph-fix-backward`, `test-graph-knots`, `test-graph-patch`, `test-graph-replace`,
`test-graph-tidy`, `test-material-label-promote`, `test-material-tidy`, `test-material`, `check-probe-graphs` -
and each one re-implements the same three subprocess calls with small variations: some stop the editor first, some
gate the rebuild behind an opt-in environment variable with a DIFFERENT name
(`TURBOMCP_APPEND_REBUILD`, `TURBOMCP_REPLACE_REBUILD`, ...), and some rebuild without stopping anything. **THAT
VARIATION IS THE DEFECT RATHER THAN A STYLE PROBLEM: eleven copies of a lifecycle means eleven chances for one of
them to disagree with the harness about who holds the editor.**

SO A SUITE ASKS THIS MODULE INSTEAD. It never calls `start-editor.ps1` or a `make-*.ps1` itself.

*** AND THE HONEST SCOPE OF THIS FILE IS IMPORTANT: IT IS THE OWNER FOR THE SUITES, NOT FOR THE HARNESS. *** The
harness is PowerShell (`Tools/run-suites.ps1`) and it must keep owning the editor BETWEEN suites, because that is
what lets it reuse a healthy one. `WORKING_PLAN.md` row 0.1 asks for a single owner across both, and that is a
LARGER change than this file: it means the harness and the suites speaking to one component. **This file is the
half that the suites can adopt today, and it is written so the harness can call into it later rather than beside
it.** Claiming the whole row would be claiming something this does not do.
"""

import hashlib
import json
import os
import shutil
import subprocess
import sys
import tempfile
import time

TOOLS = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(TOOLS)

# ONE NAME FOR THE OPT-IN, INSTEAD OF ONE PER SUITE. The old spelling is still honoured per suite by the suites
# that have not been converted yet, so nothing regresses while the change lands - but a NEW suite gets this one.
REBUILD_ENV = "TURBOMCP_REBUILD_FIXTURE"


def _run(script, args=()):
    """Run a Tools/ PowerShell script without captured child-process pipes."""
    path = os.path.join(TOOLS, script)
    try:
        # The editor launcher can inherit a pipe and wait forever for its reader. Use a real temporary
        # file instead of subprocess capture pipes; this is also the shape used by the suite runner.
        with tempfile.TemporaryFile(mode="w+", encoding="utf-8") as output:
            done = subprocess.run(
                ["pwsh", "-NoProfile", "-File", path, *args],
                stdout=output, stderr=subprocess.STDOUT, text=True, cwd=REPO)
            output.seek(0)
            text = output.read()
    except OSError as error:
        return False, "could not run %s: %s" % (script, error), ""

    first = next((line.strip() for line in text.splitlines() if line.strip()), "no output")
    # `-Stop` ANSWERS NON-ZERO WHEN NOTHING WAS RUNNING, WHICH IS NOT A FAILURE - so a stop is judged by the
    # ABSENCE of an error rather than by its exit code.
    ok = done.returncode == 0 or "RESULT: PASS" in text
    return ok, first, text


def stop_editor():
    """Stop the editor. EVERY SUITE THAT REBUILDS MUST DO THIS FIRST, AND THE REBUILD IS WHY.

    A `make-*.ps1` fixture rebuild is a `-run=` COMMANDLET: it needs the project to itself and it STOPS THE EDITOR
    ITSELF when one is running. A suite that rebuilds WITHOUT stopping first is therefore asking two processes to
    own one project, WHICH IS THE HYBRID THIS MODULE EXISTS TO REMOVE.
    """
    if os.environ.get("TURBOMCP_RUNNER_OWNS_FIXTURES") == "1":
        return True, "runner owns the editor; stop skipped"
    ok, first, _ = _run("start-editor.ps1", ("-Stop",))
    time.sleep(3)
    return ok, first


def rebuild_fixture(script, env=None):
    """Rebuild a fixture with `script`, stopping the editor first.

    *** THIS STOPS THE EDITOR AND DOES NOT START IT AGAIN. CALL `ensure_editor()` AFTERWARDS. ***

    THAT IS NOT AN OVERSIGHT, AND IT IS WRITTEN HERE BECAUSE IT COST A SUITE WHEN IT WAS NOT. MEASURED: I converted
    `Tools/test-graph-dead.py` to call this and then use the session it already had, and the suite died with
    `URLError: [WinError 10061] Aucune connexion n'a pu etre etablie` - because THE EDITOR THAT WAS ANSWERING HAD
    BEEN STOPPED AND NOTHING BROUGHT IT BACK.

    The reason the contract is split rather than fused is that FUSING IT WOULD BE WRONG FOR MOST CALLERS:
    `Tools/test-graph-replace.py` is invoked BY THE HARNESS, and the harness starts the editor again after the
    suite returns - so a fused restart would start a SECOND editor and fight the harness for the project, WHICH IS
    THE VERY HYBRID THIS MODULE EXISTS TO REMOVE. **A CALLER THAT RUNS STANDALONE NEEDS THE RESTART AND A CALLER
    THAT RUNS UNDER THE HARNESS MUST NOT DO IT**, so the two are separate functions and the caller chooses.

    RETURNS (ok, first_line). IT NEVER RAISES AND IT NEVER PRETENDS: a rebuild that did not report success is
    reported as FALSE so the caller can say the suite cannot run, rather than continuing against a fixture of
    unknown shape. `Tools/test-graph-replace.py` already did this correctly and this function is its shape made
    shared. `(None, reason)` means the rebuild was NOT REQUESTED, which is not a failure.
    """
    # The PowerShell runner is the lifecycle owner. A legacy suite may opt in to its old
    # rebuild spelling, but it must never stop the editor that the runner keeps alive.
    if os.environ.get("TURBOMCP_RUNNER_OWNS_FIXTURES") == "1":
        return restore_runner_fixture()

    names = [n for n in (list(env or []) + [REBUILD_ENV]) if n]
    if not any(os.environ.get(name) == "1" for name in names):
        # NAME EVERY VARIABLE THAT WOULD HAVE WORKED, because a message naming the wrong one sends the reader to
        # set something nothing reads. MEASURED WHILE WRITING THIS: the first version printed the CALLER's spelling
        # and omitted the shared one, so the message advised setting a variable that existed nowhere.
        return None, "not rebuilt (set %s=1 to rebuild)" % " or ".join(names)

    if not os.path.exists(os.path.join(TOOLS, script)):
        return False, "%s does not exist" % script

    stop_editor()
    ok, first, _ = _run(script)
    return ok, first


def _fixture_manifest(root):
    """Return a byte manifest for a fixture tree, including untracked sidecars."""
    result = {}
    for base, _, names in os.walk(root):
        for name in names:
            path = os.path.join(base, name)
            relative = os.path.relpath(path, root).replace("\\", "/")
            digest = hashlib.sha256()
            with open(path, "rb") as stream:
                for chunk in iter(lambda: stream.read(1024 * 1024), b""):
                    digest.update(chunk)
            result[relative] = digest.hexdigest()
    return result


def restore_runner_fixture():
    """Restore the runner's snapshot without stopping its kept-alive editor.

    Returns ``(None, reason)`` outside run-suites, or ``(ok, detail)`` inside it.
    The byte manifest is checked here as well as by the runner, because suites can
    request a restore from inside their own process.
    """
    if os.environ.get("TURBOMCP_RUNNER_OWNS_FIXTURES") != "1":
        return None, "the suite runner does not own fixtures"

    fixture = os.environ.get("TURBOMCP_FIXTURE_DIR", "")
    snapshot = os.environ.get("TURBOMCP_FIXTURE_SNAPSHOT", "")
    temp_root = os.environ.get("TURBOMCP_FIXTURE_TEMP", "")
    if not all((fixture, snapshot, temp_root)):
        return False, "the suite runner did not publish its fixture paths"

    baseline = _fixture_manifest(snapshot)
    if not baseline:
        return False, "the suite runner published an empty fixture snapshot"

    plan = os.path.join(temp_root, "fixture-plan-in-suite.json")
    helper = os.path.join(TOOLS, "fixture-isolation.py")
    prepared = subprocess.call([
        sys.executable, helper, "prepare", "--fixture", fixture,
        "--snapshot", snapshot, "--plan", plan,
    ], cwd=REPO)
    if prepared != 0:
        return False, "the editor could not prepare an in-suite fixture restore"

    with open(plan, encoding="utf-8") as stream:
        restore_plan = json.load(stream)
    for relative in restore_plan.get("changed", []):
        target = os.path.join(fixture, relative.replace("/", os.sep))
        os.makedirs(os.path.dirname(target), exist_ok=True)
        shutil.copy2(os.path.join(snapshot, relative.replace("/", os.sep)), target)
    for relative in restore_plan.get("extras", []):
        target = os.path.join(fixture, relative.replace("/", os.sep))
        if os.path.exists(target):
            os.remove(target)

    reloaded = subprocess.call([
        sys.executable, helper, "reload", "--plan", plan,
    ], cwd=REPO)
    if reloaded != 0:
        return False, "the editor could not reload an in-suite fixture restore"
    actual = _fixture_manifest(fixture)
    if actual != baseline:
        return False, "in-suite fixture restore is not byte-exact"
    return True, "runner snapshot restored inside the kept-alive editor (byte-exact)"


def ensure_editor():
    """Make sure an editor is up and its MCP server answers, and return (ok, detail).

    THE CHECK IS THE SERVER, NOT THE PROCESS. MEASURED, AND IT IS THE FOURTH OF THE SIX HARNESS DEFECTS THIS
    PROJECT RECORDED: "the probe could pass on an editor that was still shutting down", and separately a live
    editor process whose SERVER NEVER CAME UP made the launcher refuse every later start. **A PROCESS IS NOT A
    WORKING EDITOR.**
    """
    check = os.path.join(TOOLS, "check-mcp-server.py")
    try:
        with tempfile.TemporaryFile(mode="w+", encoding="utf-8") as output:
            done = subprocess.run(
                [sys.executable, check], stdout=output, stderr=subprocess.STDOUT,
                text=True, cwd=REPO)
            output.seek(0)
            probe_text = output.read()
        if "PASS" in probe_text:
            return True, "an editor is already answering"
    except OSError:
        pass

    ok, first, text = _run("start-editor.ps1")
    return ok, first
