# R04 checkpoint 11: process-reader lifecycle admission

Status: `proposed_unapproved`. Source investigation only. No code, test, catalog, process, PowerShell, native, CIM, editor, build or recovery action was performed.

- **Approved base:** `2d120a9e282e149b7e0f2368a4acf44f5035fd03`
- **Claim:** `b3223a4a39b04b599d4ab4bf14029a78` (`dsh-r04-lifecycle-01`)
- **Engine:** 5.8.3 at `396c9f059903aed5fec78ecd3d437a40c6415368`
- **Live status:** blocked. Checkpoint 10 is accepted only as an offline-tested implementation.

## Question

Can the current `read_process` runner guarantee that Add-Type's compiler work is gone after timeout, error or parent death? If not, what is the smallest admission strategy that contains the dedicated collection process and its own descendants without touching shared CIM/WMI services or unrelated processes?

## Known facts

1. **Windows PowerShell 5.1 Add-Type uses CodeDom and can start a compiler child.**
   - Add-Type's 5.1 syntax exposes `-CodeDomProvider` and `-CompilerParameters` and documents source compilation into an in-memory assembly: <https://learn.microsoft.com/en-us/powershell/module/microsoft.powershell.utility/add-type?view=powershell-5.1>.
   - .NET Framework `CSharpCodeProvider` delegates to the CodeDom compiler. The Microsoft reference source names the compiler `csc.exe` (`CompilerName`) and its `Executor` uses `Process`/`CreateProcess` to wait for the compiler: <https://github.com/microsoft/referencesource/blob/main/System/compmod/microsoft/csharp/csharpcodeprovider.cs>, <https://github.com/microsoft/referencesource/blob/main/System/compmod/system/codedom/compiler/Executor.cs>.
   - Therefore Add-Type **can** create a compiler child. Whether this exact Windows build/PS 5.1 invocation spawns `csc.exe` rather than compiling another way is **unverified** without live execution.

2. **Whether the compiler child inherits the PowerShell result pipes is unverified.**
   - The CodeDom Executor captures compiler output through its own files/streams; that suggests it may not write to the PowerShell stdout/stderr pipes.
   - This is not established from repository code. Treat the compiler as a possible pipe-holder and as an owned descendant, not as a proven independent process.

3. **The current runner cleans up only its direct Popen child.**
   - `Tools/persistence_process_reader.py:192-341` drains bounded pipes, kills/reaps `proc`, and joins drain threads.
   - It has no Job Object. It cannot prove a `csc.exe` child of PowerShell is gone. On timeout/parent death, a compiler child that inherited or shared a pipe can outlive the PowerShell child; closing the pipe in Python does not kill it.
   - Its cleanup is otherwise sound for the direct child: `_cleanup` kills, waits, closes pipes and bounds joins (`:196-242`).

4. **CIM/WMI is not a child process.**
   - `Get-CimInstance Win32_Process` talks to the shared WMI/CIM service/provider host. The service is external and must never be killed or treated as an owned descendant.
   - Only the dedicated PowerShell process and processes it creates for the collection are in scope.

5. **An existing Job Object helper is available.**
   - `Tools/execution_runtime.py:70-92` creates a Job Object with `JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE` (`0x2000`), assigns a process, and terminates the job on close.
   - `run_owned` (`:95-117`) launches a Python worker, assigns it to the job **before** writing the command, and the worker waits on stdin before calling `subprocess.call` (`:155-158`). This is the existing “bootstrap waits for assignment” pattern.
   - The existing synthetic test at `Tools/test-execution-runtime.py:52-64` proves a timed-out worker's descendant is killed and an unrelated sentinel survives. It is evidence for the helper pattern, not for Add-Type or the process reader.

## Decision

Choose **(a) Job Object containment of the dedicated collection process and its descendants**. Reject **(b) avoiding the compiler child**: removing Add-Type would require a precompiled helper or managed API that still supplies exact native FILETIME/process handles; that would add a binary/dependency or a persistent service, both outside this slice. A narrower alternative does not exist without weakening the evidence.

The containment must be established **before PowerShell executes any script code**, not merely before the Python `Popen` returns.

## Proposed admission strategy

Use the existing bootstrap pattern, without a broad runner rewrite:

1. Create a fresh Job Object with `KILL_ON_JOB_CLOSE` before launching the collection process.
2. Launch the fixed Python worker (`execution_runtime.py --worker`) with hidden/no-window startup and bounded stdin/stdout/stderr pipes. The worker waits for one JSON command before doing anything else.
3. Assign the worker to the job immediately. If assignment fails, terminate the worker and refuse; do not feed a command and do not fall back to killing a process tree.
4. Send exactly the fixed `[powershell.exe, -NoProfile, -NonInteractive, -File, fixed-script, -ProcessId, pid]` command. The worker launches PowerShell; PowerShell and any `csc.exe` child created by Add-Type inherit the job by default.
5. Keep the existing 64 KiB bounded drains and monotonic deadline. On timeout/overflow/error, terminate/close the job, then wait a bounded time for the worker and pipes. Do not collect a new child or enumerate the process tree.
6. On all exits close the job handle and both pipes. If cleanup cannot be proven (job still has active processes, worker wait failed, pipe close/join failed), report `cleanup_failed`/unknown.

