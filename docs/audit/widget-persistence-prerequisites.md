# R04 checkpoint 6: Widget persistence prerequisites

Status: `proposed_unapproved`. This is a design investigation, not implementation permission and not a runtime result.

- **Approved base:** `633c86cd6871f69bd997eefe6bae2176306b13f1`
- **Checkpoint 5 accepted candidate:** `1c0cb15f737d619bd9777136366d141d75da8cd9`
- **Claim:** `66394016d7b64f77a5106c8b316f43bd` (`dsh-r04-persistence-01`)
- **Claim store:** `c:\ExampleDev\turbomcp\.git\turbomcp\task-claims.json`, SHA-256 `f83d96d0b67c3e34e22265ed4bdb19a9f247d46a35011a111717f1f81cbdd67a`
- **Engine:** 5.8.3 at `396c9f059903aed5fec78ecd3d437a40c6415368`
- **Evidence class:** source/offline only. No editor, engine suite, build, live MCP, packaging or recovery command was run.

## Decision

Choose one design: a **supervisor-owned two-pass persistence transaction** in the existing suite runner, plus a **test-project automation seam** that reads and sets the effective `UBlueprintEditorSettings::SaveOnCompile` value **in memory**.

This is smaller than a named-pipe/IPC service and does not clear ownership flags or call `start-editor.ps1` from a suite. The suite runs twice:

1. **phase A (mutate):** observe effective `SaveOnCompile == Never` before any mutation, make the reversible `TitleText` `IsVariable` edit, compile, save or deliberately not save, write a validated state file, and exit `75` to request a restart.
2. **supervisor restart:** only the supervisor stops its owned editor, proves the old process identity is gone, starts a new owned editor, proves the new identity and server ownership, and writes a validated response.
3. **phase B (readback):** reconnect as a new client, read the expected tree/flag from disk, and exit `0`.

The saved trial preserves package bytes from phase A through phase B. The no-save control is an otherwise identical trial with `SaveOnCompile` observed as Never and no save; phase B must show the edit was discarded. Fixture bytes are restored only between independent trials and at safe cleanup.

Current status is **no-go for automated Monday persistence acceptance**. The design is implementable only after the prerequisites below; the existing suite helper pair alone cannot produce the required restart.

## Evidence and current boundary

### Supervisor/lifecycle facts

- `Tools/execute-tests.py:83-108` creates the run directory, execution context/token, shared resource locks for repo/engine/port, snapshots the project, and places the command in a Windows Job Object through `execution_runtime.run_owned` (`:95-117`). It restores the run-start snapshot and finishes the lease in `:130-137`.
- `Tools/run-suites.ps1:643-646` sets `TURBOMCP_RUNNER_OWNS_FIXTURES=1` and publishes fixture paths. It reuses or starts one healthy editor (`:648-658`), invokes each suite (`:687`), and restores the fixture after a suite (`:700-755`).
- `Start-EditorAndWait` (`run-suites.ps1:219-319`) returns the existing healthy editor if the server answers. `Stop-Editor` (`:322-358`) stops only editor processes this runner owns, verifies owned descendants, and throws if an owned project editor remains after 30 seconds.
- `Tools/tmcp_editor.py:74-85` returns success without stopping when `TURBOMCP_RUNNER_OWNS_FIXTURES=1`; `:195-217` returns success if a server already answers. It is not a restart primitive.
- `Tools/start-editor.ps1:130-131` refuses a direct start under the runner unless the runner's own starting/stop flags are set. The suite must not set those flags.
- `Tools/ownership.ps1:316-430` establishes ownership from the editor command line; `:461-507` enumerates owned descendants and any owned editor. A process whose command line cannot be read is not owned.
- `Tools/check-mcp-server.py:93-187` checks project ownership and, with `--editor-pid`, only checks that the PID is an `UnrealEditor`/`UnrealEditor-Cmd` process. It does not compare process creation time, so a recycled PID can pass that check.
- `Tools/run-suites.ps1:126-186` preserves a suite's exit code and bounds a hung suite by killing its child tree. It has no request channel to restart the editor.
- `Tools/fixture-isolation.py` and `Restore-FixtureSnapshot` (`run-suites.ps1:395-464`) restore the `TestProject/Content/Tests` fixture. The runner snapshots and restores that boundary but does not provide a two-trial persistence mode.
- `Tools/start-editor.ps1` takes a lease before launch and records `EditorPid`; `Tools/ownership.ps1` treats a lease whose editor is gone as free and permits same-project takeover, while refusing foreign editors.

