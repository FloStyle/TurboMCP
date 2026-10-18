# R04 checkpoint 8: trusted identity collection contract

Status: `proposed_unapproved`. This is a source investigation and implementation contract, not collector code, a live ownership result or persistence acceptance.

- **Approved base:** `577fdaafed023485adb4e1d3f65c4d6218c5eef3`
- **Claim:** `8dfd55914dce466ea7d0a6de64edc3a6` (`dsh-r04-collector-01`)
- **Engine:** 5.8.3 at `396c9f059903aed5fec78ecd3d437a40c6415368`
- **Evidence class:** source review only. No process command line, socket table, editor, engine suite, build, MCP call or recovery command was read or run.

## Decision

Use a small, read-only Windows collector that produces the already-reviewed `turbomcp.persistence-identity/1` evidence for two closely spaced snapshots. It must read process and TCP state from the operating system; it must not infer identity from a server response, a process name or a port probe alone.

The collection contract is:

1. The supervisor records the **expected** PID, creation time, executable path, canonical project path, engine executable path and literal MCP endpoint at launch.
2. Each collection attempt reads the expected process, all TCP listeners on the configured port, and the identity of every listener owner.
3. Each attempt re-reads the expected process and the listener set before accepting the snapshot.
4. Two bounded attempts are paired and handed to `persistence_identity.validate_evidence` by the supervisor.
5. A validation pass is evidence consistency only. It does not authorize a connection, mutation, stop, restart or persistence claim.

No existing caller is changed by this checkpoint. The launch-record field additions and live acceptance remain separate, supervisor-owned prerequisites.

## Evidence and current boundary

### Validator contract

`Tools/persistence_identity.py` fixes the input shape:

- Exact expected `pid`, `start_time_utc`, `project_path`, `engine_executable` and `endpoint`.
- Exactly two observations with `sequence`, `collected_at_utc`, `collection_ok`, `process` and `listeners`.
- Listener rows contain family, address, port, state, PID, creation time, project and engine path.
- The endpoint is literal `127.0.0.1` or `::1`; matching uses the exact endpoint or the same-family wildcard.
- Both observations must be ordered, within the caller's age/gap bounds, not later than collection, and consistent with one another.
- The validator uses ASCII-only path folding. It does not prove filesystem identity.

Relevant source: `Tools/persistence_identity.py:90-143` (schema/reasons/limits), `:306-390` (snapshot and listener assembly), `:390-490` (matching and validation).

### Existing process and ownership code

