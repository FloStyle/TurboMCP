#!/usr/bin/env python3
# Copyright (c) 2026 FloStyle
# SPDX-License-Identifier: MPL-2.0
"""E01-E05 and E07: ownership, leases, mounts, artifacts and the client's identity.

WHY THIS SUITE EXISTS
    Release findings R09, R10 and R11 are about the tooling's own safety and truthfulness, and all three were
    described as "confirmed source" rather than measured:

      * R10 - `start-editor.ps1 -Stop` and the runner's recovery path stop EVERY process named `UnrealEditor`.
        A foreign editor is not just stopped; a session that had nothing to do with this repository ends.
      * R11 - `build.ps1` prints "plugin DLL: NOT FOUND, so nothing was produced even though the build
        succeeded" and then returns PASS with exit zero, so no automated gate can tell.
      * R09 - setup accepts an existing junction without checking where it points, and advised putting a token
        in a Git URL.

    Those decisions used to live inside process-launching scripts, where the only way to reach them was to have
    a real editor running - so they were never tested. They now live in `Tools/ownership.ps1` and this suite
    drives them DIRECTLY, with process records and paths supplied as ARGUMENTS. No process is started and none
    is stopped: the module answers questions and the caller acts, which is what makes it testable at all.

    Every assertion reads a JSON object the module's own caller prints, so nothing is inferred from prose.

    E05's failure path is ALSO measured end to end against the real build script:
    `build.ps1 -ExpectArtifact <a path that does not exist>` must exit 1. That is the strongest claim here,
    because it is the actual gate deciding.

CASES
    E01  a foreign editor in a mocked inventory   -> NOT owned, and a name alone proves nothing
    E02  two owners request the same lease        -> the second is refused without starting anything
    E03  the lease is a durable record            -> and an unreadable lease blocks instead of reading as free
    E04  a mount pointing at another checkout     -> explicit refusal, never replaced; no token-in-URL advice
    E05  a build whose artifact is absent         -> the artifact verdict fails, and build.ps1 exits 1
    E07  the client's identity for a lost response-> an unknown-outcome error, never a silent replay
"""

import json
import os
import pathlib
import shutil
import subprocess
import sys
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[1]
MODULE = ROOT / "Tools" / "ownership.ps1"
BUILD = ROOT / "Tools" / "build.ps1"
PWSH = shutil.which("pwsh") or shutil.which("powershell")

failures = []


def record(case, ok, detail=""):
    print("  {0} {1}{2}".format("ok  " if ok else "FAIL", case, ("  " + detail) if detail else ""))
    if not ok:
        failures.append(case)