### SaveOnCompile facts

- `UBlueprintEditorSettings` is `UCLASS(config=EditorPerProjectUserSettings)` (`Engine/Source/Editor/BlueprintGraph/Public/BlueprintEditorSettings.h:89`); `SaveOnCompile` is the config property at `:242`.
- `FBlueprintCompilationManagerImpl::CompileSynchronouslyImpl` reads `GetMutableDefault<UBlueprintEditorSettings>()->SaveOnCompile` and, for `SoC_Always` or `SoC_SuccessOnly`, adds `PackagesToSave` and calls `FEditorFileUtils::PromptForCheckoutAndSave(..., bCheckDirty=true)` (`BlueprintCompilationManager.cpp:458-490`). The same setting is read at `:1560-1561` in the reinstancer path.
- The enum is `SoC_Never`, `SoC_SuccessOnly`, `SoC_Always` (`BlueprintEditorSettings.h:41-45`). The constructor migrates a legacy `bSaveOnCompile=true` to `SoC_SuccessOnly` (`BlueprintEditorSettings.cpp:100-105`); the effective value depends on the loaded editor/user configuration.
- There is **no existing Python or MCP reader** for this editor settings object. `AutomationTestToolset.RunTests` and `GetLogEntries` are existing test surfaces, used by `Tools/test-compile-ensure.py:112-173`; they do not expose the setting directly.

### Why the old plan was not runnable

`run-suites.ps1:643` makes the runner the fixture/lifecycle owner. The suite-side `stop_editor()` returns without stopping, `ensure_editor()` reuses the answering server, and a direct `start-editor.ps1` is guarded. Therefore the previously proposed step could leave the same editor alive and compare the same in-memory package. A real restart must be owned by the runner, not by the suite.

## Chosen design

### Supervisor-owned two-pass transaction

Add a mode to the existing runner rather than a new service:

- **Request:** phase A suite writes one atomic JSON state file under a supervisor-created trial directory and exits `75` (`EX_TEMPFAIL`-style local sentinel, not a product code). The state is a *request to restart*, not a pass.
- **Supervisor validation:** exact schema, supervisor token, trial name, suite name, phase, fixture path and expected tree/flag. Missing, extra, stale, replayed or mismatched fields are refused.
- **Response:** after an owned restart, supervisor atomically writes a response containing the old and new process identities and the readback phase. Phase B reads and validates both the response and the file from the new editor.
- **No IPC/service:** no named pipe, HTTP listener, broker or cloud service. The state file is local and bounded to the run directory. A test-only state file is simpler than a new service and does not bypass the existing Job Object, lease or resource locks.

The supervisor is still `execute-tests.py` + `run-suites.ps1`; the execution context and resource locks remain held across both passes.

### State schema (exact)

Phase A request:

```json
{
  "schema": 1,
  "token": "<supervisor-supplied 32-byte hex>",
  "suite": "test-widget-compile-save-reopen",
  "trial": "saved",
  "phase": "mutate_complete",
  "fixture": "/Game/Tests/WBP_NodeTest",
  "expected": {"titleTextVariable": true},
  "editorPid": 12345,
  "editorStartUtc": "2026-10-18T01:23:45.678Z"
}
```

Supervisor response:

```json
{
  "schema": 1,
  "token": "<same token>",
  "suite": "test-widget-compile-save-reopen",
  "trial": "saved",
  "phase": "readback",
  "oldEditorPid": 12345,
  "oldEditorStartUtc": "2026-10-18T01:23:45.678Z",
  "newEditorPid": 23456,
  "newEditorStartUtc": "2026-10-18T01:24:31.112Z",
  "serverOwner": true,
  "fixture": "/Game/Tests/WBP_NodeTest",
  "expected": {"titleTextVariable": true}
}
```

`phase B` may write a `readback_complete` record with the observed value for evidence. The supervisor rejects `mutate_complete`/`readback` states with the wrong token/trial/phase, any unknown field, a non-absolute state path, a state file larger than a small fixed limit, or a second request after a response.

### Process identity, PID reuse and foreign refusal

Extend the existing ownership checks rather than inventing a new service:

