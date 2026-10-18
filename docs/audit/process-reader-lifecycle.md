# R04 checkpoint 11: process-reader lifecycle admission

Status: `proposed_unapproved`. Source investigation only. No code, test, catalog, process, PowerShell, native, CIM, editor, build or recovery action was performed.

- **Approved base:** `2d120a9e282e149b7e0f2368a4acf44f5035fd03`
- **Claim:** `b3223a4a39b04b599d4ab4bf14029a78` (`dsh-r04-lifecycle-01`)
- **Engine:** 5.8.3 at `396c9f059903aed5fec78ecd3d437a40c6415368`
- **Live status:** blocked. Checkpoint 10 is accepted only as an offline-tested implementation.

## Question

Can the current `read_process` runner guarantee that Add-Type's compiler work is gone after timeout, error or parent death? What is the smallest admission strategy that contains the dedicated collection process and its own descendants without touching shared CIM/WMI services or unrelated processes?

## Known facts

1. **Windows PowerShell 5.1 Add-Type uses CodeDom and can start a compiler child.**
   - Add-Type's 5.1 syntax exposes `-CodeDomProvider` and `-CompilerParameters` and documents source compilation into an in-memory assembly: <https://learn.microsoft.com/en-us/powershell/module/microsoft.powershell.utility/add-type?view=powershell-5.1>.
   - .NET Framework `CSharpCodeProvider` delegates to the CodeDom compiler. The Microsoft reference source names the compiler `csc.exe` (`CompilerName`) and its `Executor` uses `Process`/`CreateProcess` to wait for the compiler: <https://github.com/microsoft/referencesource/blob/main/System/compmod/microsoft/csharp/csharpcodeprovider.cs>, <https://github.com/microsoft/referencesource/blob/main/System/compmod/system/codedom/compiler/Executor.cs>.
   - Therefore Add-Type **can** create a compiler child. Whether this exact Windows build/PS 5.1 invocation spawns `csc.exe` rather than compiling another way is **unverified**.

2. **Compiler pipe inheritance is unverified.**
   - CodeDom captures compiler output through its own files/streams, suggesting it may not use the PowerShell result pipes.
   - This is not established. Treat the compiler as a possible pipe-holder and an owned descendant; do not assume independence.

3. **The current runner cleans up only its direct Popen child.**
   - `Tools/persistence_process_reader.py:192-341` drains bounded pipes, kills/reaps `proc`, closes pipes, and bounds joins (`:196-242`).
   - This is **offline-tested direct-child cleanup**, not native proof for compiler descendants. It has no Job Object; a compiler child that shares a pipe or survives PowerShell is outside its guarantee.

4. **CIM/WMI is a shared external service.**
   - `Get-CimInstance Win32_Process` talks to WMI/CIM. It is not an owned child and must never be killed or treated as a descendant.

5. **An existing Job Object helper exists.**
   - `Tools/execution_runtime.py:70-92` creates a Job Object with `JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE` (`0x2000`), assigns a process, and terminates the job on close.
   - `run_owned` (`:95-117`) launches a Python worker, assigns it before writing the command, and the worker waits on stdin before `subprocess.call` (`:155-158`).
   - The existing synthetic test at `Tools/test-execution-runtime.py:52-64` proves a timed-out worker descendant is killed and an unrelated sentinel survives. It is helper evidence, not Add-Type evidence.
   - `WindowsJob.close` (`:89-92`) calls `TerminateJobObject` then `CloseHandle`, ignores both results and sets `handle=None`; it has no active-process accounting. It cannot support a post-close query.

## Decision

Select **(a) Job Object containment** of the dedicated collection process and its descendants for this scope. This is a scoped choice, not a claim that every narrower compiler-free alternative has been disproved. **(b)** is not selected because replacing Add-Type would require a different native/helper mechanism with its own unverified containment and dependency cost.

Containment must be established before PowerShell executes any Add-Type code.

