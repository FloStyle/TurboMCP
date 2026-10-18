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
UNRESOLVED_OBSERVERS = []


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


class SECURITY_ATTRIBUTES(ctypes.Structure):
    _fields_ = [("nLength", W.DWORD), ("lpSecurityDescriptor", ctypes.c_void_p),
                ("bInheritHandle", W.BOOL)]


class STARTUPINFOW(ctypes.Structure):
    _fields_ = [
        ("cb", W.DWORD), ("lpReserved", W.LPWSTR), ("lpDesktop", W.LPWSTR),
        ("lpTitle", W.LPWSTR), ("dwX", W.DWORD), ("dwY", W.DWORD),
        ("dwXSize", W.DWORD), ("dwYSize", W.DWORD),
        ("dwXCountChars", W.DWORD), ("dwYCountChars", W.DWORD),
        ("dwFillAttribute", W.DWORD), ("dwFlags", W.DWORD),
        ("wShowWindow", W.WORD), ("cbReserved2", W.WORD),
        ("lpReserved2", ctypes.POINTER(ctypes.c_byte)),
        ("hStdInput", W.HANDLE), ("hStdOutput", W.HANDLE), ("hStdError", W.HANDLE),
    ]


class STARTUPINFOEXW(ctypes.Structure):
    _fields_ = [("StartupInfo", STARTUPINFOW), ("lpAttributeList", ctypes.c_void_p)]


class PROCESS_INFORMATION(ctypes.Structure):
    _fields_ = [("hProcess", W.HANDLE), ("hThread", W.HANDLE),
                ("dwProcessId", W.DWORD), ("dwThreadId", W.DWORD)]


