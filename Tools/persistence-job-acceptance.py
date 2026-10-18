"""Supervisor-only synthetic containment acceptance for R04.

Import-safe: no process, job, PowerShell or native call is made at import time.
Normal execution requires the supervisor context written by execute-tests.py.
This script exercises fixed synthetic analogues; it does not prove the production
reader's orchestration and never inspects a real user process command line.
"""

from __future__ import annotations

import ctypes
from ctypes import wintypes as W
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import threading
import time
import uuid

from execution_runtime import BASIC_ACCOUNTING, EXTENDED, WindowsJob, canonical, parent_ids

ROOT = Path(__file__).resolve().parents[1]
CASE_DEADLINE_SECONDS = 30
CLEANUP_LIMIT_SECONDS = 5
JOB_OBJECT_MSG_ACTIVE_PROCESS_ZERO = 4
JOB_OBJECT_MSG_NEW_PROCESS = 6
JOB_OBJECT_MSG_EXIT_PROCESS = 7
JOB_OBJECT_MSG_ABNORMAL_EXIT_PROCESS = 8
JOB_OBJECT_ASSOCIATE_COMPLETION_PORT_INFORMATION = 7
HELPER_THREADS = []


def _join_helper_threads(deadline):
    errors = []
    for thread in list(HELPER_THREADS):
        while thread.is_alive() and time.monotonic() < deadline:
            thread.join(min(0.05, max(0, deadline - time.monotonic())))
        if thread.is_alive():
            errors.append("helper-thread-timeout")
        elif thread in HELPER_THREADS:
            HELPER_THREADS.remove(thread)
    return errors


class JOBOBJECT_ASSOCIATE_COMPLETION_PORT(ctypes.Structure):
    _fields_ = [("CompletionKey", ctypes.c_void_p), ("CompletionPort", W.HANDLE)]


def _win():
    api = ctypes.WinDLL("kernel32", use_last_error=True)
    api.CreateJobObjectW.argtypes = [ctypes.c_void_p, W.LPCWSTR]
    api.CreateJobObjectW.restype = W.HANDLE
    api.SetInformationJobObject.argtypes = [W.HANDLE, ctypes.c_int, ctypes.c_void_p, W.DWORD]
    api.SetInformationJobObject.restype = W.BOOL
    api.AssignProcessToJobObject.argtypes = [W.HANDLE, W.HANDLE]
    api.AssignProcessToJobObject.restype = W.BOOL
    api.TerminateJobObject.argtypes = [W.HANDLE, W.UINT]
    api.TerminateJobObject.restype = W.BOOL
    api.QueryInformationJobObject.argtypes = [W.HANDLE, ctypes.c_int, ctypes.c_void_p, W.DWORD, ctypes.POINTER(W.DWORD)]
    api.QueryInformationJobObject.restype = W.BOOL
    api.CreateIoCompletionPort.argtypes = [W.HANDLE, W.HANDLE, ctypes.c_size_t, W.DWORD]
    api.CreateIoCompletionPort.restype = W.HANDLE
    api.GetQueuedCompletionStatus.argtypes = [W.HANDLE, ctypes.POINTER(W.DWORD), ctypes.POINTER(ctypes.c_size_t), ctypes.POINTER(ctypes.c_void_p), W.DWORD]
    api.GetQueuedCompletionStatus.restype = W.BOOL
    api.OpenProcess.argtypes = [W.DWORD, W.BOOL, W.DWORD]
    api.OpenProcess.restype = W.HANDLE
    api.QueryFullProcessImageNameW.argtypes = [W.HANDLE, W.DWORD, W.LPWSTR, ctypes.POINTER(W.DWORD)]
    api.QueryFullProcessImageNameW.restype = W.BOOL
    api.GetProcessTimes.argtypes = [W.HANDLE, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p]
    api.GetProcessTimes.restype = W.BOOL
    api.IsProcessInJob.argtypes = [W.HANDLE, W.HANDLE, ctypes.POINTER(W.BOOL)]
    api.IsProcessInJob.restype = W.BOOL
    api.TerminateProcess.argtypes = [W.HANDLE, W.UINT]
    api.TerminateProcess.restype = W.BOOL
    api.CloseHandle.argtypes = [W.HANDLE]
    api.CloseHandle.restype = W.BOOL
    return api