## Admission strategy

Use a new **fixed persistence worker**, not the generic `--worker`:

1. Create a fresh Job Object with `KILL_ON_JOB_CLOSE`; no breakaway limits.
2. Launch `execution_runtime.py --persistence-worker` with hidden/no-window startup and bounded stdin/stdout/stderr pipes.
3. Assign the worker to the job. If `AssignProcessToJobObject` or `IsProcessInJob` verification fails, terminate the worker and refuse before sending a request.
4. Only after successful assignment, send one strict request:
   `{"schema": 1, "pid": <positive uint32>}`
   The worker derives the fixed PowerShell executable (`%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe`) and fixed script path locally. It accepts no executable, script, argv, shell text, launch path or arbitrary metadata. Only the PID is necessary for this read contract; no expected-launch metadata is supplied to the worker.
5. The worker reads exactly one bounded line (maximum 512 bytes), validates exact keys/types/schema/PID, rejects duplicate keys, nonfinite values, unknown fields, extra bytes, malformed JSON, oversized input, EOF and a second/replayed request, then launches the fixed command.
6. The worker waits for that request before any PowerShell/Add-Type code. Parent death before assignment leaves it waiting on stdin; EOF exits without a child. This is an assumption to verify, not a proven no-orphan guarantee.
7. Keep 64 KiB bounded drains and the existing monotonic deadline. The parent sends the request only if the deadline is still open; delayed admission is terminated/refused without a request.

The generic `--worker` and `run_owned` remain unchanged.

### Suspended launch and ownership

A suspended alternative would create the process with `CREATE_SUSPENDED`, assign the job, verify `IsProcessInJob`, then call `ResumeThread`; failure before resume terminates the process, closes the thread/process handles and does not run PowerShell. The fixed worker is selected instead because it waits for the request before spawning, so it provides assignment-before-code with the existing bootstrap pattern and fewer native handles.

Ownership invariants: the Python supervisor owns the job handle, worker process, request and result pipes; the worker owns only its stdin request and its PowerShell child; PowerShell owns only its own children. The job handle is non-inheritable, the worker closes request stdin after one line, PowerShell receives only the intended stdout/stderr pipe ends, and no unintended pipe end or job handle is inherited. CIM/WMI remains an external shared service outside this ownership. A failed assignment, authorization, cleanup query, terminate, wait, drain join or close is a refusal, never a success report.

## Job cleanup and accounting

Do not use `WindowsJob.close` for the proposed admission path. Add additive, separately tested methods while preserving `run_owned`/`close` behavior:

- `terminate_checked()`: call `TerminateJobObject`; any failure is cleanup failure; keep the handle open.
- `active_processes()`: call `QueryInformationJobObject` with `JobObjectBasicAccountingInformation` and return `ActiveProcesses`; query failure is unknown/cleanup failure.
- `close_checked()`: call `CloseHandle`; failure is reported honestly; only clear the handle on success.
- `is_process_in_job()`: verify assignment with `IsProcessInJob` before authorization.

One shared cleanup deadline bounds all cleanup work: job polling, worker wait and drain joins. There are no successive independent five-second waits. Terminate/close within that deadline; if active count is nonzero, the worker wait or drain join fails, or any close/terminate/query fails, report `cleanup_failed`/unknown. Do not retry by probing unrelated processes.

`JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE` handles abnormal supervisor death: the OS closes the job handle and terminates job members. That path cannot produce a cleanup success report; no worker code runs after parent death. If death occurs before assignment/request, the worker sees stdin EOF and must exit without spawning; handle inheritance must ensure the job handle is non-inheritable and no unintended pipe end is inherited.

Nested jobs are supported on Windows 8/Server 2012 and later; assignment failure on other environments refuses. Shared CIM/WMI services are outside the job and are never terminated.

## Compiler-child observation

