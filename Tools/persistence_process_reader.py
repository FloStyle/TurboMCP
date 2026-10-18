"""Bounded Windows read_process backend for the persistence collector.

The Python layer validates the request, runs one fixed one-shot Python worker in
a fresh kill-on-close Job Object, sends only the validated PID, enforces the
caller's monotonic deadline, bounds stdout/stderr while reading, terminates and
accounts for the job, and validates one complete JSON result. It never
enumerates processes or reads a command line itself and never launches an
editor.

Tests use an injected runner, job, streams and clock. The default runner is not
exercised by offline tests and no live collection is authorized by this module.
"""

from __future__ import annotations

import json
import os
import subprocess
import sys
import threading
import time
from pathlib import Path


MAX_CAPTURE = 64 * 1024
SCRIPT_NAME = "read-persistence-process.ps1"
PROCESS_SCHEMA = 1
_ERRORS = {"timeout", "permission", "not_found", "provider_error", "incomplete", "ambiguous", "unknown"}


class _DuplicateKey(ValueError):
    pass


def _reject_nonfinite(value):
    raise ValueError("nonfinite JSON constant")


def _unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise _DuplicateKey("duplicate JSON key")
        result[key] = value
    return result


def _failure(error):
    if not isinstance(error, str) or error not in _ERRORS:
        error = "unknown"
    return {"ok": False, "error": error}


def _runner_failure(error):
    if not isinstance(error, str) or error not in _ERRORS:
        error = "unknown"
    return {
        "returncode": None,
        "stdout": b"",
        "stderr": b"",
        "timed_out": False,
        "overflow": False,
        "cleanup_failed": False,
        "error": error,
    }


def _valid_pid(value):
    return isinstance(value, int) and not isinstance(value, bool) and 1 <= value <= 0xFFFFFFFF


def _valid_deadline(value):
    return isinstance(value, int) and not isinstance(value, bool)


class _ClockError(Exception):
    pass


class _ClockTracker:
    def __init__(self, clock):
        self.clock = clock
        self.last = None

    def __call__(self):
        try:
            value = self.clock()
        except Exception:
            raise _ClockError()
        if isinstance(value, bool) or not isinstance(value, int):
            raise _ClockError()
        if self.last is not None and value < self.last:
            raise _ClockError()
        self.last = value
        return value


def _script_path():
    return Path(__file__).resolve().with_name(SCRIPT_NAME)


def _powershell_path():
    root = os.environ.get("SystemRoot") or os.environ.get("WINDIR")
    if not root:
        return None
    return str(Path(root) / "System32" / "WindowsPowerShell" / "v1.0" / "powershell.exe")


def _argv(pid, powershell):
    return [
        powershell,
        "-NoProfile",
        "-NonInteractive",
        "-File",
        str(_script_path()),
        "-ProcessId",
        str(pid),
    ]


def _valid_runner_result(value):
    return (
        isinstance(value, dict)
        and set(value) == {
            "returncode", "stdout", "stderr", "timed_out",
            "overflow", "cleanup_failed", "error",
        }
        and (value["returncode"] is None or (
            isinstance(value["returncode"], int) and not isinstance(value["returncode"], bool)
        ))
        and isinstance(value["stdout"], (bytes, bytearray))
        and isinstance(value["stderr"], (bytes, bytearray))
        and isinstance(value["timed_out"], bool)
        and isinstance(value["overflow"], bool)
        and isinstance(value["cleanup_failed"], bool)
        and (value["error"] is None or (
            isinstance(value["error"], str) and value["error"] in _ERRORS
        ))
    )