CREATE_SUSPENDED = 0x00000004
DETACHED_PROCESS = 0x00000008
EXTENDED_STARTUPINFO_PRESENT = 0x00080000
STARTF_USESTDHANDLES = 0x00000100
HANDLE_FLAG_INHERIT = 0x00000001
PROC_THREAD_ATTRIBUTE_HANDLE_LIST = 0x00020002
WAIT_OBJECT_0 = 0
WAIT_TIMEOUT = 258
INFINITE = 0xFFFFFFFF
DUPLICATE_SAME_ACCESS = 0x00000002


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
    api.CreatePipe.argtypes = [ctypes.POINTER(W.HANDLE), ctypes.POINTER(W.HANDLE),
                               ctypes.POINTER(SECURITY_ATTRIBUTES), W.DWORD]
    api.CreatePipe.restype = W.BOOL
    api.SetHandleInformation.argtypes = [W.HANDLE, W.DWORD, W.DWORD]
    api.SetHandleInformation.restype = W.BOOL
    api.InitializeProcThreadAttributeList.argtypes = [ctypes.c_void_p, W.DWORD, W.DWORD,
                                                      ctypes.POINTER(ctypes.c_size_t)]
    api.InitializeProcThreadAttributeList.restype = W.BOOL
    api.UpdateProcThreadAttribute.argtypes = [ctypes.c_void_p, W.DWORD, ctypes.c_size_t,
                                              ctypes.c_void_p, ctypes.c_size_t,
                                              ctypes.c_void_p, ctypes.POINTER(ctypes.c_size_t)]
    api.UpdateProcThreadAttribute.restype = W.BOOL
    api.DeleteProcThreadAttributeList.argtypes = [ctypes.c_void_p]
    api.DeleteProcThreadAttributeList.restype = None
    api.CreateProcessW.argtypes = [W.LPCWSTR, W.LPWSTR, ctypes.POINTER(SECURITY_ATTRIBUTES),
                                   ctypes.POINTER(SECURITY_ATTRIBUTES), W.BOOL, W.DWORD,
                                   ctypes.c_void_p, W.LPCWSTR, ctypes.POINTER(STARTUPINFOEXW),
                                   ctypes.POINTER(PROCESS_INFORMATION)]
    api.CreateProcessW.restype = W.BOOL
    api.ResumeThread.argtypes = [W.HANDLE]
    api.ResumeThread.restype = W.DWORD
    api.WaitForSingleObject.argtypes = [W.HANDLE, W.DWORD]
    api.WaitForSingleObject.restype = W.DWORD
    api.GetExitCodeProcess.argtypes = [W.HANDLE, ctypes.POINTER(W.DWORD)]
    api.GetExitCodeProcess.restype = W.BOOL
    api.GetCurrentProcess.argtypes = []
    api.GetCurrentProcess.restype = W.HANDLE
    api.DuplicateHandle.argtypes = [W.HANDLE, W.HANDLE, W.HANDLE,
                                    ctypes.POINTER(W.HANDLE), W.DWORD, W.BOOL, W.DWORD]
    api.DuplicateHandle.restype = W.BOOL
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

    def accounting(self):
        info = BASIC_ACCOUNTING()
        if not self.api.QueryInformationJobObject(self.handle, 1, ctypes.byref(info), ctypes.sizeof(info), None):
            raise ctypes.WinError(ctypes.get_last_error())
        return int(info.TotalProcesses), int(info.ActiveProcesses)

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

    def drain_messages(self, deadline, on_new_process=None, clock=time.monotonic,
                       stop_event=None):
        error = None
        while self.handle and clock() < deadline:
            if stop_event is not None and stop_event.is_set():
                break
            remaining = max(0, int((deadline - clock()) * 1000))
            if remaining == 0:
                break
            if stop_event is not None:
                remaining = min(remaining, 50)
            count = W.DWORD()
            key = ctypes.c_size_t()
            overlay = ctypes.c_void_p()
            if not self.api.GetQueuedCompletionStatus(self.port, ctypes.byref(count), ctypes.byref(key), ctypes.byref(overlay), remaining):
                code = ctypes.get_last_error()
                if code == 258:
                    if stop_event is None:
                        break
                    continue
                error = "completion-port:" + str(code)
                break
            message = int(count.value)
            pid = int(overlay.value or 0)
            self.messages.append((message, pid))
            if message == JOB_OBJECT_MSG_NEW_PROCESS and on_new_process is not None:
                on_new_process(pid)
        return error


    def start_observation(self, deadline, callback, clock=time.monotonic):
        stop = threading.Event()
        errors = []
        view = None
        thread = None
        try:
            view = ObservationView.duplicate(self.api, self.handle, self.port)
            def observe():
                try:
                    error = view.drain_messages(deadline, lambda pid: callback(pid, view),
                                                clock=clock, stop_event=stop)
                    if error:
                        errors.append(error)
                except Exception as exc:
                    errors.append("observer:" + str(exc))
                finally:
                    try:
                        errors.extend(view.close())
                    except Exception as exc:
                        errors.append("observer-close:" + str(exc))
            thread = threading.Thread(target=observe, daemon=True)
            thread.start()
            HELPER_THREADS.append(thread)
            return stop, thread, errors, view
        except Exception as original:
            cleanup_errors = []
            if view is not None:
                try:
                    cleanup_errors.extend(view.close())
                except Exception as exc:
                    cleanup_errors.append("observer-close:" + str(exc))
            if thread is not None and thread in HELPER_THREADS:
                HELPER_THREADS.remove(thread)
            if cleanup_errors:
                raise ObserverOwnershipError(original, view, cleanup_errors) from original
            raise


class ObserverOwnershipError(RuntimeError):
    """Typed unresolved observer ownership; view handles are retained, not retried."""

    def __init__(self, original, view, close_errors):
        self.original = original
        self.view = view
        self.close_errors = list(close_errors)
        super().__init__(str(original) + "; cleanup:" + ";".join(self.close_errors) +
                         "; observer-unresolved")