class ObservedJob:
    """Synthetic inner job with a completion port; not a production reader seam."""
    def __init__(self, api=None):
        self.api = api or _win()
        self.handle = None
        self.port = None
        try:
            self.handle = self.api.CreateJobObjectW(None, None)
            if not self.handle:
                raise ctypes.WinError(ctypes.get_last_error())
            limits = EXTENDED()
            limits.BasicLimitInformation.LimitFlags = 0x2000
            if not self.api.SetInformationJobObject(self.handle, 9, ctypes.byref(limits), ctypes.sizeof(limits)):
                raise ctypes.WinError(ctypes.get_last_error())
            self.port = self.api.CreateIoCompletionPort(W.HANDLE(-1), None, 0, 1)
            if not self.port:
                raise ctypes.WinError(ctypes.get_last_error())
            assoc = JOBOBJECT_ASSOCIATE_COMPLETION_PORT(1, self.port)
            if not self.api.SetInformationJobObject(
                    self.handle, JOB_OBJECT_ASSOCIATE_COMPLETION_PORT_INFORMATION,
                    ctypes.byref(assoc), ctypes.sizeof(assoc)):
                raise ctypes.WinError(ctypes.get_last_error())
        except Exception as original:
            cleanup_errors = self.close_best_effort()
            if cleanup_errors:
                raise RuntimeError(str(original) + "; cleanup:" + ";".join(cleanup_errors)) from original
            raise
        self.messages = []

    def close_best_effort(self):
        errors = []
        if self.port:
            try:
                if self.api.CloseHandle(self.port):
                    self.port = None
                else:
                    errors.append("completion-port-close:" + str(ctypes.get_last_error()))
            except Exception as exc:
                errors.append("completion-port-close:" + str(exc))
        if self.handle:
            try:
                if self.api.CloseHandle(self.handle):
                    self.handle = None
                else:
                    errors.append("job-close:" + str(ctypes.get_last_error()))
            except Exception as exc:
                errors.append("job-close:" + str(exc))
        return errors

    def assign(self, proc):
        if not self.api.AssignProcessToJobObject(self.handle, W.HANDLE(int(proc._handle))):
            raise ctypes.WinError(ctypes.get_last_error())

    def active_processes(self):
        info = BASIC_ACCOUNTING()
        if not self.api.QueryInformationJobObject(self.handle, 1, ctypes.byref(info), ctypes.sizeof(info), None):
            raise ctypes.WinError(ctypes.get_last_error())
        return int(info.ActiveProcesses)

    def in_job(self, handle):
        present = W.BOOL()
        if not self.api.IsProcessInJob(handle, self.handle, ctypes.byref(present)):
            raise ctypes.WinError(ctypes.get_last_error())
        return bool(present.value)

    def is_process_in_job(self, process):
        return self.in_job(W.HANDLE(int(process._handle)))

    def terminate_checked(self):
        if not self.api.TerminateJobObject(self.handle, 125):
            raise ctypes.WinError(ctypes.get_last_error())

    def close_checked(self):
        errors = []
        if self.port:
            if not self.api.CloseHandle(self.port):
                errors.append("completion-port-close:" + str(ctypes.get_last_error()))
            else:
                self.port = None
        if self.handle:
            if not self.api.CloseHandle(self.handle):
                errors.append("job-close:" + str(ctypes.get_last_error()))
            else:
                self.handle = None
        if errors:
            raise RuntimeError(";".join(errors))

    def drain_messages(self, deadline, on_new_process=None, clock=time.monotonic):
        error = None
        while self.handle and clock() < deadline:
            remaining = max(0, int((deadline - clock()) * 1000))
            if remaining == 0:
                break
            count = W.DWORD()
            key = ctypes.c_size_t()
            overlay = ctypes.c_void_p()
            if not self.api.GetQueuedCompletionStatus(self.port, ctypes.byref(count), ctypes.byref(key), ctypes.byref(overlay), remaining):
                code = ctypes.get_last_error()
                if code == 258:
                    break
                error = "completion-port:" + str(code)
                break
            message = int(count.value)
            pid = int(overlay.value or 0)
            self.messages.append((message, pid))
            if message == JOB_OBJECT_MSG_NEW_PROCESS and on_new_process is not None:
                on_new_process(pid)
        return error


def process_evidence(pid, job):
    """Immediate handle-scoped identity; the handle is closed after observation."""
    api = _win()
    handle = api.OpenProcess(0x101000, False, pid)
    if not handle:
        return None, "open:" + str(ctypes.get_last_error())
    identity = None
    error = None
    try:
        try:
            if hasattr(job, "in_job"):
                member = job.in_job(handle)
            else:
                class HandleHolder:
                    _handle = handle
                member = job.is_process_in_job(HandleHolder())
            if not member:
                error = "membership"
            else:
                size = W.DWORD(32768)
                buffer = ctypes.create_unicode_buffer(size.value)
                if not api.QueryFullProcessImageNameW(handle, 0, buffer, ctypes.byref(size)):
                    error = "image:" + str(ctypes.get_last_error())
                else:
                    created = ctypes.c_ulonglong()
                    exited = ctypes.c_ulonglong()
                    kernel = ctypes.c_ulonglong()
                    user = ctypes.c_ulonglong()
                    if not api.GetProcessTimes(handle, ctypes.byref(created), ctypes.byref(exited), ctypes.byref(kernel), ctypes.byref(user)):
                        error = "times:" + str(ctypes.get_last_error())
                    elif created.value == 0:
                        error = "creation-zero"
                    else:
                        identity = {"pid": int(pid), "image": buffer.value, "created": int(created.value), "handle_closed": True}
        except Exception as exc:
            error = "membership:" + str(exc)
    finally:
        if not api.CloseHandle(handle):
            error = ((error + ";") if error else "") + "close:" + str(ctypes.get_last_error())
            identity = None
    return identity, error