- `Tools/ownership.ps1:261-314` parses `-Project=`, quoted `-Project`, a quoted positional `.uproject`, and an unquoted positional `.uproject`. It returns the first match and does not reject multiple candidates; that is not sufficient for a trusted collector.
- `Tools/ownership.ps1:316-432` checks the process exists, reads `Win32_Process.CommandLine` and `ExecutablePath` through `Get-CimInstance`, normalizes `/` to `\`, uses `GetFullPath`, and compares engine/project paths. A process with an unreadable command line is not owned.
- `Tools/ownership.ps1:461-507` walks descendants by `Win32_Process.ParentProcessId`; it is cleanup logic, not listener evidence.
- `Tools/ownership.ps1:43-258` owns lease persistence and conflict handling; `start-editor.ps1:360-370` takes the lease before launch.
- `Tools/start-editor.ps1:157-160` canonicalizes `$Project` and builds `$EditorExe`; `:388` uses `Get-NetTCPConnection` only as a busy/not-busy test; `:529-556` starts the editor and records only `EditorPid` in the lease. It does not record creation time or executable path.
- `Tools/run-suites.ps1:303-308` filters fresh editors by `Get-Process.StartTime`; `:650-652` reuses a healthy editor by PID; `:668` passes `--editor-pid` to the probe.
- `Tools/check-mcp-server.py:114-132` with `--editor-pid` checks only that the PID is an `UnrealEditor`/`UnrealEditor-Cmd` process. `:149-158` reads `Win32_Process.CommandLine`; `:93-187` is not a listener-owner check.

These helpers are useful evidence sources, but none is a complete collector. In particular, none records a trusted creation time at launch and none binds the listening socket to the expected process.

## Data sources

| Evidence field | Source | Precision / failure behavior |
| --- | --- | --- |
| Expected PID | Supervisor launch record / lease `EditorPid` | Positive integer. Missing or changed: refuse. |
| Expected creation time | `GetProcessTimes` `lpCreationTime` on the launched PID | `FILETIME` ticks of 100 ns; convert the 1601-epoch 64-bit count to UTC with integer arithmetic, then format exactly 7 fractional digits. Access denied or process gone: refuse. Do not use CIM `CreationDate` or a padded millisecond value. |
| Observed creation time | Same `GetProcessTimes` call for the PID | 100 ns `FILETIME`; compare exact ticks to expected and across snapshots. |
| Expected executable | Launch record built from `$EditorExe`, canonicalized | Absolute Windows path. Missing/changed: refuse. |
| Observed executable | `Win32_Process.ExecutablePath`; fall back only to a readable `Process.Path` | Normalize `/` to `\`, `GetFullPath`, trim trailing `\`. If unreadable: refuse. |
| Project identity | Expected project from launch record; observed project parsed from `Win32_Process.CommandLine` | Parse one project token; canonicalize; compare to expected. Unparseable/multiple/ambiguous: refuse. |
| Command line | `Get-CimInstance Win32_Process -Filter "ProcessId=<pid>"` `.CommandLine` | Raw Windows text. Do not log it. Unreadable: refuse. |
| TCP rows | `Get-NetTCPConnection -State Listen -LocalPort <port>` | All rows for the port. Include IPv4/IPv6 exact and wildcard. Any exception or incomplete result: refuse. |
| Listener owner | Each row's `OwningProcess`; then the same process reads | Owner 0/4, missing process, unreadable command line/executable or unreadable creation time: refuse. |
| Collection time | `DateTimeOffset.UtcNow` at attempt completion; `Stopwatch` for duration | Clock rollback or duration over budget: refuse. |
| Endpoint | Supervisor configuration, literal host and port | `127.0.0.1` or `::1` only; hostnames/remote hosts refused before collection. |

The creation-time source is deliberately `GetProcessTimes`, not `Get-CimInstance Win32_Process.CreationDate`. CIM is acceptable for command line and executable discovery only; its serialized date precision is not relied on for the 100 ns schema field. If a future implementation cannot expose `FILETIME` at 100 ns, it must refuse rather than round or pad.

## Trusted launch record

`start-editor.ps1` already starts the process and records `EditorPid` (`:534-556`); the lease already carries owner, project, engine and MCP port (`ownership.ps1:192-238`). The supervisor must extend that record **before any observation or client call** with:

```text
LaunchRecordVersion = 1
EditorPid            = <process id>
EditorStartTimeUtc   = <FILETIME, canonical 7-digit UTC>
EditorExecutablePath = <canonical absolute path>
ProjectPath          = <canonical absolute .uproject path>
EngineExecutablePath = <canonical absolute UnrealEditor.exe path>
McpHost              = "127.0.0.1" or "::1"
McpPort              = <configured port>
```

The expected identity is written from the launch call, not copied from a later process or TCP observation. If any field cannot be captured, the launch is refused; a later observation must not fill it in. A reused editor takes the identity from the lease that already names it, after `Test-TurboMCPOwnsProcess` revalidates project/engine.

This record does not prove file identity. See the path limits below.

## Command-line parsing and path identity

The collector must not use a substring or first-match rule. Use `CommandLineToArgvW` (Windows shell quoting rules) on the raw command line, or refuse if that parser is unavailable. Accept:

- `-Project=<path>`;
- `-Project <path>`;
- one positional `.uproject` argument.

Require exactly one recognized project argument. If `-Project` occurs more than once, a positional project and a `-Project` disagree, or any other `.uproject` token is present, refuse. Quoted paths with spaces are one argument. A path containing a quote that the Windows parser cannot resolve is refused. The existing `Get-TurboMCPCommandLineProject` may be used only as a diagnostic cross-check; its first-match behavior is not the collector's authority.

Canonicalize with `GetFullPath` and separator normalization, then compare using the same ASCII-only path key as the validator. `GetFullPath` handles relative segments, `.`/`..` and separator forms; it does **not** resolve symlinks, junctions, 8.3 aliases, hardlinks, case-sensitive directories or case-only differences that Windows may treat specially.

Therefore:

- A string match is **path consistency**, not proof that both paths name the same file.
- Before accepting a path, read `GetFileAttributes`; a `ReparsePoint` on the project or engine path is refused. Compare `GetLongPathName` with the supplied path (ASCII-case-insensitive); a short/8.3 alias or an unexpected long-path difference is refused.
- Do not silently treat a case-insensitive match in a case-sensitive directory as the same file.
- Do not claim volume/file identity from a path string.

If file identity is required, schema v1 is insufficient. The exact proposed adjustment is a future `turbomcp.persistence-identity/2` with `project_file_id` and `engine_file_id` objects, each containing `volume_serial`, `file_index_high` and `file_index_low` from `GetFileInformationByHandle` (or `GetFinalPathNameByHandle` plus file-ID comparison). The validator would compare those IDs, not strings. This is not implemented here.

## Collection algorithm and bounds

Proposed function:

```text
Collect-PersistencePair(expectedRecord, endpoint, maxAttemptSeconds, maxPairSeconds)
```

For each of two attempts:

1. Record `attemptStartUtc` and a monotonic stopwatch.
2. Read the expected PID with `GetProcessTimes` and `Win32_Process`; reject if missing, access-denied, reused, or changed from `expectedRecord`.
3. Parse the observed command line to one project; canonicalize; require expected project; canonicalize executable and require expected executable.
4. Read all `Get-NetTCPConnection -State Listen -LocalPort <port>` rows. Reject exceptions, rows with owner 0/4, rows outside the supported address families, or more than 32 rows.
5. For every distinct owner PID, read the same process identity. A listener owned by a foreign or unreadable process must be represented honestly or the attempt must fail; it must not be silently dropped.
6. Re-read the expected process creation time/executable/project and re-read the listener rows. Any change, disappearance or new row fails the attempt.
7. Set `collected_at_utc` to the completion time. Reject if it is before the process start, before the attempt start, or if the attempt exceeded `maxAttemptSeconds` (proposed 5 seconds).
8. Emit one observation with `collection_ok=true` only when all reads and both rechecks succeeded.

After both attempts:

- Reject if the pair exceeded `maxPairSeconds` (proposed 15 seconds), if `collected_at` is out of order, or if the gap exceeds the proposed 5 seconds.
- Set `evaluated_at_utc` after the pair and use proposed `max_age_seconds=10`.
- Emit one schema-v1 payload; do not emit a partial observation with guessed fields.
- Any exception, permission denial, process exit, PID reuse, listener replacement, overflow or unreadable row is a collection refusal, not a pass.

The existing `Get-NetTCPConnection` call in `start-editor.ps1:388` is only a busy check. The collector must enumerate the complete set for the port, not ask whether the port is merely occupied.

## TCP mapping

`Get-NetTCPConnection` provides `LocalAddress`, `LocalPort`, `State` and `OwningProcess`. Map:

- `127.0.0.1` -> `family=ipv4`, `address=127.0.0.1`;
- `0.0.0.0` -> `family=ipv4`, `address=0.0.0.0`;
- `::1` -> `family=ipv6`, `address=::1`;
- `::` -> `family=ipv6`, `address=::`;
- any other address, including IPv4-mapped IPv6 forms: refuse, unless a later reviewed mapping is added.

Keep every row for the configured port, including duplicate/multiple rows owned by the same process. The validator decides whether multiple rows are one owner or multiple owners. Do not pre-filter to the expected PID: a foreign listener must be visible so it can be refused, and the schema has a `foreign_listener` and `multiple_listener_owners` outcome.

More than 32 rows exceeds the schema limit and is refused. A partial row set is never enough to claim completeness.

## Schema v1 mapping and gaps

| v1 field | Collector source |
| --- | --- |
| `expected.*` | Trusted launch record; endpoint from supervisor config |
| `bounds.*` | Collector/supervisor policy; evaluated after both attempts |
| `observations[].sequence` | Attempt 1 or 2 |
| `observations[].collected_at_utc` | Attempt completion time, canonical UTC |
| `observations[].collection_ok` | True only after all reads and rechecks |
| `observations[].process.*` | Expected PID; FILETIME creation; executable/project from CIM + parser |
| `observations[].listeners[].family/address/port/state` | `Get-NetTCPConnection` rows |
| `observations[].listeners[].pid/start_time_utc/project_path/engine_executable` | Owner process reads |

v1 can represent the **path-based** evidence above. It cannot represent:

- file identity across aliases/junctions/hardlinks/case-sensitive directories; or
- a truthful 100 ns creation time if the chosen source only has coarser precision.

For the second gap, the contract is to refuse rather than fabricate. For the first, the exact v2 file-ID fields above are the proposed adjustment. Neither change is implemented in checkpoint 8.

## Residual race and authorization

A passing validation means the supplied snapshots agree. It does **not** prove that the PID still exists, that the listener still belongs to it, or that a future connection will reach it. The process can exit or the PID can be reused after the final read; the listener can be replaced after the final TCP read. The collector and validator cannot close that race.

Any future caller must, immediately before connecting, mutating, stopping or restarting:

1. re-read the expected PID/creation time/executable/project;
2. re-read the listener owner rows;
3. refuse on any change; and
4. treat the validation result as evidence, never permission.

A refusal is the safe outcome. A `consistent_evidence` result must not be converted into an authorization flag.

## Minimal proposed implementation slice

**Proposed files only; all `proposed_unapproved`:**

- `Tools/persistence_collector.py` (new) — `collect_pair(expected_record, endpoint, backend, bounds)` orchestration. It accepts an injected backend protocol and imports `persistence_identity` only to validate the completed payload/refusal.
- `Tools/test-persistence-collector.py` (new) — offline tests using only injected records; no process, socket, filesystem or engine calls.
- `Tests/catalog.json` — exactly one offline row for `test-persistence-collector`, bounded timeout, `acceptance: not_mapped`.

Backend protocol:

```text
utc_now() -> canonical UTC
process(pid) -> {pid, start_time_utc, executable_path, command_line}
listeners(port) -> [{family, address, port, state, pid}]
```

The Windows backend is a bounded read-only adapter over `GetProcessTimes`, `Get-CimInstance Win32_Process`, `CommandLineToArgvW` and `Get-NetTCPConnection`. Offline tests substitute a fake backend and do not invoke the Windows adapter.

This slice deliberately does **not** include `start-editor.ps1`, lease fields, runner integration, restart, settings or persistence work. The trusted launch record is a prerequisite that must be reviewed before live collection; until then, only injected tests are meaningful. No caller is wired.

### Offline cases

- Two valid snapshots with one IPv4 exact listener.
- IPv6 exact, IPv4/IPv6 wildcard and same-owner multiple rows.
- Intended editor alive while another process owns the listening port.
- Multiple listener owners; foreign owner; missing listener; wrong port.
- PID reuse/different creation time; process exit between reads; changed executable/project/engine.
- Listener replacement between the first and final read.
- Permission denial/partial process or TCP enumeration; owner PID 0/4; unreadable command line.
- More than 32 listener rows.
- Quoted project path, `-Project=` and positional forms; multiple/ambiguous project tokens.
- Reparse/short/alias path and case-sensitive-directory ambiguity refusal.
- Future or coarser creation time; no fabricated 100 ns digits.
- Pair timeout, out-of-order observations, stale pair and clock rollback.
- Validator acceptance for a complete pair; no payload on any refusal.

Success acceptance is a schema-v1 payload that `persistence_identity.validate_evidence` accepts and whose two observations came from the same expected identity. Negative acceptance is an explicit refusal with no partial payload and no side effect. Neither acceptance grants permission to connect or mutate.

## Privacy, logs and rollback

- The evidence file contains the paths and PIDs required by the schema and stays in the local run directory; it is not committed.
- Logs contain reason codes, PID, creation time, endpoint and redacted path basenames. They must not contain raw command lines, environment variables, credentials or full CIM output.
- Errors from `Get-CimInstance`/`Get-NetTCPConnection` are summarized as failure categories, not dumped.
- Rollback removes `Tools/persistence_collector.py`, `Tools/test-persistence-collector.py`, the one catalog row and this contract's implementation evidence. No existing caller is affected.

## Live acceptance and blockers

Live acceptance is supervisor-owned and **unexecuted**. A future supervisor must first add and review the launch-record fields, then run the collector twice through the existing ownership/lease path and pass the resulting pair to the validator. The live run must demonstrate:

- old/new creation time and PID, including PID reuse refusal;
- listener ownership by the expected process, not a same-named process or a port probe;
- complete relevant IPv4/IPv6 rows;
- identical evidence across the bounded pair;
- no connection, mutation, stop or restart on a failed/refused collection.

No live editor, socket, process, build or engine command was executed for this checkpoint. The collector is not implemented; the launch-record seam is not implemented; and the path/file-identity and 100 ns precision limits above remain open. All implementation steps remain `proposed_unapproved`.