class ObservationView:
    """Observer-owned duplicate job/port handles; closed only by its thread."""

    def __init__(self, api, handle, port):
        self.api = api; self.handle = handle; self.port = port; self.messages = []
        self.close_failed = False; self.close_errors = []; self.unresolved_handles = []
        self.close_attempted = False

    @classmethod
    def duplicate(cls, api, handle, port):
        current = api.GetCurrentProcess()
        job = W.HANDLE()
        port_copy = W.HANDLE()
        acquired = []
        try:
            if not api.DuplicateHandle(current, handle, current, ctypes.byref(job), 0,
                                        False, DUPLICATE_SAME_ACCESS):
                raise ctypes.WinError(ctypes.get_last_error())
            acquired.append(("observer-job-duplicate", job))
            if not api.DuplicateHandle(current, port, current, ctypes.byref(port_copy), 0,
                                        False, DUPLICATE_SAME_ACCESS):
                raise ctypes.WinError(ctypes.get_last_error())
            acquired.append(("observer-port-duplicate", port_copy))
            return cls(api, job, port_copy)
        except Exception as original:
            view = cls(api, job if any(name == "observer-job-duplicate" for name, _ in acquired) else None,
                       port_copy if any(name == "observer-port-duplicate" for name, _ in acquired) else None)
            errors = []
            for name, acquired_handle in acquired:
                try:
                    if not api.CloseHandle(acquired_handle):
                        errors.append(name + "-close:" + str(ctypes.get_last_error()))
                        view.unresolved_handles.append(name)
                except Exception as exc:
                    errors.append(name + "-close:" + str(exc))
                    view.unresolved_handles.append(name)
            if errors:
                view.close_failed = True; view.close_errors = list(errors)
                raise ObserverOwnershipError(original, view, errors) from original
            raise

    def in_job(self, handle):
        present = W.BOOL()
        if not self.api.IsProcessInJob(handle, self.handle, ctypes.byref(present)):
            raise ctypes.WinError(ctypes.get_last_error())
        return bool(present.value)

    def is_process_in_job(self, process):
        return self.in_job(W.HANDLE(int(process._handle)))

    def drain_messages(self, deadline, on_new_process=None, clock=time.monotonic,
                       stop_event=None):
        return ObservedJob.drain_messages(self, deadline, on_new_process, clock, stop_event)

    def close(self):
        if self.close_attempted:
            return list(self.close_errors)
        self.close_attempted = True
        for name in ("port", "handle"):
            handle = getattr(self, name)
            if not handle:
                continue
            try:
                if self.api.CloseHandle(handle):
                    setattr(self, name, None)
                else:
                    self.close_errors.append("observer-" + name + "-close:" +
                                             str(ctypes.get_last_error()))
                    self.unresolved_handles.append(name)
            except Exception as exc:
                self.close_errors.append("observer-" + name + "-close:" + str(exc))
                self.unresolved_handles.append(name)
        self.close_failed = bool(self.close_errors)
        return list(self.close_errors)


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


class SuspendedLaunch:
    """Harness-owned process, primary thread and parent-side pipe handles."""

    def __init__(self, api, process, thread, pid, stdin_write, stdout_read, stderr_read):
        self.api = api
        self.process_handle = process
        self.thread_handle = thread
        self.pid = pid
        self._handle = int(process) if process else 0
        self.stdin_write = stdin_write
        self.stdout_read = stdout_read
        self.stderr_read = stderr_read
        self.resumed = False
        self.exited = False
        self.exit_code = None

    def _close(self, name):
        handle = getattr(self, name)
        if not handle:
            return []
        try:
            if not self.api.CloseHandle(handle):
                return [name + ":" + str(ctypes.get_last_error())]
        except Exception as exc:
            return [name + ":" + str(exc)]
        setattr(self, name, None)
        return []

    def resume(self):
        if not self.thread_handle:
            raise RuntimeError("resume:thread-handle-missing")
        result = self.api.ResumeThread(self.thread_handle)
        if result != 1:
            raise RuntimeError("resume-unexpected-suspend-count:" + str(result))
        self.resumed = True
        errors = self._close("thread_handle")
        if errors:
            raise RuntimeError("resume-thread-close:" + ";".join(errors))

    def close_stdin(self):
        errors = self._close("stdin_write")
        if errors:
            raise RuntimeError("stdin-close:" + ";".join(errors))

    def wait(self, timeout_ms):
        result = self.api.WaitForSingleObject(self.process_handle, int(timeout_ms))
        if result == WAIT_OBJECT_0:
            self.exited = True
            code = W.DWORD()
            if not self.api.GetExitCodeProcess(self.process_handle, ctypes.byref(code)):
                raise ctypes.WinError(ctypes.get_last_error())
            self.exit_code = int(code.value)
            return True
        if result == WAIT_TIMEOUT:
            return False
        raise ctypes.WinError(ctypes.get_last_error())

    def close_process(self):
        errors = self._close("thread_handle") + self._close("process_handle")
        if errors:
            raise RuntimeError("process-close:" + ";".join(errors))

    def terminate(self):
        errors = []
        if self.process_handle:
            try:
                if not self.api.TerminateProcess(self.process_handle, 125):
                    errors.append("terminate-process:" + str(ctypes.get_last_error()))
            except Exception as exc:
                errors.append("terminate-process:" + str(exc))
        return errors

    def close(self, deadline=None, clock=time.monotonic):
        errors = []
        if self.process_handle and not self.exited:
            errors.extend(self.terminate())
            if deadline is not None and self.process_handle:
                remaining = max(0, int((deadline - clock()) * 1000))
                try:
                    result = self.api.WaitForSingleObject(self.process_handle, remaining)
                    if result != WAIT_OBJECT_0:
                        errors.append("terminate-wait-not-signaled")
                except Exception as exc:
                    errors.append("terminate-wait:" + str(exc))
        for name in ("thread_handle", "process_handle", "stdin_write", "stdout_read", "stderr_read"):
            errors.extend(self._close(name))
        return errors


