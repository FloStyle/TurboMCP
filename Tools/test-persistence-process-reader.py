"""Offline tests for the bounded Windows process reader.

All execution is injected. These tests never spawn PowerShell, enumerate a
process, query CIM or read a real command line. A separate source inspection
checks the fixed script and subprocess launch policy.
"""

from __future__ import annotations

import json
import unittest
from pathlib import Path

import persistence_process_reader as reader


PID = 1234
START_TICKS = 134_000_000_000_000_000
EXE = r"C:\ExampleUnrealengine\Engine\Binaries\Win64\UnrealEditor.exe"
COMMAND = '"' + EXE + '" "C:\\ExampleProject\\SampleRoot006.uproject"'
SCRIPT = Path(reader.__file__).with_name("read-persistence-process.ps1")
MODULE = Path(reader.__file__)


def success_message():
    return {
        "schema": 1,
        "ok": True,
        "pid": PID,
        "start_ticks": START_TICKS,
        "executable_path": EXE,
        "command_line": COMMAND,
    }


def runner_result(stdout=None, returncode=0, stderr=b"", timed_out=False,
                  overflow=False, cleanup_failed=False, error=None):
    return {
        "returncode": returncode,
        "stdout": json.dumps(success_message()).encode("utf-8") if stdout is None else stdout,
        "stderr": stderr,
        "timed_out": timed_out,
        "overflow": overflow,
        "cleanup_failed": cleanup_failed,
        "error": error,
    }


class FakeRunner:
    def __init__(self, result=None, exception=None):
        self.result = runner_result() if result is None else result
        self.exception = exception
        self.calls = []

    def run(self, argv, deadline_ns, clock):
        self.calls.append({"argv": list(argv), "deadline": deadline_ns, "clock": clock})
        if self.exception:
            raise self.exception
        return self.result


class Clock:
    def __init__(self, values):
        self.values = list(values)
        self.calls = 0

    def __call__(self):
        self.calls += 1
        if not self.values:
            raise AssertionError("clock exhausted")
        return self.values.pop(0)