def _parse_result(pid, stdout):
    try:
        text = bytes(stdout).decode("utf-8")
    except UnicodeDecodeError:
        return _failure("unknown")
    try:
        message = json.loads(
            text,
            object_pairs_hook=_unique_object,
            parse_constant=_reject_nonfinite,
        )
    except (ValueError, TypeError, RecursionError):
        return _failure("unknown")
    if not isinstance(message, dict) or type(message.get("schema")) is not int or message.get("schema") != PROCESS_SCHEMA:
        return _failure("unknown")
    if message.get("ok") is True:
        if set(message) != {
            "schema", "ok", "pid", "start_ticks", "executable_path", "command_line"
        }:
            return _failure("unknown")
        if (
            isinstance(message.get("pid"), bool)
            or not isinstance(message.get("pid"), int)
            or message.get("pid") != pid
            or isinstance(message.get("start_ticks"), bool)
            or not isinstance(message.get("start_ticks"), int)
            or message["start_ticks"] <= 0
            or not isinstance(message.get("executable_path"), str)
            or not message["executable_path"]
            or len(message["executable_path"]) > 4096
            or not isinstance(message.get("command_line"), str)
            or not message["command_line"]
            or len(message["command_line"]) > 32768
        ):
            return _failure("unknown")
        return {
            "ok": True,
            "value": {
                "pid": message["pid"],
                "start_ticks": message["start_ticks"],
                "executable_path": message["executable_path"],
                "command_line": message["command_line"],
            },
        }
    if message.get("ok") is False:
        if set(message) != {"schema", "ok", "error"}:
            return _failure("unknown")
        return _failure(message.get("error"))
    return _failure("unknown")


CLEANUP_LIMIT_NS = 5_000_000_000


def _default_job():
    from execution_runtime import WindowsJob
    return WindowsJob()


