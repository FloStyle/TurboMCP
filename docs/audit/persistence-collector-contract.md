# R04 checkpoint 8: trusted identity collection contract

Status: `proposed_unapproved`. This is a source investigation and implementation contract, not collector code, a live ownership result or persistence acceptance.

- **Approved base:** `577fdaafed023485adb4e1d3f65c4d6218c5eef3`
- **Claim:** `8dfd55914dce466ea7d0a6de64edc3a6` (`dsh-r04-collector-01`)
- **Engine:** 5.8.3 at `396c9f059903aed5fec78ecd3d437a40c6415368`
- **Evidence class:** source/docs review only. No live process command line, socket table, editor, engine suite, build, MCP call or recovery command was read or run.

## Decision

Use a small, read-only Windows collector that supplies the accepted `turbomcp.persistence-identity/1` validator with two closely spaced snapshots. The collector must not infer identity from a server response, a process name or a port probe.

The first implementation slice is **pure orchestration with an injected backend**; it contains no Windows adapter. A future, separately reviewed adapter performs operating-system reads and enforces deadlines. This keeps the first slice offline-testable and prevents a fake backend from standing in for an unproven live collector.

The contract is:

1. The supervisor records the **expected** identity from the launched process object, not from a later PID lookup.
2. The adapter reads process, path and TCP observations within explicit deadlines and returns typed results or a refusal.
3. Pure orchestration validates field shapes, binds expected and observed identities, detects process/listener changes, builds the two observations and calls the validator.
4. Any timeout, unknown completion, partial result, permission failure, exit/PID reuse, listener replacement or identity ambiguity is a refusal.
5. A validation pass is evidence consistency only. It never authorizes a connection, mutation, stop, restart or persistence claim.

No existing caller is changed here. The launch-record fields and Windows adapter remain separate, supervisor-owned prerequisites.

## Current source boundary

### Validator contract

`Tools/persistence_identity.py` fixes the schema:

- Exact expected `pid`, `start_time_utc`, `project_path`, `engine_executable` and endpoint.
- Exactly two observations with `sequence`, `collected_at_utc`, `collection_ok`, `process` and `listeners`.
- Listener rows contain family, address, port, state, PID, creation time, project and engine path.
- Literal `127.0.0.1` or `::1` endpoints only; exact or same-family wildcard matching.
- Ordered snapshots, bounded age/gap, no creation time after collection and no identity/ownership change between snapshots.
- ASCII-only path comparison; the validator does not prove filesystem identity.

Relevant source: `Tools/persistence_identity.py:90-143` (schema/reasons/limits), `:306-390` (snapshot/listener assembly), `:390-490` (matching and validation).

### Existing process, lease and runner code

- `Tools/ownership.ps1:261-314` recognizes `-Project=`, quoted `-Project`, quoted positional and unquoted positional `.uproject`; it returns the first match rather than rejecting multiple candidates.
- `Tools/ownership.ps1:316-432` checks process existence, reads `Win32_Process.CommandLine`/`ExecutablePath` through `Get-CimInstance`, normalizes separators and compares `GetFullPath` strings. An unreadable command line is not owned.
- `Tools/ownership.ps1:43-258` owns lease persistence/conflict handling; `start-editor.ps1:360-370` takes the lease before launch.
- `Tools/start-editor.ps1:157-160` canonicalizes `$Project` and builds `$EditorExe`; `:388` uses `Get-NetTCPConnection` only as a busy test; `:529-556` calls `Start-Process -PassThru`, records only `EditorPid`, and writes no creation time or executable path to the lease.
- `Tools/run-suites.ps1:303-308` filters fresh editors by `Get-Process.StartTime`; `:650-652` reuses an editor by PID; `:668` passes `--editor-pid`.
- `Tools/check-mcp-server.py:114-132` with `--editor-pid` checks only that the PID names an editor process; `:149-158` reads `Win32_Process.CommandLine`; `:93-187` is not a listener-owner check.

None of these is a complete collector. `start-editor.ps1` does not bind the expected creation time to the launched process object, and no current code binds the listening socket to the expected process.

## Trusted launch record