def validate_admission(env, repo, parent_resolver=parent_ids):
    """Pure supervisor-context validation used by offline tests."""
    if env.get("TURBOMCP_EXECUTION_CONTEXT") is None or env.get("TURBOMCP_EXECUTION_TOKEN") is None:
        raise ValueError("supervisor context is missing")
    context = json.loads(Path(env["TURBOMCP_EXECUTION_CONTEXT"]).read_text(encoding="utf8"))
    if set(context) != {"repo", "engine", "pid", "token"}:
        raise ValueError("supervisor context has unknown fields")
    if context["repo"] != canonical(repo):
        raise ValueError("supervisor context repo mismatch")
    if context["engine"] != "":
        raise ValueError("persistence-native must not bind an engine")
    if context["token"] != env["TURBOMCP_EXECUTION_TOKEN"]:
        raise ValueError("supervisor token mismatch")
    if type(context["pid"]) is not int or context["pid"] not in parent_resolver():
        raise ValueError("supervisor is not a live ancestor")
    return context


def aggregate(results, required):
    """Missing/blocked evidence is not success."""
    by_name = {r["case"]: r for r in results}
    missing = [name for name in required if name not in by_name]
    if missing:
        return "blocked", missing
    if any(by_name[name]["status"] != "passed" for name in required):
        status = "blocked" if any(by_name[name]["status"] == "blocked" for name in required) else "failed"
        return status, []
    return "passed", []


def _wait_for_file(path, deadline):
    while time.monotonic() < deadline:
        if path.exists():
            return True
        time.sleep(0.01)
    return False


def _wait_for_exit(proc, deadline):
    while time.monotonic() < deadline:
        if proc.poll() is not None:
            return True
        time.sleep(0.01)
    return False


def _write_stdin_bounded(proc, data, deadline, clock=time.monotonic, thread_factory=None):
    """deadline and clock values are monotonic seconds, matching Thread.join."""
    result = []
    try:
        expired = clock() >= deadline
    except Exception as exc:
        return ["stdin-clock:" + str(exc)]
    if expired:
        result.append("stdin-deadline-expired")
        result.extend(_close_bounded(proc.stdin, deadline, clock))
        return result

    def write():
        try:
            if data is not None:
                written = proc.stdin.write(data)
                if type(written) is not int or isinstance(written, bool) or written != len(data):
                    result.append("stdin-write-not-confirmed")
                    return
                proc.stdin.flush()
        except Exception as exc:
            result.append("stdin:" + str(exc))

    factory = thread_factory or threading.Thread
    try:
        thread = factory(target=write, daemon=True)
    except Exception as exc:
        result.append("stdin-thread:" + str(exc))
        result.extend(_close_bounded(proc.stdin, deadline, clock))
        return result
    HELPER_THREADS.append(thread)
    try:
        thread.start()
    except Exception as exc:
        result.append("stdin-thread:" + str(exc))
    else:
        while thread.is_alive() and clock() < deadline:
            remaining_seconds = max(0.0, deadline - clock())
            thread.join(min(0.05, remaining_seconds))
        if thread.is_alive():
            result.append("stdin-timeout")
        elif clock() >= deadline:
            result.append("stdin-completion-after-deadline")
    result.extend(_close_bounded(proc.stdin, deadline, clock))
    return result


def _close_bounded(stream, deadline, clock=time.monotonic, sleeper=time.sleep,
                   thread_factory=None):
    """deadline and clock values are monotonic seconds, matching Thread.join."""
    if stream is None:
        return []
    try:
        if clock() >= deadline:
            return ["close-deadline-expired"]
    except Exception as exc:
        return ["close-clock:" + str(exc)]
    error = []
    def close():
        try:
            stream.close()
        except Exception as exc:
            error.append("close:" + str(exc))
    factory = thread_factory or threading.Thread
    try:
        thread = factory(target=close, daemon=True)
    except Exception as exc:
        return ["close-thread:" + str(exc)]
    HELPER_THREADS.append(thread)
    try:
        thread.start()
    except Exception as exc:
        return ["close-thread:" + str(exc)]
    while thread.is_alive() and clock() < deadline:
        remaining_seconds = max(0.0, deadline - clock())
        thread.join(min(0.05, remaining_seconds))
    if thread.is_alive():
        return ["close-timeout"]
    if clock() >= deadline:
        return ["close-completion-after-deadline"]
    return error