def _fixed_launch_command(mode):
    if mode == "worker":
        return [sys.executable, str(Path(__file__).resolve().with_name("execution_runtime.py")),
                "--persistence-worker"]
    if mode == "control":
        return _helper_command("preassignment-parent")
    raise ValueError("unsupported preassignment mode")


def launch_suspended(mode, api=None):
    """Create one fixed-mode suspended process with an explicit inherited-handle list."""
    api = api or _win()
    # A fixed pipe-only worker needs no console. CREATE_NO_WINDOW can still
    # associate a console host with the job on this Windows installation.
    # Keep explicit standard-handle inheritance and suspended assignment.
    command = _fixed_launch_command(mode)
    command_line = ctypes.create_unicode_buffer(subprocess.list2cmdline(command))
    ledger = []
    attr_buffer = None
    attr_initialized = False
    launch = None

    def acquire(handle, name):
        entry = {"handle": handle, "name": name, "closed": False, "attempted": False,
                 "transferred": False}
        ledger.append(entry)
        return entry

    def close_entry(entry):
        if entry["closed"] or entry["attempted"] or entry["transferred"]:
            return []
        entry["attempted"] = True
        try:
            if api.CloseHandle(entry["handle"]):
                entry["closed"] = True
                return []
            return [entry["name"] + ":" + str(ctypes.get_last_error())]
        except Exception as exc:
            return [entry["name"] + ":" + str(exc)]

    def close_ledger():
        errors = []
        for entry in list(ledger):
            errors.extend(close_entry(entry))
        return errors

    try:
        def pipe(child_is_read):
            read = W.HANDLE()
            write = W.HANDLE()
            sa = SECURITY_ATTRIBUTES(ctypes.sizeof(SECURITY_ATTRIBUTES), None, True)
            if not api.CreatePipe(ctypes.byref(read), ctypes.byref(write), ctypes.byref(sa), 0):
                raise ctypes.WinError(ctypes.get_last_error())
            read_entry = acquire(read, "pipe-read")
            write_entry = acquire(write, "pipe-write")
            child = read if child_is_read else write
            parent = write if child_is_read else read
            child_entry = read_entry if child_is_read else write_entry
            parent_entry = write_entry if child_is_read else read_entry
            if not api.SetHandleInformation(parent, HANDLE_FLAG_INHERIT, 0):
                error = ctypes.WinError(ctypes.get_last_error())
                cleanup = close_entry(read_entry) + close_entry(write_entry)
                if cleanup:
                    raise RuntimeError(str(error) + "; cleanup:" + ";".join(cleanup)) from error
                raise error
            return child_entry, parent_entry

        stdin_read, stdin_write = pipe(True)
        stdout_write, stdout_read = pipe(False)
        stderr_write, stderr_read = pipe(False)
        child_entries = [stdin_read, stdout_write, stderr_write]
        parent_entries = [stdin_write, stdout_read, stderr_read]

        size = ctypes.c_size_t()
        sizing_ok = api.InitializeProcThreadAttributeList(None, 1, 0, ctypes.byref(size))
        sizing_error = ctypes.get_last_error()
        if sizing_ok or sizing_error != 122 or size.value <= 0 or size.value > (1 << 20):
            raise ctypes.WinError(sizing_error or 122)
        attr_buffer = ctypes.create_string_buffer(size.value)
        attr_list = ctypes.cast(attr_buffer, ctypes.c_void_p)
        if not api.InitializeProcThreadAttributeList(attr_list, 1, 0, ctypes.byref(size)):
            raise ctypes.WinError(ctypes.get_last_error())
        attr_initialized = True
        inherited = (W.HANDLE * 3)(stdin_read["handle"], stdout_write["handle"], stderr_write["handle"])
        if not api.UpdateProcThreadAttribute(
                attr_list, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
                ctypes.cast(inherited, ctypes.c_void_p), ctypes.sizeof(inherited),
                None, None):
            raise ctypes.WinError(ctypes.get_last_error())

        startup = STARTUPINFOEXW()
        startup.StartupInfo.cb = ctypes.sizeof(STARTUPINFOEXW)
        startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES
        startup.StartupInfo.hStdInput = stdin_read["handle"]
        startup.StartupInfo.hStdOutput = stdout_write["handle"]
        startup.StartupInfo.hStdError = stderr_write["handle"]
        startup.lpAttributeList = attr_list
        process_info = PROCESS_INFORMATION()
        if not api.CreateProcessW(
                None, command_line, None, None, True,
                CREATE_SUSPENDED | DETACHED_PROCESS | EXTENDED_STARTUPINFO_PRESENT,
                None, str(ROOT), ctypes.byref(startup), ctypes.byref(process_info)):
            raise ctypes.WinError(ctypes.get_last_error())
        launch = SuspendedLaunch(api, process_info.hProcess, process_info.hThread,
                                 int(process_info.dwProcessId), stdin_write["handle"],
                                 stdout_read["handle"], stderr_read["handle"])
        for entry in parent_entries:
            entry["transferred"] = True
        if not api.SetHandleInformation(process_info.hProcess, HANDLE_FLAG_INHERIT, 0):
            raise ctypes.WinError(ctypes.get_last_error())
        if not api.SetHandleInformation(process_info.hThread, HANDLE_FLAG_INHERIT, 0):
            raise ctypes.WinError(ctypes.get_last_error())
        child_errors = []
        for entry in child_entries:
            child_errors.extend(close_entry(entry))
        if child_errors:
            raise RuntimeError("child-handle-close:" + ";".join(child_errors))
        if attr_initialized:
            api.DeleteProcThreadAttributeList(ctypes.cast(attr_buffer, ctypes.c_void_p))
            attr_initialized = False
            attr_buffer = None
        return launch
    except Exception as primary:
        cleanup_errors = []
        deadline = time.monotonic() + CLEANUP_LIMIT_SECONDS
        if launch is not None:
            try:
                cleanup_errors.extend(launch.close(deadline))
            except Exception as exc:
                cleanup_errors.append("launch-close:" + str(exc))
        cleanup_errors.extend(close_ledger())
        if attr_initialized:
            try:
                api.DeleteProcThreadAttributeList(ctypes.cast(attr_buffer, ctypes.c_void_p))
            except Exception as exc:
                cleanup_errors.append("attribute-delete:" + str(exc))
        if cleanup_errors:
            raise RuntimeError(str(primary) + "; cleanup:" + ";".join(cleanup_errors)) from primary
        raise