- Record `{pid, startTimeUtc, commandLineHash, project, engine}` for the old editor. `Get-TurboMCPProcessIdentity` belongs in `Tools/ownership.ps1`; it must use the process creation time, not only PID/name.
- After `Stop-Editor`, require:
  - no process with the old PID **and the old start time** remains;
  - `Test-TurboMCPAnyOwnedEditorRunning -Project -Engine` is false for this project;
  - the old PID is not merely reused by a different process: if the PID exists with a different start time, it is a different identity and the old identity is gone.
- Start a **fresh** editor with a new `Start-FreshOwnedEditor` path. It must not reuse an answering server. Before starting, if `check-mcp-server.py` answers with no owned identity, or with an identity for another project/engine, refuse without stopping it.
- Require the new identity to have a different `{pid, startTimeUtc}` from the old identity. The same PID with a different start time is a new process only after the old identity is proved gone; the safer default is to refuse same-PID replacement.
- Extend `Tools/check-mcp-server.py` with an optional `--editor-start-utc` (or equivalent identity parameter) and verify it against the process creation time. `--editor-pid` alone is insufficient because it checks only the process name (`:114-132`).
- Phase B establishes a **new `Session`** through `Tools/tmcp.py` and requires the new server to answer; the supervisor records `serverOwner=true` only after `check-mcp-server.py` passes with the new identity and project/engine.
- Foreign listeners/processes are never stopped. A healthy server without a proved owned identity is a refusal, not a reusable editor. Lease conflicts preserve the foreign editor and block the trial.

### Deadlines, interruption and cleanup

- Phase A suite deadline: 300 s; stop deadline: 30 s; start deadline: 240 s; phase B deadline: 300 s; total trial deadline bounded by the existing `execute-tests.py --max-seconds` (default 1800).
- A timed-out child is killed only through its own process tree (`Invoke-SuiteProcess`); an owned editor is stopped only through `Stop-Editor`; no name-based global kill.
- `try/finally` cleanup stops the owned editor if needed, verifies the config file is unchanged, restores the fixture at a safe point, preserves the state/log, and marks the trial blocked/failed. A supervisor process death is handled by the existing `execute-tests.py` recovery snapshot/restore; no automatic replay of a save or readback.
- The execution lease and resource locks remain held for the whole two-pass trial. A restart must reuse the same `TURBOMCP_OWNER_ID` and must not release the lease between passes. `start-editor.ps1`'s same-project lease takeover is used only by the supervisor.
- Do not restore fixture bytes between phase A and phase B. A failure before readback restores at safe cleanup and reports **not saved/not persisted**, never a pass.

### Save and discard semantics

- **Saved trial:** baseline tree shows `TitleText` unmarked; phase A applies `EditTurboMCPWidgetsAsVars(TitleText, bIsVariable=true)`, confirms a `(widget TitleText ...)` line with `:variable` and the `ReadTurboMCPBlueprintLoad` flag, compiles, saves explicitly, exits `75`; phase B re-reads and requires the flag. The saved `.uasset` bytes remain untouched until readback.
- **No-save control:** restore the baseline before the trial; apply the same edit; compile with effective `SaveOnCompile=Never` observed; do **not** save; exit `75`; after the same restart, require `TitleText` not marked. This proves memory-only state was discarded.
- **Unrelated unsaved assets:** before any stop, query `ReadTurboMCPDirtyList`; only the owned `WBP_NodeTest` package may be dirty. If any unrelated package is dirty, refuse/block rather than stop and discard it. Restore only the owned fixture at cleanup.
- File hashes are supplementary. The primary evidence is the expected edited `TitleText` state after a verified new editor read the package from disk. A dirty package with unchanged serialized content does not need a different hash.

### Effective SaveOnCompile observation

No existing Python/MCP API reads the setting. Add a **test-project-only** C++ fixture and automation tests; this is the tiny missing seam, not a new production tool:

- Fixture functions:
  - `ReadEffectiveSaveOnCompile()` returns `effective=<Never|SuccessOnly|Always>; config=<absolute path>; configHash=<sha256>; exists=<0|1>`.
  - `SetSaveOnCompileNeverForTest()` reads `GetDefault<UBlueprintEditorSettings>()->SaveOnCompile`, sets the mutable default to `SoC_Never` **in memory only**, then re-reads and returns `before=...; effective=Never; config=<path>; configHash=<sha256>`. It must not call `SaveConfig`/`TryUpdateDefaultConfigFile`.
  - `RestoreSaveOnCompileForTest()` restores the prior in-memory enum for failure cleanup in the same editor. A restart naturally reloads the user's unchanged config.