class ProcessReaderTests(unittest.TestCase):
    def read(self, result=None, *, values=(100, 200), pid=PID, deadline=1000,
             platform="win32", runner=None):
        fake = FakeRunner(result) if runner is None else runner
        clock = Clock(values)
        out = reader.read_process(
            pid, deadline, runner=fake, clock=clock, platform=platform
        )
        return out, fake, clock

    def assert_failure(self, out, error):
        self.assertFalse(out["ok"], out)
        self.assertEqual(out["error"], error, out)
        self.assertNotIn("command_line", out)

    # Fixed command and requests -----------------------------------------
    def test_exact_fixed_command(self):
        out, fake, _ = self.read()
        self.assertTrue(out["ok"])
        self.assertEqual(fake.calls[0]["argv"], [
            "powershell",
            "-NoProfile",
            "-NonInteractive",
            "-File",
            str(SCRIPT),
            "-ProcessId",
            str(PID),
        ])
        self.assertEqual(fake.calls[0]["deadline"], 1000)

    def test_invalid_pid_and_deadline_do_not_spawn(self):
        for pid in (0, -1, True, 1.0, 0x100000000):
            out, fake, _ = self.read(pid=pid)
            self.assert_failure(out, "unknown")
            self.assertEqual(fake.calls, [])
        for deadline in (True, 1.0, "100"):
            out, fake, _ = self.read(deadline=deadline)
            self.assert_failure(out, "unknown")
            self.assertEqual(fake.calls, [])

    def test_non_windows_does_not_spawn(self):
        out, fake, _ = self.read(platform="linux")
        self.assert_failure(out, "unknown")
        self.assertEqual(fake.calls, [])

    def test_expired_deadline_does_not_spawn(self):
        out, fake, _ = self.read(values=(1000,), deadline=1000)
        self.assert_failure(out, "timeout")
        self.assertEqual(fake.calls, [])

    def test_deadline_after_return_refuses(self):
        out, fake, _ = self.read(values=(100, 1000), deadline=1000)
        self.assert_failure(out, "timeout")
        self.assertEqual(len(fake.calls), 1)

    # Successful result ---------------------------------------------------
    def test_success_returns_exact_process_read(self):
        out, _, _ = self.read()
        self.assertEqual(out, {
            "ok": True,
            "value": {
                "pid": PID,
                "start_ticks": START_TICKS,
                "executable_path": EXE,
                "command_line": COMMAND,
            },
        })

    def test_result_not_aliased_to_runner_bytes_or_dict(self):
        raw = runner_result()
        out, _, _ = self.read(raw)
        raw["stdout"] = b"changed"
        self.assertEqual(out["value"]["pid"], PID)
        self.assertEqual(out["value"]["command_line"], COMMAND)

    # Wire failures -------------------------------------------------------
    def test_eof_and_partial_output_refused(self):
        for payload in (b"", b'{"schema":1,"ok":true'):
            out, _, _ = self.read(runner_result(stdout=payload))
            self.assert_failure(out, "unknown")

    def test_nonzero_exit_refused(self):
        out, _, _ = self.read(runner_result(returncode=1))
        self.assert_failure(out, "provider_error")

    def test_nonempty_stderr_refused(self):
        out, _, _ = self.read(runner_result(stderr=b"provider text"))
        self.assert_failure(out, "provider_error")

    def test_invalid_utf8_refused(self):
        out, _, _ = self.read(runner_result(stdout=b"\xff\xfe"))
        self.assert_failure(out, "unknown")

    def test_extra_json_output_refused(self):
        out, _, _ = self.read(runner_result(stdout=json.dumps(success_message()).encode() + b"{}"))
        self.assert_failure(out, "unknown")

    def test_schema_and_shape_errors_refused(self):
        cases = []
        bad = success_message(); bad["schema"] = 2; cases.append(bad)
        bad = success_message(); bad["schema"] = True; cases.append(bad)
        bad = success_message(); del bad["command_line"]; cases.append(bad)
        bad = success_message(); bad["pid"] = True; cases.append(bad)
        bad = success_message(); bad["start_ticks"] = True; cases.append(bad)
        bad = success_message(); bad["executable_path"] = ""; cases.append(bad)
        bad = success_message(); bad["command_line"] = ""; cases.append(bad)
        for message in cases:
            out, _, _ = self.read(runner_result(stdout=json.dumps(message).encode()))
            self.assert_failure(out, "unknown")

    def test_provider_error_taxonomy_and_unknown(self):
        for code in ("timeout", "permission", "not_found", "provider_error",
                     "incomplete", "ambiguous", "unknown"):
            out, _, _ = self.read(runner_result(
                stdout=json.dumps({"schema": 1, "ok": False, "error": code}).encode()
            ))
            self.assert_failure(out, code)
        out, _, _ = self.read(runner_result(
            stdout=json.dumps({"schema": 1, "ok": False, "error": "raw"}).encode()
        ))
        self.assert_failure(out, "unknown")
        for bad_error in ([], {}, None, 1, True):
            out, _, _ = self.read(runner_result(
                stdout=json.dumps({"schema": 1, "ok": False, "error": bad_error}).encode()
            ))
            self.assert_failure(out, "unknown")

    def test_early_process_exit_is_not_found(self):
        out, _, _ = self.read(runner_result(
            stdout=json.dumps({"schema": 1, "ok": False, "error": "not_found"}).encode()
        ))
        self.assert_failure(out, "not_found")

    def test_overflow_cleanup_and_timeout(self):
        for raw, expected in (
            (runner_result(overflow=True), "incomplete"),
            (runner_result(cleanup_failed=True), "unknown"),
            (runner_result(timed_out=True), "timeout"),
            (runner_result(error="permission"), "permission"),
        ):
            out, _, _ = self.read(raw)
            self.assert_failure(out, expected)

    def test_runner_exception_and_malformed_result(self):
        out, _, _ = self.read(runner=FakeRunner(exception=RuntimeError("secret")))
        self.assert_failure(out, "unknown")
        out, _, _ = self.read(runner={"not": "a result"})
        self.assert_failure(out, "unknown")

    def test_large_stdout_refused(self):
        message = json.dumps(success_message()).encode()
        out, _, _ = self.read(runner_result(stdout=message + b" " * (reader.MAX_CAPTURE + 1)))
        self.assert_failure(out, "incomplete")

    # Source/script inspection --------------------------------------------
    def test_fixed_script_is_narrow_and_native(self):
        text = SCRIPT.read_text(encoding="utf-8")
        self.assertIn("[uint32] $ProcessId", text)
        self.assertIn("Get-CimInstance Win32_Process", text)
        self.assertIn('ProcessId=$ProcessId', text)
        self.assertIn("OpenProcess(0x1000", text)
        self.assertIn("GetProcessTimes", text)
        self.assertIn("GetExitCodeProcess", text)
        self.assertIn("CloseHandle", text)
        self.assertIn(".Replace('/', '\\')", text)
        self.assertIn("finally", text)
        self.assertNotIn("Start-Process", text)
        self.assertNotIn("taskkill", text)
        self.assertNotIn("Get-Process", text)

    def test_python_runner_is_bounded_and_child_only(self):
        text = MODULE.read_text(encoding="utf-8")
        self.assertIn("MAX_CAPTURE = 64 * 1024", text)
        self.assertIn("CREATE_NO_WINDOW", text)
        self.assertIn("STARTF_USESHOWWINDOW", text)
        self.assertIn("shell=False", text)
        self.assertIn("proc.kill()", text)
        self.assertIn("proc.wait", text)
        self.assertNotIn("taskkill", text)
        self.assertNotIn("shell=True", text)
        self.assertNotIn("process tree", text)


if __name__ == "__main__":
    unittest.main(verbosity=2)
