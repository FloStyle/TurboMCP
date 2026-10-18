"""Bounded Windows read_process backend for the persistence collector.

The Python layer validates the request, constructs one fixed argv, runs the
checked-in PowerShell script as a hidden child, enforces the caller's monotonic
deadline, bounds stdout/stderr while reading, reaps the child, and validates one
complete JSON result. It never enumerates processes or reads a command line
itself and never launches an editor.

Tests use an injected runner and clock. The default runner is not exercised by
offline tests and no live collection is authorized by this module.
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


def _script_path():
    return Path(__file__).resolve().with_name(SCRIPT_NAME)


def _argv(pid):
    return [
        "powershell",
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
        message = json.loads(text)
    except (ValueError, TypeError):
        return _failure("unknown")
    if not isinstance(message, dict) or isinstance(message.get("schema"), bool) or message.get("schema") != PROCESS_SCHEMA:
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


class _PopenRunner:
    """Default bounded subprocess runner. Not executed by offline tests."""

    def run(self, argv, deadline_ns, clock):
        if os.name != "nt":
            return _runner_failure("unknown")
        startup = None
        flags = 0
        if os.name == "nt":
            startup = subprocess.STARTUPINFO()
            startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
            startup.wShowWindow = subprocess.SW_HIDE
            flags = getattr(subprocess, "CREATE_NO_WINDOW", 0)
        try:
            proc = subprocess.Popen(
                argv,
                stdin=subprocess.DEVNULL,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                shell=False,
                bufsize=0,
                startupinfo=startup,
                creationflags=flags,
            )
        except OSError as exc:
            return _runner_failure("permission" if getattr(exc, "winerror", None) == 5 else "unknown")

        out = bytearray()
        err = bytearray()
        overflow = threading.Event()
        thread_failure = threading.Event()

        def drain(stream, target):
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
                    pass

        out_thread = threading.Thread(target=drain, args=(proc.stdout, out), daemon=True)
        err_thread = threading.Thread(target=drain, args=(proc.stderr, err), daemon=True)
        out_thread.start()
        err_thread.start()

        timed_out = False
        cleanup_failed = False
        try:
            while proc.poll() is None:
                if overflow.is_set() or thread_failure.is_set():
                    break
                now = clock()
                if now >= deadline_ns:
                    timed_out = True
                    break
                remaining = max(0.001, min(0.05, (deadline_ns - now) / 1_000_000_000))
                try:
                    proc.wait(timeout=remaining)
                except subprocess.TimeoutExpired:
                    continue
            if timed_out or overflow.is_set() or thread_failure.is_set():
                try:
                    proc.kill()
                except Exception:
                    cleanup_failed = True
            try:
                proc.wait(timeout=5)
            except Exception:
                cleanup_failed = True
        finally:
            for thread in (out_thread, err_thread):
                thread.join(timeout=1.0)
                if thread.is_alive():
                    cleanup_failed = True
            if proc.poll() is None:
                try:
                    proc.kill()
                    proc.wait(timeout=5)
                except Exception:
                    cleanup_failed = True

        return {
            "returncode": proc.returncode,
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

    def _now(self):
        try:
            value = self.clock()
        except Exception:
            return None
        if isinstance(value, bool) or not isinstance(value, int):
            return None
        return value

    def read_process(self, pid, deadline_ns):
        if not _valid_pid(pid) or not _valid_deadline(deadline_ns):
            return _failure("unknown")
        if self.platform != "win32":
            return _failure("unknown")
        now = self._now()
        if now is None:
            return _failure("unknown")
        if now >= deadline_ns:
            return _failure("timeout")
        argv = _argv(pid)
        try:
            raw = self.runner.run(argv, deadline_ns, self.clock)
        except Exception:
            return _failure("unknown")
        if not _valid_runner_result(raw):
            return _failure("unknown")
        if raw["timed_out"]:
            return _failure("timeout")
        if raw["overflow"]:
            return _failure("incomplete")
        if raw["cleanup_failed"]:
            return _failure("unknown")
        if raw["error"] is not None:
            return _failure(raw["error"])
        if raw["returncode"] != 0:
            return _failure("provider_error")
        if raw["stderr"]:
            return _failure("provider_error")
        now = self._now()
        if now is None:
            return _failure("unknown")
        if now >= deadline_ns:
            return _failure("timeout")
        if len(raw["stdout"]) > MAX_CAPTURE:
            return _failure("incomplete")
        return _parse_result(pid, raw["stdout"])


def read_process(pid, deadline_ns, *, runner=None, clock=None, platform=None):
    return ProcessReader(runner=runner, clock=clock, platform=platform).read_process(pid, deadline_ns)