- Automation tests:
  - `TurboMCP.T06.SaveOnCompileRead` logs the effective value, exact config path, existence and hash.
  - `TurboMCP.T06.SaveOnCompileSetNever` logs `before=...`, `after=Never`, `effective=Never`, `config=<path>` and `configHash=<same hash>` from the live `GetDefault<UBlueprintEditorSettings>()`.
- The persistence suite invokes these through the existing `AutomationTestToolset.RunTests`/`GetLogEntries` pattern (`test-compile-ensure.py:112-173`), not a new Python object API.
- Per trial, before any `EditTurboMCPWidgetsAsVars` call, the suite requires a fresh log line `effective=Never`. Setting the object alone is not accepted: the same test must read the in-editor default after the assignment. If the setting is already Never, no assignment is needed; still require the live read.
- **The chosen design never writes the user's config file.** The supervisor hashes the exact config path before phase A and again after phase B and requires equality; a change by any third party blocks the trial and is preserved. This keeps user configuration recoverable and restored by construction, rather than writing and then repairing it.
- The config path is discovered, never assumed to be a project-only path; base/default and user layers are whatever `GetDefaultConfigFilename()` reports. The effective value comes from `GetDefault<UBlueprintEditorSettings>()`, the same object the compilation manager reads.

## Implementation subtasks (dependency order, all proposed_unapproved)

### P1 — Test-project SaveOnCompile observation seam

- **Exact files:** `TestProject/Source/TurboMCPTest/TurboMCPTestPersistenceSettings.h` (new), `.cpp` (new), `TestProject/Source/TurboMCPTest/Private/TurboMCPTestPersistenceSettingsTest.cpp` (new), `TestProject/Source/TurboMCPTest/TurboMCPTest.Build.cs`.
- **Build scope:** add `BlueprintGraph` as a private dependency for `UBlueprintEditorSettings`; no plugin dependency and no production source.
- **Preserved:** no normal editor/tool behavior; setting is changed only in memory during the owned persistence trial; the user config file is never written and its hash must remain unchanged.
- **Offline checks:** source-level review that the seam is test-only; no engine execution in this task. Build verification is deferred to a supervisor-only live prerequisite.
- **Negative controls:** read reports the actual enum/path/hash; set reports `effective=Never` only after a live re-read; absence of a readable config path blocks rather than guessing; the config hash is unchanged before/after.
- **Rollback:** remove the two new files/test and revert `TurboMCPTest.Build.cs` only.

### P2 — Process identity and foreign-listener proof

- **Exact files:** `Tools/ownership.ps1`; `Tools/check-mcp-server.py`; `Tools/test-ownership.py`.
- **Functions:** `Get-TurboMCPProcessIdentity` (`pid`, `startTimeUtc`, command-line/project/engine); `Test-TurboMCPProcessIdentity`; `--editor-start-utc` validation in the server probe. Reuse `Test-TurboMCPOwnsProcess`.
- **Preserved:** current foreign-editor refusal, lease behavior, owned-descendant cleanup, and `--editor-pid` compatibility.
- **Offline fault injection:** mock process records for same PID/different start time, old PID alive, foreign project, unreadable command line, and server answering without an owned identity.
- **Negative controls:** a current owned PID/start time still passes; a foreign editor still refuses; no process is killed.
- **Rollback:** revert the two code files and test additions only.

### P3 — Two-pass persistence supervisor and state validation

- **Exact files:** `Tools/persistence_supervisor.ps1` (new pure module); `Tools/run-suites.ps1`; `Tools/test-persistence-supervisor.py` (new offline suite).
- **Functions:** `Invoke-PersistenceSuite`, `Start-FreshOwnedEditor`, `Stop-OwnedEditorForRestart`, `Read/Write/Test-PersistenceState`, `Assert-PersistenceConfigUnchanged`.
- **Protocol:** validate the schema/token/trial/suite/phase/expected fields; only the configured persistence suite may exit `75`; reject phase A exit `0`; write a response only after a proved restart.
- **Preserved:** normal `run-suites` suites, one-editor reuse, existing timeouts, fixture snapshot/restore and verdicts. No normal suite enters the new path.
- **Offline fault injection:** malformed/missing/extra fields, wrong token/trial/phase, duplicate/replayed request, phase A exit `0`, old process alive, same PID reused, foreign listener, phase/stop/start timeout, interrupted supervisor, fixture bytes preserved between passes, config path/hash unchanged (external change blocks), unrelated dirty package.
- **Negative controls:** normal suite still passes; no-save control is not reported saved; a refused restart leaves the foreign editor and state preserved.
- **Rollback:** remove the new helper/test and revert `run-suites.ps1` only.