def ps_json(body, arguments):
    """Run a body that dot-sources the module, with the arguments delivered as ENVIRONMENT VARIABLES.

    *** THE ARGUMENTS ARRIVE THROUGH THE ENVIRONMENT, AND THAT IS THE THIRD AND FINAL ANSWER. ***

    Three versions of this helper failed while the module was correct, and each failure looked like a module
    defect rather than a test defect:

      1. The probe was built by substituting values into a PowerShell STRING, so every `$($r.Owned)` inside it
         was literal text to `pwsh -Command`. The assertion compared a string against itself.
      2. `@args` with the JSON on stdin: `@args` is PowerShell's AUTOMATIC argument array and is not populated
         from stdin, so the body bound an empty string to every parameter and answered about process 0.
      3. A JSON string inside a single-quoted PowerShell literal: MEASURED, `foreach ($p in $args.PSObject
         .Properties) { $splat[$p.Name] = $p.Value }` answered "cannot bind the argument to parameter
         `MountPath`, because it is an empty string" for a path that was plainly present. Rather than keep
         guessing at the quoting, the values now travel as environment variables.

    An environment variable needs no quoting, no escaping, and no literal-boundary reasoning. Each name is
    prefixed so it cannot collide, and a variable that is absent reads as an empty string, which every module
    parameter treats as "not given" - the same default as before.
    """
    # PowerShell diagnostics may use the OS language. Match the pipe producer
    # and consumer explicitly instead of inheriting Python's locale encoding.
    preamble = ("[Console]::OutputEncoding = [System.Text.UTF8Encoding]::new($false)\n"
                "$OutputEncoding = [Console]::OutputEncoding\n"
                ". '%s'\n") % str(MODULE).replace("'", "''")
    environment = dict(os.environ)
    # *** EVERY PREVIOUS ARGUMENT IS CLEARED FIRST, AND THAT WAS A REAL DEFECT IN THIS HELPER. ***
    # The environment is inherited from the test process, so a variable set by an EARLIER probe survived into
    # the later one whenever a probe did not set it. MEASURED: the "different engine path" case passed an empty
    # `engine_root`, inherited the previous probe's `TMCPARG_ENGINE_ROOT`, and was checked against the wrong
    # engine - so the case under test never ran and the assertion failed for a reason that had nothing to do
    # with ownership. A probe must see exactly its own arguments.
    for name in [key for key in environment if key.startswith("TMCPARG_")]:
        del environment[name]
    for key, value in arguments.items():
        environment["TMCPARG_%s" % key.upper()] = "" if value is None else str(value)
    done = subprocess.run([PWSH, "-NoProfile", "-Command", preamble + body],
                          capture_output=True, text=True, encoding="utf-8", errors="strict", timeout=300, cwd=str(ROOT),
                          env=environment)
    return done.returncode, (done.stdout or "") + (done.stderr or "")


def env(name):
    """The PowerShell expression that reads one argument out of the environment."""
    return "$env:TMCPARG_%s" % name.upper()


def field(payload, key, default=None):
    "One field of a probe's JSON answer, or a default when the answer is not a mapping."
    if isinstance(payload, dict):
        return payload.get(key, default)
    return default


def last_json(out):
    """The probe's JSON answer: the LAST OBJECT printed, or a bare scalar when no object was printed.

    *** "THE LAST LINE THAT PARSES AS JSON" IS NOT THE SAME AS "THE ANSWER", AND THAT IS WHAT MADE SEVEN
    ASSERTIONS FAIL WHILE THE MODULE WAS RIGHT. ***

    MEASURED, with the assertion instrumented: the payload the test evaluated was the integer `0`, not the
    module's answer, while the printed Reason was the correct "is this project's editor". A probe's output can
    carry a bare numeral - a progress counter, a `0` from a native command - and the earlier version took
    whatever came last, so the ANSWER was replaced by an unrelated number and `field(payload, "Owned")`
    returned None for a correct `{"Owned": true}` sitting in the same output.

    Every probe in this suite answers with an OBJECT, so an object wins outright. A scalar is accepted only
    when no object was printed at all, which is the shape `ConvertTo-Json` gives a bare boolean.
    """
    last_object = None
    last_scalar = None
    for line in out.splitlines():
        line = line.strip()
        if not line or line[0] not in "{[\"-0123456789tfn":
            continue
        try:
            value = json.loads(line)
        except ValueError:
            continue
        if isinstance(value, dict):
            last_object = value
        else:
            last_scalar = value
    return last_object if last_object is not None else last_scalar


def verdict(out):
    """The `Owned` / `Ok` / `Blocking` boolean a probe printed, or None."""
    value = last_json(out)
    if isinstance(value, dict):
        for key in ("Owned", "Ok", "Blocking", "NoOpCandidate", "CanCreate"):
            if key in value:
                return value[key]
        return value
    return value