def _quote_windows_command(argv):
    return subprocess.list2cmdline(list(argv))


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
    if mode == "preassignment-child":
        return 0
    if mode == "preassignment-parent":
        child = subprocess.Popen(
            _helper_command("preassignment-child"),
            stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
        )
        child.wait(timeout=10)
        return 0
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


def preassignment_accounting_decision(initial, baseline, final, ordering_valid=None,
                                      worker_refused=None, cleanup_ok=None,
                                      unexpected_notifications=0, port_api_error=None):
    """Narrow decision for one fresh, controlled, no-breakaway observer job."""
    result = {
        "accounting_property": "no-additional-associated-process-in-fresh-no-breakaway-job",
        "initial": initial, "baseline": baseline, "final": final,
        "ordering_valid": ordering_valid, "worker_refused": worker_refused,
        "cleanup_ok": cleanup_ok,
        "unexpected_notifications": unexpected_notifications,
        "port_api_error": port_api_error,
    }

    def pair(value):
        if not isinstance(value, tuple) or len(value) != 2:
            return None
        total, active = value
        if any(isinstance(item, bool) or type(item) is not int for item in (total, active)):
            return None
        if not (0 <= total <= 0xFFFFFFFF and 0 <= active <= 0xFFFFFFFF):
            return None
        return total, active

    if any(flag is not True and flag is not False
           for flag in (ordering_valid, worker_refused, cleanup_ok)):
        result.update(status="blocked", detail="malformed-flags")
        return result
    if any(pair(value) is None for value in (initial, baseline, final)):
        result.update(status="blocked", detail="malformed-accounting")
        return result
    if (isinstance(unexpected_notifications, bool)
            or type(unexpected_notifications) is not int or unexpected_notifications < 0):
        result.update(status="blocked", detail="malformed-notifications")
        return result

    initial_total, initial_active = pair(initial)
    baseline_total, baseline_active = pair(baseline)
    final_total, final_active = pair(final)
    if not ordering_valid:
        result.update(status="blocked", detail="ordering-not-validated")
        return result
    if initial_total != 0 or initial_active != 0:
        result.update(status="blocked", detail="initial-accounting-not-zero")
        return result
    if baseline_total != 1 or baseline_active != 1:
        result.update(status="blocked", detail="baseline-accounting-not-one")
        return result
    if final_total > baseline_total:
        result.update(status="failed", detail="extra-associated-process",
                      final_total=final_total, final_active=final_active)
        return result
    if final_total < baseline_total:
        result.update(status="blocked", detail="inconsistent-lifetime-total",
                      final_total=final_total, final_active=final_active)
        return result
    if final_active != 0:
        result.update(status="blocked", detail="final-active-not-zero",
                      final_total=final_total, final_active=final_active)
        return result
    if unexpected_notifications > 0:
        result.update(status="failed", detail="unexpected-new-process-notification",
                      final_total=final_total, final_active=final_active)
        return result
    if not worker_refused:
        result.update(status="blocked", detail="worker-refusal-not-observed")
        return result
    if not cleanup_ok:
        result.update(status="blocked", detail="cleanup-not-successful")
        return result

    notification_state = "api-error" if port_api_error is not None else "no-unexpected-notifications"
    result.update(status="passed", detail="single-associated-process",
                  final_total=final_total, final_active=final_active,
                  notification_corroboration=notification_state,
                  clean_run=port_api_error is None)
    return result