### P4 — Supervised persistence profile and catalog entry

- **Exact files:** `Tools/execute-tests.py`; `Tests/catalog.json`; `Tools/run-suites.ps1`.
- **Change:** add an explicit `persistence` profile that accepts exactly one catalogued persistence suite and invokes `run-suites.ps1 -PersistenceSuite <name>`; add the new suite to the catalog. Existing profiles and suite selection remain unchanged; no `all`, arbitrary command or hidden suite.
- **Preserved:** request schema validation, resource locks, execution token/context, Job Object, snapshot/restore and report format.
- **Offline fault injection:** duplicate suite names, zero/multiple suites, uncatalogued suite, profile name spoofing, missing persistence mode argument.
- **Negative controls:** `profile=engine` still requires catalogued engine suites; `offline` still runs `Tools/validate.py`; an ordinary suite is not run through persistence mode.
- **Rollback:** revert `execute-tests.py`, the catalog entry and the `run-suites` switch only.

### P5 — Two-trial Widget Blueprint persistence suite

- **Exact files:** `Tools/test-widget-compile-save-reopen.py` (new); `Tests/catalog.json`.
- **Inputs:** owned `/Game/Tests/WBP_NodeTest`, `TitleText`; existing `EditTurboMCPWidgetsAsVars`, `EditTurboMCPCompile`, `EditTurboMCPSaveWithoutThumbnail` (or `EditTurboMCPSave` when allowed), `ReadTurboMCPWidgetTree`, `ReadTurboMCPBlueprintLoad`, `ReadTurboMCPAssetDescribe`, `ReadTurboMCPDirtyList`, `AutomationTestToolset.RunTests`/`GetLogEntries`.
- **Behavior:** phase A observes effective `SaveOnCompile=Never`, applies the reversible edit, compiles, saves or (control) does not save, validates state and exits `75`; phase B reconnects after the new editor, re-reads the expected flag, writes `readback_complete`, exits `0`. Saved trial expects `true`; no-save control expects `false`.
- **Preserved:** existing tools, fixture names, save and widget-tree semantics; no production authoring feature.
- **Offline fault injection:** state/token/trial errors, missing setting log, wrong expected flag, no-save control accidentally saved, unrelated dirty package, missing phase B.
- **Negative controls:** no-save control must fail if it persists the edit; saved trial must fail if the flag was not observed after restart; a failed compile/save must not be called persistence.
- **Rollback:** remove the new test and catalog entry only.

### P6 — Supervisor-only live acceptance (no new source)

- **Scope:** no code; integration owner runs the P1-P5 build/test path under the existing supervisor.
- **Command:** `python Tools/execute-tests.py --profile persistence --suites test-widget-compile-save-reopen --execute` (proposed; unavailable until P4/P5).
- **Acceptance:** saved trial retains `TitleText` variable after new PID/new session; no-save control loses it; old PID/start time gone; new PID/start time owned; server owner/project/engine match; config path/hash/existence unchanged; fixture restored only after readback; no unrelated dirty package touched; exact report/request/tree digest recorded.
- **Manual fallback:** a supervised operator may perform the same two phases manually through the runner owner, supplying old-PID-exit, new-PID/server-owner and new-session read evidence. It is not an automated gate and must preserve unrelated editors/unsaved state. If the supervisor cannot restart, report persistence **blocked**, never passed.
- **Rollback:** none; this is evidence/acceptance only.

## Offline fault-injection matrix

