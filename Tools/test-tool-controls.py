"""Focused personal tool-policy rules; restart and dispatch checks are separate."""
import runpy
from pathlib import Path
import re
import subprocess
import shutil
import uuid
import os
from tmcp import Session, READ_TOOLSET
support = runpy.run_path(str(Path(__file__).with_name("test-widget-roundtrip.py")))
session = Session()
session.connect()
name = "TurboMCP.Workflow.ToolControls"
session.call(support["AUTO_TOOLSET"], "DiscoverTests", {})
raw = session.call(support["AUTO_TOOLSET"], "RunTests", {"testNames": [name]})
print(raw)
support["parse_run_tests_report"](raw, name)
# Test only this checkout's supervised project, never another running editor.
root = Path(__file__).resolve().parents[1]
fixture = root / "TestProject/Saved/TurboMCP/Workflow/tool-policy-fixture.ini"
section = "/Script/TurboMCP.TurboMCPSettings"
pattern = re.compile(r"(?ms)^\[" + re.escape(section) + r"\]\r?\n.*?(?=^\[|\Z)")
match = pattern.search(fixture.read_text(encoding="utf-8-sig"))
if not match:
    raise RuntimeError("Unreal did not serialize the policy fixture section")
config = root / "TestProject/Saved/Config/WindowsEditor/EditorPerProjectUserSettings.ini"
pwsh = shutil.which("pwsh")
if not pwsh:
    raise RuntimeError("PowerShell launcher unavailable")
def editor(stop=False):
    command = [pwsh, "-NoProfile", "-File", str(root / "Tools/start-editor.ps1")]
    command += ["-Stop"] if stop else ["-NullRHI", "-LowPriority"]
    # This suite explicitly owns a restart acceptance scenario. Keep the normal
    # launcher identity/lease checks; permit its documented supervisor lifecycle.
    environment = dict(os.environ)
    environment["TURBOMCP_RUNNER_ALLOW_STOP"] = "1"
    environment["TURBOMCP_RUNNER_STARTING_EDITOR"] = "1"
    result = subprocess.run(command, cwd=root, env=environment, check=True, timeout=450,
                            capture_output=True, text=True, encoding="utf-8", errors="replace")
    print(result.stdout)
    if "intentionally skipped" in result.stdout or "intentionally reused" in result.stdout:
        raise RuntimeError("Restart was suppressed; persistence is not verified")
    if not stop:
        pid = re.search(r"(?m)^\s*PID:\s*(\d+)", result.stdout)
        if not pid:
            raise RuntimeError("New editor PID was not reported")
        return int(pid.group(1))
editor(stop=True)
original = config.read_bytes() if config.exists() else None
backup = root / ".local" / ("tool-controls-preferences-" + uuid.uuid4().hex + ".backup")
backup.parent.mkdir(parents=True, exist_ok=True)
if original is not None:
    backup.write_bytes(original)
try:
    old = original.decode("utf-8-sig") if original is not None else ""
    config.parent.mkdir(parents=True, exist_ok=True)
    config.write_text(pattern.sub("", old).rstrip() + "\n\n" + match.group(0) + "\n", encoding="utf-8")
    previous_pid = None
    for launch in range(2):
        current_pid = editor()
        if current_pid == previous_pid:
            raise RuntimeError("The editor was reused; restart persistence is not verified")
        previous_pid = current_pid
        live = Session()
        live.connect()
        status = live.call(READ_TOOLSET, "ReadTurboMCPStatus", {})
        print(status)
        if "ReadTurboMCPGraphLoad [Blueprint]: Disabled" not in status:
            raise RuntimeError("Persisted Blueprint disable is missing on launch " + str(launch))
        if "ReadTurboMCPNodeFind [Blueprint]:" in status:
            raise RuntimeError("Explicit reader enable did not override domain disable")
        if "Unknown tool override" in status or "Unknown tool domain" in status:
            raise RuntimeError("Persisted fixture contains invalid controls")
        try:
            denied = live.call(READ_TOOLSET, "ReadTurboMCPGraphLoad", {"Asset": "/Game/DoesNotExist", "GraphName": "EventGraph"})
        except RuntimeError as exc:
            denied = str(exc)
        print("Disabled call response:", denied)
        if not any(word in denied.lower() for word in ("disabled", "blocked", "not found", "not allowed")):
            raise RuntimeError("Disabled tool reached asset lookup or returned an unexpected result")
        editor(stop=True)
finally:
    # Stop before restoring, preventing editor shutdown from overwriting restoration.
    editor(stop=True)
    if original is None:
        config.unlink(missing_ok=True)
    else:
        config.write_bytes(original)
        if config.read_bytes() != original:
            raise RuntimeError("Personal preference restoration failed; backup retained")
    backup.unlink(missing_ok=True)
    editor()
print("RESULT: PASS")
