"""Offline tests for the bounded Windows process reader.

All execution is injected. These tests never spawn PowerShell, enumerate a
process, query CIM or read a real command line. A separate source inspection
checks the fixed script and subprocess launch policy.
"""

from __future__ import annotations

import json
import os
import ctypes
import importlib.util
import io
import sys
import tempfile
import unittest
from contextlib import redirect_stdout
from pathlib import Path
from unittest import mock

import execution_runtime as runtime
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
        value = self.values.pop(0)
        if isinstance(value, BaseException):
            raise value
        return value


class ProcessReaderTests(unittest.TestCase):
    def read(self, result=None, *, values=(100, 101, 200), pid=PID, deadline=1000,
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
        out, fake, _ = self.read(values=(100, 150, 1000), deadline=1000)
        self.assert_failure(out, "timeout")
        self.assertEqual(len(fake.calls), 1)

    def test_setup_exhausted_deadline_does_not_spawn(self):
        out, fake, _ = self.read(values=(100, 1000), deadline=1000)
        self.assert_failure(out, "timeout")
        self.assertEqual(fake.calls, [])

    def test_after_return_clock_rollback_refused(self):
        out, fake, _ = self.read(values=(100, 101, 99))
        self.assert_failure(out, "unknown")
        self.assertEqual(len(fake.calls), 1)

    def test_clock_rollback_and_malformed_refuse(self):
        for values in ((100, 99), (True,), (100, "later")):
            out, fake, _ = self.read(values=values)
            self.assert_failure(out, "unknown")
            self.assertEqual(fake.calls, [])

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
        self.assertIn("OpenProcess(0x101000", text)
        self.assertIn("GetProcessTimes", text)
        self.assertIn("WaitForSingleObject", text)
        self.assertIn("0x101000", text)
        self.assertIn("Get-Liveness", text)
        self.assertIn("0x102", text)
        self.assertIn("CloseHandle", text)
        self.assertIn(".Replace('/', '\\')", text)
        self.assertNotIn("GetExitCodeProcess", text)
        self.assertNotIn("Test-StillRunning", text)
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
        self.assertIn("CLEANUP_LIMIT_NS = 5_000_000_000", text)
        self.assertIn("CREATE_NO_WINDOW", text)
        self.assertIn("STARTF_USESHOWWINDOW", text)
        self.assertIn("shell=False", text)
        self.assertIn("terminate_checked", text)
        self.assertIn("active_processes", text)
        self.assertIn("close_checked", text)
        self.assertIn("--persistence-worker", text)
        self.assertIn("job_factory", text)
        self.assertIn("proc.wait", text)
        self.assertEqual(text.count("pid = int(argv[-1])"), 1)
        self.assertIn("_powershell_path", text)
        self.assertIn("SystemRoot", text)
        self.assertIn("proc.kill()", text)
        self.assertNotIn("taskkill", text)
        self.assertNotIn("shell=True", text)
        self.assertNotIn("process tree", text)


class WorkerProtocolTests(unittest.TestCase):
    def valid(self, **changes):
        value = {"schema": 1, "pid": PID}
        value.update(changes)
        return json.dumps(value).encode("utf-8")

    def test_complete_request_and_fixed_command(self):
        self.assertEqual(runtime.parse_persistence_request(self.valid()), PID)
        command = runtime.persistence_command(PID)
        self.assertEqual(command[-2:], ["-ProcessId", str(PID)])
        self.assertTrue(command[4].endswith("read-persistence-process.ps1"))
        self.assertTrue(command[0].endswith("powershell.exe"))

    def test_empty_oversized_trailing_and_second_request_refused(self):
        for raw in (
            b"",
            b" " * 513,
            self.valid() + b" ",
            self.valid() + b"\n",
            self.valid() + self.valid(),
            b'{"schema":1,"pid":1} {"schema":1,"pid":2}',
        ):
            with self.subTest(raw=raw[:40]):
                with self.assertRaises(ValueError):
                    runtime.parse_persistence_request(raw)

    def test_duplicate_nonfinite_and_malformed_fields_refused(self):
        cases = (
            b'{"schema":1,"pid":1,"pid":2}',
            b'{"schema":1,"pid":NaN}',
            b'{"schema":1,"pid":Infinity}',
            b'{"schema":1.0,"pid":1}',
            b'{"schema":true,"pid":1}',
            b'{"schema":1,"pid":true}',
            b'{"schema":1,"pid":0}',
            b'{"schema":1,"pid":4294967296}',
            b'{"schema":1,"pid":"1"}',
            b'{"schema":1}',
            b'{"schema":1,"pid":1,"x":2}',
            b"not json",
        )
        for raw in cases:
            with self.subTest(raw=raw):
                with self.assertRaises(ValueError):
                    runtime.parse_persistence_request(raw)

    def test_worker_refuses_before_launcher(self):
        for raw in (b"", b"{}", b"{"):
            launched = []
            code = runtime.persistence_worker(stream=mock.Mock(read=lambda n, raw=raw: raw),
                                               launcher=lambda *a, **k: launched.append((a, k)) or 0)
            self.assertEqual(code, 2)
            self.assertEqual(launched, [])

    def test_worker_refuses_trailing_byte_before_launcher(self):
        launched = []
        stream = mock.Mock()
        stream.read.side_effect = [self.valid(), b"x"]
        code = runtime.persistence_worker(
            stream=stream,
            launcher=lambda *a, **k: launched.append((a, k)) or 0,
        )
        self.assertEqual(code, 2)
        self.assertEqual(launched, [])

    def test_worker_launches_fixed_command_after_complete_request(self):
        launched = []
        stream = mock.Mock()
        stream.read.side_effect = [self.valid(), b""]
        code = runtime.persistence_worker(
            stream=stream,
            launcher=lambda command, **kwargs: launched.append((command, kwargs)) or 0,
        )
        self.assertEqual(code, 0)
        self.assertEqual(launched[0][0][-2:], ["-ProcessId", str(PID)])
        self.assertEqual(launched[0][1]["stdin"], reader.subprocess.DEVNULL)
        self.assertIn("creationflags", launched[0][1])


class LegacyJobTests(unittest.TestCase):
    class Api:
        def __init__(self):
            self.closed = 0
            self.terminated = 0
            self.close_result = 1
            self.terminate_result = 1
            self.query_result = 1
            self.active = 3

        def CreateJobObjectW(self, *args):
            return 1

        def SetInformationJobObject(self, *args):
            return 1

        def AssignProcessToJobObject(self, *args):
            return 1

        def IsProcessInJob(self, process, job, out):
            import ctypes
            ctypes.cast(out, ctypes.POINTER(runtime.W.BOOL)).contents.value = 1
            return 1

        def TerminateJobObject(self, *args):
            self.terminated += 1
            return self.terminate_result

        def QueryInformationJobObject(self, job, cls, info, size, used):
            import ctypes
            ctypes.cast(info, ctypes.POINTER(runtime.BASIC_ACCOUNTING)).contents.ActiveProcesses = self.active
            return self.query_result

        def CloseHandle(self, *args):
            self.closed += 1
            return self.close_result

    def test_checked_job_methods_and_legacy_close(self):
        self.assertEqual(ctypes.sizeof(runtime.BASIC_ACCOUNTING), 48)
        api = self.Api()
        job = runtime.WindowsJob(api=api)
        self.assertTrue(job.is_process_in_job(FakeProcess()))
        self.assertEqual(job.active_processes(), 3)
        job.terminate_checked()
        self.assertEqual(api.terminated, 1)
        job.close_checked()
        self.assertIsNone(job.handle)

        job = runtime.WindowsJob(api=self.Api())
        job.close()
        self.assertIsNone(job.handle)

    def test_checked_failures_leave_or_report_handle_state(self):
        api = self.Api()
        api.close_result = 0
        job = runtime.WindowsJob(api=api)
        with self.assertRaises(Exception):
            job.close_checked()
        self.assertEqual(job.handle, 1)
        api.terminate_result = 0
        with self.assertRaises(Exception):
            job.terminate_checked()
        api.query_result = 0
        with self.assertRaises(Exception):
            job.active_processes()

    def test_generic_worker_and_run_owned_remain_present(self):
        text = Path(runtime.__file__).read_text(encoding="utf-8")
        self.assertIn("sys.argv[1:] == ['--worker']", text)
        self.assertIn("sys.argv[1:] == ['--persistence-worker']", text)
        self.assertIn("subprocess.call(command)", text)
        self.assertIn("def run_owned", text)
        self.assertIn("QueryInformationJobObject", text)
        self.assertIn("IsProcessInJob", text)
        self.assertIn("BASIC_ACCOUNTING", text)
        self.assertIn("terminate_checked", text)
        self.assertIn("active_processes", text)
        self.assertIn("close_checked", text)
        self.assertIn("0x2000", text)


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


class FakeStdin:
    def __init__(self, write_raises=False, close_raises=False, events=None,
                 write_result=None):
        self.written = []
        self.closed = False
        self.write_raises = write_raises
        self.close_raises = close_raises
        self.events = events if events is not None else []
        self.write_result = write_result

    def write(self, data):
        self.events.append(("request-write",))
        if self.write_raises:
            raise RuntimeError("write failed")
        self.written.append(bytes(data))
        if self.write_result is None:
            return len(data)
        return self.write_result

    def close(self):
        self.events.append(("request-close",))
        if self.close_raises:
            raise RuntimeError("close failed")
        self.closed = True


class FakeJob:
    def __init__(self, active=(0,), assign_raises=False, member=True,
                 terminate_raises=False, active_raises=False,
                 close_raises=False):
        self.active = list(active)
        self.assign_raises = assign_raises
        self.member = member
        self.terminate_raises = terminate_raises
        self.active_raises = active_raises
        self.close_raises = close_raises
        self.assigned = []
        self.terminated = 0
        self.closed = 0
        self.events = []

    def assign(self, proc):
        self.events.append(("assign", proc))
        if self.assign_raises:
            raise RuntimeError("assign failed")
        self.assigned.append(proc)

    def is_process_in_job(self, proc):
        self.events.append(("member", proc))
        return self.member

    def terminate_checked(self):
        self.events.append(("terminate",))
        self.terminated += 1
        if self.terminate_raises:
            raise RuntimeError("terminate failed")

    def active_processes(self):
        self.events.append(("active",))
        if self.active_raises:
            raise RuntimeError("query failed")
        if len(self.active) > 1:
            return self.active.pop(0)
        return self.active[0] if self.active else 0

    def close_checked(self):
        self.events.append(("close",))
        self.closed += 1
        if self.close_raises:
            raise RuntimeError("close failed")


class FakeProcess:
    def __init__(self, returncode=0, stdout=None, stderr=None, poll_raises=False,
                 timeout_once=False, wait_fail=False, kill_fail=False,
                 close_fail=False, stdin=None):
        self.returncode = returncode
        self.stdout = stdout if stdout is not None else FakeStream()
        self.stderr = stderr if stderr is not None else FakeStream()
        self.stdin = stdin if stdin is not None else FakeStdin()
        self.poll_raises = poll_raises
        self.timeout_once = timeout_once
        self.wait_fail = wait_fail
        self.kill_fail = kill_fail
        self.close_fail = close_fail
        self.killed = 0
        self.waits = []
        self.polls = 0
        self._handle = 1

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
        self.finished = False
        self.joins = []

    def start(self):
        if self.behavior == "raise":
            raise RuntimeError("start failed")
        self.started = True
        if self.behavior == "run":
            self.target(*self.args)
            self.finished = True

    def join(self, timeout=None):
        if not self.started:
            raise RuntimeError("cannot join thread before it is started")
        self.joins.append(timeout)
        if self.behavior == "deferred" and not self.finished:
            self.target(*self.args)
            self.finished = True
        if self.behavior == "finish_on_join":
            self.finished = True
        self.joined = True

    def is_alive(self):
        if self.behavior == "stuck":
            return self.started
        if self.behavior == "deferred":
            return self.started and not self.finished
        if self.behavior == "finish_on_join":
            return self.started and not self.finished
        return False


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
    def run_with(self, proc, threads, clock=None, job=None, auth_threads=None):
        clock = clock if clock is not None else Clock([0, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 110])
        calls = []
        sleeps = []
        job = job if job is not None else FakeJob()
        auth_threads = auth_threads if auth_threads is not None else ThreadFactory(["run"])

        def fake_popen(argv, **kwargs):
            calls.append({"argv": list(argv), "kwargs": dict(kwargs)})
            return proc

        with mock.patch("persistence_process_reader.subprocess.Popen", side_effect=fake_popen), \
             mock.patch("persistence_process_reader.threading.Thread", side_effect=threads):
            result = reader._PopenRunner(
                job_factory=lambda: job,
                worker_script="worker.py",
                sleeper=lambda seconds: sleeps.append(seconds),
                auth_thread_factory=auth_threads,
            ).run(["fixed", str(PID)], 1000, clock)
        return result, calls, job, sleeps

    def test_assignment_before_request_and_fixed_worker(self):
        job = FakeJob()
        proc = FakeProcess(returncode=0, stdin=FakeStdin(events=job.events))
        result, calls, job, _ = self.run_with(proc, ThreadFactory(["run", "run"]), job=job)
        self.assertEqual(result["returncode"], 0)
        self.assertFalse(result["cleanup_failed"], result)
        self.assertEqual(len(calls), 1)
        self.assertEqual(calls[0]["argv"], [reader.sys.executable, "worker.py", "--persistence-worker"])
        self.assertFalse(calls[0]["kwargs"]["shell"])
        self.assertEqual(proc.stdin.written, [b'{"schema":1,"pid":1234}'])
        self.assertTrue(proc.stdin.closed)
        self.assertEqual(job.assigned, [proc])
        kinds = [event[0] for event in job.events]
        self.assertLess(kinds.index("assign"), kinds.index("request-write"))
        self.assertLess(kinds.index("member"), kinds.index("request-write"))
        self.assertGreaterEqual(job.terminated, 1)
        self.assertEqual(job.closed, 1)
        self.assertTrue(proc.stdout.closed)
        self.assertTrue(proc.stderr.closed)

    def test_failed_assignment_refuses_without_request(self):
        job = FakeJob(assign_raises=True)
        proc = FakeProcess(returncode=None, stdin=FakeStdin(events=job.events))
        result, calls, job, _ = self.run_with(proc, ThreadFactory(["run", "run"]), job=job)
        self.assertEqual(result["error"], "unknown")
        self.assertEqual(proc.stdin.written, [])
        self.assertEqual(len(calls), 1)
        self.assertGreaterEqual(proc.killed, 1)
        self.assertGreaterEqual(job.terminated, 1)
        self.assertEqual(job.closed, 1)
        self.assertTrue(proc.stdout.closed)

    def test_membership_failure_refuses_without_request(self):
        job = FakeJob(member=False)
        proc = FakeProcess(returncode=None, stdin=FakeStdin(events=job.events))
        result, _, job, _ = self.run_with(proc, ThreadFactory(["run", "run"]), job=job)
        self.assertEqual(result["error"], "unknown")
        self.assertEqual(proc.stdin.written, [])
        self.assertGreaterEqual(proc.killed, 1)
        self.assertGreaterEqual(job.terminated, 1)
        self.assertEqual(job.closed, 1)

    def test_job_constructor_failure_does_not_spawn(self):
        proc = FakeProcess(returncode=0)

        def fail_job():
            raise RuntimeError("job failed")

        with mock.patch("persistence_process_reader.subprocess.Popen") as popen:
            result = reader._PopenRunner(
                job_factory=fail_job, worker_script="worker.py", sleeper=lambda _: None
            ).run(["fixed", str(PID)], 1000, Clock([0]))
        self.assertEqual(result["error"], "unknown")
        self.assertFalse(popen.called)

    def test_expired_before_spawn_does_not_popen(self):
        jobs = []
        with mock.patch("persistence_process_reader.subprocess.Popen") as popen:
            result = reader._PopenRunner(
                job_factory=lambda: jobs.append(FakeJob()) or jobs[-1],
                worker_script="worker.py",
                sleeper=lambda _: None,
            ).run(["fixed", str(PID)], 1000, Clock([1000]))
        self.assertEqual(result["error"], "timeout")
        self.assertEqual(len(jobs), 1)
        self.assertEqual(jobs[0].closed, 1)
        self.assertFalse(popen.called)

    def test_thread_constructor_failure_reports_cleanup(self):
        job = FakeJob()
        proc = FakeProcess(returncode=None, stdin=FakeStdin(events=job.events))
        attempts = []

        def thread_factory(target=None, args=(), daemon=None):
            attempts.append(target)
            if len(attempts) <= 1:
                raise RuntimeError("ctor failed")
            return FakeThread(target=target, args=args, daemon=daemon, behavior="run")

        with mock.patch("persistence_process_reader.subprocess.Popen", return_value=proc), \
             mock.patch("persistence_process_reader.threading.Thread", side_effect=thread_factory):
            result = reader._PopenRunner(
                job_factory=lambda: job,
                worker_script="worker.py",
                sleeper=lambda _: None,
                auth_thread_factory=ThreadFactory(["run"]),
            ).run(["fixed", str(PID)], 1000, Clock([0, 10, 20, 30, 40, 50, 60, 70]))
        self.assertEqual(result["error"], "unknown")
        self.assertFalse(result["cleanup_failed"])
        self.assertEqual(job.closed, 1)
        self.assertTrue(proc.stdout.closed)

    def test_drain_exception_and_clock_rollback_cleanup(self):
        job = FakeJob()
        proc = FakeProcess(
            returncode=None,
            stdin=FakeStdin(events=job.events),
            stdout=FakeStream(read_raises=True),
        )
        result, _, job, _ = self.run_with(
            proc, ThreadFactory(["run", "run"]), Clock([0, 10, 20, 30, 40, 50, 60]), job
        )
        self.assertTrue(result["overflow"])
        self.assertEqual(job.closed, 1)
        self.assertTrue(proc.stdout.closed)

        job = FakeJob()
        proc = FakeProcess(returncode=None, stdin=FakeStdin(events=job.events))
        result, _, job, _ = self.run_with(
            proc, ThreadFactory(["run", "run"]), Clock([0, 10, 5, 20, 30, 40, 50]), job
        )
        self.assertEqual(result["error"], "unknown")
        self.assertEqual(job.closed, 1)

    def test_expired_after_assignment_does_not_authorize(self):
        job = FakeJob()
        proc = FakeProcess(returncode=None, stdin=FakeStdin(events=job.events))
        result, _, job, _ = self.run_with(
            proc, ThreadFactory(["run", "run"]), Clock([0, 1000, 1010, 1020, 1030, 1040]), job
        )
        self.assertTrue(result["timed_out"])
        self.assertEqual(proc.stdin.written, [])
        self.assertGreaterEqual(job.terminated, 1)
        self.assertEqual(job.closed, 1)

    def test_thread_start_failures_report_cleanup_failure(self):
        for behaviors in (["raise", "run"], ["run", "raise"]):
            job = FakeJob()
            proc = FakeProcess(returncode=None, stdin=FakeStdin(events=job.events))
            result, _, job, _ = self.run_with(proc, ThreadFactory(behaviors), job=job)
            with self.subTest(behaviors=behaviors):
                self.assertEqual(result["error"], "unknown")
                self.assertTrue(result["cleanup_failed"])
                self.assertEqual(proc.stdin.written, [b'{"schema":1,"pid":1234}'])
                self.assertGreaterEqual(job.terminated, 1)
                self.assertEqual(job.closed, 1)
                self.assertTrue(proc.stdout.closed)

    def test_stuck_drains_report_cleanup_failure(self):
        job = FakeJob()
        proc = FakeProcess(returncode=0, stdin=FakeStdin(events=job.events))
        result, _, job, _ = self.run_with(proc, ThreadFactory(["stuck", "stuck"]), job=job)
        self.assertTrue(result["cleanup_failed"])
        self.assertEqual(job.closed, 1)
        self.assertTrue(proc.stdout.closed)
        self.assertTrue(proc.stderr.closed)

    def test_deferred_drains_capture_stdout_and_stderr(self):
        job = FakeJob()
        proc = FakeProcess(
            returncode=0,
            stdin=FakeStdin(events=job.events),
            stdout=FakeStream([b'{"schema":1,"ok":false,"error":"not_found"}']),
            stderr=FakeStream([b"diagnostic"]),
        )
        result, _, job, _ = self.run_with(
            proc, ThreadFactory(["deferred", "deferred"]), job=job
        )
        self.assertFalse(result["cleanup_failed"], result)
        self.assertEqual(result["stdout"], b'{"schema":1,"ok":false,"error":"not_found"}')
        self.assertEqual(result["stderr"], b"diagnostic")
        self.assertTrue(proc.stdout.closed)
        self.assertTrue(proc.stderr.closed)

    def test_deferred_stderr_after_stdout_is_not_truncated(self):
        job = FakeJob()
        proc = FakeProcess(
            returncode=0,
            stdin=FakeStdin(events=job.events),
            stdout=FakeStream([b"out-1", b"out-2"]),
            stderr=FakeStream([b"err-after"]),
        )
        result, _, job, _ = self.run_with(
            proc, ThreadFactory(["deferred", "deferred"]), job=job
        )
        self.assertFalse(result["cleanup_failed"], result)
        self.assertEqual(result["stdout"], b"out-1out-2")
        self.assertEqual(result["stderr"], b"err-after")

    def test_short_write_and_close_failure_do_not_authorize(self):
        for stdin in (
            FakeStdin(write_result=1),
            FakeStdin(write_raises=True),
            FakeStdin(close_raises=True),
        ):
            job = FakeJob()
            proc = FakeProcess(returncode=None, stdin=stdin)
            result, _, job, _ = self.run_with(proc, ThreadFactory(["run", "run"]), job=job)
            self.assertEqual(result["error"], "unknown")
            self.assertFalse(result["returncode"] == 0 and result["stdout"])
            self.assertGreaterEqual(job.terminated, 1)
            self.assertEqual(job.closed, 1)
            self.assertTrue(proc.stdout.closed)

    def test_blocked_authorization_times_out_and_cleans_owned_job(self):
        job = FakeJob()
        proc = FakeProcess(returncode=None, stdin=FakeStdin(events=job.events))
        result, _, job, _ = self.run_with(
            proc,
            ThreadFactory(["run", "run"]),
            Clock([0, 10, 1000, 1010, 1020, 1030, 1040, 1050]),
            job,
            auth_threads=ThreadFactory(["stuck"]),
        )
        self.assertTrue(result["timed_out"])
        self.assertTrue(result["cleanup_failed"])
        self.assertEqual(job.closed, 1)
        self.assertTrue(proc.stdout.closed)
        self.assertTrue(proc.stderr.closed)

    def test_wait_thread_respects_expired_and_short_budgets(self):
        runner = reader._PopenRunner(
            job_factory=FakeJob, worker_script="worker.py", sleeper=lambda _: None
        )
        expired = FakeThread(behavior="stuck")
        expired.start()
        self.assertFalse(runner._wait_thread(expired, Clock([200]), 100))
        self.assertEqual(expired.joins, [])

        short = FakeThread(behavior="stuck")
        short.start()
        self.assertFalse(
            runner._wait_thread(short, Clock([0, 90_000_000, 100_000_000]), 100_000_000)
        )
        self.assertEqual(len(short.joins), 1)
        self.assertLessEqual(short.joins[0], 0.01)
        self.assertGreater(short.joins[0], 0)

        crossing = FakeThread(behavior="finish_on_join")
        crossing.start()
        self.assertFalse(
            runner._wait_thread(crossing, Clock([0, 0, 100_000_000]), 100_000_000)
        )
        self.assertLessEqual(crossing.joins[0], 0.05)

        first = FakeThread(behavior="stuck")
        first.start()
        second = FakeThread(behavior="stuck")
        second.start()
        shared = Clock([0, 100_000_000, 100_000_000])
        self.assertFalse(runner._wait_thread(first, shared, 100_000_000))
        self.assertFalse(runner._wait_thread(second, shared, 100_000_000))
        self.assertEqual(first.joins, [])
        self.assertEqual(second.joins, [])

    def test_blocked_close_on_no_drain_path_refuses_without_unbounded_close(self):
        job = FakeJob(assign_raises=True)
        proc = FakeProcess(returncode=None, stdin=FakeStdin(events=job.events))
        result, _, job, _ = self.run_with(
            proc,
            ThreadFactory(["stuck", "stuck", "stuck", "stuck"]),
            job=job,
        )
        self.assertEqual(result["error"], "unknown")
        self.assertTrue(result["cleanup_failed"])
        self.assertGreaterEqual(proc.killed, 1)
        self.assertEqual(job.closed, 1)
        self.assertFalse(proc.stdout.closed)
        self.assertFalse(proc.stderr.closed)

    def test_timeout_uses_job_and_shared_cleanup(self):
        job = FakeJob(active=[1, 1, 0])
        proc = FakeProcess(returncode=None, timeout_once=False, stdin=FakeStdin(events=job.events))
        result, _, job, sleeps = self.run_with(
            proc,
            ThreadFactory(["run", "run"]),
            Clock([0, 10, 20, 1000, 1010, 1020, 1030, 1040, 1050, 1060, 1070]),
            job,
        )
        self.assertTrue(result["timed_out"])
        self.assertFalse(result["cleanup_failed"], result)
        self.assertGreaterEqual(job.terminated, 1)
        self.assertEqual(job.closed, 1)
        self.assertTrue(sleeps)
        self.assertTrue(all(0 <= value <= 0.05 for value in sleeps))
        self.assertTrue(proc.stdout.closed)

    def test_active_count_deadline_and_query_failure_refuse(self):
        job = FakeJob(active=[1], terminate_raises=False)
        proc = FakeProcess(returncode=None, timeout_once=True, stdin=FakeStdin(events=job.events))
        result, _, job, _ = self.run_with(
            proc,
            ThreadFactory(["run", "run"]),
            Clock([0, 10, 20, 1000, 1010, 6_000_000_000, 6_000_000_000, 6_000_000_000, 6_000_000_000]),
            job,
        )
        self.assertTrue(result["timed_out"])
        self.assertTrue(result["cleanup_failed"])
        self.assertEqual(job.closed, 1)

        job = FakeJob(active_raises=True)
        proc = FakeProcess(returncode=None, timeout_once=True, stdin=FakeStdin(events=job.events))
        result, _, job, _ = self.run_with(
            proc,
            ThreadFactory(["run", "run"]),
            Clock([0, 10, 20, 1000, 1010, 1020, 1030, 1040]),
            job,
        )
        self.assertTrue(result["cleanup_failed"])
        self.assertEqual(job.closed, 1)

    def test_terminate_and_close_failures_refuse(self):
        job = FakeJob(terminate_raises=True, close_raises=True)
        proc = FakeProcess(returncode=None, timeout_once=True, stdin=FakeStdin(events=job.events))
        result, _, job, _ = self.run_with(
            proc,
            ThreadFactory(["run", "run"]),
            Clock([0, 10, 20, 1000, 1010, 1020, 1030, 1040, 1050]),
            job,
        )
        self.assertTrue(result["cleanup_failed"])
        self.assertGreaterEqual(job.terminated, 1)
        self.assertEqual(job.closed, 1)

    def test_worker_failure_and_overflow_cleanup(self):
        job = FakeJob()
        proc = FakeProcess(returncode=2, stdin=FakeStdin(events=job.events))
        result, _, job, _ = self.run_with(proc, ThreadFactory(["run", "run"]), job=job)
        self.assertEqual(result["returncode"], 2)
        self.assertFalse(result["cleanup_failed"])
        self.assertEqual(job.closed, 1)

        job = FakeJob()
        proc = FakeProcess(
            returncode=0,
            stdin=FakeStdin(events=job.events),
            stdout=FakeStream([b"x" * (reader.MAX_CAPTURE + 1)]),
        )
        result, _, job, _ = self.run_with(proc, ThreadFactory(["run", "run"]), job=job)
        self.assertTrue(result["overflow"])
        self.assertEqual(job.closed, 1)
        self.assertTrue(proc.stdout.closed)

    def test_wait_failure_and_kill_failure_refuse(self):
        job = FakeJob()
        proc = FakeProcess(returncode=0, wait_fail=True, stdin=FakeStdin(events=job.events))
        result, _, job, _ = self.run_with(proc, ThreadFactory(["run", "run"]), job=job)
        self.assertTrue(result["cleanup_failed"])
        self.assertEqual(job.closed, 1)

        job = FakeJob(assign_raises=True)
        proc = FakeProcess(returncode=None, kill_fail=True, stdin=FakeStdin(events=job.events))
        result, _, job, _ = self.run_with(proc, ThreadFactory(["run", "run"]), job=job)
        self.assertTrue(result["cleanup_failed"])
        self.assertEqual(job.closed, 1)

    def test_clock_error_before_and_after_assignment_cleans(self):
        with mock.patch("persistence_process_reader.subprocess.Popen") as popen:
            result = reader._PopenRunner(
                job_factory=FakeJob, worker_script="worker.py", sleeper=lambda _: None
            ).run(["fixed", str(PID)], 1000, Clock([RuntimeError("clock failed")]))
        self.assertEqual(result["error"], "unknown")
        self.assertFalse(popen.called)

        job = FakeJob()
        proc = FakeProcess(returncode=None, stdin=FakeStdin(events=job.events))
        result, _, job, _ = self.run_with(
            proc,
            ThreadFactory(["run", "run"]),
            Clock([0, RuntimeError("clock failed"), 20, 30, 40, 50]),
            job,
        )
        self.assertEqual(result["error"], "unknown")
        self.assertEqual(job.closed, 1)
        self.assertTrue(proc.stdout.closed)

    def test_request_write_failure_still_closes_job_and_pipes(self):
        job = FakeJob()
        proc = FakeProcess(returncode=None, stdin=FakeStdin(write_raises=True, events=job.events))
        result, _, job, _ = self.run_with(proc, ThreadFactory(["run", "run"]), job=job)
        self.assertEqual(result["error"], "unknown")
        self.assertGreaterEqual(job.terminated, 1)
        self.assertEqual(job.closed, 1)
        self.assertTrue(proc.stdout.closed)
        self.assertTrue(proc.stderr.closed)


class AcceptanceHarnessTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.root = Path(__file__).resolve().parents[1]
        spec = importlib.util.spec_from_file_location("acceptance", cls.root / "Tools/persistence-job-acceptance.py")
        cls.acceptance = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(cls.acceptance)
        spec = importlib.util.spec_from_file_location("executor", cls.root / "Tools/execute-tests.py")
        cls.executor = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(cls.executor)

    def context(self, **changes):
        value = {"repo": runtime.canonical(self.root), "engine": "", "pid": 4242, "token": "secret"}
        value.update(changes)
        return value

    def test_profile_is_fixed_and_has_no_engine_or_editor_path(self):
        request = {"profile": "persistence-native", "suites": []}
        commands = self.executor.commands(request, "")
        self.assertEqual(commands, [[self.executor.sys.executable, "Tools/persistence-job-acceptance.py"]])
        joined = " ".join(commands[0])
        for forbidden in ("build.ps1", "run-suites", "doctor.py", "start-editor", "make-acceptance-asset"):
            self.assertNotIn(forbidden, joined)

    def test_profile_rejects_suite_selection(self):
        request = {
            "schema": 1, "commit": "x", "tree_digest": "x",
            "profile": "persistence-native", "suites": ["anything"], "max_seconds": 30,
        }
        with self.assertRaises(ValueError):
            self.executor.validate_request(request, self.root)

    def test_admission_refuses_missing_malformed_and_unsupervised_context(self):
        with self.assertRaises(ValueError):
            self.acceptance.validate_admission({}, self.root, lambda: {4242})
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / "context.json"
            path.write_text("{}", encoding="utf8")
            env = {"TURBOMCP_EXECUTION_CONTEXT": str(path), "TURBOMCP_EXECUTION_TOKEN": "secret"}
            with self.assertRaises(ValueError):
                self.acceptance.validate_admission(env, self.root, lambda: {4242})
            path.write_text(json.dumps(self.context(engine="engine")), encoding="utf8")
            with self.assertRaises(ValueError):
                self.acceptance.validate_admission(env, self.root, lambda: {4242})
            path.write_text(json.dumps(self.context(token="other")), encoding="utf8")
            with self.assertRaises(ValueError):
                self.acceptance.validate_admission(env, self.root, lambda: {4242})
            path.write_text(json.dumps(self.context()), encoding="utf8")
            with self.assertRaises(ValueError):
                self.acceptance.validate_admission(env, self.root, lambda: {1})

    def test_admission_accepts_only_matching_live_context(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / "context.json"
            path.write_text(json.dumps(self.context()), encoding="utf8")
            env = {"TURBOMCP_EXECUTION_CONTEXT": str(path), "TURBOMCP_EXECUTION_TOKEN": "secret"}
            self.assertEqual(
                self.acceptance.validate_admission(env, self.root, lambda: {4242})["pid"], 4242
            )

    def test_aggregation_blocks_missing_and_blocked_evidence(self):
        passed = [{"case": "a", "status": "passed"}, {"case": "b", "status": "passed"}]
        self.assertEqual(self.acceptance.aggregate(passed, ["a", "b"]), ("passed", []))
        self.assertEqual(self.acceptance.aggregate(passed[:1], ["a", "b"]), ("blocked", ["b"]))
        blocked = [{"case": "a", "status": "blocked"}]
        self.assertEqual(self.acceptance.aggregate(blocked, ["a"]), ("blocked", []))
        failed = [{"case": "a", "status": "failed"}]
        self.assertEqual(self.acceptance.aggregate(failed, ["a"]), ("failed", []))

    def test_unsupervised_main_refuses_without_native_action(self):
        with mock.patch.dict(os.environ, {}, clear=True), \
             mock.patch.object(sys, "argv", ["acceptance"]), \
             redirect_stdout(io.StringIO()) as output:
            self.assertEqual(self.acceptance.main(), 2)
        self.assertIn('"blocked"', output.getvalue())

    def test_cleanup_requires_observed_zero_and_reports_query_failure(self):
        expired = FakeJob(active=[1])
        proc = FakeProcess(returncode=0)
        errors = self.acceptance.cleanup_job(expired, proc, 5, clock=lambda: 10, sleeper=lambda _: None)
        self.assertIn("active-not-zero", errors)
        query = FakeJob(active_raises=True)
        errors = self.acceptance.cleanup_job(query, FakeProcess(returncode=0), 100, clock=lambda: 0, sleeper=lambda _: None)
        self.assertTrue(any(value.startswith("active:") for value in errors))
        clean = FakeJob()
        self.assertEqual(self.acceptance.cleanup_job(clean, FakeProcess(returncode=0), 100, clock=lambda: 0, sleeper=lambda _: None), [])

    def test_add_type_case_full_orchestration_and_cleanup_failure(self):
        class Stdin:
            def __init__(self): self.closed = False
            def write(self, data): self.closed = False; return len(data)
            def flush(self): pass
            def close(self): self.closed = True
        class Proc:
            pid = 700
            _handle = 1
            def __init__(self):
                self.stdin = Stdin(); self.stdout = FakeStream(); self.stderr = FakeStream()
                self.returncode = 0
            def poll(self): return 0
            def wait(self, timeout=None): return 0
        class Observed:
            def __init__(self, api=None): self.closed = 0
            def assign(self, proc): pass
            def is_process_in_job(self, proc): return True
            def drain_messages(self, deadline, callback): callback(701); return None
            def terminate_checked(self): pass
            def active_processes(self): return 0
            def close_checked(self): self.closed += 1
        with tempfile.TemporaryDirectory() as folder, \
             mock.patch.object(self.acceptance, "ObservedJob", Observed), \
             mock.patch.object(self.acceptance.subprocess, "Popen", return_value=Proc()), \
             mock.patch.object(self.acceptance, "process_evidence", return_value=({"pid": 701, "image": "csc.exe", "created": 1}, None)):
            work = Path(folder)
            (work / "add-type.result").write_text("0", encoding="utf8")
            result = self.acceptance._case_add_type(work)
        self.assertEqual(result["status"], "passed", result)
        self.assertEqual(result["compiler"][0]["pid"], 701)

        class Failing(Observed):
            def close_checked(self): raise RuntimeError("close failed")
        with tempfile.TemporaryDirectory() as folder, \
             mock.patch.object(self.acceptance, "ObservedJob", Failing), \
             mock.patch.object(self.acceptance.subprocess, "Popen", return_value=Proc()), \
             mock.patch.object(self.acceptance, "process_evidence", return_value=({"pid": 701, "image": "csc.exe", "created": 1}, None)):
            work = Path(folder)
            (work / "add-type.result").write_text("0", encoding="utf8")
            result = self.acceptance._case_add_type(work)
        self.assertEqual(result["status"], "failed", result)
        self.assertTrue(result["cleanup_errors"])

        with tempfile.TemporaryDirectory() as folder, \
             mock.patch.object(self.acceptance, "ObservedJob", Observed), \
             mock.patch.object(self.acceptance.subprocess, "Popen", return_value=Proc()), \
             mock.patch.object(self.acceptance, "process_evidence", return_value=({"pid": 701, "image": "csc.exe", "created": 1}, None)), \
             mock.patch.object(self.acceptance, "_write_stdin_bounded", return_value=["stdin-timeout"]):
            work = Path(folder)
            (work / "add-type.result").write_text("0", encoding="utf8")
            result = self.acceptance._case_add_type(work)
        self.assertNotEqual(result["status"], "passed", result)
        self.assertNotIn("compiler", result)

        with tempfile.TemporaryDirectory() as folder, \
             mock.patch.object(self.acceptance, "ObservedJob", Observed), \
             mock.patch.object(self.acceptance.subprocess, "Popen", return_value=Proc()), \
             mock.patch.object(self.acceptance, "process_evidence", return_value=(None, "membership")):
            work = Path(folder)
            (work / "add-type.result").write_text("0", encoding="utf8")
            result = self.acceptance._case_add_type(work)
        self.assertEqual(result["status"], "blocked", result)

        class PortFailure(Observed):
            def drain_messages(self, deadline, callback):
                callback(701)
                return "completion-port:5"
        with tempfile.TemporaryDirectory() as folder, \
             mock.patch.object(self.acceptance, "ObservedJob", PortFailure), \
             mock.patch.object(self.acceptance.subprocess, "Popen", return_value=Proc()), \
             mock.patch.object(self.acceptance, "process_evidence", return_value=({"pid": 701, "image": "csc.exe", "created": 1}, None)):
            work = Path(folder)
            (work / "add-type.result").write_text("0", encoding="utf8")
            result = self.acceptance._case_add_type(work)
        self.assertEqual(result["status"], "blocked", result)

        with tempfile.TemporaryDirectory() as folder, \
             mock.patch.object(self.acceptance, "ObservedJob", Observed), \
             mock.patch.object(self.acceptance.subprocess, "Popen", return_value=Proc()), \
             mock.patch.object(self.acceptance, "process_evidence", return_value=({"pid": 701, "image": "csc.exe", "created": 1}, None)):
            result = self.acceptance._case_add_type(Path(folder))
        self.assertEqual(result["status"], "blocked", result)
        self.assertEqual(result["detail"], "compile-result-missing")

        with tempfile.TemporaryDirectory() as folder, \
             mock.patch.object(self.acceptance, "ObservedJob", Observed), \
             mock.patch.object(self.acceptance.subprocess, "Popen", return_value=Proc()), \
             mock.patch.object(self.acceptance, "process_evidence", return_value=({"pid": 701, "image": "csc.exe", "created": 1}, None)):
            work = Path(folder)
            (work / "add-type.result").write_text("21", encoding="utf8")
            result = self.acceptance._case_add_type(work)
        self.assertEqual(result["status"], "failed", result)

        with tempfile.TemporaryDirectory() as folder, \
             mock.patch.object(self.acceptance, "ObservedJob", Observed), \
             mock.patch.object(self.acceptance.subprocess, "Popen", return_value=Proc()), \
             mock.patch.object(self.acceptance, "process_evidence", return_value=({"pid": 701, "image": "python.exe", "created": 1}, None)):
            work = Path(folder)
            (work / "add-type.result").write_text("0", encoding="utf8")
            result = self.acceptance._case_add_type(work)
        self.assertEqual(result["status"], "blocked", result)
        self.assertEqual(result["detail"], "compiler-not-identified")

    def test_short_write_and_preassignment_empty_snapshot_are_not_success(self):
        class Stdin:
            def write(self, data): return 1
            def flush(self): pass
            def close(self): pass
        class Proc:
            pid = 900
            _handle = 1
            returncode = 2
            stdin = Stdin()
            stdout = FakeStream(); stderr = FakeStream()
            def poll(self): return 2
            def wait(self, timeout=None): return 2
        class WThread:
            def __init__(self, target=None, daemon=None): self.target = target; self.alive = True
            def start(self): pass
            def join(self, timeout=None): self.target(); self.alive = False
            def is_alive(self): return self.alive
        proc = Proc()
        with mock.patch.object(self.acceptance, "_close_bounded", return_value=[]):
            errors = self.acceptance._write_stdin_bounded(
                proc, b"go\n", 100, clock=lambda: 0, thread_factory=WThread
            )
        self.assertIn("stdin-write-not-confirmed", errors)

        class Job:
            def assign(self, proc): pass
            def is_process_in_job(self, proc): return True
            def active_processes(self): return 0
            def terminate_checked(self): pass
            def close_checked(self): pass
        with tempfile.TemporaryDirectory() as folder, \
             mock.patch.object(self.acceptance, "WindowsJob", Job), \
             mock.patch.object(self.acceptance.subprocess, "Popen", return_value=Proc()):
            result = self.acceptance._case_preassignment_eof(Path(folder))
        self.assertEqual(result["status"], "blocked", result)
        self.assertTrue(result["worker_refused"])
        self.assertFalse(result["no_descendant_end_snapshot"] is False and result["status"] == "passed")

    def parent_case(self, data, identity, wait_result, terminate_errors=None,
                    close_result=True, supervisor_wait_fail=False):
        class Proc:
            pid = 500
            def __init__(self):
                self.returncode = None; self.killed = 0
                self.stdout = FakeStream(); self.stderr = FakeStream()
            def kill(self):
                self.killed += 1
                if not supervisor_wait_fail: self.returncode = -9
            def poll(self): return self.returncode
            def wait(self, timeout=None):
                if supervisor_wait_fail: raise RuntimeError("supervisor wait failed")
                return self.returncode
        class Api:
            def __init__(self): self.closed = []
            def CloseHandle(self, handle): self.closed.append(handle); return close_result
        proc = Proc(); api = Api(); terminated = []; closes = []; deadlines = []
        def terminate(handle, child_api, deadline):
            terminated.append(handle); deadlines.append(("terminate", deadline))
            return list(terminate_errors or [])
        def close(stream, deadline):
            deadlines.append(("close", deadline)); return []
        with tempfile.TemporaryDirectory() as folder, \
             mock.patch.object(self.acceptance.subprocess, "Popen", return_value=proc), \
             mock.patch.object(self.acceptance, "_read_json_bounded", return_value=data), \
             mock.patch.object(self.acceptance, "_open_process_handle", return_value=(555, api)), \
             mock.patch.object(self.acceptance, "_identity_from_handle", return_value=identity), \
             mock.patch.object(self.acceptance, "_wait_process_handle", return_value=wait_result), \
             mock.patch.object(self.acceptance, "_terminate_process_handle", side_effect=terminate), \
             mock.patch.object(self.acceptance, "_close_bounded", side_effect=close):
            result = self.acceptance._case_parent_death(Path(folder))
        return result, proc, api, terminated, deadlines

    def test_parent_death_does_not_terminate_mismatched_or_missing_identity(self):
        for data, identity in (
            ({"pid": 501, "created": 100}, {"created": 200}),
            ({"pid": 501, "created": 100}, None),
        ):
            result, proc, api, terminated, _ = self.parent_case(data, identity, ("passed", "child-signaled"))
            self.assertEqual(result["status"], "blocked", result)
            self.assertEqual(terminated, [])
            self.assertIn(555, api.closed)
            self.assertGreaterEqual(proc.killed, 1)

    def test_parent_death_verified_failure_uses_same_handle_fallback(self):
        result, proc, api, terminated, _ = self.parent_case(
            {"pid": 501, "created": 100}, {"created": 100}, ("failed", "child-timeout")
        )
        self.assertEqual(result["status"], "failed", result)
        self.assertEqual(result["detail"], "child-timeout")
        self.assertEqual(terminated, [555])
        self.assertIn(555, api.closed)

    def test_parent_death_fallback_and_supervisor_cleanup_failures_are_reported(self):
        result, proc, api, terminated, _ = self.parent_case(
            {"pid": 501, "created": 100}, {"created": 100}, ("failed", "child-timeout"),
            terminate_errors=["terminate-child"], close_result=False, supervisor_wait_fail=True
        )
        self.assertNotEqual(result["status"], "passed", result)
        self.assertTrue(any("terminate-child" in value for value in result["cleanup_errors"]))
        self.assertTrue(any("child-handle-close" in value for value in result["cleanup_errors"]))
        self.assertTrue(any("supervisor-exit" in value for value in result["cleanup_errors"]))
        self.assertEqual(terminated, [555])

    def test_parent_death_verified_success_and_single_cleanup_budget(self):
        result, proc, api, terminated, deadlines = self.parent_case(
            {"pid": 501, "created": 100}, {"created": 100}, ("passed", "child-signaled")
        )
        self.assertEqual(result["status"], "passed", result)
        self.assertEqual(terminated, [])
        self.assertIn(555, api.closed)
        self.assertTrue(deadlines)
        self.assertEqual(len({value for _, value in deadlines}), 1)

    def test_write_stdin_requires_exact_integer_and_always_closes(self):
        class Stdin:
            def __init__(self, result): self.result = result; self.closed = False
            def write(self, data):
                if isinstance(self.result, BaseException): raise self.result
                return self.result
            def flush(self): pass
            def close(self): self.closed = True
        class Proc:
            def __init__(self, result): self.stdin = Stdin(result)
        class WThread:
            def __init__(self, target=None, daemon=None): self.target = target; self.alive = True; self.joins = []
            def start(self): pass
            def join(self, timeout=None): self.joins.append(timeout); self.target(); self.alive = False
            def is_alive(self): return self.alive
        closes = []
        clock = lambda: 0
        with mock.patch.object(self.acceptance, "_close_bounded", side_effect=lambda stream, deadline, *a: closes.append(deadline) or []):
            for value in (None, 0, 1, True, -1, "go"):
                proc = Proc(value)
                errors = self.acceptance._write_stdin_bounded(
                    proc, b"go\n", 100, clock=clock, thread_factory=WThread
                )
                with self.subTest(value=value):
                    self.assertTrue(errors, value)
                    self.assertTrue(closes)
            proc = Proc(RuntimeError("write failed"))
            self.assertTrue(self.acceptance._write_stdin_bounded(proc, b"go\n", 100, clock=clock, thread_factory=WThread))
            proc = Proc(3)
            self.assertEqual(self.acceptance._write_stdin_bounded(proc, b"go\n", 100, clock=clock, thread_factory=WThread), [])

            expired = Proc(3)
            factory_calls = []
            def factory(**kwargs): factory_calls.append(kwargs); return WThread(**kwargs)
            errors = self.acceptance._write_stdin_bounded(expired, b"go\n", 100.0, clock=lambda: 100.0, thread_factory=factory)
            self.assertIn("stdin-deadline-expired", errors)
            self.assertEqual(factory_calls, [])

            budget = Proc(3)
            thread_holder = []
            def budget_factory(**kwargs):
                thread = WThread(**kwargs); thread_holder.append(thread); return thread
            errors = self.acceptance._write_stdin_bounded(
                budget, b"go\n", 0.01, clock=lambda: 0.0, thread_factory=budget_factory
            )
            self.assertEqual(errors, [])
            self.assertAlmostEqual(thread_holder[0].joins[0], 0.01, places=12)

            cap = Proc(3)
            cap_holder = []
            def cap_factory(**kwargs):
                thread = WThread(**kwargs); cap_holder.append(thread); return thread
            self.assertEqual(self.acceptance._write_stdin_bounded(
                cap, b"go\n", 1.0, clock=lambda: 0.0, thread_factory=cap_factory
            ), [])
            self.assertAlmostEqual(cap_holder[0].joins[0], 0.05, places=12)

            crossing = Proc(3)
            times = iter([0.0, 0.0, 0.0, 0.01])
            errors = self.acceptance._write_stdin_bounded(
                crossing, b"go\n", 0.01, clock=lambda: next(times), thread_factory=WThread
            )
            self.assertIn("stdin-completion-after-deadline", errors)
        self.assertTrue(closes)

    def test_close_bounded_uses_second_remaining_budget(self):
        class WThread:
            def __init__(self, target=None, daemon=None): self.target = target; self.alive = True; self.joins = []
            def start(self): pass
            def join(self, timeout=None): self.joins.append(timeout); self.target(); self.alive = False
            def is_alive(self): return self.alive
        thread_holder = []
        def factory(**kwargs):
            thread = WThread(**kwargs); thread_holder.append(thread); return thread
        self.assertEqual(self.acceptance._close_bounded(FakeStream(), 100.01, clock=lambda: 100.0, thread_factory=factory), [])
        self.assertAlmostEqual(thread_holder[0].joins[0], 0.01, places=12)
        self.assertEqual(self.acceptance._close_bounded(FakeStream(), 101.0, clock=lambda: 100.0, thread_factory=factory), [])
        self.assertAlmostEqual(thread_holder[1].joins[0], 0.05, places=12)
        calls = []
        self.assertTrue(self.acceptance._close_bounded(FakeStream(), 100.0, clock=lambda: 100.0,
                                                       thread_factory=lambda **k: calls.append(k) or WThread(**k)))
        self.assertEqual(calls, [])

        class Clock:
            value = 100.0
            def __call__(self): return self.value
        class Crossing(WThread):
            def __init__(self, target=None, daemon=None): super().__init__(target, daemon)
            def join(self, timeout=None):
                self.joins.append(timeout); self.target(); self.alive = False; clock.value = 100.01
        clock = Clock()
        self.assertIn("close-completion-after-deadline",
                      self.acceptance._close_bounded(FakeStream(), 100.01, clock=clock,
                                                     thread_factory=lambda **k: Crossing(**k)))

    def test_write_and_close_thread_construction_failures_are_reported(self):
        class Proc:
            stdin = object()
        close_calls = []
        with mock.patch.object(self.acceptance, "_close_bounded", side_effect=lambda *a: close_calls.append(a) or ["close-error"]):
            errors = self.acceptance._write_stdin_bounded(
                Proc(), b"go\n", 100, clock=lambda: 0,
                thread_factory=lambda **kwargs: (_ for _ in ()).throw(RuntimeError("ctor")),
            )
        self.assertTrue(any("stdin-thread" in e for e in errors))
        self.assertEqual(len(close_calls), 1)
        with mock.patch.object(self.acceptance.threading, "Thread", side_effect=RuntimeError("ctor")):
            errors = self.acceptance._close_bounded(FakeStream(), 100, clock=lambda: 0)
        self.assertTrue(any("close-thread" in e for e in errors))

    def test_sentinel_teardown_reports_all_failures_and_uses_one_budget(self):
        class Sentinel:
            pid = 1
            def __init__(self):
                self.stdin = type("Stdin", (), {"write": lambda s, d: len(d), "flush": lambda s: None, "close": lambda s: None})()
                self.stdout = FakeStream(close_raises=True); self.stderr = FakeStream(close_raises=True)
                self.waits = 0
            def wait(self, timeout=None): self.waits += 1; raise RuntimeError("wait")
            def kill(self): raise RuntimeError("kill")
        sentinel = Sentinel()
        errors = self.acceptance._teardown_sentinel(sentinel, 10)
        self.assertTrue(any("sentinel-wait" in e for e in errors))
        self.assertTrue(any("sentinel-kill" in e for e in errors))
        self.assertTrue(any("sentinel-fallback-wait" in e for e in errors))
        self.assertGreaterEqual(sentinel.waits, 2)
        self.assertTrue(any("close" in e for e in errors))

        deadlines = []
        sentinel = type("S", (), {
            "stdin": type("Stdin", (), {"close": lambda s: None})(),
            "stdout": FakeStream(), "stderr": FakeStream(),
            "wait": lambda s, timeout=None: 0, "kill": lambda s: None,
        })()
        with mock.patch.object(self.acceptance, "_write_stdin_bounded", side_effect=lambda *a: deadlines.append(a[2]) or []), \
             mock.patch.object(self.acceptance, "_close_bounded", side_effect=lambda *a: deadlines.append(a[1]) or []), \
             mock.patch.object(self.acceptance, "_join_helper_threads", side_effect=lambda deadline: deadlines.append(deadline) or []):
            self.acceptance._teardown_sentinel(sentinel, 123)
        self.assertTrue(deadlines)
        self.assertEqual(set(deadlines), {123})

    def test_sentinel_teardown_exceptions_do_not_skip_remaining_steps(self):
        calls = []
        class Sentinel:
            pid = 1
            stdin = object(); stdout = FakeStream(); stderr = FakeStream()
            def wait(self, timeout=None):
                calls.append("wait")
                raise RuntimeError("wait")
            def kill(self):
                calls.append("kill")
                raise RuntimeError("kill")
        count = [0]
        def close(stream, deadline):
            count[0] += 1
            calls.append("close%d" % count[0])
            if count[0] == 1:
                raise RuntimeError("close")
            return ["close-error"]
        with mock.patch.object(self.acceptance, "_write_stdin_bounded", side_effect=RuntimeError("thread constructor failed")), \
             mock.patch.object(self.acceptance, "_close_bounded", side_effect=close), \
             mock.patch.object(self.acceptance, "_join_helper_threads", side_effect=RuntimeError("join")):
            errors = self.acceptance._teardown_sentinel(Sentinel(), 123)
        self.assertEqual(calls.count("wait"), 2)
        self.assertEqual(calls.count("kill"), 1)
        self.assertEqual(calls.count("close1"), 1)
        self.assertEqual(calls.count("close2"), 1)
        self.assertTrue(any("sentinel-stdin" in e for e in errors))
        self.assertTrue(any("sentinel-wait" in e for e in errors))
        self.assertTrue(any("sentinel-kill" in e for e in errors))
        self.assertTrue(any("sentinel-fallback-wait" in e for e in errors))
        self.assertTrue(any("sentinel-close" in e for e in errors))
        self.assertTrue(any("sentinel-helper-join" in e for e in errors))

    def test_run_cases_teardown_failure_forces_failed_even_when_cases_pass(self):
        class Sentinel:
            pid = 1; stdout = FakeStream(); stderr = FakeStream()
            def poll(self): return None
        passed = lambda work: {"status": "passed"}
        with tempfile.TemporaryDirectory() as folder, \
             mock.patch.object(self.acceptance.subprocess, "Popen", return_value=Sentinel()), \
             mock.patch.object(self.acceptance, "_read_handshake", return_value="SENTINEL-READY"), \
             mock.patch.object(self.acceptance, "_case_timeout_descendant", passed), \
             mock.patch.object(self.acceptance, "_case_pipe_holder", passed), \
             mock.patch.object(self.acceptance, "_case_parent_death", passed), \
             mock.patch.object(self.acceptance, "_case_preassignment_eof", passed), \
             mock.patch.object(self.acceptance, "_case_add_type", passed), \
             mock.patch.object(self.acceptance, "_teardown_sentinel", return_value=["sentinel-fail"]):
            result = self.acceptance.run_cases(Path(folder))
        self.assertEqual(result["status"], "failed", result)
        self.assertTrue(any(item.get("case") == "suite_cleanup" for item in result["cases"]))

    def test_run_cases_readiness_exception_does_not_skip_teardown(self):
        class Sentinel:
            pid = 1; stdout = FakeStream(); stderr = FakeStream()
            def poll(self): return None
        calls = []
        with tempfile.TemporaryDirectory() as folder, \
             mock.patch.object(self.acceptance.subprocess, "Popen", return_value=Sentinel()), \
             mock.patch.object(self.acceptance, "_read_handshake", side_effect=RuntimeError("read")), \
             mock.patch.object(self.acceptance, "_teardown_sentinel", side_effect=lambda *a: calls.append(a) or []):
            result = self.acceptance.run_cases(Path(folder))
        self.assertEqual(len(calls), 1)
        self.assertNotEqual(result["status"], "passed", result)

    def test_timeout_authorization_failure_is_terminal(self):
        class Job:
            def assign(self, proc): pass
            def is_process_in_job(self, proc): return True
            def active_processes(self): return 0
            def terminate_checked(self): pass
            def close_checked(self): pass
        class Proc:
            pid = 1; _handle = 1
            stdin = FakeStdin(); stdout = FakeStream(); stderr = FakeStream()
            returncode = 0
            def poll(self): return 0
            def wait(self, timeout=None): return 0
        with tempfile.TemporaryDirectory() as folder, \
             mock.patch.object(self.acceptance, "WindowsJob", Job), \
             mock.patch.object(self.acceptance.subprocess, "Popen", return_value=Proc()), \
             mock.patch.object(self.acceptance, "_write_stdin_bounded", return_value=["stdin-timeout"]):
            result = self.acceptance._case_timeout_descendant(Path(folder))
        self.assertNotEqual(result["status"], "passed", result)
        self.assertNotIn("descendant", result)

    def timeout_case(self, *, early_exit=False, identity=(None, "membership"),
                     active=2, active_raises=False, cleanup_fails=False,
                     member=True):
        class Clock:
            value = 100.0
            def __call__(self): return self.value
            def sleep(self, seconds): self.value += seconds
        class LineStream(FakeStream):
            def __init__(self, lines): super().__init__(); self.lines = list(lines)
            def readline(self, size=-1): return self.lines.pop(0) if self.lines else b""
            def read(self, size=-1): return b""
        class Stdin:
            def write(self, data): return len(data)
            def flush(self): pass
            def close(self): pass
        class Proc:
            pid = 701; _handle = 1
            def __init__(self):
                self.returncode = 0 if early_exit else None
                self.stdin = Stdin(); self.stdout = LineStream([b"READY 702\n"]); self.stderr = LineStream([])
            def poll(self): return self.returncode
            def wait(self, timeout=None): return self.returncode
        class Job:
            def __init__(self): self.terminated = False
            def assign(self, proc): pass
            def is_process_in_job(self, proc): return member
            def terminate_checked(self):
                if cleanup_fails: raise RuntimeError("terminate failed")
                self.terminated = True; proc.returncode = 0
            def active_processes(self):
                if active_raises: raise RuntimeError("active failed")
                return 0 if self.terminated else active
            def close_checked(self): pass
        clock = Clock()
        proc = Proc(); job = Job()
        with tempfile.TemporaryDirectory() as folder, \
             mock.patch.object(self.acceptance, "WindowsJob", return_value=job), \
             mock.patch.object(self.acceptance.subprocess, "Popen", return_value=proc), \
             mock.patch.object(self.acceptance, "_write_stdin_bounded", return_value=[]), \
             mock.patch.object(self.acceptance, "process_evidence", return_value=identity):
            result = self.acceptance._case_timeout_descendant(
                Path(folder), clock=clock, sleeper=clock.sleep, timeout_seconds=0.05
            )
        return result, proc, job

    def test_timeout_case_end_to_end_outcomes(self):
        success, _, _ = self.timeout_case(identity=({"pid": 702}, None), active=2)
        self.assertEqual(success["status"], "passed", success)
        self.assertEqual(success["detail"], "forced-timeout")

        early, _, _ = self.timeout_case(early_exit=True, identity=({"pid": 702}, None))
        self.assertEqual(early["status"], "failed", early)
        self.assertEqual(early["detail"], "worker-exited-early")

        missing, _, _ = self.timeout_case(identity=({"pid": 702}, None), active=1)
        self.assertEqual(missing["status"], "failed", missing)
        self.assertEqual(missing["detail"], "timeout-evidence-missing")

        no_identity, _, _ = self.timeout_case(identity=(None, "membership"))
        self.assertEqual(no_identity["status"], "blocked", no_identity)

        query, _, _ = self.timeout_case(identity=({"pid": 702}, None), active_raises=True)
        self.assertNotEqual(query["status"], "passed", query)
        self.assertIn("active_error", query)

        membership, _, _ = self.timeout_case(identity=({"pid": 702}, None), member=False)
        self.assertEqual(membership["status"], "failed", membership)
        self.assertEqual(membership["detail"], "membership")

    def test_timeout_cleanup_failure_after_valid_timeout_is_not_success(self):
        result, _, _ = self.timeout_case(
            identity=({"pid": 702}, None), cleanup_fails=True
        )
        self.assertNotEqual(result["status"], "passed", result)
        self.assertTrue(result["cleanup_errors"])

    def test_process_evidence_handle_scoped_failures(self):
        class Job:
            def __init__(self, member=True, raises=False): self.member = member; self.raises = raises
            def in_job(self, handle):
                if self.raises: raise RuntimeError("membership")
                return self.member
        class Api:
            def __init__(self, open_ok=True, image_ok=True, times_ok=True, created=1, close_ok=True):
                self.open_ok = open_ok; self.image_ok = image_ok; self.times_ok = times_ok
                self.created = created; self.close_ok = close_ok; self.opens = []; self.closed = []
            def OpenProcess(self, access, inherit, pid): self.opens.append((access, pid)); return 11 if self.open_ok else 0
            def QueryFullProcessImageNameW(self, handle, flags, buffer, size):
                if not self.image_ok: return 0
                buffer.value = "C:\\synthetic\\csc.exe"; return 1
            def GetProcessTimes(self, handle, created, exited, kernel, user):
                if not self.times_ok: return 0
                created._obj.value = self.created; return 1
            def CloseHandle(self, handle): self.closed.append(handle); return 1 if self.close_ok else 0
        cases = (
            (Api(open_ok=False), Job(), "open:"),
            (Api(), Job(member=False), "membership"),
            (Api(), Job(raises=True), "membership:"),
            (Api(image_ok=False), Job(), "image:"),
            (Api(times_ok=False), Job(), "times:"),
            (Api(created=0), Job(), "creation-zero"),
            (Api(close_ok=False), Job(), "close:"),
        )
        for api, job, expected in cases:
            with self.subTest(expected=expected), mock.patch.object(self.acceptance, "_win", return_value=api):
                identity, error = self.acceptance.process_evidence(701, job)
                self.assertIsNone(identity)
                self.assertTrue(error.startswith(expected), error)
                self.assertLessEqual(len(api.opens), 1)
        api = Api()
        with mock.patch.object(self.acceptance, "_win", return_value=api):
            identity, error = self.acceptance.process_evidence(701, Job())
        self.assertIsNone(error)
        self.assertEqual(identity["pid"], 701)
        self.assertEqual(api.opens, [(0x101000, 701)])
        self.assertEqual(api.closed, [11])

    def test_drain_messages_callback_order_and_port_errors(self):
        class Api:
            def __init__(self, sequence, error=None): self.sequence = list(sequence); self.error = error; self.events = []
            def GetQueuedCompletionStatus(self, port, count, key, overlay, timeout):
                self.events.append("poll")
                if self.sequence:
                    message, pid = self.sequence.pop(0); count._obj.value = message; overlay._obj.value = pid; return 1
                ctypes.set_last_error(self.error or 258); return 0
        job = object.__new__(self.acceptance.ObservedJob)
        job.handle = 1; job.port = 1; job.messages = []
        api = Api([(self.acceptance.JOB_OBJECT_MSG_NEW_PROCESS, 900), (7, 900)])
        job.api = api
        seen = []
        def callback(pid):
            seen.append(("callback", pid)); api.events.append("callback")
        error = job.drain_messages(2, callback, clock=lambda: 0)
        self.assertIsNone(error)
        self.assertEqual(seen, [("callback", 900)])
        self.assertEqual(api.events[:3], ["poll", "callback", "poll"])
        self.assertLess(api.events.index("poll"), len(api.events))
        self.assertEqual(job.messages, [(self.acceptance.JOB_OBJECT_MSG_NEW_PROCESS, 900), (7, 900)])

        job = object.__new__(self.acceptance.ObservedJob)
        job.handle = 1; job.port = 1; job.messages = []; job.api = Api([], error=5)
        self.assertEqual(job.drain_messages(2, None, clock=lambda: 0), "completion-port:5")
        job.api = Api([], error=258)
        self.assertIsNone(job.drain_messages(2, None, clock=lambda: 0))

    def test_pipe_case_uses_ordered_handshake_and_reports_missing_output(self):
        class Stream:
            def __init__(self, lines):
                self.lines = list(lines); self.closed = False
            def readline(self, size=-1):
                return (self.lines.pop(0) if self.lines else b"")
            def read(self, size=-1):
                return b""
            def close(self): self.closed = True
        class Stdin:
            def write(self, data): return len(data)
            def flush(self): pass
            def close(self): pass
        class Proc:
            pid = 800
            _handle = 1
            def __init__(self, lines):
                self.stdin = Stdin(); self.stdout = Stream(lines); self.stderr = Stream([])
                self.returncode = None
            def poll(self): return 0
            def wait(self, timeout=None): return 0
        class Job:
            def assign(self, proc): pass
            def is_process_in_job(self, proc): return True
            def terminate_checked(self): pass
            def active_processes(self): return 0
            def close_checked(self): pass
        with tempfile.TemporaryDirectory() as folder, \
             mock.patch.object(self.acceptance, "WindowsJob", Job), \
             mock.patch.object(self.acceptance.subprocess, "Popen", return_value=Proc([b"SYNTHETIC-PIPE\n", b"READY 801\n"])), \
             mock.patch.object(self.acceptance, "process_evidence", return_value=({"pid": 801, "image": "python.exe", "created": 1}, None)):
            result = self.acceptance._case_pipe_holder(Path(folder))
        self.assertEqual(result["status"], "passed", result)
        self.assertEqual(result["detail"], "stdout-observed")

        with tempfile.TemporaryDirectory() as folder, \
             mock.patch.object(self.acceptance, "WindowsJob", Job), \
             mock.patch.object(self.acceptance.subprocess, "Popen", return_value=Proc([b""])), \
             mock.patch.object(self.acceptance, "process_evidence", return_value=({"pid": 801, "image": "python.exe", "created": 1}, None)):
            result = self.acceptance._case_pipe_holder(Path(folder))
        self.assertEqual(result["status"], "failed", result)

        class FailingJob(Job):
            def close_checked(self): raise RuntimeError("job close failed")
        with tempfile.TemporaryDirectory() as folder, \
             mock.patch.object(self.acceptance, "WindowsJob", FailingJob), \
             mock.patch.object(self.acceptance.subprocess, "Popen", return_value=Proc([b"SYNTHETIC-PIPE\n", b"READY 801\n"])), \
             mock.patch.object(self.acceptance, "process_evidence", side_effect=RuntimeError("identity failed")):
            result = self.acceptance._case_pipe_holder(Path(folder))
        self.assertEqual(result["status"], "failed", result)
        self.assertTrue(any("job-close" in value for value in result["cleanup_errors"]))

        with tempfile.TemporaryDirectory() as folder, \
             mock.patch.object(self.acceptance, "WindowsJob", Job), \
             mock.patch.object(self.acceptance.subprocess, "Popen", return_value=Proc([b"SYNTHETIC-PIPE\n", b"READY 801\n"])), \
             mock.patch.object(self.acceptance, "_write_stdin_bounded", return_value=["stdin-timeout"]):
            result = self.acceptance._case_pipe_holder(Path(folder))
        self.assertNotEqual(result["status"], "passed", result)
        self.assertNotIn("descendant", result)

    def test_helper_entry_refuses_every_mode_without_supervisor_context(self):
        called = []
        for mode in ("timeout-descendant", "pipe-holder", "supervisor-death", "wait-for-go", "add-type-wait"):
            with self.subTest(mode=mode):
                with self.assertRaises(ValueError):
                    self.acceptance.helper_entry(mode, [], {}, self.root, action=lambda *a, **k: called.append(a))
        self.assertEqual(called, [])

    def test_open_process_handle_uses_query_and_synchronize_rights(self):
        class Api:
            def __init__(self): self.access = None
            def OpenProcess(self, access, inherit, pid):
                self.access = access
                return 1
        api = Api()
        with mock.patch.object(self.acceptance, "_win", return_value=api):
            handle, returned = self.acceptance._open_process_handle(900)
        self.assertEqual(handle, 1)
        self.assertEqual(api.access, 0x101001)

    def test_win_binds_is_process_in_job_abi(self):
        text = Path(self.acceptance.__file__).read_text(encoding="utf8")
        self.assertIn("api.IsProcessInJob.argtypes", text)
        self.assertIn("api.IsProcessInJob.restype", text)

    def test_observed_job_acquisition_failures_close_exactly_acquired_handles(self):
        class Api:
            def __init__(self):
                self.handle = 11
                self.port = 22
                self.closed = []
                self.set_results = [1, 1]
                self.port_result = 22
            def CreateJobObjectW(self, *a): return self.handle
            def SetInformationJobObject(self, *a): return self.set_results.pop(0) if self.set_results else 1
            def CreateIoCompletionPort(self, *a): return self.port_result
            def CloseHandle(self, h): self.closed.append(h); return 1

        api = Api(); api.port_result = 0
        with self.assertRaises(Exception):
            self.acceptance.ObservedJob(api=api)
        self.assertIn(11, api.closed)

        api = Api(); api.set_results = [1, 0]
        with self.assertRaises(Exception):
            self.acceptance.ObservedJob(api=api)
        self.assertIn(22, api.closed)
        self.assertIn(11, api.closed)

        api = Api()
        job = self.acceptance.ObservedJob(api=api)
        api.closed.clear()
        api_close = api.CloseHandle
        def close(h):
            if h == 22:
                return 0
            return api_close(h)
        api.CloseHandle = close
        with self.assertRaises(Exception):
            job.close_checked()
        self.assertEqual(job.port, 22)
        self.assertIsNone(job.handle)


if __name__ == "__main__":
    unittest.main(verbosity=2)