| Case | Expected result |
|---|---|
| State missing, malformed, extra field, wrong schema/token/trial/suite/phase | Refuse before write/restart; no editor action; preserve state for review |
| Phase A exits 0 or non-75 | Fail the persistence suite; no restart/readback claim |
| Duplicate/replayed state after response | Refuse; do not replay a save or restart |
| Old PID alive with same start time | Refuse restart; stop only through owned path if still owned |
| Old PID reused by different start time | Treat old identity gone, but require explicit new identity proof; never accept the recycled PID as old |
| New PID same as old, new start absent, foreign project/engine, unreadable command line | Refuse; preserve foreign process |
| Server answers with no owned identity | Refuse; do not measure it |
| Phase A/B/stop/start timeout | Kill only child tree/owned editor; restore at safe cleanup; mark blocked |
| Interrupted supervisor after save before readback | Preserve evidence; restore baseline at recovery; no persistence claim |
| Fixture restore before phase B | Test must fail: saved bytes are not to be restored until readback |
| Config file absent/present/changed externally | File is never written; hash/existence must match before/after. An external change blocks and is preserved |
| Unrelated dirty package before stop | Refuse/block; do not discard unrelated unsaved work |
| No-save control shows `TitleText` marked | Fail the control (it persisted); not a pass |
| Normal suite through normal path | Unchanged one-editor reuse/verdict behavior |

## Supervisor-only live acceptance

Only the integration owner may run engine work. The sequence is:

1. Build the test module dependency changes and the plugin under the existing supervisor; confirm provenance.
2. Run P2/P3/P4 offline fault suites; all must pass and must not start an editor.
3. Run the exact candidate with `persistence` profile. The report must name the candidate commit, tree digest, suite name, old/new identities, config path/hash, fixture bytes between phases, saved/no-save results and final restore.
4. Reject a run that cannot prove a new editor process (same PID/start time), a new client session, effective `SaveOnCompile=Never` before mutation, or the no-save control's lost edit.
5. Do not mark Monday persistence accepted from an offline-only or manual-only run.

## Validation

- `python Tools/audit-inventory.py --check` — exit 0; 101 definitions; 95 `parsed_call`, 6 `name_reference_only`.
- `python Tools/test-result-contract.py` — exit 0; PASS, C03-C08.
- `python Tools/test-client-retry.py` — exit 0; PASS, C01/C02.
- `python Tools/execute-tests.py --profile offline --execute` — exit 0 on the pre-commit report tree; report `.local/executions/f79179ea0da34139a0db2a46ef85097b/report.json`; request commit `633c86cd6871f69bd997eefe6bae2176306b13f1`; tree digest `410c0bb04e794336050729e47af33d9134071340b574378e430323aab9b07c2e`.
- No engine/editor/build/MCP/recovery command was run. The proposed live acceptance is unexecuted.

## Missing seams and blockers

- No existing Python/MCP API exposes `UBlueprintEditorSettings::SaveOnCompile`; P1 is the tiny test-project seam.
- No existing supervisor API restarts its owned editor; P3 is the bounded two-pass mode. `tmcp_editor.stop_editor`/`ensure_editor` remain unusable under the runner and are not changed.
- `check-mcp-server.py --editor-pid` does not verify process creation time; P2 adds identity proof for PID reuse.
- No existing `persistence` profile/catalog tier; P4 adds it.
- No existing `Tools/test-widget-compile-save-reopen.py`; P5 adds it.
- No existing full-suite run proves the save/no-save behavior. If the `BlueprintGraph` test dependency, automation fixture, supervisor mode or observed setting cannot be built/reviewed, the automated plan is **blocked**. Do not substitute a direct `start-editor` call, clear `TURBOMCP_RUNNER_OWNS_FIXTURES`, or treat setting intent as effective observation.

## Monday go/no-go

**No-go for automated Monday persistence acceptance now.** The current supervisor cannot restart under the suite runner and there is no effective `SaveOnCompile` observation. The smallest viable path is P1-P5, followed by the supervisor-only P6 acceptance.

**Conditional go after P1-P6:** one saved trial and one no-save control on the owned `TitleText`/`WBP_NodeTest` fixture, with new-PID/start-time proof, new-session disk read, effective `Never` before mutation, config path/hash unchanged, unrelated assets untouched, and exact candidate-bound evidence. This is a narrow compile/save/reopen check, not general persistence certification.

**Fallback:** a supervised operator may run the same two phases manually through the runner owner. It must supply the same process, server-owner, config, fixture and readback evidence; it is not an automated gate and remains **unverified** until that evidence exists. No direct helper workaround is approved.

## Rollback summary

Each implementation subtask is independently revertible by its listed files. P1 has no production effect. P2/P3/P4 preserve the normal suite path and can be removed without changing existing tools. P5 is a new test only. If any step discovers a foreign editor, an unreadable lease/lock, a missing config path, an unprovable process identity, or an unrelated dirty package, stop and report the blocker; do not kill, clear, overwrite or replay.