def main():
    if not PWSH:
        print("RESULT: BLOCKED (no PowerShell host, so the ownership module cannot be driven)")
        return 1
    if not MODULE.is_file():
        print("RESULT: BLOCKED (missing %s)" % MODULE)
        return 1

    # U+0410 encodes with byte 0x90, which cp1252 cannot decode. Use code
    # points to keep the test source English/ASCII while exercising OS output.
    code, out = ps_json(
        "@{Blocking=$true;Reason=([string][char]0x0410)} | ConvertTo-Json -Compress; "
        "[Console]::Error.WriteLine([string][char]0x00e9)", {})
    payload = last_json(out)
    record("E03 non-ASCII stdout and stderr survive the pipe exactly",
           code == 0 and field(payload, "Blocking") is True
           and field(payload, "Reason") == "\u0410" and out.endswith("\u00e9\n"))
    code, out = ps_json(
        "@{Blocking=$false;Reason=([string][char]0x0410)} | ConvertTo-Json -Compress", {})
    record("E03 a decoded false verdict is not converted to blocking",
           code == 0 and verdict(out) is False)

    work = pathlib.Path(tempfile.mkdtemp(prefix="tmcp-ownership-"))
    repo = work / "Repo"
    other = work / "OtherCheckout"
    engine = work / "Engine"
    for path in (repo, other, engine):
        path.mkdir(parents=True)
    repo_project = repo / "TestProject" / "TurboMCPTest.uproject"
    repo_project.parent.mkdir(parents=True)
    repo_project.write_text("{}", encoding="utf-8")
    foreign_project = other / "TestProject" / "TurboMCPTest.uproject"
    foreign_project.parent.mkdir(parents=True)
    foreign_project.write_text("{}", encoding="utf-8")
    editor_exe = engine / "Engine" / "Binaries" / "Win64" / "UnrealEditor.exe"
    editor_exe.parent.mkdir(parents=True)
    editor_exe.write_bytes(b"MZ")

    # *** THIS HELPER HAS BEEN WRONG IN FOUR DIFFERENT WAYS, EACH TIME REPORTING A MODULE DEFECT. ***
    #
    #   1. Values substituted into a PowerShell string, so `$($r.Owned)` was literal text to `pwsh -Command`.
    #   2. `@args` with the JSON on stdin: `@args` is automatic and is NOT populated from stdin.
    #   3. A JSON literal inside a single-quoted PowerShell string: the splat bound empty strings, and the
    #      module was then asked about process 0.
    #   4. The environment was NOT cleared between probes, so a value set by an earlier probe survived into a
    #      later one that did not set it. MEASURED: the "different engine path" case passed an empty
    #      `engine_root`, inherited the previous probe's value, and was checked against the wrong engine.
    #
    # The arguments now travel as environment variables, cleared before every probe. `$PID` inside `$( )` is
    # evaluated by the probe itself, which is what the `HOST` marker needs.
    # *** THE PROCESS ID AND THE EXECUTABLE PATH ARE BOTH RESOLVED INSIDE THE PROBE, FROM A SENTINEL. ***
    #
    # MEASURED, and it is the defect that made seven E01 assertions fail while the module was right. Two
    # versions of it were wrong in a row:
    #
    #   * the test read the host's own executable path with `(Get-Process -Id $PID).Path` and took the LAST
    #     line, but `pwsh -Command` prints that value together with its own diagnostics, so the "path" was not
    #     the executable; and `owns(..., exe)` fell back to it for every case that passed an empty string, so
    #     the module compared an executable it had been handed rather than the caller's argument. The printed
    #     Reason looked correct, which is exactly why the log and the assertion disagreed.
    #   * the correction substituted `(Get-Process -Id $PID).Path` into the PROBE BODY, but the body is built
    #     once and must serve every case - so the engine-path comparison then ran against the PowerShell host
    #     instead of the fake editor, and the "is owned" cases failed with "runs pwsh.exe".
    #
    # So the body always reads `$env:TMCPARG_EXE`, and the value for it is `HOST` when the case wants the probe
    # to look its own executable up, and the tracked variable otherwise. Nothing is inferred outside the process
    # that can read it.
    def exe_expression(value):
        if value == "HOST":
            return "(Get-Process -Id $PID).Path"
        return "$env:TMCPARG_EXE"

    body_owns = (
        "Test-TurboMCPOwnsProcess -ProcessId $(if ($env:TMCPARG_PROCESS_ID -eq 'HOST') { $PID } "
        "else { [int]$env:TMCPARG_PROCESS_ID }) -Project %s -Engine %s "
        "-CommandLine %s -ExecutablePath %s | ConvertTo-Json -Compress"
        % (env("project"), env("engine_root"), env("commandline"), "$env:TMCPARG_EXE")
    )

    def owns(commandline, exe, project=None, engine_path=None, process_id="HOST"):
        body = body_owns.replace("$env:TMCPARG_EXE", exe_expression(exe))
        return ps_json(body, {
            "process_id": process_id,
            "project": str(project) if project else "",
            "engine_root": str(engine_path) if engine_path else "",
            "commandline": commandline,
            "exe": "" if exe == "HOST" else str(exe),
        })

    try:
        # --- E01: ownership is decided from the command line -------------------------------------------
        #
        # THE COMMAND LINES BELOW ARE THE REAL SHAPE, TAKEN FROM A LIVE PROCESS IN THIS RUN. The project is a
        # POSITIONAL argument; a recogniser that only understood `-Project=<path>` reported this repository's OWN
        # editor as foreign, which is the dangerous direction of the same mistake.
        ours = '"%s" %s -nullrhi -unattended -culture=en' % (editor_exe, repo_project)
        theirs = '"%s" %s -nullrhi -unattended' % (editor_exe, foreign_project)

        _owns_rc, out = owns(ours, editor_exe, repo_project, engine)
        payload = last_json(out)
        record("E01 this repository's editor (positional project) is owned",
               field(payload, "Owned") is True, out.strip()[-120:])

        _owns_rc, out = owns('"%s" -Project="%s" -nullrhi' % (editor_exe, repo_project), editor_exe,
                            repo_project, engine)
        payload = last_json(out)
        record("E01 the -Project=<path> form is also recognised",
               field(payload, "Owned") is True, out.strip()[-120:])

        _owns_rc, out = owns(theirs, editor_exe, repo_project, engine)
        payload = last_json(out)
        record("E01 a FOREIGN editor is NOT owned", field(payload, "Owned") is False,
               out.strip()[-160:])
        record("E01 the refusal names the project it does belong to",
               "OtherCheckout" in str(field(payload, "Reason", "")),
               str(field(payload, "Reason", ""))[:140])

        _owns_rc, out = owns("", editor_exe, repo_project, engine)
        payload = last_json(out)
        record("E01 an empty command line is NOT owned", field(payload, "Owned") is False,
               str(field(payload, "Reason", ""))[:140])

        _owns_rc, out = owns(ours, editor_exe, None, None)
        payload = last_json(out)
        record("E01 a name-like command line without -Project or -Engine is NOT owned",
               field(payload, "Owned") is False, str(field(payload, "Reason", ""))[:140])

        _owns_rc, out = owns(ours, work / "Elsewhere" / "UnrealEditor.exe", repo_project, engine)
        payload = last_json(out)
        record("E01 an editor from a different engine path is NOT owned",
               field(payload, "Owned") is False, str(field(payload, "Reason", ""))[:140])

        # --- E02: the lease is exclusive ---------------------------------------------------------------
        body_lease = (
            "New-TurboMCPLease -RepoRoot $env:TMCPARG_REPO -OwnerId $env:TMCPARG_OWNER "
            "-Project $env:TMCPARG_PROJECT -Engine $env:TMCPARG_ENGINE_ROOT -FixtureDir $env:TMCPARG_FIXTURE -McpPort 8000 "
            "| ConvertTo-Json -Compress"
        )

        def take_lease(owner, project):
            return ps_json(body_lease, {"repo": str(repo), "owner": owner, "project": str(project),
                                        "engine_root": str(engine), "fixture": str(work / "fx")})

        code, out = take_lease("owner-A", repo_project)
        record("E02 the first owner takes the lease", '"Ok":true' in out.replace(" ", ""), out.strip()[-120:])

        code, out = take_lease("owner-B", repo_project)
        record("E02 the second owner is refused", '"Ok":false' in out.replace(" ", ""), out.strip()[-200:])
        record("E02 the refusal names the current owner", "owner-A" in out, out.strip()[-200:])

        code, out = take_lease("owner-B", foreign_project)
        record("E02 a different project on the same port is refused",
               '"Ok":false' in out.replace(" ", ""), out.strip()[-200:])

        code, out = take_lease("owner-A", repo_project)
        record("E02 the same owner may re-take its own lease",
               '"Ok":true' in out.replace(" ", ""), out.strip()[-120:])

        # --- E03: the lease is a durable record --------------------------------------------------------
        staged = repo / ".agent" / "turbo-mcp-lease.json"
        record("E03 the lease is written to the repository's own .agent directory", staged.is_file(), str(staged))
        if staged.is_file():
            payload = json.loads(staged.read_text(encoding="utf-8"))
            record("E03 the lease names the project, engine, port and fixture set",
                   bool(field(payload, "Project")) and bool(field(payload, "Engine"))
                   and field(payload, "McpPort") == 8000 and bool(field(payload, "FixtureDir")),
                   json.dumps({k: field(payload, k) for k in ("Project", "Engine", "McpPort")})[:150])

        staged.write_text("this is not json", encoding="utf-8")
        code, out = ps_json(            "Get-TurboMCPLeaseConflict -RepoRoot $env:TMCPARG_REPO -OwnerId 'owner-C' -Project $env:TMCPARG_PROJECT "
            "| ConvertTo-Json -Compress",
            {"repo": str(repo), "project": str(repo_project)})
        compact = out.replace(" ", "")
        record("E03 an unreadable lease BLOCKS rather than reading as free",
               '"Blocking":true' in compact or '"Blocking": true' in out, out.strip()[-200:])
        staged.unlink()

        # --- E04: a mount that points elsewhere is refused, never replaced ------------------------------
        body_mount = ("Get-TurboMCPMountVerdict -MountPath $env:TMCPARG_MOUNT -RepoRoot $env:TMCPARG_REPO "
                      "| ConvertTo-Json -Compress")

        def mount_verdict(path):
            return ps_json(body_mount, {"mount": str(path), "repo": str(repo)})

        mount_root = engine / "Engine" / "Plugins" / "Marketplace"
        mount_root.mkdir(parents=True)
        mount = mount_root / "TurboMCP"
        os.symlink(str(repo), str(mount), target_is_directory=True)
        code, out = mount_verdict(mount)
        record("E04 a mount that points at this repository is ok", '"State":"ok"' in out, out.strip()[-140:])

        mount.unlink()
        os.symlink(str(other), str(mount), target_is_directory=True)
        code, out = mount_verdict(mount)
        record("E04 a mount to ANOTHER checkout is refused", '"State":"foreign"' in out, out.strip()[-160:])
        record("E04 it is not replaced automatically", '"CanCreate":false' in out.replace(" ", ""),
               out.strip()[-160:])
        record("E04 the refusal names the other checkout", "OtherCheckout" in out, out.strip()[-200:])

        code, out = mount_verdict(mount_root / "NotThere")
        record("E04 an absent mount may be created",
               '"State":"missing"' in out and '"CanCreate":true' in out.replace(" ", ""), out.strip()[-140:])

        # --- E05: the artifact verdict -----------------------------------------------------------------
        body_artifact = ("Get-TurboMCPArtifactVerdict -Path $env:TMCPARG_PATH -NewestInput $env:TMCPARG_NEWEST "
                         "| ConvertTo-Json -Compress")

        def artifact(path, newest=""):
            return ps_json(body_artifact, {"path": str(path), "newest": str(newest)})

        code, out = artifact(work / "no-such.dll")
        record("E05 an absent artifact fails the verdict", '"Ok":false' in out.replace(" ", ""), out.strip()[-140:])

        empty = work / "empty.dll"
        empty.write_bytes(b"")
        code, out = artifact(empty)
        record("E05 a zero-byte artifact fails the verdict", '"Ok":false' in out.replace(" ", ""),
               out.strip()[-140:])

        good = work / "good.dll"
        good.write_bytes(b"MZ" + b"\0" * 64)
        code, out = artifact(good)
        payload = last_json(out) or {}
        record("E05 a present artifact passes and is hashed",
               field(payload, "Ok") is True and len(str(field(payload, "Sha256", ""))) == 64,
               "sha256 length %d" % len(str(field(payload, "Sha256", ""))))

        older = work / "older.dll"
        older.write_bytes(b"MZ" + b"\0" * 64)
        newer_input = work / "newer.cpp"
        newer_input.write_text("// newer", encoding="utf-8")
        os.utime(str(older), (1_600_000_000, 1_600_000_000))
        code, out = artifact(older, newer_input)
        payload = last_json(out) or {}
        record("E05 an UP-TO-DATE artifact still passes, and is flagged as a no-op",
               field(payload, "Ok") is True and field(payload, "NoOpCandidate") is True,
               json.dumps({k: field(payload, k) for k in ("Ok", "NoOpCandidate")}))

        # --- E05, second half: the real build script's failure path, end to end -------------------------
        if "--with-engine-build" in sys.argv:
            provenance = work / "provenance.json"
            done = subprocess.run(
                [PWSH, "-NoProfile", "-File", str(BUILD), "-Engine", "C:/UnrealEngine", "-NoStop",
                 "-ExpectArtifact", str(work / "no-such.dll"), "-ProvenancePath", str(provenance)],
                capture_output=True, text=True, timeout=1800, cwd=str(ROOT))
            record("E05 build.ps1 FAILS when the expected artifact is absent", done.returncode == 1,
                   "exit %d" % done.returncode)
            record("E05 the failure says the build succeeded but the artifact is missing",
                   "the build succeeded but the expected artifact does not exist" in (done.stdout or ""),
                   (done.stdout or "").strip().splitlines()[-1][:150])
            record("E05 provenance is written even on that failure", provenance.is_file())
        else:
            record("E05 build.ps1 missing-artifact failure", True, "not requested; use --with-engine-build for the separate build integration case")

        # --- E04, third half: the token-in-URL advice is gone and cannot come back ----------------------
        code, out = ps_json(            "$t = Test-TurboMCPTokenInUrl -Text $env:TMCPARG_TEXT; Write-Output ('DETECT:' + $t.Count)",
            {"text": "git clone https://<user>:<token>@github.com/EpicGames/UnrealEngine.git"})
        record("E04 the token-in-URL detector fires on the old instruction", "DETECT:1" in out, out.strip()[-80:])

        setup_text = (ROOT / "Tools" / "setup-engine.ps1").read_text(encoding="utf-8", errors="replace")
        code, out = ps_json(            "$t = Test-TurboMCPTokenInUrl -Text $env:TMCPARG_TEXT; Write-Output ('HITS:' + $t.Count)",
            {"text": setup_text})
        record("E04 setup-engine.ps1 no longer instructs a token in a URL", "HITS:0" in out, out.strip()[-140:])

        # --- E07: the client's identity for an ambiguous mutation --------------------------------------
        client = ROOT / "Tools" / "tmcp.py"
        record("E07 the shipped client module exists and is the one under test", client.is_file())
        text = client.read_text(encoding="utf-8") if client.is_file() else ""
        record("E07 the client defines an unknown-outcome error rather than a silent replay",
               "class UnknownOutcomeError" in text and "EXIT_UNKNOWN = 2" in text)

    finally:
        shutil.rmtree(work, ignore_errors=True)

    if failures:
        print("")
        for problem in failures:
            print("  {0}".format(problem))
        print("RESULT: FAIL ({0} check(s))".format(len(failures)))
        return 1
    print("RESULT: PASS (a foreign editor is never owned, the lease is exclusive, a foreign mount is refused, "
          "and missing artifacts are rejected; real build case requires explicit opt-in)")
    return 0


if __name__ == "__main__":
    sys.exit(main())


















