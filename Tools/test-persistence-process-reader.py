"""Offline tests for the bounded Windows process reader.

All execution is injected. These tests never spawn PowerShell, enumerate a
process, query CIM or read a real command line. A separate source inspection
checks the fixed script and subprocess launch policy.
"""

from __future__ import annotations

import json
import os
import ctypes
import unittest
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
        if self.behavior == "deferred" and not self.finished:
            self.target(*self.args)
            self.finished = True
        self.joined = True

    def is_alive(self):
        if self.behavior == "stuck":
            return self.started
        if self.behavior == "deferred":
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
        clock = clock if clock is not None else Clock([0, 10, 20, 30, 40, 50, 60])
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
            if len(attempts) <= 2:
                raise RuntimeError("ctor failed")
            return FakeThread(target=target, args=args, daemon=daemon, behavior="run")

        with mock.patch("persistence_process_reader.subprocess.Popen", return_value=proc), \
             mock.patch("persistence_process_reader.threading.Thread", side_effect=thread_factory):
            result = reader._PopenRunner(
                job_factory=lambda: job,
                worker_script="worker.py",
                sleeper=lambda _: None,
                auth_thread_factory=ThreadFactory(["run"]),
            ).run(["fixed", str(PID)], 1000, Clock([0, 10, 20, 30, 40, 50, 60]))
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

    def test_timeout_uses_job_and_shared_cleanup(self):
        job = FakeJob(active=[1, 1, 0])
        proc = FakeProcess(returncode=None, timeout_once=False, stdin=FakeStdin(events=job.events))
        result, _, job, sleeps = self.run_with(
            proc,
            ThreadFactory(["run", "run"]),
            Clock([0, 10, 20, 1000, 1010, 1020, 1030, 1040, 1050, 1060]),
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


if __name__ == "__main__":
    unittest.main(verbosity=2)