`Start-Process -PassThru` in `start-editor.ps1:534-535` returns a `System.Diagnostics.Process`. The launch sequence must be:

1. Start the process and retain the returned `Process` object.
2. Verify the object's `Id` equals the PID that will be recorded, and `HasExited` is false.
3. Retain its process handle (`Process.Handle` or the underlying safe handle) and call `GetProcessTimes` on **that handle** immediately. Do not look up the expected creation time by PID later.
4. Recheck `HasExited` and the handle's creation time. If either check fails, refuse admission.
5. Capture the executable path and file ID, canonical project path and file ID, and literal endpoint from the launch configuration.
6. Write the complete record to the lease before any observation or client call.

Proposed launch record:

```text
LaunchRecordVersion = 1
LaunchId             = <unique supervisor run id>
EditorPid            = <process id>
EditorStartTicks     = <FILETIME 100 ns ticks since 1601>
EditorExecutablePath = <canonical absolute path>
EngineFileId         = <volume serial + 64-bit file index + link count>
ProjectPath          = <canonical absolute .uproject path>
ProjectFileId        = <volume serial + 64-bit file index + link count>
McpHost              = "127.0.0.1" or "::1"
McpPort              = <configured port>
```

The existing lease has only `EditorPid` and project/engine/port (`ownership.ps1:192-238`; `start-editor.ps1:552-556`). It is insufficient. A reused editor whose lease lacks `LaunchRecordVersion`, creation ticks or file IDs must **refuse admission**; it must not be upgraded by copying values from an observation. `run-suites.ps1` reuse by PID (`:650-652`) is therefore not sufficient for this contract.

If launch metadata cannot be captured after `Start-Process` returned, admission is refused. That is not proof that no process was launched: cleanup is a separate supervisor responsibility. The report does not implement that cleanup or a lifecycle change.

## Layer contract

### Pure orchestration (first slice)

`collect_pair(expected_record, endpoint, backend, policy)` performs only:

- schema/type checks of the expected record, endpoint and policy;
- deadline calculation from `backend.monotonic_ns()` and `backend.utc_now()`;
- calls to the backend with an absolute monotonic deadline and a per-call limit;
- project-parse result handling (the adapter returns the parsed project or a refusal);
- canonical path/file-ID result handling;
- process creation <= collection checks;
- listener-row matching and owner identity assembly;
- process/listener recheck comparison and two-snapshot assembly;
- final call to `persistence_identity.validate_evidence`;
- returning a typed refusal or the validated payload.

It performs no process, TCP, filesystem, PowerShell or engine call.

### Public record and result contract

`ExpectedRecord` has exactly these keys:

```text
schema: "turbomcp.persistence-expected/1"   # string, exact
launch_id: str                              # non-empty, bounded
pid: int                                    # positive, not bool
editor_start_ticks: int                     # FILETIME ticks, positive, not bool
project: {path: str, file_id: FileId}
engine:  {path: str, file_id: FileId}
endpoint: {host: "127.0.0.1"|"::1", port: 1..65535}

FileId:
  volume_serial: int
  file_index_high: int
  file_index_low: int
  link_count: int              # >= 1; > 1 is a path refusal
```

`link_count` zero/missing is invalid evidence, not a pass. All FileId fields are integers and never bool; unreadable/missing values are a refusal.

`collect_pair(expected_record, endpoint, backend, policy)` also takes a separate `endpoint` argument. It must be byte-for-byte identical in host and integer-identical in port to `expected_record.endpoint`; otherwise `endpoint_mismatch` is returned before any backend call. This duplication is intentional: the launch record and supervisor configuration must agree.

`Policy` has exactly these keys and defaults:

```text
per_call_ns: int            default 2_000_000_000, range 1..5_000_000_000
attempt_ns: int             default 5_000_000_000, range 1..10_000_000_000
pair_ns: int                default 15_000_000_000, range 1..30_000_000_000
snapshot_gap_ns: int        default 5_000_000_000, range 0..10_000_000_000
age_ns: int                 default 10_000_000_000, range 1..30_000_000_000
max_listener_rows: int      default 32, range 1..32
```