class _PopenRunner:
    """Bounded worker orchestration; no native operation on import."""

    def __init__(self, job_factory=None, worker_script=None, sleeper=None,
                 auth_thread_factory=None):
        self.job_factory = job_factory or _default_job
        self.worker_script = Path(worker_script) if worker_script else Path(__file__).with_name("execution_runtime.py")
        self.sleeper = sleeper or time.sleep
        self.auth_thread_factory = auth_thread_factory or threading.Thread

    def _wait_thread(self, thread, clock, cleanup_deadline):
        if thread is None or cleanup_deadline is None:
            return False
        try:
            now = clock()
        except _ClockError:
            return False
        if now >= cleanup_deadline:
            return False
        try:
            if not thread.is_alive():
                return True
        except Exception:
            return False
        while True:
            try:
                now = clock()
            except _ClockError:
                return False
            if now >= cleanup_deadline:
                return False
            remaining_ns = cleanup_deadline - now
            timeout = min(0.05, remaining_ns / 1_000_000_000)
            if timeout <= 0:
                return False
            try:
                thread.join(timeout=timeout)
            except Exception:
                return False
            try:
                alive = thread.is_alive()
            except Exception:
                return False
            if not alive:
                try:
                    now = clock()
                except _ClockError:
                    return False
                return now < cleanup_deadline

    def _close_bounded(self, stream, clock, cleanup_deadline):
        error = threading.Event()

        def close():
            try:
                stream.close()
            except Exception:
                error.set()

        try:
            thread = threading.Thread(target=close, daemon=True)
            thread.start()
        except Exception:
            return False
        if not self._wait_thread(thread, clock, cleanup_deadline):
            return False
        return not error.is_set()

    def _cleanup(self, job, proc, drains, request, clock, contained,
                 auth_thread, thread_failure):
        failed = False
        try:
            started = clock()
            cleanup_deadline = started + CLEANUP_LIMIT_NS
        except _ClockError:
            cleanup_deadline = None
            failed = True

        if job is not None:
            try:
                job.terminate_checked()
            except Exception:
                failed = True
            if cleanup_deadline is not None:
                while True:
                    try:
                        active = job.active_processes()
                    except Exception:
                        failed = True
                        break
                    if active == 0:
                        break
                    try:
                        now = clock()
                    except _ClockError:
                        failed = True
                        break
                    if now >= cleanup_deadline:
                        failed = True
                        break
                    try:
                        self.sleeper(min(0.05, (cleanup_deadline - now) / 1_000_000_000))
                    except Exception:
                        failed = True
                        break
            try:
                job.close_checked()
            except Exception:
                failed = True

        if proc is not None and not contained:
            try:
                if proc.poll() is None:
                    proc.kill()
            except Exception:
                failed = True

        if auth_thread is not None:
            if not self._wait_thread(auth_thread, clock, cleanup_deadline):
                failed = True
            elif request is not None:
                if not self._close_bounded(request, clock, cleanup_deadline):
                    failed = True
        elif request is not None:
            if not self._close_bounded(request, clock, cleanup_deadline):
                failed = True

        for thread, stream, done in drains:
            if thread is None:
                if not self._close_bounded(stream, clock, cleanup_deadline):
                    failed = True
                continue
            if not self._wait_thread(thread, clock, cleanup_deadline):
                failed = True
                if not self._close_bounded(stream, clock, cleanup_deadline):
                    failed = True
                continue
            if not done.is_set():
                failed = True
                if not self._close_bounded(stream, clock, cleanup_deadline):
                    failed = True
        if thread_failure.is_set():
            failed = True

        if proc is not None:
            try:
                if cleanup_deadline is None:
                    proc.wait(timeout=0)
                else:
                    now = clock()
                    remaining = max(0, cleanup_deadline - now) / 1_000_000_000
                    proc.wait(timeout=remaining)
            except Exception:
                failed = True
        return failed

    def run(self, argv, deadline_ns, clock):
        if os.name != "nt":
            return _runner_failure("unknown")
        if not isinstance(clock, _ClockTracker):
            clock = _ClockTracker(clock)
        try:
            pid = int(argv[-1])
        except (TypeError, ValueError, IndexError):
            return _runner_failure("unknown")
        if not _valid_pid(pid):
            return _runner_failure("unknown")

        out = bytearray()
        err = bytearray()
        out_thread = None
        err_thread = None
        out_done = threading.Event()
        err_done = threading.Event()
        auth_done = threading.Event()
        auth_error = threading.Event()
        auth_thread = None
        drains = []
        timed_out = False
        overflow = threading.Event()
        thread_failure = threading.Event()
        error = None
        cleanup_failed = False
        proc = None
        request = None
        contained = False
        job = None
        startup = subprocess.STARTUPINFO()
        startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
        startup.wShowWindow = subprocess.SW_HIDE
        flags = getattr(subprocess, "CREATE_NO_WINDOW", 0)
        worker_argv = [sys.executable, str(self.worker_script), "--persistence-worker"]

        def drain(stream, target, done):
            try:
                while True:
                    chunk = stream.read(4096)
                    if not chunk:
                        return
                    if len(target) + len(chunk) > MAX_CAPTURE:
                        overflow.set()
                        return
                    target.extend(chunk)
            except Exception:
                thread_failure.set()
            finally:
                try:
                    stream.close()
                except Exception:
                    thread_failure.set()
                done.set()

        try:
            job = self.job_factory()
        except Exception:
            return _runner_failure("unknown")
        if job is None:
            return _runner_failure("unknown")

        try:
            now = clock()
        except _ClockError:
            cleanup_failed = self._cleanup(job, None, [], None, clock, False, None, thread_failure)
            result = _runner_failure("unknown")
            result["cleanup_failed"] = cleanup_failed
            return result
        if now >= deadline_ns:
            cleanup_failed = self._cleanup(job, None, [], None, clock, False, None, thread_failure)
            result = _runner_failure("timeout")
            result["cleanup_failed"] = cleanup_failed
            return result

        try:
            proc = subprocess.Popen(
                worker_argv,
                stdin=subprocess.PIPE,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                shell=False,
                bufsize=0,
                startupinfo=startup,
                creationflags=flags,
            )
            request = proc.stdin
            drains = [
                (None, proc.stdout, out_done),
                (None, proc.stderr, err_done),
            ]
            job.assign(proc)
            if not job.is_process_in_job(proc):
                raise RuntimeError("worker is not assigned to the job")
            contained = True
            try:
                now = clock()
            except _ClockError:
                error = "unknown"
            else:
                if now >= deadline_ns:
                    timed_out = True
                else:
                    payload = json.dumps(
                        {"schema": 1, "pid": pid}, separators=(",", ":")
                    ).encode("utf-8")

                    def authorize():
                        try:
                            written = request.write(payload)
                            if written != len(payload):
                                auth_error.set()
                        except Exception:
                            auth_error.set()
                        finally:
                            try:
                                request.close()
                            except Exception:
                                auth_error.set()
                            auth_done.set()

                    try:
                        auth_thread = self.auth_thread_factory(target=authorize, daemon=True)
                        auth_thread.start()
                    except Exception:
                        auth_thread = None
                        error = "unknown"
                    else:
                        while not auth_done.is_set():
                            try:
                                now = clock()
                            except _ClockError:
                                error = "unknown"
                                break
                            if now >= deadline_ns:
                                timed_out = True
                                break
                            try:
                                self.sleeper(min(0.05, (deadline_ns - now) / 1_000_000_000))
                            except Exception:
                                error = "unknown"
                                break
                        if auth_done.is_set():
                            try:
                                now = clock()
                            except _ClockError:
                                error = "unknown"
                            else:
                                if now >= deadline_ns:
                                    timed_out = True
                        if error is None and not timed_out:
                            if auth_error.is_set():
                                error = "unknown"
                            else:
                                request = None
                                drains = [
                                    (None, proc.stdout, out_done),
                                    (None, proc.stderr, err_done),
                                ]
                                out_thread = threading.Thread(
                                    target=drain, args=(proc.stdout, out, out_done), daemon=True
                                )
                                drains[0] = (out_thread, proc.stdout, out_done)
                                err_thread = threading.Thread(
                                    target=drain, args=(proc.stderr, err, err_done), daemon=True
                                )
                                drains[1] = (err_thread, proc.stderr, err_done)
                                out_thread.start()
                                err_thread.start()
                                while proc.poll() is None:
                                    if overflow.is_set() or thread_failure.is_set():
                                        break
                                    try:
                                        now = clock()
                                    except _ClockError:
                                        error = "unknown"
                                        break
                                    if now >= deadline_ns:
                                        timed_out = True
                                        break
                                    remaining = max(0.001, min(0.05, (deadline_ns - now) / 1_000_000_000))
                                    try:
                                        proc.wait(timeout=remaining)
                                    except subprocess.TimeoutExpired:
                                        continue
        except OSError as exc:
            error = "permission" if getattr(exc, "winerror", None) == 5 else "unknown"
        except (RuntimeError, ValueError, TypeError):
            error = "unknown"
        finally:
            cleanup_failed = self._cleanup(
                job, proc, drains, request, clock, contained, auth_thread, thread_failure
            )

        if error is not None:
            result = _runner_failure(error)
            result["cleanup_failed"] = cleanup_failed
            return result
        return {
            "returncode": proc.returncode if proc is not None else None,
            "stdout": bytes(out),
            "stderr": bytes(err),
            "timed_out": timed_out,
            "overflow": overflow.is_set() or thread_failure.is_set(),
            "cleanup_failed": cleanup_failed,
            "error": None,
        }


