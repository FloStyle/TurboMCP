"""Windows execution ownership primitives. Same-user coordination, not a sandbox."""
import ctypes
from ctypes import wintypes as W
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import time


def canonical(path):
    return os.path.normcase(str(Path(path).resolve()))


class ResourceLocks:
    """Kernel byte-range locks: atomic across processes, released on process death."""
    def __init__(self, root, resources):
        self.root = Path(root)
        self.resources = sorted(set(resources))
        self.handles = []

    def __enter__(self):
        import msvcrt
        self.root.mkdir(parents=True, exist_ok=True)
        try:
            for resource in self.resources:
                path = self.root / (hashlib.sha256(resource.encode()).hexdigest() + '.lock')
                handle = open(path, 'a+b')
                if path.stat().st_size == 0:
                    handle.write(b'0'); handle.flush()
                handle.seek(0)
                try:
                    msvcrt.locking(handle.fileno(), msvcrt.LK_NBLCK, 1)
                except OSError:
                    handle.close()
                    raise RuntimeError('Resource is already owned; no process was stopped')
                self.handles.append(handle)
            return self
        except BaseException:
            self.__exit__(None, None, None)
            raise

    def __exit__(self, *unused):
        import msvcrt
        for handle in reversed(self.handles):
            handle.seek(0)
            msvcrt.locking(handle.fileno(), msvcrt.LK_UNLCK, 1)
            handle.close()
        self.handles.clear()


class BASIC(ctypes.Structure):
    _fields_ = [('PerProcessUserTimeLimit', ctypes.c_longlong), ('PerJobUserTimeLimit', ctypes.c_longlong),
                ('LimitFlags', W.DWORD), ('MinimumWorkingSetSize', ctypes.c_size_t),
                ('MaximumWorkingSetSize', ctypes.c_size_t), ('ActiveProcessLimit', W.DWORD),
                ('Affinity', ctypes.c_size_t), ('PriorityClass', W.DWORD), ('SchedulingClass', W.DWORD)]


class IO(ctypes.Structure):
    _fields_ = [(name, ctypes.c_ulonglong) for name in ('ReadOperationCount', 'WriteOperationCount', 'OtherOperationCount', 'ReadTransferCount', 'WriteTransferCount', 'OtherTransferCount')]


class EXTENDED(ctypes.Structure):
    _fields_ = [('BasicLimitInformation', BASIC), ('IoInfo', IO), ('ProcessMemoryLimit', ctypes.c_size_t),
                ('JobMemoryLimit', ctypes.c_size_t), ('PeakProcessMemoryUsed', ctypes.c_size_t), ('PeakJobMemoryUsed', ctypes.c_size_t)]


class WindowsJob:
    def __init__(self):
        self.api = ctypes.WinDLL('kernel32', use_last_error=True)
        self.api.CreateJobObjectW.argtypes = [ctypes.c_void_p, W.LPCWSTR]
        self.api.CreateJobObjectW.restype = W.HANDLE
        self.api.SetInformationJobObject.argtypes = [W.HANDLE, ctypes.c_int, ctypes.c_void_p, W.DWORD]
        self.api.AssignProcessToJobObject.argtypes = [W.HANDLE, W.HANDLE]
        self.api.TerminateJobObject.argtypes = [W.HANDLE, W.UINT]
        self.api.CloseHandle.argtypes = [W.HANDLE]
        self.handle = self.api.CreateJobObjectW(None, None)
        if not self.handle: raise ctypes.WinError(ctypes.get_last_error())
        info = EXTENDED(); info.BasicLimitInformation.LimitFlags = 0x2000  # KILL_ON_JOB_CLOSE
        if not self.api.SetInformationJobObject(self.handle, 9, ctypes.byref(info), ctypes.sizeof(info)):
            self.close(); raise ctypes.WinError(ctypes.get_last_error())

    def assign(self, process):
        if not self.api.AssignProcessToJobObject(self.handle, W.HANDLE(int(process._handle))):
            raise ctypes.WinError(ctypes.get_last_error())

    def close(self):
        if self.handle:
            self.api.TerminateJobObject(self.handle, 125)
            self.api.CloseHandle(self.handle); self.handle = None


