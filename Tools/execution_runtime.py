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


class BASIC_ACCOUNTING(ctypes.Structure):
    _fields_ = [
        ('TotalUserTime', ctypes.c_longlong),
        ('TotalKernelTime', ctypes.c_longlong),
        ('ThisPeriodTotalUserTime', ctypes.c_longlong),
        ('ThisPeriodTotalKernelTime', ctypes.c_longlong),
        ('TotalPageFaultCount', W.DWORD),
        ('TotalProcesses', W.DWORD),
        ('ActiveProcesses', W.DWORD),
        ('TotalTerminatedProcesses', W.DWORD),
    ]


class WindowsJob:
    def __init__(self, api=None, handle=None):
        self.api = api or ctypes.WinDLL('kernel32', use_last_error=True)
        if api is None:
            self.api.CreateJobObjectW.argtypes = [ctypes.c_void_p, W.LPCWSTR]
            self.api.CreateJobObjectW.restype = W.HANDLE
            self.api.SetInformationJobObject.argtypes = [W.HANDLE, ctypes.c_int, ctypes.c_void_p, W.DWORD]
            self.api.SetInformationJobObject.restype = W.BOOL
            self.api.AssignProcessToJobObject.argtypes = [W.HANDLE, W.HANDLE]
            self.api.AssignProcessToJobObject.restype = W.BOOL
            self.api.TerminateJobObject.argtypes = [W.HANDLE, W.UINT]
            self.api.TerminateJobObject.restype = W.BOOL
            self.api.QueryInformationJobObject.argtypes = [W.HANDLE, ctypes.c_int, ctypes.c_void_p, W.DWORD, ctypes.POINTER(W.DWORD)]
            self.api.QueryInformationJobObject.restype = W.BOOL
            self.api.IsProcessInJob.argtypes = [W.HANDLE, W.HANDLE, ctypes.POINTER(W.BOOL)]
            self.api.IsProcessInJob.restype = W.BOOL
            self.api.CloseHandle.argtypes = [W.HANDLE]
            self.api.CloseHandle.restype = W.BOOL
        self.handle = handle
        if self.handle is None:
            self.handle = self.api.CreateJobObjectW(None, None)
            if not self.handle: raise ctypes.WinError(ctypes.get_last_error())
            info = EXTENDED(); info.BasicLimitInformation.LimitFlags = 0x2000  # KILL_ON_JOB_CLOSE
            if not self.api.SetInformationJobObject(self.handle, 9, ctypes.byref(info), ctypes.sizeof(info)):
                self.close(); raise ctypes.WinError(ctypes.get_last_error())

    def assign(self, process):
        if not self.api.AssignProcessToJobObject(self.handle, W.HANDLE(int(process._handle))):
            raise ctypes.WinError(ctypes.get_last_error())

    def is_process_in_job(self, process):
        present = W.BOOL()
        if not self.api.IsProcessInJob(W.HANDLE(int(process._handle)), self.handle, ctypes.byref(present)):
            raise ctypes.WinError(ctypes.get_last_error())
        return bool(present.value)

    def terminate_checked(self, exit_code=125):
        if not self.handle:
            raise RuntimeError('Job handle is closed')
        if not self.api.TerminateJobObject(self.handle, exit_code):
            raise ctypes.WinError(ctypes.get_last_error())

    def active_processes(self):
        if not self.handle:
            raise RuntimeError('Job handle is closed')
        info = BASIC_ACCOUNTING()
        if not self.api.QueryInformationJobObject(
                self.handle, 1, ctypes.byref(info), ctypes.sizeof(info), None):
            raise ctypes.WinError(ctypes.get_last_error())
        return int(info.ActiveProcesses)

    def close_checked(self):
        if self.handle:
            if not self.api.CloseHandle(self.handle):
                raise ctypes.WinError(ctypes.get_last_error())
            self.handle = None

    def close(self):
        if self.handle:
            self.api.TerminateJobObject(self.handle, 125)
            self.api.CloseHandle(self.handle); self.handle = None


PERSISTENCE_REQUEST_LIMIT = 512
PERSISTENCE_SCHEMA = 1


def _persistence_unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError('duplicate JSON key')
        result[key] = value
    return result


def _persistence_reject_constant(value):
    raise ValueError('nonfinite JSON constant')


def parse_persistence_request(raw):
    """Parse the one complete UTF-8 request accepted by --persistence-worker."""
    if not isinstance(raw, (bytes, bytearray)) or not raw or len(raw) > PERSISTENCE_REQUEST_LIMIT:
        raise ValueError('request must be one bounded nonempty message')
    try:
        text = bytes(raw).decode('utf-8')
    except UnicodeDecodeError:
        raise ValueError('request is not UTF-8')
    decoder = json.JSONDecoder(
        object_pairs_hook=_persistence_unique_object,
        parse_constant=_persistence_reject_constant,
    )
    try:
        value, end = decoder.raw_decode(text)
    except (ValueError, TypeError, RecursionError):
        raise ValueError('request is not one JSON value')
    if end != len(text):
        raise ValueError('request has trailing bytes')
    if not isinstance(value, dict) or set(value) != {'schema', 'pid'}:
        raise ValueError('request has unknown or missing fields')
    schema = value['schema']
    pid = value['pid']
    if isinstance(schema, bool) or type(schema) is not int or schema != PERSISTENCE_SCHEMA:
        raise ValueError('unsupported request schema')
    if isinstance(pid, bool) or type(pid) is not int or not 1 <= pid <= 0xFFFFFFFF:
        raise ValueError('invalid process id')
    return pid


def persistence_command(pid):
    """Derive the fixed collection command; no request-supplied paths or argv."""
    root = os.environ.get('SystemRoot') or os.environ.get('WINDIR')
    if not root:
        raise ValueError('Windows installation is unavailable')
    powershell = str(Path(root) / 'System32' / 'WindowsPowerShell' / 'v1.0' / 'powershell.exe')
    script = Path(__file__).resolve().with_name('read-persistence-process.ps1')
    return [
        powershell,
        '-NoProfile',
        '-NonInteractive',
        '-File',
        str(script),
        '-ProcessId',
        str(pid),
    ]


def persistence_worker(stream=None, launcher=None):
    """One-shot worker: complete request plus EOF, then the fixed hidden child."""
    if os.name != 'nt':
        return 2
    source = stream if stream is not None else sys.stdin.buffer
    try:
        raw = source.read(PERSISTENCE_REQUEST_LIMIT + 1)
        if 0 < len(raw) <= PERSISTENCE_REQUEST_LIMIT:
            if source.read(1):
                return 2
    except Exception:
        return 2
    try:
        pid = parse_persistence_request(raw)
        command = persistence_command(pid)
    except ValueError:
        return 2
    call = launcher if launcher is not None else subprocess.call
    try:
        return call(
            command,
            stdin=subprocess.DEVNULL,
            creationflags=getattr(subprocess, 'CREATE_NO_WINDOW', 0),
        )
    except OSError:
        return 2


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
    if sys.argv[1:] == ['--persistence-worker']:
        sys.exit(persistence_worker())
    if sys.argv[1:] == ['--worker']:
        command=json.loads(sys.stdin.buffer.readline())
        sys.exit(subprocess.call(command))
    if len(sys.argv)==4 and sys.argv[1]=='--authorize':
        try:require_context(sys.argv[2],sys.argv[3])
        except (OSError,ValueError,KeyError,RuntimeError) as error:sys.exit('REFUSED: '+str(error))
    else:sys.exit('Internal execution helper; use execute-tests.py')
