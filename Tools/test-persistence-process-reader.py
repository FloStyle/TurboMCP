"""Offline tests for the bounded Windows process reader.

All execution is injected. These tests never spawn PowerShell, enumerate a
process, query CIM or read a real command line. A separate source inspection
checks the fixed script and subprocess launch policy.
"""

from __future__ import annotations

import json
import os
import unittest
from pathlib import Path
from unittest import mock

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
        root = os.environ.get("SystemRoot") or os.environ.get("WINDIR")
        powershell = str(Path(root) / "System32" / "WindowsPowerShell" / "v1.0" / "powershell.exe")
        self.assertEqual(fake.calls[0]["argv"], [
            powershell,
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

    def test_missing_windows_installation_does_not_spawn(self):
        fake = FakeRunner()
        with mock.patch.dict(os.environ, {"SystemRoot": "", "WINDIR": ""}):
            out = reader.read_process(
                PID, 1000, runner=fake, clock=Clock((100,)), platform="win32"
            )
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
        bad = success_message(); bad["schema"] = 1.0; cases.append(bad)
        bad = success_message(); bad["schema"] = True; cases.append(bad)
        bad = success_message(); del bad["command_line"]; cases.append(bad)
        bad = success_message(); bad["pid"] = True; cases.append(bad)
        bad = success_message(); bad["start_ticks"] = True; cases.append(bad)
        bad = success_message(); bad["executable_path"] = ""; cases.append(bad)
        bad = success_message(); bad["command_line"] = ""; cases.append(bad)
        for message in cases:
            out, _, _ = self.read(runner_result(stdout=json.dumps(message).encode()))
            self.assert_failure(out, "unknown")

    def test_duplicate_json_keys_and_nonfinite_refused(self):
        duplicate = b'{"schema":1,"ok":false,"ok":true,"error":"timeout"}'
        out, _, _ = self.read(runner_result(stdout=duplicate))
        self.assert_failure(out, "unknown")
        for payload in (
            b'{"schema":1,"ok":true,"pid":1,"start_ticks":NaN,"executable_path":"x","command_line":"y"}',
            b'{"schema":1,"ok":true,"pid":1,"start_ticks":Infinity,"executable_path":"x","command_line":"y"}',
            b"[" * 2000 + b"]" * 2000,
        ):
            out, _, _ = self.read(runner_result(stdout=payload))
            self.assert_failure(out, "unknown")

    def test_escaped_unicode_wire_data_is_decoded(self):
        message = success_message()
        message["executable_path"] = "C:\\Donn\u00e9es\\UnrealEditor.exe"
        message["command_line"] = "\"C:\\Donn\u00e9es\\UnrealEditor.exe\" \"C:\\Proj\u00e9t\\T\u00e9st.uproject\""
        payload = json.dumps(message, ensure_ascii=True).encode("ascii")
        out, _, _ = self.read(runner_result(stdout=payload))
        self.assertTrue(out["ok"], out)
        self.assertEqual(out["value"]["executable_path"], message["executable_path"])
        self.assertEqual(out["value"]["command_line"], message["command_line"])

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
        self.assertIn("Test-StillRunning", text)
        self.assertIn("$beforeExitTime", text)
        self.assertIn("$afterExitTime", text)
        self.assertIn("System.Text.UTF8Encoding", text)
        self.assertIn("-ArgumentList $false", text)
        self.assertIn("OpenStandardOutput", text)
        self.assertIn("StreamWriter", text)
        self.assertIn("Write-Json", text)
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
        self.assertIn("_powershell_path", text)
        self.assertIn("SystemRoot", text)
        self.assertNotIn("taskkill", text)
        self.assertNotIn("shell=True", text)
        self.assertNotIn("process tree", text)


class FakeStream:
    def __init__(self, chunks=(), read_raises=False, close_raises=False):
        self.chunks = list(chunks)
        self.read_raises = read_raises
        self.close_raises = close_raises
        self.closed = False

    def read(self, size):
        if self.read_raises:
            raise RuntimeError("read failed")
        if self.chunks:
            return self.chunks.pop(0)
        return b""

    def close(self):
        if self.close_raises:
            raise RuntimeError("close failed")
        self.closed = True


class FakeProcess:
    def __init__(self, returncode=0, stdout=None, stderr=None, poll_raises=False,
                 timeout_once=False, wait_fail=False, kill_fail=False,
                 close_fail=False):
        self.returncode = returncode
        self.stdout = stdout if stdout is not None else FakeStream()
        self.stderr = stderr if stderr is not None else FakeStream()
        self.poll_raises = poll_raises
        self.timeout_once = timeout_once
        self.wait_fail = wait_fail
        self.kill_fail = kill_fail
        self.close_fail = close_fail
        self.killed = 0
        self.waits = []
        self.polls = 0

    def poll(self):
        self.polls += 1
        if self.poll_raises:
            raise RuntimeError("poll failed")
        return self.returncode

    def wait(self, timeout=None):
        self.waits.append(timeout)
        if self.timeout_once:
            self.timeout_once = False
            raise reader.subprocess.TimeoutExpired("fake", timeout)
        if self.wait_fail:
            raise RuntimeError("wait failed")
        if self.returncode is None:
            self.returncode = 0
        return self.returncode

    def kill(self):
        self.killed += 1
        if self.kill_fail:
            raise RuntimeError("kill failed")
        self.returncode = -9


class FakeThread:
    def __init__(self, target=None, args=(), daemon=None, behavior="run"):
        self.target = target
        self.args = args
        self.behavior = behavior
        self.started = False
        self.joined = False

    def start(self):
        if self.behavior == "raise":
            raise RuntimeError("start failed")
        self.started = True
        if self.behavior == "run":
            self.target(*self.args)

    def join(self, timeout=None):
        if not self.started:
            return
        self.joined = True

    def is_alive(self):
        return self.behavior == "stuck" and self.started


class ThreadFactory:
    def __init__(self, behaviors):
        self.behaviors = list(behaviors)
        self.threads = []

    def __call__(self, target=None, args=(), daemon=None):
        behavior = self.behaviors.pop(0) if self.behaviors else "run"
        thread = FakeThread(target=target, args=args, daemon=daemon, behavior=behavior)
        self.threads.append(thread)
        return thread


class PopenRunnerTests(unittest.TestCase):
    def run_with(self, proc, threads, clock=None):
        clock = clock if clock is not None else Clock([0, 10, 20, 30])
        calls = []

        def fake_popen(argv, **kwargs):
            calls.append({"argv": list(argv), "kwargs": dict(kwargs)})
            return proc

        with mock.patch("persistence_process_reader.subprocess.Popen", side_effect=fake_popen), \
             mock.patch("persistence_process_reader.threading.Thread", side_effect=threads):
            result = reader._PopenRunner().run(["fixed"], 1000, clock)
        return result, calls

    def test_runner_starts_both_drains_and_closes_pipes(self):
        proc = FakeProcess(returncode=0, stdout=FakeStream([b'{"schema":1,"ok":false,"error":"not_found"}']))
        threads = ThreadFactory(["run", "run"])
        result, calls = self.run_with(proc, threads)
        self.assertEqual(result["returncode"], 0)
        self.assertFalse(result["cleanup_failed"])
        self.assertEqual(len(calls), 1)
        self.assertFalse(calls[0]["kwargs"]["shell"])
        self.assertTrue(proc.stdout.closed)
        self.assertTrue(proc.stderr.closed)

    def test_first_and_second_thread_start_failures_still_clean_child(self):
        for behaviors in (["raise", "run"], ["run", "raise"]):
            proc = FakeProcess(returncode=None)
            result, _ = self.run_with(proc, ThreadFactory(behaviors))
            with self.subTest(behaviors=behaviors):
                self.assertGreaterEqual(proc.killed, 1)
                self.assertTrue(proc.waits)
                self.assertTrue(proc.stdout.closed)
                self.assertTrue(proc.stderr.closed)
                self.assertFalse(result["cleanup_failed"])
                self.assertEqual(result["error"], "unknown")

    def test_clock_and_read_exceptions_clean_child(self):
        def bad_clock():
            raise RuntimeError("clock failed")

        proc = FakeProcess(returncode=None)
        result, _ = self.run_with(proc, ThreadFactory(["run", "run"]), bad_clock)
        self.assertGreaterEqual(proc.killed, 1)
        self.assertTrue(proc.stdout.closed)
        self.assertEqual(result["error"], "unknown")

        proc = FakeProcess(returncode=None, stdout=FakeStream(read_raises=True))
        result, _ = self.run_with(proc, ThreadFactory(["run", "run"]))
        self.assertGreaterEqual(proc.killed, 1)
        self.assertTrue(result["overflow"])
        self.assertTrue(proc.stdout.closed)

    def test_timeout_kills_and_reaps_only_child(self):
        proc = FakeProcess(returncode=None, timeout_once=True)
        result, _ = self.run_with(proc, ThreadFactory(["run", "run"]), Clock([0, 1000]))
        self.assertTrue(result["timed_out"])
        self.assertGreaterEqual(proc.killed, 1)
        self.assertGreaterEqual(len(proc.waits), 2)
        self.assertTrue(proc.stdout.closed)

    def test_overflow_cleans_child(self):
        proc = FakeProcess(
            returncode=None,
            stdout=FakeStream([b"x" * (reader.MAX_CAPTURE + 1)]),
        )
        result, _ = self.run_with(proc, ThreadFactory(["run", "run"]))
        self.assertTrue(result["overflow"])
        self.assertGreaterEqual(proc.killed, 1)
        self.assertTrue(proc.stdout.closed)

    def test_early_exit_is_reaped_without_kill(self):
        proc = FakeProcess(returncode=0)
        result, _ = self.run_with(proc, ThreadFactory(["run", "run"]))
        self.assertEqual(result["returncode"], 0)
        self.assertEqual(proc.killed, 0)
        self.assertTrue(proc.waits)

    def test_wait_and_kill_failures_are_reported(self):
        proc = FakeProcess(returncode=0, wait_fail=True)
        result, _ = self.run_with(proc, ThreadFactory(["run", "run"]))
        self.assertTrue(result["cleanup_failed"])
        self.assertTrue(proc.stdout.closed)

        proc = FakeProcess(returncode=None, timeout_once=True, kill_fail=True)
        result, _ = self.run_with(proc, ThreadFactory(["run", "run"]), Clock([0, 1000]))
        self.assertTrue(result["timed_out"])
        self.assertTrue(result["cleanup_failed"])
        self.assertTrue(proc.stdout.closed)

    def test_stuck_drains_and_close_failures_are_reported(self):
        proc = FakeProcess(
            returncode=0,
            stdout=FakeStream(close_raises=True),
        )
        result, _ = self.run_with(proc, ThreadFactory(["stuck", "stuck"]))
        self.assertTrue(result["cleanup_failed"])
        self.assertTrue(proc.stdout.closed is False)


if __name__ == "__main__":
    unittest.main(verbosity=2)