def run_owned(command, cwd, env, log, timeout):
    """Bootstrap waits for assignment before it can spawn any descendants."""
    start = time.monotonic(); job = WindowsJob(); process = None
    try:
        with open(log, 'wb') as output:
            process = subprocess.Popen([sys.executable, str(Path(__file__).resolve()), '--worker'],
                                       stdin=subprocess.PIPE, stdout=output, stderr=subprocess.STDOUT,
                                       cwd=cwd, env=env, creationflags=subprocess.CREATE_NO_WINDOW)
            job.assign(process)
            process.stdin.write(json.dumps(command).encode() + b'\n')
            process.stdin.close()
            try:
                code = process.wait(timeout=timeout)
                timed_out = False
            except subprocess.TimeoutExpired:
                code = 124
                timed_out = True
                job.close(); process.wait(timeout=15)
        return {'exit_code': code, 'duration_seconds': round(time.monotonic()-start, 3), 'timed_out': timed_out}
    finally:
        job.close()
        if process is not None and process.poll() is None:
            process.kill(); process.wait(timeout=15)


def parent_ids():
    """Read actual process ancestry. Environment variables alone grant nothing."""
    class ENTRY(ctypes.Structure):
        _fields_ = [('dwSize', W.DWORD), ('cntUsage', W.DWORD), ('th32ProcessID', W.DWORD),
                    ('th32DefaultHeapID', ctypes.c_size_t), ('th32ModuleID', W.DWORD), ('cntThreads', W.DWORD),
                    ('th32ParentProcessID', W.DWORD), ('pcPriClassBase', W.LONG), ('dwFlags', W.DWORD), ('szExeFile', W.WCHAR*260)]
    api=ctypes.WinDLL('kernel32',use_last_error=True)
    api.CreateToolhelp32Snapshot.argtypes=[W.DWORD,W.DWORD];api.CreateToolhelp32Snapshot.restype=W.HANDLE
    api.Process32FirstW.argtypes=[W.HANDLE,ctypes.POINTER(ENTRY)];api.Process32NextW.argtypes=[W.HANDLE,ctypes.POINTER(ENTRY)]
    api.CloseHandle.argtypes=[W.HANDLE]
    snap=api.CreateToolhelp32Snapshot(2,0)
    if snap == ctypes.c_void_p(-1).value: raise ctypes.WinError(ctypes.get_last_error())
    parents={};entry=ENTRY();entry.dwSize=ctypes.sizeof(entry)
    try:
        ok=api.Process32FirstW(snap,ctypes.byref(entry))
        while ok:
            parents[entry.th32ProcessID]=entry.th32ParentProcessID
            ok=api.Process32NextW(snap,ctypes.byref(entry))
    finally:api.CloseHandle(snap)
    result=set();pid=os.getpid()
    while pid in parents and pid not in result:
        result.add(pid);pid=parents[pid]
    return result


def require_context(repo, engine):
    context=os.environ.get('TURBOMCP_EXECUTION_CONTEXT','')
    if not context: raise RuntimeError('Use python Tools/execute-tests.py; direct engine operations require owned execution')
    data=json.loads(Path(context).read_text(encoding='utf8'))
    if data['repo'] != canonical(repo) or data['engine'] != canonical(engine):
        raise RuntimeError('Execution context does not own this repository and engine')
    if data['pid'] not in parent_ids() or data['token'] != os.environ.get('TURBOMCP_EXECUTION_TOKEN'):
        raise RuntimeError('Execution owner is not a live ancestor; refusing stale or copied context')


if __name__ == '__main__':
    if sys.argv[1:] == ['--worker']:
        command=json.loads(sys.stdin.buffer.readline())
        sys.exit(subprocess.call(command))
    if len(sys.argv)==4 and sys.argv[1]=='--authorize':
        try:require_context(sys.argv[2],sys.argv[3])
        except (OSError,ValueError,KeyError,RuntimeError) as error:sys.exit('REFUSED: '+str(error))
    else:sys.exit('Internal execution helper; use execute-tests.py')