All policy values are positive integers (except `snapshot_gap_ns`, which may be zero) and never bool. `pair_ns` must be at least `attempt_ns`; otherwise `invalid_policy`.

Public result:

```text
success:
  {ok: true, reason: "consistent_evidence", payload: <schema-v1 object>}

refusal:
  {ok: false, reason: <public_code>, category: one of
     clock|deadline|backend|identity|schema|validator}
```

Public codes include `invalid_expected`, `invalid_policy`, `endpoint_mismatch`, `backend_timeout`, `backend_permission`, `backend_provider_error`, `backend_incomplete`, `backend_ambiguous`, `backend_malformed`, `clock_rollback`, `deadline_exceeded`, `process_mismatch`, `path_mismatch`, `listener_missing`, `listener_foreign`, `listener_multiple_owners`, `observation_changed` and `validator_refused`. Unknown fields, wrong types and bool-as-int in the expected record/policy are refusals; they are not ignored, coerced or raised.

The backend `ReadResult` is internal. It is not returned to the public caller and its raw error text, command line or provider dump is never placed in the public result. A malformed backend result, including a missing value, unknown error, non-bool `ok` or wrong field type, becomes `backend_malformed`; it never propagates an exception or partial data. `parse_project` fake tests prove only the orchestration's handling of injected parser outcomes, not the correctness of `CommandLineToArgvW`.

### Injected backend contract

The backend is a protocol with typed results. Every method receives an absolute monotonic deadline and must return `ReadResult`; it must not return partial data as success.

```text
monotonic_ns() -> int          # integer nanoseconds, arbitrary epoch
utc_now() -> canonical UTC string

read_process(pid, deadline_ns) -> ReadResult[ProcessRead]
read_listeners(port, deadline_ns) -> ReadResult[ListenerReadList]
read_path(path, deadline_ns) -> ReadResult[PathRead]
parse_project(command_line, deadline_ns) -> ReadResult[ProjectRead]
```

`monotonic_ns` and `utc_now` are injected, immediate and nonblocking. The monotonic value is an integer nanosecond counter with an arbitrary epoch (it may be large and nonzero). It is never compared with FILETIME ticks or UTC values. All duration and deadline arithmetic uses `monotonic_ns`; all calendar comparisons use canonical UTC converted by integer arithmetic.

```text
ProcessRead:
  pid: int
  start_ticks: int             # FILETIME 100 ns since 1601
  executable_path: str
  command_line: str            # never logged

ListenerRead:
  family: "ipv4" | "ipv6"
  address: str
  port: int
  state: "listen"
  owner_pid: int

PathRead:
  path: str
  exists: bool
  has_reparse_component: bool
  has_short_name_component: bool
  long_path: str
  volume_serial: int
  file_index_high: int
  file_index_low: int
  link_count: int

ProjectRead:
  project_path: str
  token_count: int

ReadResult[T]:
  ok: true, value: T
  ok: false, error: timeout | permission | not_found | provider_error |
                    incomplete | ambiguous | unknown
```

A clean empty listener list is `ok=true, value=[]`; it is not a provider error. The validator then refuses with `listener_missing`. A provider failure is `ok=false, provider_error`; the orchestrator must not substitute an empty list. Timeout or unknown completion is `ok=false`.

`PathRead` is malformed unless `has_reparse_component`/`has_short_name_component` are bool, `link_count` is an integer greater than zero, and the file-ID fields are integers. `link_count > 1` is a path refusal; `link_count == 0`, missing flags or unreadable values are `backend_malformed`/`path_mismatch`, never silently passed. File-ID components are unsigned 32-bit integers, including zero. Missing, negative, out-of-range or bool components refuse; equality compares the full tuple.

### Future Windows adapter (separate, not implemented)

A separately reviewed adapter implements the four methods. Exact proposed file: `Tools/persistence_collector_windows.py`. It is **not** in the first slice and must not be inferred from fake-backend tests.

The adapter:

- opens the expected process handle and calls `GetProcessTimes`;
- reads `Win32_Process` command line/executable through a bounded child PowerShell process;
- parses the command line with `CommandLineToArgvW`;
- reads all listener rows with `Get-NetTCPConnection`;
- opens each path read-only for `GetFileInformationByHandle`;
- walks path components for reparse/8.3 checks;
- runs each bounded operation in its own child process and enforces an absolute deadline.

It returns `ReadResult` only. It never calls a stop/restart, never connects to the port, and never terminates the editor.

## Clock domains and deadline contract

A stopwatch checked only after a call returns does not bound a hung CIM or TCP call. The deadline must be enforced by the layer that owns the blocking operation.

There are two distinct clock domains:

| Use | Field/source | Unit/epoch | Arithmetic |
| --- | --- | --- | --- |
| Budgets and durations | `attempt_start_monotonic_ns`, `pair_start_monotonic_ns`, `backend.monotonic_ns()` | Integer nanoseconds, arbitrary monotonic epoch | Addition/subtraction only |
| Calendar evidence | `attempt_start_utc`, `collected_at_utc`, `evaluated_at_utc` | Canonical UTC, 100 ns ticks since Unix epoch | Integer tick conversion only |

`monotonic_ns` is never compared with FILETIME or UTC and never formatted as a date. `utc_now()` is immediate and nonblocking; it is not used for deadlines. The monotonic epoch is arbitrary and may be nonzero; tests must use a large nonzero epoch.

For each attempt:

```text
attempt_start_monotonic_ns = backend.monotonic_ns()
attempt_start_utc = backend.utc_now()
attempt_deadline_ns = attempt_start_monotonic_ns + policy.attempt_ns
pair_deadline_ns = pair_start_monotonic_ns + policy.pair_ns
```

Before each call, `now_ns = backend.monotonic_ns()`. If `now_ns >= attempt_deadline_ns` or `now_ns >= pair_deadline_ns`, refuse `deadline_exceeded` without starting the call. A call deadline is `min(attempt_deadline_ns, pair_deadline_ns, now_ns + policy.per_call_ns)`. Equality at a deadline is refusal, not a grant. The adapter enforces that absolute deadline in its child; the orchestrator also rejects a returned call whose end `monotonic_ns` is at or beyond the deadline.

All duration checks use monotonic ns. `attempt_elapsed_ns = end_ns - attempt_start_monotonic_ns`; `pair_elapsed_ns = pair_end_ns - pair_start_monotonic_ns`. Equality with `attempt_ns`/`pair_ns` is refusal. Calendar age/gap checks use canonical UTC ticks: equality at `snapshot_gap_ns` or `age_ns` is allowed, consistent with validator v1; a later snapshot before an earlier one is `observation_order`. An observed UTC value earlier than `attempt_start_utc`, or a monotonic value lower than its previous value, is `clock_rollback`.

On timeout the adapter terminates only its own collection child/tree; it never terminates the editor or another process. A child that exits without a complete structured result is `unknown`/`timeout`, never success. No partial row is retained.

There is no implicit sleep between snapshots. Attempts are started back-to-back after their reads; if the backend is slow, the budget refuses rather than sleeping. The adapter must not add a delay to "wait for" a listener or process.

The adapter's child PowerShell must use `-NoProfile -NonInteractive`, `$ErrorActionPreference='Stop'`, and `-ErrorAction Stop` on each cmdlet. It must clear/inspect `$Error`; non-terminating errors are not covered by a `try/catch` alone. See [about_CommonParameters](https://learn.microsoft.com/en-us/powershell/module/microsoft.powershell.core/about/about_commonparameters?view=powershell-5.1). The child must write one structured result file only at the end, after the error count and command exit checks are clean; the parent discards stdout/partial files on nonzero exit, timeout or unknown completion.

`Get-NetTCPConnection` has no documented atomic-snapshot guarantee. A clean command exit is evidence that the child did not report an error, not proof that every relevant row existed at one instant. Empty is accepted as a successful empty list; provider failure and incomplete/unknown data are distinct refusals. The collector cannot prove completeness beyond the API's documented behavior and must retain that limitation.

## Process and command-line binding

For every process read:

