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
import threading
import time

from execution_runtime import BASIC_ACCOUNTING, EXTENDED, WindowsJob, canonical, parent_ids

ROOT = Path(__file__).resolve().parents[1]
CASE_DEADLINE_SECONDS = 30
CLEANUP_LIMIT_SECONDS = 5
JOB_OBJECT_MSG_ACTIVE_PROCESS_ZERO = 4
JOB_OBJECT_MSG_NEW_PROCESS = 6
JOB_OBJECT_MSG_EXIT_PROCESS = 7
JOB_OBJECT_MSG_ABNORMAL_EXIT_PROCESS = 8
JOB_OBJECT_ASSOCIATE_COMPLETION_PORT_INFORMATION = 7


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
    api.CloseHandle.argtypes = [W.HANDLE]
    api.CloseHandle.restype = W.BOOL
    return api


class ObservedJob:
    """Synthetic inner job with a completion port; not a production reader seam."""
    def __init__(self):
        self.api = _win()
        self.handle = self.api.CreateJobObjectW(None, None)
        if not self.handle:
            raise ctypes.WinError(ctypes.get_last_error())
        limits = EXTENDED()
        limits.BasicLimitInformation.LimitFlags = 0x2000
        if not self.api.SetInformationJobObject(self.handle, 9, ctypes.byref(limits), ctypes.sizeof(limits)):
            self.close_checked()
            raise ctypes.WinError(ctypes.get_last_error())
        self.port = self.api.CreateIoCompletionPort(W.HANDLE(-1), None, 0, 1)
        if not self.port:
            self.close_checked()
            raise ctypes.WinError(ctypes.get_last_error())
        assoc = JOBOBJECT_ASSOCIATE_COMPLETION_PORT(1, self.port)
        if not self.api.SetInformationJobObject(
                self.handle, JOB_OBJECT_ASSOCIATE_COMPLETION_PORT_INFORMATION,
                ctypes.byref(assoc), ctypes.sizeof(assoc)):
            self.close_checked()
            raise ctypes.WinError(ctypes.get_last_error())
        self.messages = []

    def assign(self, proc):
        if not self.api.AssignProcessToJobObject(self.handle, W.HANDLE(int(proc._handle))):
            raise ctypes.WinError(ctypes.get_last_error())

    def active_processes(self):
        info = BASIC_ACCOUNTING()
        if not self.api.QueryInformationJobObject(self.handle, 1, ctypes.byref(info), ctypes.sizeof(info), None):
            raise ctypes.WinError(ctypes.get_last_error())
        return int(info.ActiveProcesses)

    def terminate_checked(self):
        if not self.api.TerminateJobObject(self.handle, 125):
            raise ctypes.WinError(ctypes.get_last_error())

    def close_checked(self):
        if self.port:
            self.api.CloseHandle(self.port)
            self.port = None
        if self.handle:
            if not self.api.CloseHandle(self.handle):
                raise ctypes.WinError(ctypes.get_last_error())
            self.handle = None

    def drain_messages(self, deadline):
        while self.handle and time.monotonic() < deadline:
            remaining = max(0, int((deadline - time.monotonic()) * 1000))
            if remaining == 0:
                break
            count = W.DWORD()
            key = ctypes.c_size_t()
            overlay = ctypes.c_void_p()
            if not self.api.GetQueuedCompletionStatus(self.port, ctypes.byref(count), ctypes.byref(key), ctypes.byref(overlay), remaining):
                if ctypes.get_last_error() == 258:
                    break
                break
            self.messages.append((int(count.value), int(overlay.value or 0)))
        return list(self.messages)


def process_identity(pid):
    """Return synthetic observed identity or None; never searches by name."""
    if not pid:
        return None
    api = _win()
    handle = api.OpenProcess(0x1000, False, pid)
    if not handle:
        return None
    try:
        size = W.DWORD(32768)
        buffer = ctypes.create_unicode_buffer(size.value)
        if not api.QueryFullProcessImageNameW(handle, 0, buffer, ctypes.byref(size)):
            return None
        created = ctypes.c_ulonglong()
        exited = ctypes.c_ulonglong()
        kernel = ctypes.c_ulonglong()
        user = ctypes.c_ulonglong()
        if not api.GetProcessTimes(handle, ctypes.byref(created), ctypes.byref(exited), ctypes.byref(kernel), ctypes.byref(user)):
            created = ctypes.c_ulonglong()
        return {"pid": int(pid), "image": buffer.value, "created": int(created.value)}
    finally:
        api.CloseHandle(handle)


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


def _cleanup_job(job, proc, deadline):
    errors = []
    try:
        job.terminate_checked()
    except Exception as exc:
        errors.append("terminate:" + str(exc))
    while time.monotonic() < deadline:
        try:
            if job.active_processes() == 0:
                break
        except Exception as exc:
            errors.append("active:" + str(exc))
            break
        time.sleep(0.01)
    try:
        job.close_checked()
    except Exception as exc:
        errors.append("close:" + str(exc))
    if proc is not None:
        if not _wait_for_exit(proc, deadline):
            errors.append("worker-exit")
    return errors