def _read_all_bounded(stream, deadline):
    result = {}
    def read():
        try:
            data = bytearray()
            while len(data) <= 65536:
                chunk = stream.read(4096)
                if not chunk:
                    break
                data.extend(chunk)
            if len(data) > 65536:
                result["error"] = "drain-overflow"
            else:
                result["data"] = bytes(data)
        except Exception as exc:
            result["error"] = str(exc)
    thread = threading.Thread(target=read, daemon=True)
    HELPER_THREADS.append(thread)
    try:
        thread.start()
    except Exception as exc:
        return False, "read-thread:" + str(exc)
    while thread.is_alive() and time.monotonic() < deadline:
        thread.join(min(0.05, max(0, deadline - time.monotonic())))
    if thread.is_alive():
        return False, "drain-timeout"
    if "error" in result:
        return False, "drain-error:" + result["error"]
    return True, "eof"


def cleanup_job(job, proc, deadline, uncontained=False, clock=time.monotonic,
                sleeper=time.sleep, drain=False):
    """Truthful cleanup: explicit zero while the job handle remains open."""
    errors = []
    if uncontained and proc is not None:
        try:
            if proc.poll() is None:
                proc.kill()
        except Exception as exc:
            errors.append("kill:" + str(exc))
    if job is not None:
        try:
            job.terminate_checked()
        except Exception as exc:
            errors.append("terminate:" + str(exc))
        observed_zero = False
        while clock() < deadline:
            try:
                active = job.active_processes()
            except Exception as exc:
                errors.append("active:" + str(exc))
                break
            if active == 0:
                observed_zero = True
                break
            sleeper(min(0.05, max(0, deadline - clock())))
        if not observed_zero:
            errors.append("active-not-zero")
        try:
            job.close_checked()
        except Exception as exc:
            errors.append("job-close:" + str(exc))
    if proc is not None:
        while proc.poll() is None and clock() < deadline:
            sleeper(min(0.05, max(0, deadline - clock())))
        if proc.poll() is None:
            errors.append("worker-exit")
        if drain and proc.stdout is not None:
            complete, detail = _read_all_bounded(proc.stdout, deadline)
            if not complete:
                errors.append(detail)
        if proc.stdin is not None:
            errors.extend(_close_bounded(proc.stdin, deadline, clock, sleeper))
        for stream in (proc.stdout, proc.stderr):
            errors.extend(_close_bounded(stream, deadline, clock, sleeper))
    errors.extend(_join_helper_threads(deadline))
    return errors


def finalize_case(result, errors):
    if errors:
        return {"status": "failed", "detail": result.get("detail", "cleanup-failed"),
                "cleanup_errors": errors, **{k: v for k, v in result.items() if k not in ("status", "cleanup_errors")}}
    return result


def _helper_command(mode, *args):
    return [sys.executable, str(Path(__file__).resolve()), "--synthetic-helper", mode, *map(str, args)]


def _identity_from_handle(handle):
    api = _win()
    size = W.DWORD(32768)
    buffer = ctypes.create_unicode_buffer(size.value)
    if not api.QueryFullProcessImageNameW(handle, 0, buffer, ctypes.byref(size)):
        return None
    created = ctypes.c_ulonglong()
    exited = ctypes.c_ulonglong()
    kernel = ctypes.c_ulonglong()
    user = ctypes.c_ulonglong()
    if not api.GetProcessTimes(handle, ctypes.byref(created), ctypes.byref(exited), ctypes.byref(kernel), ctypes.byref(user)):
        return None
    if created.value == 0:
        return None
    return {"image": buffer.value, "created": int(created.value)}


def _open_process_handle(pid):
    api = _win()
    handle = api.OpenProcess(0x101001, False, pid)  # SYNCHRONIZE | QUERY_LIMITED | TERMINATE
    if not handle:
        return None, api
    return handle, api


def _wait_process_handle(handle, api, deadline):
    wait = api.WaitForSingleObject
    wait.argtypes = [W.HANDLE, W.DWORD]
    wait.restype = W.DWORD
    while time.monotonic() < deadline:
        remaining = max(0, int((deadline - time.monotonic()) * 1000))
        if remaining == 0:
            break
        result = wait(handle, remaining)
        if result == 0:
            return "passed", "child-signaled"
        if result != 258:
            return "failed", "wait-failed:" + str(result)
    return "failed", "child-timeout"


def _read_handshake(proc, deadline):
    """Bounded line reader without claiming fixed sleep as readiness."""
    result = {}

    def read():
        try:
            result["line"] = proc.stdout.readline(4096).decode("utf-8", "replace").strip()
        except Exception as exc:
            result["error"] = str(exc)

    thread = threading.Thread(target=read, daemon=True)
    HELPER_THREADS.append(thread)
    thread.start()
    thread.join(max(0, deadline - time.monotonic()))
    if thread.is_alive():
        return None
    return result.get("line")