Polling the job's active count can miss a short-lived `csc.exe`. Absence of a child from polling is **not proof** that none exists. The supervisor-only acceptance must use Job Object completion-port notifications (`JOB_OBJECT_MSG_NEW_PROCESS`/assignment notifications) while running a synthetic Add-Type compile, recording every new process ID and its job membership. If no compiler child is observed or its membership cannot be established, live admission remains **blocked**, not passed. The report does not demand an impossible inference from absence.

## Test placement

- **Offline mocks:** `Tools/test-persistence-process-reader.py`. It must test strict request parsing, assignment-before-authorization ordering, EOF/malformed/duplicate/replayed input, delayed admission, cleanup state transitions, and the mock-fidelity repair. Mocks do not certify native containment.
- **Supervisor-only native suite:** new `Tools/test-persistence-job-acceptance.py`, registered in `Tests/catalog.json` as one `tier=engine`, `timeout_seconds=60`, `acceptance=not_mapped` row. It is selected only through the supervisor:
  `python Tools/execute-tests.py --profile engine --suites test-persistence-job-acceptance --execute`.
  It must not be added to `Tools/test-execution-runtime.py` or ordinary offline execution.

## Proposed implementation files

Not implemented here:

- `Tools/persistence_process_reader.py` — job/bootstrap launch, strict request, fixed command, shared cleanup deadline and job accounting.
- `Tools/execution_runtime.py` — additive `--persistence-worker` mode and checked job methods; preserve generic `--worker` and `run_owned`.
- `Tools/test-persistence-process-reader.py` — offline mocks and unstarted-join fidelity fix.
- `Tools/test-persistence-job-acceptance.py` — new supervisor-only synthetic suite.
- `Tests/catalog.json` — one new engine-tier row for that suite.

No PowerShell/collector/validator/production caller change is proposed.

## Supervisor-only synthetic acceptance (proposal, unexecuted)

Use only newly created harmless test children and synthetic strings; never an editor, existing process or real command line.

1. **Forced timeout / descendant containment:** synthetic worker under a new job creates a Python grandchild that sleeps then writes a unique marker. Force timeout; verify job termination, active count zero, marker absent, worker reaped, and an unrelated synthetic sentinel survives.
2. **Inherited-pipe child:** synthetic child writes to an inherited stdout/stderr pipe and sleeps past the deadline. Verify bounded drains return, no partial result is accepted, pipes close, and the child is gone.
3. **Parent death:** synthetic supervisor creates the job and child; hard-terminate the supervisor; verify child/grandchild are gone and no orphan holds the pipe or output file. Record before-assignment EOF behavior separately.
4. **Add-Type observation:** synthetic PowerShell compiles only a harmless synthetic class while the job completion port records process creations. Verify any compiler child is in the job; if none is observed or membership cannot be established, report blocked.
5. **Negative control:** unrelated synthetic process outside the job remains alive.

All runs are supervisor-only with unique temp paths; no existing process/command-line inspection.

## Mock fidelity correction

`Tools/test-persistence-process-reader.py:384-405` has `FakeThread.join` return without error for an unstarted thread; real Python `Thread.join` raises `RuntimeError` before start. This affects `cleanup_failed` classification in thread-start-failure mocks, not proof of a real child leak. The proposed test-only repair makes unstarted `join` raise and updates the expected cleanup classification.

## Rollback

Revert the reader/job/worker test changes, remove the new engine catalog row and native suite, and return to the offline-only process reader. No production caller is added.

## Blockers and unverified facts

- No live validation exists for Add-Type compiler creation, compiler pipe inheritance, job assignment/nesting, completion-port observation or parent-death containment.
- The current direct-child runner cannot guarantee compiler-descendant cleanup.
- Real PowerShell script parsing/API/UTF-8/handle behavior remains unverified.
- Shared CIM/WMI services are outside containment and must not be killed.
- Live process-reader admission remains blocked until the supervisor-only synthetic suite establishes the job and compiler facts.