def _helper_command(mode, *args):
    return [sys.executable, str(Path(__file__).resolve()), "--synthetic-helper", mode, *map(str, args)]


def _open_process_handle(pid):
    api = _win()
    handle = api.OpenProcess(0x00100000, False, pid)  # SYNCHRONIZE only
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
            result["line"] = proc.stdout.readline().decode("utf-8", "replace").strip()
        except Exception as exc:
            result["error"] = str(exc)

    thread = threading.Thread(target=read, daemon=True)
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
        sys.stdout.write("SYNTHETIC-PIPE\n")
        sys.stdout.flush()
        time.sleep(60)
        return 0
    if mode == "wait-for-go":
        sys.stdin.buffer.readline()
        return 0
    if mode == "timeout-descendant":
        marker = Path(args[0])
        if not sys.stdin.buffer.readline():
            return 3
        child = subprocess.Popen([sys.executable, str(Path(__file__).resolve()), "--synthetic-helper", "marker-child", str(marker), "0.3"])
        print("READY", child.pid, flush=True)
        time.sleep(60)
        return 0
    if mode == "pipe-holder":
        if not sys.stdin.buffer.readline():
            return 3
        child = subprocess.Popen([sys.executable, str(Path(__file__).resolve()), "--synthetic-helper", "pipe-child"], stdout=sys.stdout, stderr=sys.stderr)
        print("READY", child.pid, flush=True)
        time.sleep(60)
        return 0
    if mode == "supervisor-death":
        ready = Path(args[0])
        marker = Path(args[1])
        inner = WindowsJob()
        child = subprocess.Popen([sys.executable, str(Path(__file__).resolve()), "--synthetic-helper", "marker-child", str(marker), "0.2"])
        inner.assign(child)
        if not inner.is_process_in_job(child):
            return 4
        ready.write_text(str(child.pid), encoding="utf8")
        time.sleep(60)
        return 0
    if mode == "add-type-wait":
        if not sys.stdin.buffer.readline():
            return 3
        powershell = str(Path(os.environ.get("SystemRoot", "")) / "System32" / "WindowsPowerShell" / "v1.0" / "powershell.exe")
        source = "$null=[Console]::In.ReadLine(); Add-Type -TypeDefinition 'public class SyntheticAcceptanceType { public static int Value = 1; }'; Start-Sleep -Milliseconds 250"
        return subprocess.call([powershell, "-NoProfile", "-NonInteractive", "-Command", source], stdin=subprocess.DEVNULL, creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
    return 2


def _case_timeout_descendant(work):
    marker = work / "timeout.marker"
    job = WindowsJob()
    proc = None
    try:
        proc = subprocess.Popen(_helper_command("timeout-descendant", marker), stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        job.assign(proc)
        if not job.is_process_in_job(proc):
            return {"status": "failed", "detail": "membership"}
        proc.stdin.write(b"go\n"); proc.stdin.close()
        line = _read_handshake(proc, time.monotonic() + 10)
        if not line or not line.startswith("READY "):
            return {"status": "blocked", "detail": "no-handshake"}
        child_pid = int(line.split()[1])
        deadline = time.monotonic() + CASE_DEADLINE_SECONDS
        errors = _cleanup_job(job, proc, time.monotonic() + CLEANUP_LIMIT_SECONDS)
        return {"status": "passed" if not errors and not marker.exists() else "failed",
                "detail": "marker-absent" if not marker.exists() else "marker-present",
                "owned": [proc.pid, child_pid], "errors": errors}
    finally:
        if proc is not None and proc.poll() is None:
            proc.kill(); proc.wait(timeout=5)


def _case_pipe_holder(work):
    job = WindowsJob()
    proc = None
    try:
        proc = subprocess.Popen(_helper_command("pipe-holder"), stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        job.assign(proc)
        proc.stdin.write(b"go\n"); proc.stdin.close()
        line = _read_handshake(proc, time.monotonic() + 10)
        if not line or not line.startswith("READY "):
            return {"status": "blocked", "detail": "no-handshake"}
        child_pid = int(line.split()[1])
        pipe_line = _read_handshake(proc, time.monotonic() + 5)
        errors = _cleanup_job(job, proc, time.monotonic() + CLEANUP_LIMIT_SECONDS)
        return {"status": "passed" if pipe_line == "SYNTHETIC-PIPE" and not errors else "failed",
                "detail": "pipe-observed" if pipe_line == "SYNTHETIC-PIPE" else "pipe-missing",
                "owned": [proc.pid, child_pid], "errors": errors}
    finally:
        if proc is not None and proc.poll() is None:
            proc.kill(); proc.wait(timeout=5)


def _case_parent_death(work):
    ready = work / "supervisor.ready"
    marker = work / "parent-death.marker"
    proc = subprocess.Popen(_helper_command("supervisor-death", ready, marker), stdin=subprocess.DEVNULL, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    try:
        if not _wait_for_file(ready, time.monotonic() + 10):
            return {"status": "blocked", "detail": "no-supervisor-handshake"}
        child_pid = int(ready.read_text(encoding="utf8"))
        child_handle, child_api = _open_process_handle(child_pid)
        if not child_handle:
            return {"status": "blocked", "detail": "open-child-handle", "owned": [proc.pid, child_pid]}
        try:
            proc.kill(); proc.wait(timeout=5)
            status, detail = _wait_process_handle(child_handle, child_api, time.monotonic() + 10)
            return {"status": status, "detail": detail, "owned": [proc.pid, child_pid]}
        finally:
            child_api.CloseHandle(child_handle)
    finally:
        if proc.poll() is None:
            proc.kill(); proc.wait(timeout=5)


def _case_preassignment_eof(work):
    marker = work / "pre-eof.marker"
    proc = subprocess.Popen(_helper_command("wait-for-go"), stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    proc.stdin.close()
    exited = _wait_for_exit(proc, time.monotonic() + 10)
    return {"status": "passed" if exited and not marker.exists() else "blocked",
            "detail": "eof-no-child" if exited else "eof-timeout", "owned": [proc.pid]}


def _case_add_type(work):
    job = ObservedJob()
    proc = None
    try:
        proc = subprocess.Popen(_helper_command("add-type-wait"), stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        job.assign(proc)
        proc.stdin.write(b"go\n"); proc.stdin.close()
        deadline = time.monotonic() + CASE_DEADLINE_SECONDS
        messages = job.drain_messages(deadline)
        identities = []
        for message, pid in messages:
            if message == JOB_OBJECT_MSG_NEW_PROCESS:
                identity = process_identity(pid)
                if identity:
                    identities.append(identity)
        compiler = [x for x in identities if Path(x["image"]).name.lower() == "csc.exe"]
        errors = _cleanup_job(job, proc, time.monotonic() + CLEANUP_LIMIT_SECONDS)
        if not compiler:
            return {"status": "blocked", "detail": "compiler-not-identified", "observed": identities, "errors": errors}
        return {"status": "passed" if not errors else "failed", "detail": "compiler-observed", "compiler": compiler, "observed": identities, "errors": errors}
    finally:
        if proc is not None and proc.poll() is None:
            proc.kill(); proc.wait(timeout=5)


def run_cases(work):
    results = []
    sentinel = subprocess.Popen([sys.executable, "-c", "import time; time.sleep(60)"])
    try:
        for name, function in (
            ("timeout_descendant", _case_timeout_descendant),
            ("inherited_pipe", _case_pipe_holder),
            ("parent_death", _case_parent_death),
            ("preassignment_eof", _case_preassignment_eof),
            ("add_type_compiler", _case_add_type),
        ):
            try:
                result = function(work)
            except Exception as exc:
                result = {"status": "blocked", "detail": type(exc).__name__ + ": " + str(exc)}
            result["case"] = name
            result["synthetic_analogue"] = True
            result["reader_path_evidence"] = False
            results.append(result)
        results.append({"case": "unrelated_sentinel", "status": "passed" if sentinel.poll() is None else "failed",
                        "synthetic_analogue": True, "reader_path_evidence": False,
                        "owned": [sentinel.pid]})
    finally:
        if sentinel.poll() is None:
            sentinel.terminate()
            try:
                sentinel.wait(timeout=5)
            except Exception:
                sentinel.kill(); sentinel.wait(timeout=5)
    required = ["timeout_descendant", "inherited_pipe", "parent_death", "preassignment_eof", "add_type_compiler", "unrelated_sentinel"]
    status, missing = aggregate(results, required)
    return {"schema": 1, "status": status, "missing": missing, "cases": results,
            "limitations": ["synthetic analogues only", "no production reader path evidence",
                            "job NEW_PROCESS alone is not executable identity",
                            "compiler absence is blocked, not proof", "short-lived/PID-reuse windows remain"]}


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
    work = Path(os.environ.get("TEMP", ".")) / ("tmcp-persistence-acceptance-" + str(os.getpid()))
    work.mkdir(parents=True, exist_ok=True)
    result = run_cases(work)
    print(json.dumps(result, sort_keys=True))
    return 0 if result["status"] == "passed" else 2


if __name__ == "__main__":
    if len(sys.argv) >= 3 and sys.argv[1] == "--synthetic-helper":
        sys.exit(_synthetic_helper(sys.argv[2], *sys.argv[3:]))
    sys.exit(main())