### Why not suspended launch

`CREATE_SUSPENDED` plus `ResumeThread` would also establish assignment before execution, but Python's `Popen` does not expose the primary thread handle; the implementation would need a second native launcher. The worker already waits before spawning, so it provides the same ordering with the existing helper. No suspended/resume code is proposed.

### Nested jobs, assignment and containment

- Windows 8/Server 2012 and later allow nested jobs; older Windows permits only one job per process. The outer `execute-tests`/`run_owned` job may already contain the supervisor.
- If `AssignProcessToJobObject` fails, the bootstrap worker is killed and the call refuses. No fallback, no process-tree enumeration.
- `JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE` terminates job members when the last handle closes. The job handle must stay in the Python supervisor and must not be inherited by the worker or PowerShell.
- `JOB_OBJECT_LIMIT_BREAKAWAY_OK`/`SILENT_BREAKAWAY_OK` are **not** set; descendants inherit the job unless a child uses an explicit breakaway path.
- A compiler child created by `CreateProcess` inherits the job. A child created through `Win32_Process.Create` would not; the reference-source CodeDom path uses `CreateProcess`, but this is not proven on the target host. The supervisor-only acceptance must prove the actual Add-Type descendant is in the job. If it cannot, live use remains blocked.
- Shared CIM/WMI services are outside the job and are never terminated.

### Timeout, grace and abnormal exit

- The request deadline is authoritative. On expiry, terminate/close the job immediately; a cleanup grace of at most 5 seconds is allowed for the worker/pipes, never an extension of the collection budget.
- On overflow or any collection error, close the job and reap the worker; drains remain bounded.
- Hard supervisor death closes the job handle in the OS; `KILL_ON_JOB_CLOSE` terminates worker, PowerShell and in-job compiler descendants. If death occurs before assignment/command, the worker is blocked reading stdin; pipe EOF prevents Add-Type from running.
- The supervisor should query the job's active-process count before declaring cleanup success. A nonzero count is `cleanup_failed`; it is not retried by probing unrelated processes.
- Orphan detection is job-based. No shared service, editor or unrelated process is stopped.

## Mock fidelity correction

`Tools/test-persistence-process-reader.py:384-405` currently has `FakeThread.join` return without error for an unstarted thread. Real Python `Thread.join` raises `RuntimeError` before start. This affects `_cleanup`'s `cleanup_failed` classification in the thread-start-failure mocks; it is **not** proof that the real runner leaks a child. The proposed implementation scope includes changing the fake to raise on unstarted `join`, then asserting the runner reports cleanup failure honestly.

## Proposed implementation files

Not implemented here:

- `Tools/persistence_process_reader.py` — `_PopenRunner` job/bootstrap launch, assignment-before-command, job-based timeout/cleanup, job accounting, bounded drains.
- `Tools/execution_runtime.py` — only if the existing `WindowsJob`/worker cannot be reused unchanged; a small worker mode may be needed to preserve hidden startup for the PowerShell child. Do not rewrite `run_owned`.
- `Tools/test-persistence-process-reader.py` — injectable Job/worker mocks and the unstarted-join fidelity fix.
- `Tools/test-execution-runtime.py` — extend the existing synthetic job pattern if a real Job test is required by the future supervisor.

No change to the PowerShell script, collector/validator, catalog or production caller is proposed for this lifecycle slice.

## Supervisor-only synthetic acceptance (proposal, unexecuted)

Use only newly created harmless test children and synthetic strings; never an editor, existing editor process or real command line.

1. **Forced timeout / descendant containment:** a synthetic worker under a new job creates a Python grandchild that writes a unique marker after sleeping. Force the request timeout; verify worker and grandchild are gone, marker absent, job active count zero, and an unrelated synthetic process survives.
2. **Inherited-pipe child:** a synthetic child writes to an inherited stdout pipe and sleeps past the deadline. Verify bounded drains return, the job termination closes the pipe, no partial result is accepted, and the child is gone.
3. **Parent death:** start a synthetic supervisor that creates the job and child; hard-terminate the supervisor; verify the child/grandchild are gone and no orphan holds the pipe or output file.
4. **Add-Type compiler descendant:** run a synthetic PowerShell script containing only a harmless `Add-Type` class and a marker; record whether a compiler descendant exists and whether it is in the job. Refuse live admission if this cannot be proven.
5. **Negative control:** an unrelated synthetic Python process outside the job remains alive and unchanged.

All tests must run only under the supervisor, with unique temp paths and no existing process/command-line inspection.

## Rollback

Revert the bounded reader/job changes and test-fidelity change together. No catalog or production caller is added by this slice. Job handles and worker processes are closed/terminated on rollback.

## Blockers and unverified facts

- No live validation exists for Add-Type creating `csc.exe`, for job inheritance by the compiler, for nested-job assignment, or for parent-death containment.
- The current direct-child runner cannot guarantee compiler-descendant cleanup.
- The worker's hidden-startup behavior for its PowerShell child and the script's real parsing/API/UTF-8/handle behavior are unverified.
- Shared CIM/WMI services are outside containment and must not be killed.
- Live process-reader admission remains blocked until the supervisor-only synthetic acceptance proves the job path and compiler containment.