def _preassignment_attempt(mode, observer, launcher=None, clock=time.monotonic,
                           wait_seconds=10.0, setup_seconds=10.0,
                           observation_seconds=10.0):
    launcher = launcher or launch_suspended
    launch = None
    initial = None
    baseline = None
    final = None
    exit_code = None
    setup_deadline = clock() + setup_seconds
    observation_deadline = None
    ordering = {
        "worker_suspended": False,
        "initial_zero": False,
        "assigned_before_resume": False,
        "membership_verified": False,
        "baseline_one": False,
        "resumed": False,
        "eof_after_assignment": False,
        "worker_exited": False,
        "process_released_before_final": False,
    }
    unexpected = []
    port_error = None
    cleanup_errors = []
    operation_error = False
    detail = "not-observed"
    observer_stop = None
    observer_thread = None
    observer_errors = []
    observer_start_attempted = False
    observer_unresolved = False
    observation_view = None
    seen_notifications = set()

    def observed(pid, identity_job=None):
        if pid in seen_notifications or pid == (launch.pid if launch is not None else None):
            return
        seen_notifications.add(pid)
        identity, identity_error = process_evidence(pid, identity_job or observer)
        unexpected.append({"pid": pid, "identity": identity, "identity_error": identity_error})

    def setup_remaining():
        if clock() >= setup_deadline:
            raise RuntimeError("setup-deadline")

    def observation_remaining():
        if observation_deadline is None or clock() >= observation_deadline:
            raise RuntimeError("observation-deadline")
    try:
        observer_api = getattr(observer, "api", None)
        for handle_name in ("handle", "port"):
            handle = getattr(observer, handle_name, None)
            if observer_api is not None and handle:
                if not observer_api.SetHandleInformation(handle, HANDLE_FLAG_INHERIT, 0):
                    raise ctypes.WinError(ctypes.get_last_error())
        setup_remaining()
        initial = observer.accounting()
        ordering["initial_zero"] = initial == (0, 0)
        if initial != (0, 0):
            raise RuntimeError("initial-accounting-not-zero")
        setup_remaining()
        launch = launcher(mode)
        ordering["worker_suspended"] = True
        setup_remaining()
        observer.assign(launch)
        ordering["assigned_before_resume"] = True
        setup_remaining()
        if not observer.is_process_in_job(launch):
            raise RuntimeError("membership-not-verified")
        ordering["membership_verified"] = True
        setup_remaining()
        baseline = observer.accounting()
        ordering["baseline_one"] = baseline == (1, 1)
        if not ordering["baseline_one"]:
            raise RuntimeError("baseline-accounting-not-one")
        setup_remaining()
        observation_deadline = clock() + observation_seconds
        if hasattr(observer, "start_observation"):
            observer_start_attempted = True
            observer_stop, observer_thread, observer_errors, observation_view = observer.start_observation(
                observation_deadline, observed, clock)
            observer_start_attempted = False
        launch.resume()
        ordering["resumed"] = True
        launch.close_stdin()
        ordering["eof_after_assignment"] = True
        observation_remaining()
        remaining = max(0.0, observation_deadline - clock())
        if not launch.wait(min(wait_seconds, remaining) * 1000):
            raise RuntimeError("worker-wait-timeout")
        ordering["worker_exited"] = True
        observation_remaining()
        if observer_stop is not None:
            observer_stop.set()
        if observer_thread is not None:
            while observer_thread.is_alive() and clock() < observation_deadline:
                observer_thread.join(min(0.05, max(0, observation_deadline - clock())))
            if observer_thread.is_alive():
                observer_unresolved = True
                raise RuntimeError("observer-thread-timeout")
        exit_code = launch.exit_code
        launch.close_process()
        ordering["process_released_before_final"] = True
        observation_remaining()
        final = observer.accounting()
        observation_remaining()

        observation_remaining()
        remaining = max(0.0, observation_deadline - clock())
        try:
            late_error = observer.drain_messages(clock() + min(2.0, remaining),
                                                  lambda pid: observed(pid, observer), clock=clock)
            if late_error and port_error is None:
                port_error = late_error
        except Exception as exc:
            if port_error is None:
                port_error = "completion-port:" + str(exc)
        observation_remaining()
    except ObserverOwnershipError as exc:
        detail = type(exc).__name__ + ": " + str(exc)
        operation_error = True
        observer_unresolved = True
        observation_view = exc.view
    except Exception as exc:
        detail = type(exc).__name__ + ": " + str(exc)
        operation_error = True
        if observer_start_attempted and observer_thread is None:
            observer_unresolved = True
    finally:
        cleanup_deadline = clock() + CLEANUP_LIMIT_SECONDS
        if observer_stop is not None:
            try:
                observer_stop.set()
            except Exception as exc:
                cleanup_errors.append("observer-stop:" + str(exc))
        if observer_thread is not None:
            alive = True
            while alive and clock() < cleanup_deadline:
                try:
                    alive = observer_thread.is_alive()
                except Exception as exc:
                    cleanup_errors.append("observer-is-alive:" + str(exc))
                    alive = True
                    break
                if not alive:
                    break
                try:
                    observer_thread.join(min(0.05, max(0, cleanup_deadline - clock())))
                except Exception as exc:
                    cleanup_errors.append("observer-join:" + str(exc))
                    break
            if alive:
                observer_unresolved = True
                cleanup_errors.append("observer-thread-timeout")
        if observation_view is not None and observation_view.close_failed:
            observer_unresolved = True
            cleanup_errors.extend(observation_view.close_errors)
        if observer_errors:
            port_error = "observer:" + ";".join(str(item) for item in observer_errors)
        if observer_unresolved:
            if observation_view is not None and observation_view not in UNRESOLVED_OBSERVERS:
                UNRESOLVED_OBSERVERS.append(observation_view)
            if hasattr(observer, "terminate_checked"):
                try:
                    observer.terminate_checked()
                except Exception as exc:
                    cleanup_errors.append("observer-terminate:" + str(exc))
            else:
                cleanup_errors.append("observer-terminate-unavailable")
        if launch is not None:
            try:
                cleanup_errors.extend(launch.close(cleanup_deadline, clock))
            except Exception as exc:
                cleanup_errors.append("launch-close:" + str(exc))
        try:
            observer.close_checked()
        except Exception as exc:
            cleanup_errors.append("observer-close:" + str(exc))
    decision = preassignment_accounting_decision(
        initial, baseline, final,
        ordering_valid=all(ordering.values()),
        worker_refused=exit_code == 2,
        cleanup_ok=not cleanup_errors,
        unexpected_notifications=len(unexpected),
        port_api_error=port_error,
    )
    decision.update(mode=mode, ordering=ordering, exit_code=exit_code,
                    cleanup_errors=cleanup_errors, operation_detail=detail,
                    operation_error=operation_error, unexpected_processes=unexpected,
                    initial_observation=initial, baseline_observation=baseline,
                    final_pre_cleanup=final,
                    unresolved_observer=observer_unresolved,
                    unresolved_handles=(list(observation_view.unresolved_handles)
                                        if observation_view is not None else []),
                    post_cleanup_accounting="not-queried-after-job-close")
    if operation_error:
        if decision.get("detail") == "extra-associated-process":
            pass
        else:
            if decision.get("status") == "passed":
                decision["accounting_status"] = "passed"
            decision["status"] = "blocked"
            decision["detail"] = "operation-error"
        decision["clean_run"] = False
    elif decision["status"] == "passed" and port_error is not None:
        decision["status"] = "blocked"
        decision["detail"] = "accounting-proved-with-port-api-error"
        decision["accounting_status"] = "passed"
        decision["clean_run"] = False
    return decision