class ProcessReader:
    def __init__(self, runner=None, clock=None, platform=None):
        self.runner = runner if runner is not None else _PopenRunner()
        self.clock = clock if clock is not None else time.monotonic_ns
        self.platform = platform if platform is not None else sys.platform

    def _now(self, clock):
        try:
            return clock()
        except _ClockError:
            return None

    def read_process(self, pid, deadline_ns):
        if not _valid_pid(pid) or not _valid_deadline(deadline_ns):
            return _failure("unknown")
        if self.platform != "win32":
            return _failure("unknown")
        clock = _ClockTracker(self.clock)
        now = self._now(clock)
        if now is None:
            return _failure("unknown")
        if now >= deadline_ns:
            return _failure("timeout")
        powershell = _powershell_path()
        if not powershell:
            return _failure("unknown")
        try:
            argv = _argv(pid, powershell)
        except Exception:
            return _failure("unknown")
        now = self._now(clock)
        if now is None:
            return _failure("unknown")
        if now >= deadline_ns:
            return _failure("timeout")
        try:
            raw = self.runner.run(argv, deadline_ns, clock)
        except Exception:
            return _failure("unknown")
        if not _valid_runner_result(raw):
            return _failure("unknown")
        if raw["cleanup_failed"]:
            return _failure("unknown")
        if raw["timed_out"]:
            return _failure("timeout")
        if raw["overflow"]:
            return _failure("incomplete")
        if raw["error"] is not None:
            return _failure(raw["error"])
        if raw["returncode"] != 0:
            return _failure("provider_error")
        if raw["stderr"]:
            return _failure("provider_error")
        now = self._now(clock)
        if now is None:
            return _failure("unknown")
        if now >= deadline_ns:
            return _failure("timeout")
        if len(raw["stdout"]) > MAX_CAPTURE:
            return _failure("incomplete")
        return _parse_result(pid, raw["stdout"])


def read_process(pid, deadline_ns, *, runner=None, clock=None, platform=None):
    return ProcessReader(runner=runner, clock=clock, platform=platform).read_process(pid, deadline_ns)