- `GetProcessTimes` must be called on a handle opened with `PROCESS_QUERY_LIMITED_INFORMATION`; close the handle after the call.
- `Win32_Process.CommandLine`/`ExecutablePath` must be readable. An unreadable record is a refusal, not a foreign/owned verdict.
- The observed PID must equal the expected PID; creation ticks must equal the expected ticks; executable/file ID must match; parsed project/file ID must match.
- Re-read the expected process after the listener reads and before accepting the snapshot. Exit or changed creation/executable/project is a refusal.
- The adapter must also compare the listener owner's process identity. A listener whose owner cannot be read is a refusal; it is never dropped.

Command-line parsing uses `CommandLineToArgvW` in the adapter. Exactly one project argument is required. Multiple `-Project` options, a positional/`-Project` disagreement, extra `.uproject` tokens, unterminated quoting or parser failure are refusals. The existing `Get-TurboMCPCommandLineProject` is diagnostic only; its first-match behavior is not authoritative.

## Path identity and aliases

Every relevant supplied path component must be checked, not just the leaf:

- walk from the drive root through every component of the project and engine paths;
- `GetFileAttributes` each component; any `ReparsePoint` is refused (junctions and symlinks are not silently resolved);
- `GetLongPathName` must equal the supplied long form without an 8.3/short-name substitution;
- open each file read-only and obtain `BY_HANDLE_FILE_INFORMATION`.

`GetFileInformationByHandle` supplies a volume serial and 64-bit file index, but a file ID is shared by **hardlinks**. It does not reject hardlink aliases; equal file IDs mean the paths resolve to the same file, not that the names are distinct. The collector therefore refuses `nNumberOfLinks > 1` for the project and engine files. This is conservative and can reject a legitimate multiply-linked file.

Aliases are **rejected**, not resolved: any reparse component, short-name substitution, or hardlink-count ambiguity is a refusal. Two ordinary paths may still compare case-insensitively on default Windows directories; if the strings differ only by ASCII case, file-ID equality is required. A case-sensitive directory can contain distinct files whose names differ by case; differing file IDs are then a refusal. Every component/file check has a race between inspection and the later open; an identity that cannot be established is refused.

File IDs and the open-by-path executable do **not** prove that the running process mapped that exact image. The engine executable can be replaced or renamed after mapping. For the v1 contract, this remains a documented limit; a stronger loaded-image check would require a separate process/mapping investigation.

The validator's ASCII-only path comparison remains only a consistency check. Schema v1 does not carry file IDs. If auditable file identity is required in the evidence itself, the exact proposed adjustment is `turbomcp.persistence-identity/2` with `project_file_id` and `engine_file_id` objects containing `volume_serial`, `file_index_high` and `file_index_low`. This is not implemented.