def _synthetic_helper(mode, *args):
    if mode == "marker-child":
        marker, delay = Path(args[0]), float(args[1])
        time.sleep(max(0, delay))
        marker.write_text("synthetic-child", encoding="utf8")
        time.sleep(60)
        return 0
    if mode == "pipe-child":
        ready = Path(args[0])
        sys.stdout.write("SYNTHETIC-PIPE\n")
        sys.stdout.flush()
        ready.write_text("ready", encoding="utf8")
        time.sleep(60)
        return 0
    if mode == "wait-for-go":
        sys.stdin.buffer.readline()
        return 0
    if mode == "timeout-descendant":
        marker = Path(args[0])
        if not sys.stdin.buffer.readline():
            return 3
        child = subprocess.Popen([sys.executable, str(Path(__file__).resolve()), "--synthetic-helper", "marker-child", str(marker), "10"])
        print("READY", child.pid, flush=True)
        time.sleep(60)
        return 0
    if mode == "pipe-holder":
        ready = Path(args[0])
        if not sys.stdin.buffer.readline():
            return 3
        child = subprocess.Popen([sys.executable, str(Path(__file__).resolve()), "--synthetic-helper", "pipe-child", str(ready)], stdout=sys.stdout, stderr=sys.stderr)
        deadline = time.monotonic() + 10
        while not ready.exists() and time.monotonic() < deadline:
            time.sleep(0.01)
        if not ready.exists():
            return 4
        print("READY", child.pid, flush=True)
        time.sleep(60)
        return 0
    if mode == "supervisor-death":
        ready = Path(args[0])
        inner = WindowsJob()
        child = subprocess.Popen([sys.executable, str(Path(__file__).resolve()), "--synthetic-helper", "marker-child", str(Path(args[0]).with_suffix(".marker")), "10"])
        inner.assign(child)
        if not inner.is_process_in_job(child):
            return 4
        identity, error = process_evidence(child.pid, inner)
        if error:
            ready.write_text(json.dumps({"error": error}), encoding="utf8")
            return 5
        tmp = ready.with_suffix(".tmp")
        tmp.write_text(json.dumps({"pid": child.pid, "created": identity["created"]}), encoding="utf8")
        os.replace(tmp, ready)
        time.sleep(60)
        return 0
    if mode == "add-type-wait":
        if not sys.stdin.buffer.readline():
            return 3
        powershell = str(Path(os.environ.get("SystemRoot", "")) / "System32" / "WindowsPowerShell" / "v1.0" / "powershell.exe")
        source = "$ErrorActionPreference='Stop'; try { Add-Type -TypeDefinition 'public class SyntheticAcceptanceType { public static int Value = 1; }' -ErrorAction Stop; if ([SyntheticAcceptanceType]::Value -ne 1) { exit 20 }; exit 0 } catch { exit 21 }"
        code = subprocess.call([powershell, "-NoProfile", "-NonInteractive", "-Command", source], stdin=subprocess.DEVNULL, creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
        Path(args[0]).write_text(str(code), encoding="utf8")
        return code
    return 2


def _case_timeout_descendant(work, clock=time.monotonic, sleeper=time.sleep,
                             timeout_seconds=1.0):
    marker = work / "timeout.marker"
    job = None
    proc = None
    assigned = False
    result = {"status": "blocked", "detail": "not-observed"}
    try:
        job = WindowsJob()
        proc = subprocess.Popen(_helper_command("timeout-descendant", marker), stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        job.assign(proc)
        assigned = job.is_process_in_job(proc)
        if not assigned:
            result = {"status": "failed", "detail": "membership"}
        else:
            write_errors = _write_stdin_bounded(proc, b"go\n", time.monotonic() + 5)
            if write_errors:
                raise RuntimeError("authorization-failed:" + write_errors[0])
            line = _read_handshake(proc, time.monotonic() + 10)
            if not line or not line.startswith("READY "):
                result = {"status": "blocked", "detail": "no-handshake"}
            else:
                child_pid = int(line.split()[1])
                child_identity, error = process_evidence(child_pid, job)
                if error:
                    result = {"status": "blocked", "detail": "descendant-identity:" + error}
                else:
                    deadline = clock() + timeout_seconds
                    while clock() < deadline and proc.poll() is None:
                        sleeper(min(0.01, max(0, deadline - clock())))
                    timed_out = clock() >= deadline and proc.poll() is None
                    try:
                        active = job.active_processes()
                        active_error = None
                    except Exception as exc:
                        active = None
                        active_error = str(exc)
                    descendant_alive = active is not None and active > 1
                    if not timed_out:
                        detail = "worker-exited-early"
                    elif not descendant_alive:
                        detail = "timeout-evidence-missing"
                    else:
                        detail = "forced-timeout"
                    result = {"status": "passed" if timed_out and descendant_alive and not marker.exists() else "failed",
                              "detail": detail,
                              "owned": [proc.pid, child_pid], "descendant": child_identity,
                              "active_processes": active, "active_error": active_error}
    except Exception as exc:
        result = {"status": "blocked", "detail": type(exc).__name__ + ": " + str(exc)}
    finally:
        cleanup_errors = cleanup_job(job, proc, clock() + CLEANUP_LIMIT_SECONDS,
                                     uncontained=not assigned, clock=clock, sleeper=sleeper)
        result = finalize_case(result, cleanup_errors)
    return result


def _case_pipe_holder(work):
    ready = work / "pipe.ready"
    job = None
    proc = None
    assigned = False
    result = {"status": "blocked", "detail": "not-observed"}
    try:
        job = WindowsJob()
        proc = subprocess.Popen(_helper_command("pipe-holder", ready), stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        job.assign(proc)
        assigned = job.is_process_in_job(proc)
        if not assigned:
            result = {"status": "failed", "detail": "membership"}
        else:
            write_errors = _write_stdin_bounded(proc, b"go\n", time.monotonic() + 5)
            if write_errors:
                raise RuntimeError("authorization-failed:" + write_errors[0])
            pipe_line = _read_handshake(proc, time.monotonic() + 10)
            if pipe_line != "SYNTHETIC-PIPE":
                result = {"status": "failed", "detail": "stdout-missing-or-partial"}
            else:
                line = _read_handshake(proc, time.monotonic() + 10)
                if not line or not line.startswith("READY "):
                    result = {"status": "blocked", "detail": "no-handshake"}
                else:
                    child_pid = int(line.split()[1])
                    child_identity, error = process_evidence(child_pid, job)
                    if error:
                        result = {"status": "blocked", "detail": "descendant-identity:" + error}
                    else:
                        result = {"status": "passed", "detail": "stdout-observed",
                                  "owned": [proc.pid, child_pid], "descendant": child_identity}
    except Exception as exc:
        result = {"status": "blocked", "detail": type(exc).__name__ + ": " + str(exc)}
    finally:
        cleanup_errors = cleanup_job(job, proc, time.monotonic() + CLEANUP_LIMIT_SECONDS,
                                     uncontained=not assigned, drain=True)
        result = finalize_case(result, cleanup_errors)
    return result


def _terminate_process_handle(handle, api, deadline):
    errors = []
    if not api.TerminateProcess(handle, 125):
        errors.append("terminate-child:" + str(ctypes.get_last_error()))
    status, detail = _wait_process_handle(handle, api, deadline)
    if status != "passed":
        errors.append("child-wait:" + detail)
    return errors


def _read_json_bounded(path, deadline):
    while time.monotonic() < deadline:
        try:
            return json.loads(path.read_text(encoding="utf8"))
        except Exception:
            time.sleep(0.01)
    return None


def _case_parent_death(work):
    ready = work / "supervisor.ready"
    proc = None
    child_handle = None
    child_api = None
    verified = False
    child_pid = None
    result = {"status": "blocked", "detail": "not-observed"}
    try:
        proc = subprocess.Popen(_helper_command("supervisor-death", ready), stdin=subprocess.DEVNULL, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        data = _read_json_bounded(ready, time.monotonic() + 10)
        if not isinstance(data, dict) or type(data.get("pid")) is not int:
            result = {"status": "blocked", "detail": "no-supervisor-handshake"}
        else:
            child_pid = data["pid"]
            child_handle, child_api = _open_process_handle(child_pid)
            if not child_handle:
                result = {"status": "blocked", "detail": "open-child-handle", "owned": [proc.pid, child_pid]}
            else:
                created = data.get("created")
                identity = _identity_from_handle(child_handle)
                if type(created) is not int or created == 0 or not identity or identity["created"] != created:
                    result = {"status": "blocked", "detail": "missing-or-mismatched-created-identity",
                              "owned": [proc.pid, child_pid], "termination_authorized": False}
                else:
                    verified = True
                    proc.kill(); proc.wait(timeout=5)
                    status, detail = _wait_process_handle(child_handle, child_api, time.monotonic() + 10)
                    result = {"status": status, "detail": detail, "owned": [proc.pid, child_pid],
                              "termination_authorized": True}
    except Exception as exc:
        result = {"status": "blocked", "detail": type(exc).__name__ + ": " + str(exc)}
    finally:
        cleanup_errors = []
        cleanup_deadline = time.monotonic() + CLEANUP_LIMIT_SECONDS
        if child_handle:
            if verified and result.get("status") != "passed":
                try:
                    cleanup_errors.extend(_terminate_process_handle(child_handle, child_api, cleanup_deadline))
                except Exception as exc:
                    cleanup_errors.append("terminate-child:" + str(exc))
            try:
                if not child_api.CloseHandle(child_handle):
                    cleanup_errors.append("child-handle-close:" + str(ctypes.get_last_error()))
            except Exception as exc:
                cleanup_errors.append("child-handle-close:" + str(exc))
        if proc is not None:
            try:
                if proc.poll() is None:
                    proc.kill()
                    remaining = max(0, cleanup_deadline - time.monotonic())
                    proc.wait(timeout=remaining)
            except Exception as exc:
                cleanup_errors.append("supervisor-exit:" + str(exc))
            for stream in (proc.stdout, proc.stderr):
                try:
                    cleanup_errors.extend(_close_bounded(stream, cleanup_deadline))
                except Exception as exc:
                    cleanup_errors.append("pipe-close:" + str(exc))
        cleanup_errors.extend(_join_helper_threads(cleanup_deadline))
        result = finalize_case(result, cleanup_errors)
    return result


def _case_preassignment_eof(work):
    job = None
    proc = None
    assigned = False
    result = {"status": "blocked", "detail": "not-observed"}
    try:
        worker = Path(__file__).resolve().with_name("execution_runtime.py")
        job = WindowsJob()
        proc = subprocess.Popen([sys.executable, str(worker), "--persistence-worker"], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        bounded_close = _write_stdin_bounded(proc, None, time.monotonic() + 5)
        try:
            job.assign(proc)
            assigned = job.is_process_in_job(proc)
        except Exception:
            assigned = False
        exited = _wait_for_exit(proc, time.monotonic() + 10)
        active = None
        if assigned:
            try:
                active = job.active_processes()
            except Exception:
                active = None
        no_descendants = assigned and active == 0
        result = {"status": "blocked",
                  "detail": "worker-refused; preassignment interval not independently observed",
                  "worker_refused": exited and proc.returncode == 2,
                  "no_descendant_end_snapshot": no_descendants,
                  "owned": [proc.pid], "assigned": assigned, "active_processes": active,
                  "stdin_errors": bounded_close,
                  "synthetic_analogue": False, "actual_worker_path": True,
                  "reader_path_evidence": False}
        if bounded_close:
            result["status"] = "blocked"
            result["detail"] = "stdin-close-failed"
    except Exception as exc:
        result = {"status": "blocked", "detail": type(exc).__name__ + ": " + str(exc)}
    finally:
        cleanup_errors = cleanup_job(job, proc, time.monotonic() + CLEANUP_LIMIT_SECONDS,
                                     uncontained=not assigned)
        result = finalize_case(result, cleanup_errors)
    return result


def _case_add_type(work):
    job = None
    proc = None
    assigned = False
    result = {"status": "blocked", "detail": "not-observed"}
    try:
        job = ObservedJob()
        result_path = work / "add-type.result"
        proc = subprocess.Popen(_helper_command("add-type-wait", result_path), stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        job.assign(proc)
        assigned = job.is_process_in_job(proc)
        if not assigned:
            result = {"status": "failed", "detail": "membership"}
        else:
            write_errors = _write_stdin_bounded(proc, b"go\n", time.monotonic() + 5)
            if write_errors:
                raise RuntimeError("authorization-failed:" + write_errors[0])
            evidence = []
            evidence_errors = []
            def new_process(pid):
                identity, error = process_evidence(pid, job)
                if error:
                    evidence_errors.append({"pid": pid, "error": error})
                else:
                    evidence.append(identity)
            deadline = time.monotonic() + CASE_DEADLINE_SECONDS
            port_error = job.drain_messages(deadline, new_process)
            compiler = [x for x in evidence if Path(x["image"]).name.lower() == "csc.exe"]
            compile_code = None
            if result_path.exists():
                try:
                    compile_code = int(result_path.read_text(encoding="utf8").strip())
                except Exception:
                    compile_code = None
            if port_error or evidence_errors:
                result = {"status": "blocked", "detail": port_error or "identity-observation", "observed": evidence, "identity_errors": evidence_errors}
            elif not compiler:
                result = {"status": "blocked", "detail": "compiler-not-identified", "observed": evidence}
            elif compile_code is None:
                result = {"status": "blocked", "detail": "compile-result-missing", "compiler": compiler, "observed": evidence}
            elif compile_code != 0:
                result = {"status": "failed", "detail": "compile-code:" + str(compile_code), "compiler": compiler, "observed": evidence}
            else:
                result = {"status": "passed", "detail": "compiler-observed", "compiler": compiler, "observed": evidence}
    except Exception as exc:
        result = {"status": "blocked", "detail": type(exc).__name__ + ": " + str(exc)}
    finally:
        cleanup_errors = cleanup_job(job, proc, time.monotonic() + CLEANUP_LIMIT_SECONDS,
                                     uncontained=not assigned)
        result = finalize_case(result, cleanup_errors)
    return result


def _teardown_sentinel(sentinel, deadline):
    errors = []
    try:
        errors.extend(_write_stdin_bounded(sentinel, None, deadline))
    except Exception as exc:
        errors.append("sentinel-stdin:" + str(exc))
    try:
        remaining = max(0, deadline - time.monotonic())
        sentinel.wait(timeout=remaining)
    except Exception:
        errors.append("sentinel-wait")
        try:
            sentinel.kill()
        except Exception as exc:
            errors.append("sentinel-kill:" + str(exc))
        try:
            remaining = max(0, deadline - time.monotonic())
            sentinel.wait(timeout=remaining)
        except Exception as exc:
            errors.append("sentinel-fallback-wait:" + str(exc))
    for stream in (sentinel.stdout, sentinel.stderr):
        try:
            errors.extend(_close_bounded(stream, deadline))
        except Exception as exc:
            errors.append("sentinel-close:" + str(exc))
    try:
        errors.extend(_join_helper_threads(deadline))
    except Exception as exc:
        errors.append("sentinel-helper-join:" + str(exc))
    return errors


def run_cases(work):
    results = []
    sentinel = subprocess.Popen(
        [sys.executable, "-c", "import sys;sys.stdout.write('SENTINEL-READY\\n');sys.stdout.flush();sys.stdin.buffer.read()"],
        stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
    )
    sentinel_ready = False
    suite_errors = []
    try:
        try:
            sentinel_ready = _read_handshake(sentinel, time.monotonic() + 10) == "SENTINEL-READY"
        except Exception as exc:
            suite_errors.append("sentinel-read:" + str(exc))
        for name, function in (
            ("timeout_descendant", _case_timeout_descendant),
            ("inherited_pipe", _case_pipe_holder),
            ("parent_death", _case_parent_death),
            ("preassignment_eof", _case_preassignment_eof),
            ("add_type_compiler", _case_add_type),
        ):
            if not sentinel_ready or sentinel.poll() is not None:
                result = {"status": "blocked", "detail": "sentinel-not-surviving"}
            else:
                try:
                    result = function(work)
                except Exception as exc:
                    result = {"status": "blocked", "detail": type(exc).__name__ + ": " + str(exc)}
            result["case"] = name
            result.setdefault("synthetic_analogue", True)
            result.setdefault("reader_path_evidence", False)
            results.append(result)
        results.append({"case": "unrelated_sentinel",
                        "status": "passed" if sentinel_ready and sentinel.poll() is None else "failed",
                        "synthetic_analogue": False, "reader_path_evidence": False,
                        "owned": [sentinel.pid]})
    finally:
        cleanup_deadline = time.monotonic() + CLEANUP_LIMIT_SECONDS
        try:
            suite_errors.extend(_teardown_sentinel(sentinel, cleanup_deadline))
        except Exception as exc:
            suite_errors.append("sentinel-teardown:" + str(exc))
    required = ["timeout_descendant", "inherited_pipe", "parent_death", "preassignment_eof", "add_type_compiler", "unrelated_sentinel"]
    status, missing = aggregate(results, required)
    if suite_errors:
        for item in results:
            if item.get("case") == "unrelated_sentinel":
                item["status"] = "failed"
                item["detail"] = suite_errors
        results.append({"case": "suite_cleanup", "status": "failed", "detail": suite_errors,
                        "synthetic_analogue": False, "reader_path_evidence": False})
        status = "failed"
    return {"schema": 1, "status": status, "missing": missing, "cases": results,
            "limitations": ["synthetic analogues only", "no production reader path evidence",
                            "job NEW_PROCESS alone is not executable identity",
                            "compiler absence is blocked, not proof", "short-lived/PID-reuse windows remain"]}


def helper_entry(mode, args, env, repo, action=None):
    """Every live helper mode requires the inherited supervisor context."""
    validate_admission(env, repo)
    return (action or _synthetic_helper)(mode, *args)


def main():
    if len(sys.argv) > 1:
        return 2
    try:
        validate_admission(os.environ, ROOT)
    except Exception as exc:
        print(json.dumps({"schema": 1, "status": "blocked", "reason": str(exc)}))
        return 2
    if os.name != "nt":
        print(json.dumps({"schema": 1, "status": "blocked", "reason": "Windows required"}))
        return 2
    root = Path(os.environ.get("TURBOMCP_RUNNER_LOG_ROOT", tempfile.gettempdir()))
    root.mkdir(parents=True, exist_ok=True)
    work = Path(tempfile.mkdtemp(prefix="persistence-acceptance-" + uuid.uuid4().hex + "-", dir=root))
    result = run_cases(work)
    print(json.dumps(result, sort_keys=True))
    return 0 if result["status"] == "passed" else 2


if __name__ == "__main__":
    if len(sys.argv) >= 3 and sys.argv[1] == "--synthetic-helper":
        try:
            code = helper_entry(sys.argv[2], sys.argv[3:], os.environ, ROOT)
        except Exception as exc:
            print(json.dumps({"schema": 1, "status": "blocked", "reason": str(exc)}))
            code = 2
        sys.exit(code)
    sys.exit(main())