def preassignment_control_success(control):
    """A control is successful only when it exercised a clean extra-association refusal."""
    if not isinstance(control, dict):
        return False
    ordering = control.get("ordering")
    if not isinstance(ordering, dict) or not ordering or not all(ordering.values()):
        return False
    if control.get("status") != "failed" or control.get("detail") != "extra-associated-process":
        return False
    if control.get("exit_code") != 0 or control.get("operation_error") is not False:
        return False
    final = control.get("final")
    if (not isinstance(final, tuple) or len(final) != 2
            or any(isinstance(value, bool) or type(value) is not int for value in final)):
        return False
    if final[0] < 2 or final[1] != 0:
        return False
    if control.get("cleanup_errors"):
        return False
    if control.get("port_api_error") is not None:
        return False
    return True


def _case_preassignment_eof(work):
    attempt_errors = []
    try:
        negative = _preassignment_attempt("worker", ObservedJob())
    except Exception as exc:
        negative = {"status": "blocked", "detail": "negative-setup:" + str(exc)}
        attempt_errors.append(str(exc))
    try:
        control = _preassignment_attempt("control", ObservedJob())
    except Exception as exc:
        control = {"status": "blocked", "detail": "control-setup:" + str(exc)}
        attempt_errors.append(str(exc))
    negative_ok = (negative.get("status") == "passed"
                   and negative.get("operation_error") is False
                   and negative.get("clean_run") is True)
    control_ok = preassignment_control_success(control)
    result = {
        "status": "passed" if negative_ok and control_ok and not attempt_errors else "blocked",
        "detail": "preassignment-accounting-and-control" if negative_ok and control_ok else "preassignment-not-proved",
        "negative": negative,
        "positive_control": control,
        "owned": [],
        "synthetic_analogue": False,
        "actual_worker_path": True,
        "reader_path_evidence": False,
    }
    if attempt_errors:
        result["attempt_errors"] = attempt_errors
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