Microsoft references: [hard links and junctions](https://learn.microsoft.com/en-us/windows/win32/fileio/hard-links-and-junctions), [GetFileInformationByHandle](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-getfileinformationbyhandle), [GetProcessTimes](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-getprocesstimes).

## Time precision

`GetProcessTimes` returns `FILETIME` values whose representation is 100 ns intervals since 1601. That is a representation unit, not a guarantee that the underlying clock/timer has 100 ns physical resolution. The collector must:

- convert the 64-bit FILETIME count to UTC with integer arithmetic;
- format exactly seven fractional digits from the reported value;
- never pad a coarser value, infer missing digits, or use a CIM `CreationDate` as a substitute;
- refuse if the source cannot expose the FILETIME.

The expected launch record uses the same handle-bound source. A source that only gives milliseconds cannot populate v1 truthfully; that is an admission refusal, not a rounded record.

## Collection algorithm and bounds

Proposed pure orchestration:

```text
collect_pair(expected_record, endpoint, backend, policy)
```

Before the attempts, capture `pair_start_monotonic_ns = backend.monotonic_ns()`, `pair_start_utc = backend.utc_now()` and `pair_deadline_ns = pair_start_monotonic_ns + policy.pair_ns`. After the second attempt capture `pair_end_monotonic_ns` and `pair_end_utc`.

For each of two attempts:

1. Capture `attempt_start_monotonic_ns = backend.monotonic_ns()` and `attempt_start_utc = backend.utc_now()`; compute `attempt_deadline_ns` and `pair_deadline_ns`.
2. Read and validate the expected process within the remaining per-call deadline.
3. Read/parse the observed command line; require exactly one project; canonicalize and compare file identity.
4. Read all listeners for the configured port; require a complete `ReadResult`, not a partial list.
5. For every distinct owner PID, read its process identity. In the conservative first slice, any owner identity that does not equal the expected identity is refused immediately as `listener_foreign`; no foreign row is represented as acceptable evidence.
6. Re-read the expected process including PID, creation ticks, executable, project parse and project/engine file IDs; re-read the listener set. Any change fails the attempt.
7. Set `collected_at_utc = backend.utc_now()` at completion. Reject if its UTC tick is before `attempt_start_utc`, before the process creation ticks, or if the monotonic attempt duration is at or beyond `attempt_ns`.
8. Emit `collection_ok=true` only when every read and recheck succeeded.

After both attempts:

- reject pair monotonic duration at or beyond `pair_ns`;
- reject `pair_end_utc < pair_start_utc` as `clock_rollback`;
- allow equal UTC timestamps; reject only `second.utc < first.utc` as `observation_order`;
- require `0 <= (second.utc - first.utc) * 100 <= snapshot_gap_ns`; equality at the bound is allowed;
- set `evaluated_at_utc` after the pair; require nonnegative age for BOTH snapshots and `age_ticks * 100 <= age_ns`; equality is allowed;
- emit one schema-v1 payload;
- any timeout, unknown completion, partial output, permission failure, process exit/PID reuse, listener replacement, overflow or ambiguity is a refusal.

Recheck comparison is exact:

- process recheck compares normalized `(pid, creation_ticks, executable_path, file_id, parsed_project_path, project_file_id)`; a raw PID match alone is insufficient;
- listener rows are compared as a normalized multiset, not a list: sort or count the tuple `(family, address, port, state, owner_pid, owner_start_ticks, owner_executable, owner_file_id, owner_project, owner_project_file_id)` while preserving duplicates. Reordering identical rows is equal; a duplicate-count change fails; a PID match with changed owner creation/path identity fails;
- a changed path/file ID at final recheck fails.

The pure orchestrator uses the same `validate_evidence` function after building the payload. A validator pass is not permission.

## TCP mapping

`Get-NetTCPConnection` provides `LocalAddress`, `LocalPort`, `State` and `OwningProcess`. Map only:

- `127.0.0.1` -> IPv4 exact;
- `0.0.0.0` -> IPv4 wildcard;
- `::1` -> IPv6 exact;
- `::` -> IPv6 wildcard.

Any other address, including IPv4-mapped IPv6, is refused. Keep every row for the port, including multiple rows owned by one process. Do not filter to the expected PID: a foreign/multiple-owner row must be visible to the validator. More than 32 rows exceeds v1 and is refused. A partial row set is never accepted.

The existing `start-editor.ps1:388` call is a busy test, not a complete listener inventory.

## Schema v1 mapping and gaps

| v1 field | Collector source |
| --- | --- |
| `expected.*` | Trusted launch record; endpoint from supervisor config |
| `bounds.*` | Orchestrator policy; evaluated after both attempts |
| `observations[].sequence` | Attempt 1 or 2 |
| `observations[].collected_at_utc` | Attempt completion time |
| `observations[].collection_ok` | True only after all reads/rechecks |
| `observations[].process.*` | Expected/observed PID, FILETIME ticks, executable/project |
| `observations[].listeners[].family/address/port/state` | Complete listener records |
| `observations[].listeners[].pid/start_time_utc/project_path/engine_executable` | Owner process reads |

v1 cannot carry auditable file IDs or a precision qualifier. The collector can use file IDs and link counts as an internal admission gate, but the validator sees paths. A v2 file-ID adjustment is required if that identity must be part of the evidence.

## Residual race and authorization

A pass means the supplied snapshots agree. It does not prove the PID/listener still exist, that the executable image is unchanged, or that a future connection reaches the expected process. PID reuse, listener replacement, file replacement and path/alias races remain after the final read.

Before connecting, mutating, stopping or restarting, a future caller must re-read the expected process identity and listener owner rows, refuse on any change, and treat validation as evidence only. A `consistent_evidence` result must never become an authorization flag.

## Minimal proposed first slice

All `proposed_unapproved`. Exact files:

- `Tools/persistence_collector.py` (new) — pure `collect_pair` orchestration and typed records; no Windows adapter.
- `Tools/test-persistence-collector.py` (new) — offline fake-backend tests.
- `Tests/catalog.json` — exactly one offline row: `id=test-persistence-collector`, `path=Tools/test-persistence-collector.py`, `tier=offline`, `timeout_seconds=30`, `acceptance=not_mapped`.

No `start-editor.ps1`, lease, runner, lifecycle, restart, settings or persistence change is in this slice. The future Windows adapter (`Tools/persistence_collector_windows.py`) is separately reviewed and unimplemented.

### Offline cases

- Valid IPv4 exact and IPv6 exact; wildcard and same-owner multiple rows.
- Intended editor alive with a foreign listener; multiple owners; missing listener; wrong port. Foreign owner rows are refused immediately.
- PID reuse/different creation ticks; process exit between reads; changed executable/project/file ID.
- Final-recheck changes in PID, creation, executable, parsed project or path/file ID.
- Reordered identical listener rows pass; duplicate-count change fails.
- Listener replacement between reads; >32 rows; partial/error enumeration.
- Successful empty enumeration versus provider failure versus timeout/unknown completion.
- Quoted/`-Project=`/positional project; multiple/ambiguous tokens; parser failure (injected outcomes only, not parser correctness).
- Reparse component, short/8.3 alias, hardlink count >1, case-sensitive directory/file-ID mismatch.
- Missing/zero link counts, missing flags, bad booleans and malformed path records.
- Creation time after collection, one-tick-boundary and coarser-source refusal.
- Nonzero monotonic epoch with a much larger/smaller UTC value; a bug that mixes domains must fail.
- Equal UTC timestamps allowed with zero gap; attempt/pair deadline equality refused; monotonic and UTC rollback refused.
- Pair timeout, per-call timeout, aggregate budget exhaustion, no implicit sleep.
- Unknown expected/policy fields, bool-as-int, endpoint duplication mismatch and malformed backend results.
- Clean success becomes validator `consistent_evidence`; every refusal returns no payload.
- Backend error taxonomy preserved; no partial success; no side effect.

Success acceptance is a typed schema-v1 payload accepted by `persistence_identity.validate_evidence`; negative acceptance is a typed refusal with no payload. Neither grants connection or mutation permission.

## Privacy, logs and rollback

- The local run payload carries paths/PIDs/endpoint as required; it is not committed.
- Logs carry reason codes, PID, creation ticks, endpoint and redacted basenames; never raw command lines, environment, credentials or full CIM output.
- Provider errors are categories, not raw dumps.
- Rollback removes `Tools/persistence_collector.py`, `Tools/test-persistence-collector.py`, the one catalog row and this contract evidence. No existing caller is affected.

## Live acceptance and blockers

Live acceptance is supervisor-owned and **unexecuted**. It requires the proposed launch-record fields and a reviewed Windows adapter. It must prove handle-bound creation time, PID-reuse refusal, complete listener ownership, file-ID/path gates and pair bounds without connecting, mutating, stopping or restarting. No live editor, process, socket, build or engine command was executed for this checkpoint.

Open blockers: launch-record fields are absent; the Windows adapter is unimplemented; v1 lacks auditable file IDs; path/alias and loaded-image identity cannot be fully established; CIM/TCP completeness is not atomic; and all remaining work is `proposed_unapproved`.


## Integration decision

Accepted as a design investigation. Checkpoint 9 in roadmap-r04 authorizes only
the pure offline slice and supplies binding file-ID and time-conversion
clarifications. All Windows adapter, launch-record and lifecycle work remains
proposed and unapproved. Offline tests cannot certify live ownership.
