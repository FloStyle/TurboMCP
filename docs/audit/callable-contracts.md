# R04 callable contracts — checkpoints 1–5

Source/offline investigation only. Checkpoint 1 audits four call-log tools; checkpoint 2 adds status/help/schema; checkpoint 3 adds source-control state and checkout; checkpoint 4 adds the two save tools; checkpoint 5 adds the compile tool. None completes R04.

- **Checkpoint 1 base:** `1180c6d43655195dc2772b6d1f0b80588e4b466a`
- **Checkpoint 1 claim:** `ab2cf6535c584bc6a9419c79e85d0b04` (`dsh-r04-01`)
- **Checkpoint 2 base:** `b353fdd8b44f405c84ef9a67fde9db679f8c4804`
- **Checkpoint 2 claim:** `350b288bbc2d4f5ab0e408d760bf2dfb` (`dsh-r04-02`)
- **Checkpoint 3 base:** `fc46e01845c4fbd6814c07f5f228d946aeaef6b9`
- **Checkpoint 3 claim:** `c44cc6b13e824f5682e53c43bee3cd5d` (`dsh-r04-vcs-01`)
- **Checkpoint 4 base:** `15ab395ac88eaa52ea31d7628d032446c90fddb6`
- **Checkpoint 4 claim:** `613fbd8768e04378859aad03a5a26bb6` (`dsh-r04-save-01`)
- **Checkpoint 5 base:** `a92cae75abd5c4a8e65179fa8b1ad32f5aef436f`
- **Checkpoint 5 claim:** `88f668a66d8740acae6454d27e162c28` (`dsh-r04-compile-01`)
- **Plugin tree:** `590661ec45c829264ea1743fccea994edb748092`
- **Engine:** 5.8.3 at `396c9f059903aed5fec78ecd3d437a40c6415368`
- **Machine-readable inventory:** [callable-contracts.json](callable-contracts.json)

## Scope and coverage

`docs/audit/capabilities.json` contains **101 unique tool IDs**. Checkpoint 1 created the four-row baseline; checkpoint 2 added three; checkpoint 3 added three VCS rows; checkpoint 4 added two save rows; checkpoint 5 adds `EditTurboMCPCompile`.

| Audit status | Count |
| --- | ---: |
| `reviewed_source` | 13 |
| `not_reviewed` | 88 |
| **Total** | **101** |

Checkpoint 1 snapshot: 4 reviewed/97 not reviewed. Checkpoint 2: 7/94. Checkpoint 3: 10/91. Checkpoint 4: 12/89. Checkpoint 5 adds `EditTurboMCPCompile`. The 88 unreviewed rows retain their capability source/test references and are explicitly marked. No conclusion is implied for them.

Reviewed tools: `ReadTurboMCPCallCost`, `ReadTurboMCPCallLog`, `EditTurboMCPCallLog`, `EditTurboMCPCallLogClear`, `ReadTurboMCPStatus`, `ReadTurboMCPToolHelp`, `ReadTurboMCPToolSchema`, `ReadTurboMCPVcsState`, `PreviewTurboMCPVcsCheckout`, `EditTurboMCPVcsCheckout`, `EditTurboMCPSave`, `EditTurboMCPSaveWithoutThumbnail`, `EditTurboMCPCompile`.

## Reviewed contracts

| Tool | Declaration / schema | Wrapper and implementation | Defaults and parsing | Effects and result |
| --- | --- | --- | --- | --- |
| `ReadTurboMCPCallCost` | `TurboMCPToolsets.h:464-468`; `AICallable` | `FRecordingToolset` at `TurboMCPCallRecorder.cpp:103-130`; implementation `TurboMCPCallCostTools.cpp:80-437`; timeline `TurboMCPCallTimeline.cpp:49-142` | All parameters are required `FString` fields; no C++ defaults. Explicit empty `Cap` = 10, max 50; empty `Since`/`Filter` means no bound/all. Omitted fields are rejected by the engine schema before the body. | Reads `FTurboMCPCallLog::GetEntries()` in memory, merges records into calls, ranks tokens/duration separately, totals by tool/asset. When logging is enabled and the recorder is installed, its own start record is included. No ARC02 result line. |
| `ReadTurboMCPCallLog` | `TurboMCPToolsets.h:425-429`; `AICallable` | Same recorder; implementation `TurboMCPCallLogTools.cpp:34-200` | All required `FString`; no C++ defaults. Explicit empty `Limit` = 50, max 500; empty `Filter`/`Since` means all/no bound. Omitted fields are rejected before the body. | Reads in-memory `Entries`; `GetLogFilePath` only constructs a path and this tool does not read the file. Shows raw records newest-first, groups sessions, displays up to 200 chars of args/result. Its own start is visible to its filter only when logging/recorder are on. No ARC02 line. |
| `EditTurboMCPCallLog` | `TurboMCPToolsets.h:1982-1983`; `AICallable` | Same recorder; implementation `TurboMCPCallLogTools.cpp:202-247` | Required `Enabled`; accepts `true/on/yes/1` and `false/off/no/0` (trimmed, case-insensitive); anything else, including empty, refused. Omitted is rejected by the schema. | Writes `bEnableCallLog` and calls `SaveConfig`; this is a persistence attempt, not verified restart/disk persistence. Returns changed/unchanged text. No ARC02 line. |
| `EditTurboMCPCallLogClear` | `TurboMCPToolsets.h:1992-1993`; `AICallable` | Same recorder; implementation `TurboMCPCallLogTools.cpp:249-266` | Required `Confirm`; accepts `yes` or `true` (trimmed, case-insensitive); anything else, including empty, refused. Omitted is rejected by the schema. | Body clears only the in-memory `Entries` array; it does not open/write the file. The wrapper can append records and trigger rotation when logging is ON, so the complete invocation is not byte-stable. No ARC02 line. |

### Recording boundary

`FRecordingToolset` creates `FScopedCall` before forwarding (`TurboMCPCallRecorder.cpp:103-130`). The constructor writes a start only if `Write` sees logging enabled (`TurboMCPCallLog.cpp:254-270`, gate `:216-221`). `Finish` writes an end through the same gate (`:272-305`). The start/end pair is therefore conditional on logging at entry and exit, not merely on recorder installation.

`Finish` stores the first **4000 result characters plus a truncation suffix** (`TurboMCPCallLog.cpp:287-298`), not at most 4000 total. `ResultChars` records the full measured length; the token estimate is over the full result. In-memory entries are capped at 1000; the file rotates at 4 MiB with one `.1` file.

### Domain refusal versus invocation error

`FFunctionLibraryToolset::ExecuteToolInternal` returns `MakeValue(JsonToString(...))` for a normal reflected return and `MakeError` only for unknown tool/toolset, script exception, parameter-conversion failure or invalid result (`FunctionLibraryToolset.cpp:134-197`). `ObjectFunctionToolCall` converts a normal `FString` return through `PropertyToJsonData`, wraps it as `returnValue`, and rejects required-param failures before invocation (`ObjectFunctionToolCall.cpp:105-162,390-413,461-521`).

The recorder sets `bError = Result.HasError()` (`TurboMCPCallRecorder.cpp:126-128`). A tool body that returns `error: ...` for invalid arguments is therefore a **successful value** to the recorder: no `ERROR` row, not counted in the cost report's failed count, and not matched by the error-status branch of `filter=error`. Other OR branches can still match tool/source/argument text containing `error`. An inner invocation failure that reaches the recorder produces `bError=true` only if logging remains enabled; transport failures and refusals before wrapper entry do not guarantee a record. See `R04-CALL-06`.

### Omitted versus empty parameters

The four C++ signatures have no defaults. `FJsonSchemaGenerator` marks non-optional function parameters without defaults as required (`JsonSchemaGenerator.cpp:288-299`), and `ObjectFunctionToolCall` rejects missing required fields and fills declared defaults before invoking (`ObjectFunctionToolCall.cpp:105-162,555-559`). An omitted field is rejected before the body; an explicitly supplied empty string reaches the body and selects the in-code default or refusal. This is source-traced, not runtime-tested. It is not correct to say a C++ `FString` signature alone proves omitted and empty are equivalent.

### Empty and populated logs

- `ReadTurboMCPCallLog` validates `Limit` before reading entries; empty logs still get a limit refusal for `banana` or `0`.
- `ReadTurboMCPCallCost` returns the empty-log report before parsing `Cap`; with no entries, `cap=banana` or `cap=0` returns the empty report instead of the cap error.
- With logging OFF, ordinary calls write no start/end. With logging ON and the recorder installed, a normal MCP read is preceded by its own start record, so the body does not see a literally empty log. ON-and-empty is a direct/synthetic state, not an ordinary MCP sequence.
- The reader's self-match occurs only when logging and the recorder are enabled. Clear-with-logging-OFF is the setup in the existing file-bytes assertion.

## Source findings

| ID | Status | Scope | Finding and exact source evidence | Existing assertions |
| --- | --- | --- | --- | --- |
| `R04-CALL-01` | confirmed source | cost, log | `IsNumeric` accepts one dot and a sign; `Atoi` converts the integer prefix. `1.5` becomes 1; overflow is unchecked. `+1` is a valid positive-integer spelling and remains a compatibility choice, not inherently malformed. Evidence: `CString.h:148-175,1219-1222`; `TurboMCPCallLogTools.cpp:53-67`; `TurboMCPCallCostTools.cpp:126-144`. | `test-call-log.py:400-408` and `test-call-cost.py:344-353` test only `banana` and `0`; no fraction, signed-positive or overflow case. |
| `R04-CALL-02` | confirmed source | cost, log | Cost returns the empty-log report before cap validation (`TurboMCPCallCostTools.cpp:104-116` vs `:126-145`); log validates limit first (`TurboMCPCallLogTools.cpp:53-69`). With no entries, an invalid cap is accepted by one reader and refused by the other. | Invalid caps are tested only with populated logs. |
| `R04-CALL-03` | confirmed source | log | When `Total > Selected`, the reader says `since=<newest shown>` pages forward (`TurboMCPCallLogTools.cpp:185-189`), but selection is newest-first (`:104-110`) and `since` excludes `Timestamp <= SinceText` (`:79`). The omitted rows are older and cannot be fetched with that value. | `test-call-log.py:262-280` proves `since` excludes older rows, not the truncation continuation. |
| `R04-CALL-04` | contract gap | all four | The four tools return plain `FString` and do not append an ARC02 result line; the only production `AppendTo` caller is `TurboMCPGraphRepointTools.cpp:27`. `Tools/tmcp.py:422-431` treats every `TurboMCP.*` toolset as migrated, so `call_structured` without `requires_result=False` raises. Migration policy is unresolved. | `test-result-contract.py` covers GraphRepoint/native only; call-log suites use `Session.call`. |
| `R04-CALL-05` | confirmed source | edit | ON→OFF writes a start and suppresses the end, leaving a start-only record rendered as running/missing-end. Sources: `TurboMCPCallLog.cpp:216-234,254-305`; `TurboMCPCallLogTools.cpp:202-247`. OFF→ON writes an end-only record; unchanged ON writes a pair; unchanged OFF writes nothing. | `test-call-log.py` tests transitions but not the resulting record shape. |
| `R04-CALL-06` | confirmed source | all four | Ordinary domain refusals (`error:` strings returned by the tool body) are reflected as successful values, so `bError=false`; their recorded plugin end has no ERROR flag and contributes no failed count. Other text filter branches can still match. Inner invocation errors are recorded with `bError=true` only when they reach the wrapper and logging remains enabled. Evidence: `FunctionLibraryToolset.cpp:134-197`; `ObjectFunctionToolCall.cpp:105-162,390-521`; `TurboMCPCallRecorder.cpp:126-128`; `TurboMCPCallLog.cpp:272-305`; `TurboMCPCallCostTools.cpp:237-240`; `TurboMCPCallLogTools.cpp:95`. | Bad-boolean text is asserted; the record's `bError`, failed count and invocation-error control are not. |

`R04-CALL-04` is a contract gap, not a reproduced runtime failure. `R04-CALL-06` has no production fix in this checkpoint: parsing prose is forbidden and a structured outcome is required.

## Bounded repair proposals

Proposals only. They do not authorize implementation, schema change, pagination or a new public result format.

| ID | Finding / owner | Exact allowed files | Preserved behavior | Probe | Expected / negative control | Validation / rollback |
| --- | --- | --- | --- | --- | --- | --- |
| `R04-REPAIR-01` | `R04-CALL-01`, owner R04 | `Plugins/TurboMCP/Source/TurboMCP/TurboMCPCallLogTools.cpp`; `Plugins/TurboMCP/Source/TurboMCP/TurboMCPCallCostTools.cpp`; `Tools/test-call-log.py`; `Tools/test-call-cost.py` | Empty defaults; positive values; `+1` remains accepted unless a separate policy says otherwise; 500/50 maxima; existing error prefixes; totals over all selected calls | Populated log; `1.5`, `+1`, `.5`, `1e3`, large decimal, valid `1` | Fractional truncation and CRT-dependent overflow removed; `+1` is a compatibility control, not presumed invalid. Negative: valid `1` works. | Engine `test-call-log,test-call-cost`; revert the two source and two test files. |
| `R04-REPAIR-02` | `R04-CALL-02`, owner R04 | `Plugins/TurboMCP/Source/TurboMCP/TurboMCPCallCostTools.cpp`; `Tools/test-call-cost.py` | Valid empty-log wording; OFF vs ON-and-empty; cap 10/50; filter/since counts | No entries; `cap=banana`, `0`, `1`; then ON-and-empty | Invalid cap refused before emptiness; valid cap still reports emptiness. Negative: `cap=1` returns the empty report. | `test-call-cost`; revert the cost file and test. |
| `R04-REPAIR-03` | `R04-CALL-03`, owner R04 | `Plugins/TurboMCP/Source/TurboMCP/TurboMCPCallLogTools.cpp`; `Tools/test-call-log.py` | Newest-first; pre-cut denominator; `since` excludes `<=`; hard max 500; 200-char display | >Limit distinct timestamps; limit 1; inspect continuation | Truthfully say these are the newest N of M and older rows are not returned; no false `since` promise; do not advise exceeding 500/50. No pagination/schema change. Negative: an older `since` still returns rows. | `test-call-log`; revert the log tool and test. |
| `R04-REPAIR-04` | `R04-CALL-05`, owner R04 | `Plugins/TurboMCP/Source/TurboMCP/TurboMCPCallLog.cpp`; `Plugins/TurboMCP/Source/TurboMCP/TurboMCPCallLogTools.cpp`; `Tools/test-call-log.py` | Persistence attempt; OFF suppresses new records; normal ON pairs; clear semantics; genuine unfinished calls show running | ON -> `EditTurboMCPCallLog(false)` -> read while OFF; ON→ON, OFF→OFF, OFF→ON | Write an end only if a start was written for that call; a call that started OFF stays unrecorded. This preserves OFF suppression and writes nothing after disable except closing an already-written start. Negative: normal ON pair, OFF no record. | `test-call-log`; revert exactly the two source files and test. If a `bStartWritten` state requires `Public/TurboMCPCallLog.h`, that is unresolved and not in this allowlist. |
| `R04-REPAIR-05` | `R04-CALL-04`, owner R04 | `Tools/tmcp.py`; `Tools/test-result-contract.py` | GraphRepoint structured result; strict missing/invalid metadata failure for a truly migrated tool; native no-line behavior; compatibility `Session.call` | `call_structured` on the four with default and explicit `requires_result=False`; compare GraphRepoint/native | Client-only: migration requirement is explicit per tool or callers opt out for compatibility-only tools; no silent native certification and no missing metadata as success. ARC02 remains the target. Tool migration is a separate alternative awaiting integrator specification. | `test-result-contract.py`, `test-client-retry.py`; revert the client and test only. |
| `R04-REPAIR-06` | `R04-CALL-06`, owner R04 | `Tools/test-call-log.py`; `Tools/test-call-cost.py` | No prose parsing; invocation errors still `bError=true`; ordinary refusal text unchanged | ON: one ordinary refusal (`Enabled=banana`) and one inner invocation error (known enabled tool, required Enabled field omitted; verify wrapper entry); inspect bError, ERROR and failed count/filter | Record the current truth: refusal `bError=false`; invocation error `bError=true`. Production fix deferred until a structured outcome exists. Negative: invocation error must still be true. | Engine suites; test-only, no production rollback. |

## Existing assertions vs unrun probes

- `Tools/test-call-cost.py` (T-HELP-6): off/empty, populated rankings, per-tool agreement, no-match filter, cap row boundary, `banana`/`0` cap refusals.
- `Tools/test-call-log.py` (T-DBG-1, T-LOG-1/2/3): off/persistence, empty wording, filter/self-match, `since` exclusion, clear confirmation, OFF-mode file unchanged, 4 MiB rotation, bad boolean, `banana`/`0` limit refusals.
- `Tools/test-result-contract.py` and `Tools/test-client-retry.py` pass but do not exercise these four tools.

Not run: engine suites, editor, build, direct MCP, packaging, recovery. Probes still needed: fraction/overflow, empty-log invalid cap, 50/500 and first-4000-plus-suffix boundaries, truncated paging, ON→OFF/OFF→ON/clear-with-log-on record shapes, omitted-vs-empty live schema, refusal-versus-invocation bError, self-record/self-filter ON/OFF, and `call_structured`.

## Validation

| Command | Result |
| --- | --- |
| `python Tools/audit-inventory.py --check` | exit 0; 101 definitions; 95 `parsed_call`, 6 `name_reference_only` |
| `python Tools/test-result-contract.py` | exit 0; PASS (C03–C08) |
| `python Tools/test-client-retry.py` | exit 0; PASS (C01/C02) |
| `python Tools/execute-tests.py --profile offline --execute` | exit 0 on corrected tree (request commit `5067616`); report `.local/executions/80e0be2ee0be46869c49d72d72483d5d/report.json`, tree `70db220b…` |

## Limits and unresolved questions

- Source paths/line numbers are from base `1180c6d`; no runtime behavior is certified.
- `R04-CALL-04` needs a policy decision: client-only migration metadata or a separately specified tool/result-line migration.
- `R04-REPAIR-04` may require a `FScopedCall` state field in `Public/TurboMCPCallLog.h`; that header expansion is unresolved and not authorized here.
- `R04-CALL-06` has no production repair; a structured outcome is required, not prose parsing.
- The 97 unreviewed rows remain unreviewed. Compile-ensure and engine runtime work are outside this checkpoint.

## Checkpoint 2: status, help and schema

Checkpoint 2 is source/offline evidence at base `b353fdd8b44f405c84ef9a67fde9db679f8c4804`; it does not relabel checkpoint 1 and does not certify runtime behavior. The three reviewed tools are `ReadTurboMCPStatus`, `ReadTurboMCPToolHelp` and `ReadTurboMCPToolSchema`.

### Reviewed contracts

| Tool | Declaration and schema | Implementation and wrapper | Parameters/defaults | Effects, success and refusal |
| --- | --- | --- | --- | --- |
| `ReadTurboMCPStatus` | `Public/Tools/TurboMCPToolsets.h:71-72`; no parameters | `TurboMCPToolsets.cpp:179-193`; helpers `:36-148`; recorder `TurboMCPCallRecorder.cpp:103-130` | No parameters; empty inputSchema/required array. | Reads the MCP module/server pointer, URL/port, tool-search setting and registry class map. The count is the static declared-class count, not a registry count. No ARC02 line. |
| `ReadTurboMCPToolHelp` | `Public/Tools/TurboMCPToolsets.h:111-112`; both `FString`s required | `TurboMCPAgentHelp.cpp:311-574`; schema read `:194-238`; known-toolset/split `:343-372`; matching `:378-383`; parameters `:461-503`; budget `:536-542` | `ToolsetName`/`ToolName` required, no defaults. `ToolsetName` is trimmed and checked as a known toolset before splitting; a joined `Toolset.Tool` is accepted only in `ToolsetName` when `ToolName` is empty and its prefix is an exact known toolset. Exact dotted toolset names and dotted suffixes work; `Read` does not. Empty toolset + nonempty tool searches all toolsets; both empty refused; empty tool with a named toolset is summary mode. Omitted fields are rejected before the body. | Reads registry schemas and traps. Prints name:type-description, not required/default. Single-tool path warns over 2000 without cutting; summary path returns before that check and caps rows at 12. Plain `error:` refusals; no ARC02 line. |
| `ReadTurboMCPToolSchema` | `Public/Tools/TurboMCPToolsets.h:484-485`; both `FString`s required | `TurboMCPToolSchema.cpp:447-792`; normalization `:291-323`; matching `:474-570`; parameter rows `:701-719`; cap `:783-789` | Empty `Toolset` refused with real names; empty `Tool` lists the toolset; both empty the same no-toolset error. Matching normalizes case/spaces/underscores/brackets/hyphens and accepts bare or full tool names. Omitted fields are rejected before the body. | Reads registry schemas/traps. One-tool output prints `REQUIRED`, `default=` and a hard 2000-character cut with omissions. Whole-toolset listing is not passed through the final cap. Plain `error:` refusals; no ARC02 line. |

All three are wrapped by `FRecordingToolset` (`TurboMCPCallRecorder.cpp:103-130`). When logging is enabled and the recorder is installed, a read writes start/end records and may rotate the log; status's declaration saying it “costs nothing and changes nothing” is therefore not unconditional (`R04-DISC-03`). When logging is off, no record is written. None of these tools appends an ARC02 result line; `call_structured` would require one for a `TurboMCP.*` toolset unless `requires_result=False` is passed (`R04-CALL-04`). Plain `error:` refusals are values to `FToolResult`, so the recorder marks `bError=false` (`R04-CALL-06`).

### Declared, registered, enabled and discoverable

| Layer | What the source establishes | What it does not establish |
| --- | --- | --- |
| Declared | The 101 capability IDs and the four `AICallable` toolset classes in `GetToolsetClasses()` (`TurboMCPToolsets.cpp:36-43`). | That any class registered. |
| Registered | `RegisterToolsetClass` creates a handler only when `HasValidTools()`; `IsToolsetClassRegistered` checks the handler map by class name (`UToolsetRegistry.cpp:22-61,91-106`). | That the handler is enabled, has enabled tools, or is exposed to MCP clients. |
| Enabled | `FToolset::IsEnabled` gates execution; `GetJsonSchema` returns empty for a disabled toolset or one with no enabled tools (`Toolset.cpp:115-120`). | That status reports enablement; it does not. |
| Discoverable | `GetAllToolsetJsonSchemas` iterates handlers and skips empty schemas (`ToolsetRegistry.cpp:231-245`); this is the list help/schema read. | That the MCP server is listening, tool search exposes it, or a client sees it. |

`ReadTurboMCPStatus` proves only that the server pointer existed when sampled, reports the configured URL/tool-search setting, and reports whether each declared class name is in the registry handler map. It does not prove listening, enablement, filtering or client discovery. With the registry subsystem unavailable, all classes print `MISSING` without a separate registry-unavailable sentence (`R04-DISC-04`).

### Parameter and name behavior

- All three tools have no C++ defaults. `ReadTurboMCPStatus` has no parameters. Help/schema parameters are required by the reflection schema; an omitted field is rejected before the body, while an explicit empty string reaches the body and is interpreted by the tool.
- Help: both names empty -> `error: no tool named`; empty toolset + tool -> search all toolsets; empty tool + named toolset -> summary (max 12, remainder counted); joined `Toolset.Tool` in `ToolsetName` with empty `ToolName` -> full help when the last-dot prefix is an exact known toolset.
- Schema: empty toolset -> `error: no toolset given` listing real names; empty tool -> whole-toolset listing; one tool -> signature, required/default rows, traps and cap.
- Help matching preserves exact dotted toolset names and dotted-suffix names, accepts the joined form only in `ToolsetName`, and requires the exact bare tool in `ToolName`. It does not accept `Read`, normalized spaces/underscores or a qualified name in `ToolName`; schema accepts those. This is `R04-DISC-05`; the shared-contract decision is unresolved.
- Help prints only parameter name/type/description. It does not print `REQUIRED` or `default=` even though the schema reader does (`R04-DISC-01`).
- Help's parameter section is absent when `inputSchema` is missing and says `parameters: none` when the properties object is missing; a real zero-parameter tool says `parameters: 0 (this tool takes no parameters)`. The schema reader records `OddSchemaNote` for the missing cases (`R04-DISC-06`).
- Help's single-tool path warns over 2000 but returns the full text; the summary path returns before that check and caps only rows at 12. The declaration at `TurboMCPToolsets.h:102-103` promises under 2000 for one tool, but that promise is not enforced (`R04-DISC-02`). The `TurboMCPToolsets.h:481` cap belongs to `ReadTurboMCPToolSchema`, not help.

#### Help name-matching matrix

| Input | Mode | Source result | Evidence |
| --- | --- | --- | --- |
| Full toolset + bare tool | full help | accepted | exact `ToolsetMatches`; `Bare.Equals` |
| Suffix toolset (`TurboMCPReadToolset`) + bare tool | full help or summary | accepted | `InName.EndsWith("." + WantedToolset)` |
| `Read` alias + bare tool | full help | not accepted by help; schema accepts via suffix match | no stem-strip/suffix rule in `ToolsetMatches` |
| Joined `Toolset.Tool` in `ToolsetName`, empty `ToolName` | full help | accepted when last-dot prefix is an exact known toolset | `IsKnownToolset`/`SplitName` at `TurboMCPAgentHelp.cpp:343-372`; test at `test-agent-help.py:128-136` |
| Qualified `Toolset.Tool` in `ToolName` | full help | not accepted; exact bare name required | `Bare.Equals(WantedTool)` at `:449-450` |
| Exact dotted toolset summary + empty `ToolName` | summary | accepted when whole value is a known toolset | known-toolset check before split `:343-357` |
| Empty `ToolsetName` + bare tool | full help | searches all toolsets; first match wins, no ambiguity report | empty `ToolsetMatches` branch `:380`; first-match loop |
| Unknown/ambiguous input | refusal | unknown refused with real names; multiple suffix matches choose first | error paths `:433-436,548-573`; first-match loop |

These are source-derived cases, not runtime observations.

### Findings

| ID | Status | Scope | Finding and evidence | Existing assertions |
| --- | --- | --- | --- | --- |
| `R04-DISC-01` | confirmed source | help, schema | Help does not read `inputSchema.required` or print defaults (`TurboMCPAgentHelp.cpp:461-493`); schema does (`TurboMCPToolSchema.cpp:127-140,701-719`). Protocol requires requiredness. The header's “whatever its default” reason is stale; the engine requires non-optional parameters without defaults (`JsonSchemaGenerator.cpp:288-299`). | `test-agent-help.py` checks `parameters:` and length only. |
| `R04-DISC-02` | confirmed source | help | Help declaration promises under 2000 for one tool (`TurboMCPToolsets.h:102-103`). Single-tool path warns and returns full text (`TurboMCPAgentHelp.cpp:536-542`); summary path returns before that check and caps rows at 12 (`:385-430`). `TurboMCPToolsets.h:481` is the schema tool's separate cap; schema's single-tool guard cuts at 2000 (`TurboMCPToolSchema.cpp:783-789`). | `test-agent-help.py:63-65` checks selected short single-tool help only. |
| `R04-DISC-03` | confirmed source | status | Declaration says status costs/changes nothing (`TurboMCPToolsets.h:64-66`); the recorder writes start/end and can rotate when logging is ON (`TurboMCPCallRecorder.cpp:103-130`; `TurboMCPCallLog.cpp:216-234,254-305`). | No status side-effect assertion. |
| `R04-DISC-04` | confirmed source | status, help, schema | Status prints the static declared count as “N toolsets” (`TurboMCPToolsets.cpp:36-43,184`); registration is handler-map presence (`UToolsetRegistry.cpp:91-106`), while disabled toolsets have empty schemas and are omitted from help/schema (`Toolset.cpp:115-120`; `ToolsetRegistry.cpp:231-245`). Registry unavailability prints only MISSING. | No declared/registered/enabled/schema-listed distinction test. |
| `R04-DISC-05` | contract gap | help, schema | Schema accepts normalized `Read`, space/underscore variants and full `Toolset.Tool` (`TurboMCPToolSchema.cpp:291-323,474-570`). Help preserves joined `Toolset.Tool` in `ToolsetName` and exact dotted toolset names, but rejects `Read`, normalization variants and qualified names in `ToolName` (`TurboMCPAgentHelp.cpp:343-383,449-450`). The matrix above enumerates the modes. `test-batch-tools.py:58` uses `Read`; help tests assert the joined form at `test-agent-help.py:128-136`. | No help test for `Read`, normalized variants, qualified `ToolName`, empty toolset + tool, or ambiguous suffixes. |
| `R04-DISC-06` | confirmed source | help | Missing `inputSchema` omits the parameter section; a present `inputSchema` without `properties` prints `parameters: none`; a real zero-parameter tool prints `parameters: 0 (this tool takes no parameters)` (`TurboMCPAgentHelp.cpp:461-503`). Schema records `OddSchemaNote` for the missing cases (`TurboMCPToolSchema.cpp:207-213,694-699`). | No missing-schema help case. |

### Bounded repair proposals

Proposals are unapproved. They preserve the accepted checkpoint-1 rows and findings.

| ID | Finding / owner | Exact allowed files | Preserved behavior | Probe | Expected / negative control | Validation / rollback |
| --- | --- | --- | --- | --- | --- | --- |
| `R04-REPAIR-07` | `R04-DISC-01`, owner R04 | `Plugins/TurboMCP/Source/TurboMCP/TurboMCPAgentHelp.cpp`; `Tools/test-agent-help.py` | Help framing, summary/full modes, traps, first-sentence description, no schema change | Required/default tool and no-parameter tool; current suite does not guarantee a defaulted parameter | Print requiredness/default matching schema; zero-param still says none. Negative: optional/default not marked required. | Proposed `test-agent-help` conditional on a missing defaulted fixture; revert help/tool test. |
| `R04-REPAIR-08` | `R04-DISC-02`, owner R04 | `Plugins/TurboMCP/Source/TurboMCP/TurboMCPAgentHelp.cpp`; `Tools/test-agent-help.py` | Short help, traps, summary's 12-row early return, first-sentence rule, schema's separate cut | Single tool >2000 and summary >12 rows; long trap/schema fixture is missing | Single-tool cap enforced/omissions named; summary remains row-capped and is not described as character-capped. Alternative warning-only needs a separate contract decision. Negative: short help byte-identical. | Proposed `test-agent-help` conditional on missing long-help fixture; revert help/tool test. |
| `R04-REPAIR-09` | `R04-DISC-03`, owner R04 | `Plugins/TurboMCP/Source/TurboMCP/Public/Tools/TurboMCPToolsets.h` | Status output, server/registration checks, no asset change | Status with logging ON/OFF | Declaration states read calls are recorded when logging is ON; no unconditional “costs nothing”. Negative: OFF has no record and text unchanged. | `python Tools/validate-repo.py`; `python Tools/validate.py`; source review. Rollback the header only. |
| `R04-REPAIR-10` | `R04-DISC-04`, owner R04 | `Plugins/TurboMCP/Source/TurboMCP/TurboMCPToolsets.cpp`; `Tools/test-agent-help.py` | Server/settings line, one line per class, registered/MISSING names | Enabled all, one disabled, registry unavailable | Label declared/registered/schema-listed/enabled separately; no client-discovery claim. Negative: missing class still MISSING; disabled not labeled enabled. | No current status assertion; proposed status case is not present. Validation unavailable until design/test scope is approved. Rollback both listed files. |
| `R04-REPAIR-11` | `R04-DISC-05`, owner R04 | `Plugins/TurboMCP/Source/TurboMCP/TurboMCPAgentHelp.cpp`; `Tools/test-agent-help.py` | Joined `Toolset.Tool`, exact dotted toolset summary, case-insensitive full/suffix, bare tool, refusal names | Eight-row matching matrix above | Preserve joined/dotted behavior; add alias/normalization/qualified forms or document narrower contract; ambiguity named. Negative: unknown refused, no silent cross-toolset match. | `test-agent-help,test-batch-tools`; revert help/test. |
| `R04-REPAIR-12` | `R04-DISC-06`, owner R04 | `Plugins/TurboMCP/Source/TurboMCP/TurboMCPAgentHelp.cpp`; `Tools/test-agent-help.py` | Zero-param wording, parameter rows, traps/refusals | Missing inputSchema/properties vs truly empty properties; production schemas cannot supply this | “parameters: unavailable” for missing schema; zero for real empty properties. Negative: no-param tool still says none. | No current fixture/test seam; validation unavailable until a synthetic schema fixture is approved. Rollback both listed files. |

### Checkpoint 2 validation

| Command | Result |
| --- | --- |
| `python Tools/audit-inventory.py --check` | exit 0; 101 definitions; 95 `parsed_call`, 6 `name_reference_only` |
| `python Tools/test-result-contract.py` | exit 0; PASS (C03–C08) |
| `python Tools/test-client-retry.py` | exit 0; PASS (C01/C02) |
| `python Tools/execute-tests.py --profile offline --execute` | exit 0 after corrections; report `.local/executions/536baf104ac5401fa25e907fe5f48c29/report.json`, tree `c603009e…` |

No engine suite, editor, build, direct MCP call, packaging or recovery was run. Existing engine suites are assertions only: `test-agent-help.py` (T-HELP-1/2/3/5; selected help under 2000, traps, refusals, joined form), `test-batch-tools.py` (schema short `Read`, engine toolset, one-tool parameters, <2400, closest names), and the status uses in `test-call-cost/log`, `test-client-retry` and `test-graph-text-guards`. The unrun probes are listed in the JSON `probe_gaps`.

### Checkpoint 2 unresolved questions

- Cap-and-announce versus warning-only help: the protocol requires a cap, but the implementation choice is not approved.
- Whether help should reuse schema normalization or document a narrower name contract; no shared helper is authorized.
- Whether status should expose enablement/discoverability through a new public query or a schema-list proxy; design unresolved.
- Help's required/default output format and the missing-schema wording have not been reviewed for implementation.
- All three tools remain plain-FString, non-ARC02 tools; `call_structured` and `R04-CALL-06` refusal recording remain unresolved.
- The 94 unreviewed rows remain unreviewed; R04 is incomplete.

## Checkpoint 3: source-control state and checkout

Checkpoint 3 is source/offline evidence at base `fc46e01845c4fbd6814c07f5f228d946aeaef6b9`; it does not certify runtime behavior or a live-depot checkout. Reviewed tools: `ReadTurboMCPVcsState`, `PreviewTurboMCPVcsCheckout` and `EditTurboMCPVcsCheckout`.

### Reviewed contracts

| Tool | Declaration | Implementation and effects | Success / refusal shape |
| --- | --- | --- | --- |
| `ReadTurboMCPVcsState` | `TurboMCPToolsets.h:741-742`; required `Assets` | `TurboMCPSourceControlTools.cpp:32-37` calls `Plan` then `Render(..., false)`; `TurboMCPSourceControl.cpp:184-408` | Provider/module facts plus one row per input; per-row state or `REFUSED`; no checkout or asset write. Plain text; no ARC02 line. |
| `PreviewTurboMCPVcsCheckout` | `TurboMCPToolsets.h:1523-1524`; required `Assets` | `:39-44` calls `Plan` then `Render(..., true)`; same Plan, no `FCheckOut` | State/refusal rows plus `WHAT A CHECKOUT WOULD DO`; no provider write operation. Plain text; no ARC02 line. |
| `EditTurboMCPVcsCheckout` | `TurboMCPToolsets.h:2316-2317`; required `Assets` | `:46-50` calls `Plan` then `Checkout` (`TurboMCPSourceControl.cpp:410-530`) | Per-row `ECommandResult::Succeeded/Failed/Cancelled` plus provider messages; skipped rows say `NOT ATTEMPTED`/`ALREADY DONE`; no aggregate or post-state verification. Plain text; no ARC02 line. |

`Assets` is a required `FString`; omitted is rejected before the body. Explicit empty string is the documented no-asset mode: `Read`/`Preview` print provider/module facts with no rows; `Edit` says there is nothing to check out. `SplitList` (`TurboMCPAssetResolver.cpp:182-206`) splits on comma or pipe, trims, culls empty parts, preserves order and does not deduplicate. Empty separators are silently discarded. Each part is resolved by `FTurboMCPAssetResolver::Resolve` (`:36-143`).

Resolution tries direct forms before any registry fallback. A slash-free input first tries `FindObject` and `CompleteFromObject`; that loaded-object route has no `/Game` mount filter and no registry uniqueness check (and rejects only `UPackage`, not every non-asset UObject). The `/Game`-only, exact-name, registry-uniqueness behavior belongs to the later `FindAllByName` fallback. `TryDirectForms` can fall from a failed loaded-object lookup to `CompleteFromPackagePath`, which accepts a sole package asset without matching the requested object member; the multi-asset-package refusal belongs only to that package fallback. The generated-class condition is also narrower than its comment: with `ClassSuffix="_C"`, `DotBeforeSuffix` is `Len-3`, so the normal `Package.Object_C` form is not recognized as a class path. Do not claim that every route refuses every invalid/ambiguous form.

Invalid, ambiguous, multiple-asset-package and missing-file inputs can be refused per row, but not universally: loaded-object and sole-package fallback routes bypass some checks. An unsaved package can resolve in memory but has no verified file, so `FileForPackagePath` refuses it (`TurboMCPSourceControl.cpp:54-85`). Files are verified with `FileExists` before a provider is asked. Duplicate inputs remain duplicate rows, duplicate `GetState` calls and potentially duplicate checkout operations.

### Provider and state trace

`Plan` asks `ISourceControlModule::IsEnabled` and, only when enabled, `IsAvailable` and the provider capability flags (`TurboMCPSourceControl.cpp:184-245`). A disabled/unavailable/module-missing run produces one refusal string per resolved asset and does not call `GetState`. The refusal strings are the engine's own `SourceControlHelpers.cpp:67,76` or a module-load reason.

For each surviving row, `Plan` calls:

`Provider.GetState(Row.File, EStateCacheUsage::ForceUpdate)` (`TurboMCPSourceControl.cpp:293`).

That reaches the single-file helper (`ISourceControlProvider.cpp:50-60`): it calls the array `GetState`, returns a state only when that call returns `Succeeded` and the array is nonempty, and returns null otherwise. It does not expose the result to TurboMCP. The pinned Perforce array implementation (`PerforceSourceControlProvider.cpp:423-433`) is where `ForceUpdate` executes `FUpdateStatus`, ignores that operation's result, reads `GetStateInternal` from cache and returns `ECommandResult::Succeeded`. A cached state can therefore be reported after a failed refresh. R13 recorded the same limit (`docs/audit/native-overlap.md:35,88`; `docs/audit/technical-debt.md:52-53`); no stale-proof claim is made here.

`Plan` checks `State.IsValid()` only (`TurboMCPSourceControl.cpp:295`). A null state becomes `UNKNOWN` with the reason. It does not call `ISourceControlState::IsUnknown()`. Perforce's default `FPerforceSourceControlState` is `DontCare` (`PerforceSourceControlState.h:52-56`); `IsUnknown()` is true for it (`PerforceSourceControlState.cpp:356-359`), and `IsSourceControlled()` is also true (`:326-329`). A non-null cached unknown state can therefore print `under source control: true` with provider text `Unknown`, rather than the unknown-state branch.

The structured flags are `IsSourceControlled`, `IsCheckedOut`, `IsAdded`, `IsCurrent`, `IsModified`, `IsIgnored` and `CanCheckout` (`TurboMCPSourceControl.cpp:309-317`). `IsCheckedOutOther` and `IsUnknown` are not read; a filesystem read-only attribute is not probed. Those conditions appear only through provider display text or `CanCheckout()`.

### Read, Preview and Edit side effects

- `Read` and `Preview` call `Plan`, so they can issue per-asset `GetState(ForceUpdate)` provider refresh/cache/network work even though they never issue `FCheckOut` or write an asset file.
- `Preview` renders checkout intent from `UsesCheckout`, `bCheckedOut`/`bAdded`, `bStateRead` and `CanCheckout`; it does not issue `FCheckOut`, but it can execute a source-control refresh through `GetState(ForceUpdate)`, so it is not a provider no-op.
- `Edit` issues one `FCheckOut` for each eligible row (`TurboMCPSourceControl.cpp:491-492`). It skips plan-refused rows and rows without readable state. The already-checked-out/added skip is only in the `UsesCheckout() == true` branch; with `UsesCheckout() == false`, Edit still issues the operation even when those flags are set. It still attempts when `CanCheckout()` is false. On a no-checkout provider it states the no-op expectation and still executes to record the provider's answer.
- All three are wrapped by `FRecordingToolset` (`TurboMCPCallRecorder.cpp:103-130`); logging ON adds start/end records and can rotate the log.
- No tool writes or saves an asset. `Edit` can change provider/depot lock state. There is no rollback, aggregate transaction or atomic multi-asset behavior.

### Preview/Edit branch matrix

`AddAssetLines` checks `!UsesCheckout` before `!bStateRead`; `Checkout` checks `!bStateRead` first. This is a source-level mismatch, not a state-change scenario:

| UsesCheckout | State | Checked-out/added | CanCheckout | Preview branch | Edit branch |
| --- | --- | --- | --- | --- | --- |
| true | readable | true | any | nothing to do | ALREADY DONE, skip |
| true | readable | false | false | cannot check out; still attempted | attempt `FCheckOut` |
| true | readable | false | true | `FCheckOut` would be issued | attempt `FCheckOut` |
| true | null | false | any | nothing would be attempted | NOT ATTEMPTED |
| false | readable | true | any | no-op; provider still asked | issue `FCheckOut` (no-op) |
| false | readable | false | any | no-op; provider still asked | issue `FCheckOut` (no-op) |
| false | null | false | any | no-op; provider still asked | NOT ATTEMPTED (mismatch) |
| any | refused | any | any | REFUSED | REFUSED, skip |

The null-state/no-checkout row is the concrete disagreement: Preview says the provider will still be asked; Edit skips. The readable/no-checkout row is not a skip: Edit issues even if checked-out/added is true. Planning refusals are separate from operation outcomes.

### Checkout execution and result limits

`Edit` calls `Plan`, renders the state half, then loops the rows. It does not re-run `GetState` before or after `Provider.Execute`; skips are based on the plan's stale fields. A file whose state changes between Plan and Execute can be skipped or attempted on the wrong condition. `ECommandResult::Succeeded` is the provider operation's answer only; no post-operation state read proves that the file is checked out. The result fields `bAttempted`/`bSucceeded`/`ProviderAnswer` in `TurboMCPSourceControl.h:101-109` are not populated by this path.

A multi-asset call continues after a failed row. The output has per-row results but no aggregate count, partial-success marker, retry or rollback. `Preview` and `Edit` each call `Plan` separately; sharing the same function does not preserve one state snapshot across two calls, so a state change between Preview and Edit can legitimately/incorrectly change the answer.

All three tools return plain `FString` with no ARC02 result line. `Tools/tmcp.py` `call_structured` requires one for `TurboMCP.*` unless `requires_result=False`; this is the prior `R04-CALL-04` contract gap. Text refusals are values to the recorder, so `bError=false` (`R04-CALL-06`); they are not failed-call counts.

### Findings

| ID | Status | Scope | Finding and evidence | Existing assertions |
| --- | --- | --- | --- | --- |
| `R04-VCS-01` | confirmed limitation | all three | `ForceUpdate` does not prove refresh success. The single-file helper returns a state only when the array call returns `Succeeded` and is nonempty; otherwise null, and it does not expose the result (`ISourceControlProvider.cpp:50-60`). Perforce ignores `FUpdateStatus` failure and returns cached state with `Succeeded` (`PerforceSourceControlProvider.cpp:423-433`). R13 recorded the same at `native-overlap.md:35,88`. | No failed-refresh/cached-state test; Perforce only tested unavailable. |
| `R04-VCS-02` | confirmed source | all three | Non-null state is never checked with `IsUnknown` (`TurboMCPSourceControl.cpp:295-317`). Perforce `DontCare` is `IsUnknown` true but `IsSourceControlled` true (`PerforceSourceControlState.cpp:326-329,356-359`); it can print `under source control: true` with provider text `Unknown`. | No non-null unknown-state test. |
| `R04-VCS-03` | confirmed source | all three | `SplitList` preserves duplicates (`TurboMCPAssetResolver.cpp:182-206`); Plan loops every row (`TurboMCPSourceControl.cpp:252-320`) and Checkout can execute per duplicate (`:443-527`). Duplicate inputs inflate `assets:` and can duplicate state queries/checkouts. | `test-vcs.py` uses two distinct fixtures and checks `len(rows)==2`; no duplicate/separator case. |
| `R04-VCS-04` | confirmed source | preview, edit | Checkout uses plan state without revalidation (`TurboMCPSourceControl.cpp:455-492`), never reads state after Execute, does not populate the `bSucceeded` fields, and has no aggregate/partial/rollback result. The checked-out/added skip is only the `UsesCheckout()==true` branch; a readable no-checkout row still issues. Preview/Edit each call Plan separately; shared code is not a shared snapshot. | `test-vcs.py` only covers no-provider, Git no-op and Perforce-unavailable; no successful live checkout or post-state. |
| `R04-VCS-05` | confirmed source | all three | Flags at `TurboMCPSourceControl.cpp:134-142,309-317` omit `IsCheckedOutOther` and `IsUnknown`; read-only is not probed. Those states are only in provider text/`CanCheckout`. | No test for checked-out-other, unknown non-null or read-only. |
| `R04-VCS-06` | confirmed source | preview, edit | Branch order differs: `AddAssetLines` checks `!bUsesCheckout` before `!bStateRead` (`TurboMCPSourceControl.cpp:145-177`); `Checkout` checks `!bStateRead` first (`:455-488`). With no refusal, no-checkout provider and null state, Preview says the provider is still asked; Edit skips NOT ATTEMPTED. The branch mismatch exists on one plan; it does not require a state change. The already-checked-out/added skip is only in the `UsesCheckout()==true` branch; with false a readable checked-out/added row still issues. | No null-state/no-checkout or readable checked-out/no-checkout test. |
| `R04-VCS-07` | confirmed source | all three | Loaded-object route has no `/Game` or registry-uniqueness check (`TurboMCPAssetResolver.cpp:60-68,269-315`); `/Game` belongs to `FindAllByName` (`:357-400`). `TryDirectForms` can fall to `CompleteFromPackagePath`, which accepts a sole package asset without matching the requested member (`:208-267,317-355`). The `_C` condition uses `DotBeforeSuffix=Len-3` (`:215-221`), so normal `Package.Object_C` is not recognized as a class path. Multi-asset refusal is package-fallback-only. | No loaded-object/absent-member/normal-C/multi-asset test. |

### Bounded repair proposals

All proposals are unapproved.

| ID | Finding / owner | Exact allowed files | Preserved behavior | Probe | Expected / negative control | Validation / rollback |
| --- | --- | --- | --- | --- | --- | --- |
| `R04-REPAIR-13` | `R04-VCS-01`, owner R04 | `Plugins/TurboMCP/Source/TurboMCP/TurboMCPSourceControl.cpp`; `Tools/test-vcs.py` | Per-asset state rows, provider/module facts, refusal strings, no checkout in Read/Preview | Cached state after failed `FUpdateStatus` via test provider | State is labeled provider-reported/freshness-unproven, never stale-proof; provider text remains. Negative: changed successful state still changes. Fixture gap: no failed-refresh provider fixture. | No current suite; proposed provider fixture; rollback both files. |
| `R04-REPAIR-14` | `R04-VCS-02`, owner R04 | `Plugins/TurboMCP/Source/TurboMCP/TurboMCPSourceControl.cpp`; `Tools/test-vcs.py` | Null-state branch, provider text, per-asset rows/order, no checkout in Read/Preview | Non-null `IsUnknown` state plus a known state | Unknown row is marked UNKNOWN and does not claim true/false source control. Negative: known state still prints known flags. Fixture gap: no unknown-state provider fixture. | No current suite; proposed fixture; rollback both files. |
| `R04-REPAIR-15` | `R04-VCS-03`, owner R04 | `Plugins/TurboMCP/Source/TurboMCP/TurboMCPSourceControl.cpp`; `Tools/test-vcs.py` | Comma/pipe, trimming, order, distinct assets, per-asset refusals | `TA01,TA01`, `TA01|TA01`, separator-only | Duplicates collapsed or explicitly refused; distinct assets/order unchanged. Policy is unresolved. Negative: two distinct assets still two rows. | Add cases to test-vcs.py; revert both files. |
| `R04-REPAIR-16` | `R04-VCS-04`, owner R04 | `Plugins/TurboMCP/Source/TurboMCP/TurboMCPSourceControl.cpp`; `Tools/test-vcs.py` | Per-asset refusals, no-op Git behavior, provider messages, no asset write | State change between Plan and Execute; post-state read; one success/failure/skip | Revalidate or state snapshot; read back state and only claim verified checkout; summarize partial results. Negative: stable success still succeeds; Git no-op never claims lock. Fixture gap: no state-changing provider fixture. | No current suite; proposed provider fixture; rollback both files. |
| `R04-REPAIR-17` | `R04-VCS-05`, owner R04 | `Plugins/TurboMCP/Source/TurboMCP/TurboMCPSourceControl.cpp`; `Tools/test-vcs.py` | Existing flags/provider text/order/refusals | Checked-out-other, non-null unknown, read-only states | New structured status or explicit inability; no false true/false. Negative: normal checked-out/current flags unchanged. Fixture gap: no such provider states. | No current suite; proposed fixture; rollback both files. |
| `R04-REPAIR-18` | `R04-VCS-06`, owner R04 | `Plugins/TurboMCP/Source/TurboMCP/TurboMCPSourceControl.cpp`; `Tools/test-vcs.py` | Per-asset refusals; readable no-checkout no-op; readable UsesCheckout=true checked-out/added skip; `CanCheckout` false still attempts; no asset write | No-checkout provider + null state; readable checked-out/added no-checkout | Preview and Edit agree on null state: both NOT ATTEMPTED/UNKNOWN; readable no-checkout still issues. Negative: UsesCheckout=true checked-out/added still skips; refusal still skips. Fixture gap: no null-state test provider. | No current suite; proposed provider fixture; rollback both files. |
| `R04-REPAIR-19` | `R04-VCS-07`, owner R04 | `Plugins/TurboMCP/Source/TurboMCP/TurboMCPAssetResolver.cpp`; `Tools/test-vcs.py` | Simple accepted path forms; case-insensitive short fallback; ambiguous short-name refusal in the `/Game` registry fallback; package/file refusal | Loaded short name, unloaded `Package.Object`, normal `Package.Object_C`, multi-asset package, absent member | Document/test actual route precedence; loaded route not described as `/Game`-only; package fallback matches requested member or says sole-asset selection; normal class form recognized or explicitly refused. Negative: ambiguous short name in the registry fallback still refuses; sole asset still resolves. Fixture gap: loaded-only/unloaded/class/multi-asset fixtures missing. | No current route assertion; proposed test-vcs.py extension; rollback both files. |

### Checkpoint 3 validation

| Command | Result |
| --- | --- |
| `python Tools/audit-inventory.py --check` | exit 0; 101 definitions; 95 `parsed_call`, 6 `name_reference_only` |
| `python Tools/test-result-contract.py` | exit 0; PASS (C03–C08) |
| `python Tools/test-client-retry.py` | exit 0; PASS (C01/C02) |
| `python Tools/execute-tests.py --profile offline --execute` | exit 0 after consistency corrections; report `.local/executions/5c23c907de3b4e08995eb1b890ff84cb/report.json`, tree `969a2a37…` |

No engine suite, editor, build, direct MCP, source-control checkout, packaging or recovery was run. Existing engine suites are assertions only:

- `Tools/test-vcs.py` (T-VCS-1): no-provider empty/per-asset refusals; mixed invalid input; Preview/Edit identical refusals; Git state and no-op checkout answer; Perforce enabled/unavailable refusal; engine-helper cross-check; no working-tree change. It explicitly says no live depot checkout, Plastic/Subversion unavailable and Phase C restore.
- `Tools/test-vcs-write.py` tests the separate automatic-checkout path in write tools; its only `ReadTurboMCPVcsState` use is empty `Assets`. It does not exercise per-asset VCS state or `EditTurboMCPVcsCheckout`.
- `test-vcs.py` line 313's “no row missing or duplicated” is a two-distinct-asset check, not a duplicate-input test. Line 471's “attempted” is the Git no-op operation, not a successful lock. Line 479 only checks Preview refusals, not state/intent equality.

### Checkpoint 3 unresolved questions

- Whether duplicates should be deduplicated or refused; no policy is approved.
- Whether checkout success requires post-state verification and how partial multi-asset results are summarized.
- How to represent freshness, non-null unknown, checked-out-other and read-only state without a provider-independent schema.
- No live-depot checkout or failed-refresh fixture exists; provider-seam scope is missing.
- All three tools remain plain text/non-ARC02; `call_structured` and refusal recording remain unresolved.
- The 91 unreviewed rows remain unreviewed; R04 is incomplete.

## Checkpoint 4: save contracts

Checkpoint 4 is source/offline evidence at base `15ab395ac88eaa52ea31d7628d032446c90fddb6`; it does not certify runtime behavior or durable persistence. Reviewed tools: `EditTurboMCPSave` and `EditTurboMCPSaveWithoutThumbnail`. Claim `613fbd8768e04378859aad03a5a26bb6` (`dsh-r04-save-01`) was inspected in this worktree; the claim store SHA-256 was `1e4930ce16f30f4623d50de4721fbb36e7b82ca7e230c9d71dd30ee0e9343473`.

### Reviewed contracts

| Tool | Declaration | Implementation and effects | Success / refusal shape |
| --- | --- | --- | --- |
| `EditTurboMCPSave` | `TurboMCPToolsets.h:1617-1618`; required `Assets`, no C++ default | `TurboMCPCompile.cpp:658-661` calls the shared `SaveTurboMCPAssets(..., false)`; body `:364-655`. `:405-406` runs the source-control preflight before the loop; `:504-516` uses `UEditorAssetLibrary::SaveLoadedAsset(AssetObject, bOnlyIfIsDirty=false)`; `:537-548` retries the same mechanism once while the package remains dirty; success is the package's clean dirty flag. | One row per nonempty input plus counts and an appended checkout block. Starts `changed: saved N of M asset(s)` when at least one input's package became clean, otherwise `unchanged: ...`. Plain text; no ARC02 line. |
| `EditTurboMCPSaveWithoutThumbnail` | `TurboMCPToolsets.h:1649-1650`; required `Assets`, no C++ default | `TurboMCPCompile.cpp:663-665` calls `SaveTurboMCPAssets(..., true)`. `:479-495` requires `/Game/` and always names a `.uasset` file; `:523-533` calls `UPackage::SavePackage` directly; `:537-548` retries once on dirty. It skips the editor thumbnail renderer, but not `PreSave`/`PostSave` or serialization. | Same input rows and counts; successful rows say the thumbnail draw was skipped. Non-`/Game/` packages are refused per input after the source-control preflight. Plain text; no ARC02 line. |

`Assets` is a required `FString`; omitted is rejected before the body by the reflection schema. Explicit empty is the body's no-asset error. `SplitList` splits on comma or pipe, trims, culls empty parts, preserves order and does not deduplicate (`TurboMCPAssetResolver.cpp:182-206`). More than 25 parts is refused before any provider call. `Resolve` is called once by the source-control `Plan` and again by the save loop; the route caveats in `R04-VCS-07` still apply.

### Package, dirty and input handling

- `HasUnsavedChanges` is `FindPackage(...) != nullptr && Package->IsDirty()` (`TurboMCPCompile.cpp:136-140`). A package not in memory reports "had no changes" without being loaded. A loaded dirty package is saved. A loaded unsaved package with no file can be written by E-SAVE, but the source-control preflight refuses it for having no file; the editor path can still mark it for add after the save.
- The package must be found by exact name and the asset object must load. The `:439-449` "reports unsaved changes but package could not be opened" branch is effectively unreachable because the preceding `HasUnsavedChanges` used the same `FindPackage`.
- Empty, separator-only, malformed and duplicate inputs: empty/separator-only is one whole-call error; malformed entries become per-row errors and failed counts; duplicates remain separate rows. A valid input after a failure is still attempted.
- A clean input receives no save call, but it receives the source-control preflight before that decision. A duplicate of a just-saved asset similarly reports "had no changes" on its second row.
- The two tools share the outer validation loop, not the write mechanism. E-SAVE saves the package through the editor path and can save external packages. The no-thumbnail tool writes one package directly, does not call `SaveExternalPackages`, does not mark a new file for add, and does not perform E-SAVE's registered-asset check.

### Execution and side-effect separation

| Stage | `EditTurboMCPSave` | `EditTurboMCPSaveWithoutThumbnail` |
| --- | --- | --- |
| Editor/PIE guard | `WriteRefusalReason` at `:50-74`; no file read-only check | Same shared guard |
| Source-control preflight | `Plan` + `EnsureCheckedOut` at `:405-406`, before dirty/load/write decisions | Same preflight before its `/Game/` refusal |
| Dirty check | `:432-437`; no save when clean | Same |
| Package/object load | `:439-467` | Same |
| Write mechanism | `SaveLoadedAsset` at `:516`; engine `SavePackages` path | `UPackage::SavePackage` at `:533` |
| Mechanism side effects | Engine checks out packages again, can save external packages, and runs the thumbnail step before writing | No thumbnail step, no external-package save, no mark-for-add; still runs `PreSave`/`PostSave` and serialization |
| Retry | Same mechanism once while dirty (`:537-548`) | Same mechanism once while dirty |
| Success verdict | `Package->IsDirty() == false`; no disk readback | `Package->IsDirty() == false`; no disk readback |
| Compile | No compile call or compile-state check | No compile call |
| Project code | Widget thumbnail draw can instantiate the widget and run design-time code | No thumbnail draw; `PreSave`/`PostSave` overrides can still run |

No compile is performed by either tool. A dirty Blueprint can be saved while it has compile errors; the result does not claim compilation. `EditTurboMCPCompile` is the separate explicit compile operation.

E-SAVE's engine save path is the editor `OBJ SAVEPACKAGE` path: `SaveLoadedAsset` -> `SavePackages` -> `InternalCheckoutAndSavePackages` -> `InternalPromptForCheckoutAndSave` -> `SaveAsset` -> `GEngine->Exec("OBJ SAVEPACKAGE ...")` (`EditorAssetSubsystem.cpp:1290-1310`; `FileHelpers.cpp:5925-5951,6019-6022,1318-1332`; `UnrealEdSrv.cpp:680-691`; `EditorServer.cpp:4691-4733`). The engine checks out packages again, can save external packages, and generates thumbnails before the actual package write. For a Widget Blueprint the renderer creates a transient widget and calls `Initialize` with `Designing | ExecutePreConstruct` (`WidgetBlueprintThumbnailRenderer.cpp:189-191`), and its frequency is `OnAssetSave` (`:216-218`).

The direct tool skips the thumbnail renderer. `UPackage::SavePackage` still calls `PreSavePackage`/`CallPreSaveRoot` and `PostSaveRoot`/`PostSavePackage` (`SavePackage2.cpp:4050-4060,4117-4129`), and `UWidgetBlueprint::PreSave` modifies the generated class widget tree (`WidgetBlueprint.cpp:705-713`). The header's "cannot ... run the asset's own code" claim therefore applies to the thumbnail draw, not to the direct save as a whole.

The E-SAVE warning at `TurboMCPCompile.cpp:604-611` is not provenance evidence. Its capture marks are taken before object load (`:451-452,456-459`), and `ErrorsHere` covers loading, save hooks, serialization, the save and retry (`:550-551`). The warning condition checks only a clean package, the editor path, a Widget Blueprint cast and `ErrorsHere > 0`; it does not verify that a thumbnail draw ran or that the renderer raised the errors. The text must be read as "errors occurred in this save interval", not as observed thumbnail/design-time attribution.

### Findings

- **`R04-SAVE-01` (confirmed source; both).** Source-control preflight runs before per-asset write eligibility. `EnsureCheckedOut(Plan(Parts), ...)` is called at `TurboMCPCompile.cpp:405-406`, before the loop's dirty check (`:432-437`), package/object resolution (`:439-467`) and the no-thumbnail `/Game/` refusal (`:479-491`). A clean asset, a refused package, an unloadable object or a duplicate can therefore receive `GetState(ForceUpdate)` and `FCheckOut` even though the tool will not write it. The preflight remains report-only, per D-57; the defect is placement, not its non-gating policy. `test-vcs-write.py:423-449,623-636` checks only a new asset with no provider and Git; provider operation counts and clean/refused cases are absent.
- **`R04-SAVE-02` (confirmed source; E-SAVE).** E-SAVE performs a second checkout path inside `SaveLoadedAsset`. The tool calls `EnsureCheckedOut` first, then `SaveLoadedAsset`; the engine path reaches `CheckoutPackages` whenever source control is enabled (`FileHelpers.cpp:5925-5929`) and again for mark-for-add after the save (`:5949-5951`). The engine ignores the checkout result and may skip a second `FCheckOut` if the file is already checked out, but the appended block describes only the tool's pass. A new asset with no file can be marked for add by the engine after the save; the block cannot say so. `test-vcs-write.py:430-449,713-732` checks only the first block and the static call to `EnsureCheckedOut`.
- **`R04-SAVE-03` (confirmed source; no-thumbnail).** The direct write does not preserve E-SAVE's package semantics. `TurboMCPCompile.cpp:479-495` always uses `GetAssetPackageExtension()` and a `/Game/`-relative path; it has no map-extension branch. E-SAVE's engine path chooses `.umap` for a World package (`FileHelpers.cpp:3747-3775`). The direct path also skips `SaveExternalPackages` (`:1318-1332`), E-SAVE's registered-asset check (`EditorAssetSubsystem.cpp:1299-1309`), mark-for-add and the uncontrolled-changelist handling. The header at `TurboMCPToolsets.h:1623-1647` calls it the same save minus the thumbnail; that is too broad. No test uses a map, external package, loaded unregistered asset or new-file source-control state.
- **`R04-SAVE-04` (confirmed source; no-thumbnail).** The direct write still runs asset code. `UPackage::SavePackage` calls `PreSavePackage`/`CallPreSaveRoot` and `PostSaveRoot`/`PostSavePackage` (`SavePackage2.cpp:4050-4060,4117-4129`), and `UWidgetBlueprint::PreSave` performs generated-class widget-tree work (`WidgetBlueprint.cpp:705-713`). The tool skips the thumbnail renderer, not all project/asset code. `test-save-thumbnail-option.py:231-236` checks only the success sentence about a draw; no test observes a `PreSave` override.
- **`R04-SAVE-05` (confirmed source; both).** Per-input counters are not per-package write outcomes. `SavedCount`/`NoChangeCount`/`FailedCount` are incremented per `Parts` row (`TurboMCPCompile.cpp:419-428,553-558,624-639`), while `SaveLoadedAsset` saves a package and can save external packages, and the direct path saves one package. Two assets in one package can produce `1 saved, 1 had no changes` even though both assets were in the written package. Duplicate inputs can inflate failures; E-SAVE can write external packages that are never named. There is no package-level saved/failed/unattempted map or explicit partial outcome. The suites save one asset per call.
- **`R04-SAVE-06` (confirmed source; both).** Failure, draw and error-attribution text is not conditioned on observed outcome or provenance. E-SAVE prints `thumbnail/design-time work: ran through the editor's save path` on every successful asset (`:553-561`), although the engine only uses a cached thumbnail or draws when the cached one is missing/dirty and not in automation (`UnrealEdSrv.cpp:438-446,478-491`). The Widget Blueprint block at `:591-601` is outside the success branch: a failed no-thumbnail widget save can print both `is still unsaved after two attempts` and `the package was written directly`. The warning at `:604-611` is also provenance-blind: capture marks are taken before object load (`:451-452,456-459`), `ErrorsHere` spans loading, save hooks, serialization, the save and retry (`:550-551`), and the condition checks only a clean package, the editor path, `UWidgetBlueprint` and `ErrorsHere > 0`. It does not check whether a draw ran or whether the renderer raised the errors. No current test forces failure through either save tool on the owned `WBP_NodeTest`, controls fresh/cached-thumbnail state, or supplies a controlled non-thumbnail error, genuine thumbnail error and zero-error control; the success tests cannot distinguish the editor path from an observed draw.
- **`R04-SAVE-07` (confirmed source; both).** There is no read-only preflight and no structured read-only failure cause. `WriteRefusalReason` checks only the editor and PIE/simulate (`:50-74`); the VCS plan checks file existence and provider state, not `IFileManager::IsReadOnly` (`TurboMCPSourceControl.cpp:269-317`; `TurboMCPSourceControlWrite.h:96-153`). An initially read-only package is not unconditionally attempted twice: a clean package is skipped at `:432-437`, checkout may make it writable, and the retry at `:539-545` occurs only while `Package->IsDirty()` after the first `SaveOnce`. For a dirty eligible package that remains dirty after the first attempt, the failure text says "may be read only, or another program may hold the file" (`:563-579`) without probing the attribute. The source confirms the missing preflight and speculative reason, not a live read-only failure; the remaining file attribute and provider outcome are unverified. `Tests/acceptance.json:981-994` (F08) is `not_mapped`.
- **`R04-SAVE-08` (contract gap; both).** "Saved" means the dirty flag cleared, not verified persistence. Success is `Package->IsDirty() == false` (`:548-558`); `UPackage::SavePackage` clears dirty only after `ESavePackageResult::Success` (`SavePackage2.cpp:3599-3607`), which is an in-process engine success signal, not a file readback. Neither tool reloads or compares the package. `test-struct.py:287-309` checks file existence/bytes; `test-save-thumbnail-option.py:210-252` checks the dirty list. `Tests/acceptance.json:967-980` (F07) is `not_mapped`.

`R04-CALL-04` and `R04-CALL-06` remain applicable: both tools return plain `FString`, `call_structured` would require ARC02 metadata unless `requires_result=False`, and a body `error:`/refusal is a successful value to the recorder (`bError=false`). `Tools/tmcp.py` treats `Edit*` as a mutation, so a lost response is reported as unknown and is not replayed.

### Bounded repair proposals

All proposals are unapproved. No production, schema, test or fixture change is made by this checkpoint.

#### `R04-REPAIR-20` — limit preflight to write-eligible inputs

- **Finding / owner:** `R04-SAVE-01`; owner R04.
- **Exact files:** `Plugins/TurboMCP/Source/TurboMCP/TurboMCPCompile.cpp`; `Plugins/TurboMCP/Source/TurboMCP/TurboMCPSourceControlWrite.cpp`; `Tools/test-vcs-write.py`; `Tools/test-save-thumbnail-option.py`.
- **Preserved:** empty/separator and >25-part refusals; PIE/simulate refusal before provider; per-asset provider rows; checkout is report-only and non-gating; Git no-op; duplicate policy unresolved.
- **Synthetic probe:** provider counting `GetState`/`FCheckOut`; save dirty, clean, duplicate, unloadable and non-`/Game/` inputs.
- **Expected:** preflight runs only for inputs that reach a write attempt; no checkout for clean/refused/unloadable inputs; any diagnostic state read is labeled and issues no checkout; a writable target still saves after checkout failure.
- **Negative control:** dirty normal asset still gets its per-asset checkout; Git no-op still repeats the provider refusal; checkout failure still does not fail a writable save.
- **Fixture gap:** no provider operation counter or clean/non-`/Game/`/unloadable/duplicate save fixture.
- **Validation:** proposed `test-vcs-write.py`/save-suite extension; no current provider-count assertion.
- **Rollback:** revert the two source files and two tests.

#### `R04-REPAIR-21` — account for E-SAVE's engine checkout

- **Finding / owner:** `R04-SAVE-02`; owner R04.
- **Exact files:** `Plugins/TurboMCP/Source/TurboMCP/TurboMCPCompile.cpp`; `Tools/test-vcs-write.py`; `Tools/test-save-thumbnail-option.py`.
- **Preserved:** E-SAVE keeps `SaveLoadedAsset`/thumbnail path; preflight remains non-gating; direct path keeps its preflight; Git/unavailable wording.
- **Synthetic probe:** count `FCheckOut`/`CheckoutPackages` for E-SAVE and no-thumbnail on an existing dirty package.
- **Expected:** result/test account for the engine's second checkout pass, or only one path is used; no single-checkout implication; new files still marked for add.
- **Negative control:** direct path one preflight; no-provider none; clean input no save.
- **Fixture gap:** no provider operation counter/live provider.
- **Validation:** proposed provider-counter test.
- **Rollback:** revert the compile file and two tests.

#### `R04-REPAIR-22` — support or refuse direct-write package kinds

- **Finding / owner:** `R04-SAVE-03`; owner R04.
- **Exact files:** `Plugins/TurboMCP/Source/TurboMCP/TurboMCPCompile.cpp`; `Tools/test-save-thumbnail-option.py`; `Tools/test-vcs-write.py`.
- **Preserved:** normal `/Game/*.uasset` Blueprint/widget path; non-`/Game/` refusal; direct `SavePackage` retry; non-gating checkout.
- **Synthetic probe:** World under `/Game/Maps`; external package; new asset; loaded unregistered asset.
- **Expected:** support map extension, external-package save and registered-asset semantics, or refuse those kinds with a stable reason before writing; no "same save minus thumbnail" claim otherwise.
- **Negative control:** normal widget still saves/skips thumbnail; non-`/Game/` still refuses without writing.
- **Fixture gap:** no map/external/loaded-unregistered/new-file fixture.
- **Validation:** proposed save-suite extension; no current map/external assertion.
- **Rollback:** revert the compile file and two tests.

#### `R04-REPAIR-23` — describe direct-write code execution honestly

- **Finding / owner:** `R04-SAVE-04`; owner R04.
- **Exact files:** `Plugins/TurboMCP/Source/TurboMCP/Public/Tools/TurboMCPToolsets.h`; `Plugins/TurboMCP/Source/TurboMCP/TurboMCPCompile.cpp`; `Tools/test-save-thumbnail-option.py`.
- **Preserved:** direct write/retry; no editor thumbnail render; `/Game/` refusal; success result remains distinguishable.
- **Synthetic probe:** Widget Blueprint and an asset with observable `PreSave`/`PreSaveRoot`; direct save vs E-SAVE.
- **Expected:** help/result say the thumbnail renderer is skipped but `PreSave`/`PreSaveRoot`/`PostSave`/serialization can still run; no code-free claim.
- **Negative control:** E-SAVE still names thumbnail/design-time possibility; plain asset still saves.
- **Fixture gap:** no observable `PreSave` fixture.
- **Validation:** proposed focused test seam; no current code-execution assertion.
- **Rollback:** revert the header, compile file and save-suite test.

#### `R04-REPAIR-24` — report package-level outcomes

- **Finding / owner:** `R04-SAVE-05`; owner R04.
- **Exact files:** `Plugins/TurboMCP/Source/TurboMCP/TurboMCPCompile.cpp`; `Tools/test-save-thumbnail-option.py`; `Tools/test-vcs-write.py`.
- **Preserved:** per-input order/resolve failures; a clean package is skipped before the save path and reported as no changes; a dirty single-asset package with a successful save still reports `1 saved`; one retry; duplicate policy unresolved; E-SAVE external/direct behavior.
- **Synthetic probe:** two assets in one package; duplicate input; external package; mixed success/failure.
- **Expected:** saved/failed/unattempted package results with asset mapping; duplicates do not inflate package outcomes; partial is explicit; a clean package remains no save/no change and a dirty successful single-asset package remains `1 saved`.
- **Negative control:** two controls: a clean single-asset package is skipped before the save path and reports no changes, not `1 saved`; a dirty single-asset package with a successful save reports `1 saved`. Input order and per-input error text are preserved.
- **Fixture gap:** no multi-asset/external-package fixture; suites save one asset per call.
- **Validation:** proposed save-suite extension; no current package-level assertion.
- **Rollback:** revert the compile file and two tests.

#### `R04-REPAIR-25` — make failure, draw and error-attribution text truthful

- **Finding / owner:** `R04-SAVE-06`; owner R04.
- **Exact files:** `Plugins/TurboMCP/Source/TurboMCP/TurboMCPCompile.cpp`; `Tools/test-save-thumbnail-option.py`.
- **Preserved:** E-SAVE keeps the editor save path and `EditTurboMCPSaveWithoutThumbnail` keeps the direct `SavePackage` path; one retry only while dirty after the first `SaveOnce`; save/failure counts and per-input error rows; successful package-write and refusal semantics; a warning count is still surfaced without pasting log text; truthful success semantics, with correction of misleading draw/provenance wording explicitly permitted.
- **Synthetic probe:** proposed test-only seams on the owned `WBP_NodeTest` Widget Blueprint; none of the failure/cache/error controls exists. Cases: (1) force a failed save through **both** `EditTurboMCPSave` and `EditTurboMCPSaveWithoutThumbnail`; (2) E-SAVE with a fresh/cached thumbnail, knowing whether a draw was requested; (3) a successful widget save with a controlled non-thumbnail error in the `:451-551` interval; (4) a successful widget save with a genuine thumbnail/design-time error; (5) a zero-error control. Record the text and whether provenance is actually known. No runtime result or implementation-feasibility claim is made.
- **Expected:** proposed policy: a failed save never says the package was written; the direct-save success sentence (`written directly`/without the thumbnail draw) must not appear on failure, and E-SAVE's `written by the editor's save` success phrase must not appear on failure. For a fresh/cached-thumbnail E-SAVE save, the text distinguishes selecting the editor save path from observing an actual thumbnail draw and does not assert a draw without evidence. A successful no-thumbnail save still says the draw was skipped. Warning text does not attribute an error to thumbnail/design-time work unless provenance was observed; otherwise it says only that engine errors were raised during this asset's save. Zero errors emit no warning. Proposed policy, not current behavior.
- **Negative control:** both failure probes must avoid package-written/direct-success wording; zero-error control emits no warning; controlled non-thumbnail error is not labeled thumbnail/design-time work; genuine thumbnail error is named only when the seam establishes provenance; fresh/cached E-SAVE still says the editor path was selected without inventing a draw; successful no-thumbnail widget save still says the draw was skipped.
- **Fixture gap:** `WBP_NodeTest` is an owned success fixture, but there is no controlled failure seam for either tool, fresh/cached-thumbnail state, non-thumbnail error, genuine thumbnail error or zero-error control. These seams are proposed, not present; implementation feasibility is not established.
- **Validation:** proposed save-suite coverage for failure through both tools, fresh/cached E-SAVE wording, non-thumbnail/thumbnail/zero-error controls, no package-written/direct-success phrase on failure, and no unsupported draw/provenance claim. No current assertion covers these cases.
- **Rollback:** revert the compile file and save-suite test.

#### `R04-REPAIR-26` — explicit read-only handling without gating checkout

- **Finding / owner:** `R04-SAVE-07`; owner R04.
- **Exact files:** `Plugins/TurboMCP/Source/TurboMCP/TurboMCPCompile.cpp`; `Tools/test-vcs-write.py`; `Tools/test-save-thumbnail-option.py`.
- **Preserved:** checkout report-only; writable save survives checkout refusal; clean package skipped; checkout may make an initially read-only file writable; retry only while dirty after the first `SaveOnce`; existing other-failure counts.
- **Synthetic probe:** dirty eligible package initially read-only and still dirty after the first attempt: no provider, failing checkout, checkout that succeeds but leaves the attribute unverified, checkout that clears read-only; clean read-only and writable controls. Record the actual attribute/provider outcome, not an assumption.
- **Expected:** proposed policy: after checkout, re-probe local read-only state. For a dirty eligible package, if still read-only, report one explicit read-only failure and do not make a second blind write attempt; if writable, save; if clean, skip. Provider failure alone still does not gate a writable save. Proposed policy, not current behavior.
- **Negative control:** clean read-only package skips/no changes; writable control saves; initially read-only package whose checkout clears the attribute saves; provider refusal on a writable file still does not fail the save. Current source does not establish a live read-only failure.
- **Fixture gap:** no read-only fixture or provider that clears read-only; F08 not mapped; current suites cover no-provider and Git only; remaining attribute/provider outcome unverified.
- **Validation:** proposed read-only/provider tests with recorded attribute and provider results.
- **Rollback:** revert the compile file and two tests.

#### `R04-REPAIR-27` — verify durable persistence (F07)

- **Finding / owner:** `R04-SAVE-08`; owner R04.
- **Exact files:** `Plugins/TurboMCP/Source/TurboMCP/TurboMCPCompile.cpp`; `Tools/test-struct.py`; `Tools/test-save-thumbnail-option.py`; `Tests/acceptance.json`.
- **Preserved:** dirty-flag verdict/retry; no automatic reload over unsaved state; existing file-byte/dirty-list assertions; failure remains failure.
- **Synthetic probe:** save owned fixture, unload, reload, compare fields; forced failure; clean no-op.
- **Expected:** focused F07 reload/compare proves durable persistence; tool text says engine write accepted without claiming verified persistence until readback; failed save never says saved.
- **Negative control:** read-only/failed save still fails; clean fixture still no changes; existing byte checks pass.
- **Fixture gap:** F07 not mapped; no reload test.
- **Validation:** proposed test/acceptance mapping; no engine run.
- **Rollback:** revert the compile file, two tests and acceptance mapping.

### Existing assertions vs unrun probes

- `Tools/test-save-thumbnail-option.py` checks the one-parameter required schema (`:145-159`), a dirty Blueprint save with `1 saved`/editor mechanism (`:175-181`), a widget save and thumbnail design-time text (`:187-197`), a 14-second dirty-list window (`:200-216`), a no-thumbnail widget save (`:223-236`) and its dirty list (`:239-252`). Its header explicitly says no thumbnail draw can be observed and the non-`/Game/` refusal is not exercised. It does not force a failure through either save tool on `WBP_NodeTest` or control fresh/cached-thumbnail state.
- `Tools/test-vcs-write.py` uses E-SAVE only: a brand-new asset with no provider (`:423-449`), file-byte/field checks (`:500-508,623-636`), empty-assets VCS read (`:401-402`) and static `EnsureCheckedOut` checks (`:713-732`). It does not exercise the no-thumbnail tool, a live checkout, read-only files, clean assets or provider operation counts.
- `Tools/test-struct.py:287-309` checks `1 saved`, file existence and saved field bytes; it does not unload/reload.
- `Tests/acceptance.json:967-994` records F07 (reload persistence) and F08 (read-only save refusal) as `not_mapped`.

Unrun probes: omitted/empty/separator/duplicate/malformed save input; 25-part boundary; clean/refused/unloadable preflight; provider checkout pass; initially read-only dirty/eligible save with recorded final attribute and provider result; map/World, external package, loaded unregistered asset and new-file mark-for-add; failure through both save tools on owned `WBP_NodeTest`; failure/partial/widget failure text; fresh/cached-thumbnail E-SAVE wording; controlled non-thumbnail/thumbnail/zero-error warning provenance; multi-asset package counts; durable unload/reload. No engine suite, editor, build, direct MCP, packaging or recovery command was run. All existing suites are assertions only.

### Checkpoint 4 validation

| Command | Result |
| --- | --- |
| `python Tools/audit-inventory.py --check` | exit 0; 101 definitions; 95 `parsed_call`, 6 `name_reference_only` |
| `python Tools/test-result-contract.py` | exit 0; PASS (C03–C08) |
| `python Tools/test-client-retry.py` | exit 0; PASS (C01/C02) |
| `python Tools/execute-tests.py --profile offline --execute` | exit 0 on the pre-commit correction tree (request commit `489268b`): report `.local/executions/2517d7617b934fc69e7038b279ccf17e/report.json`, tree `b3964e03…`; `Tools/validate.py` passed. Candidate-bound evidence is obtained after this correction commit and returned separately. |

No engine suite, editor, build, direct MCP, packaging or recovery command was run. `test-save-thumbnail-option.py`, `test-vcs-write.py`, `test-struct.py` and the acceptance catalog are existing assertions only, not results for this checkpoint.

### Checkpoint 4 unresolved questions

- Whether preflight should move after dirty/mechanism eligibility or retain a labeled state read.
- Whether E-SAVE's engine checkout pass should be suppressed or reported; both change a safety/observability boundary.
- Whether `EditTurboMCPSaveWithoutThumbnail` should support maps/external packages or refuse them; no asset-kind scope is approved.
- Whether the direct save can honestly promise any project-code-free behavior.
- How to report per-package outcomes without a new public schema.
- Whether read-only is a preflight refusal or an explicit post-checkout failure code; D-57's non-gating rule applies to writable targets.
- The source does not establish a live read-only failure or the post-checkout attribute/provider outcome; only the missing plugin-side preflight and speculative failure text are confirmed.
- Whether the E-SAVE `:604-611` warning can observe renderer provenance at all, or must be reworded as a save-interval error count; no controlled error seam exists.
- How to verify durable persistence without automatic reload over unsaved state.
- `R04-CALL-04`/`R04-CALL-06` result/recorder gaps remain; no ARC02 line is added.
- Resolver route caveats from `R04-VCS-07` apply; no top-level asset check is added, and no save-specific subobject fixture exists.
- The 89 unreviewed rows remain unreviewed; R04 is incomplete.

## Checkpoint 5: compile contract for Monday widgets

Checkpoint 5 is source/offline evidence at base `a92cae75abd5c4a8e65179fa8b1ad32f5aef436f`; it does not certify a live compile, generated class, package write or reopen. Reviewed tool: `EditTurboMCPCompile`. Claim `88f668a66d8740acae6454d27e162c28` (`dsh-r04-compile-01`) was inspected in this worktree; the claim store SHA-256 was `929824eba2c75c020eb20d3ca00fabe2d27efd27cbfc7a27f287aaaa2f34acd3`.

### Reviewed contract

| Item | Source and behavior |
| --- | --- |
| Declaration | `TurboMCPToolsets.h:1577-1578`; one required `FString` `Assets`, no C++ default. Omitted is rejected before the body; explicit empty reaches the body and is the no-asset error. |
| Input parsing | `TurboMCPCompile.cpp:143-155` uses the shared `SplitList`: comma/pipe separators, trim, cull empties, preserve order, no dedup, 25-part cap. A malformed/resolve/load/non-Blueprint part is refused per row; other parts continue. A duplicate compiles the asset again. |
| Compile call | `:220-225` calls `FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::None, &CompileLog)`. The tool does not pass `SkipSave`; it does not create a transaction or call `Modify`. |
| Result classification | `:227-270`: errors are `CompileLog.NumErrors`; compiler warnings and captured engine messages select the warning branch. Captured engine errors do not increment `ErrorCount`. `Blueprint->Status` is not read. |
| Dirty/saved text | `:272-277` prints `saved` or `not saved yet` from `HasUnsavedChanges`, which is only `Package->IsDirty()` (`:136-140`). |
| Messages | `:279-298` prints up to six compiler messages and separately summarizes captured engine warnings/errors. |
| Summary | `:301-351` prints clean/warning/error/refused counts and `changed: compiled N of M` whenever `CleanCount+WarningCount+ErrorCount > 0`; all-error batches still take that headline. |
| Wrapper/client | `FRecordingToolset` (`TurboMCPCallRecorder.cpp:103-130`) records start/end when logging is on. No ARC02 line; `call_structured` would require one unless `requires_result=False`. Body `error:`/refusal is a value, so `bError=false` (`R04-CALL-06`). `Edit*` is a client mutation; a lost response is unknown and is not replayed. |

### Compile effects and result limits

- **Class/status:** `CompileBlueprint` forwards to `FBlueprintCompilationManager::CompileSynchronously` (`Kismet2.cpp:798-807`). The manager sets `BS_UpToDate`, `BS_UpToDateWithWarnings` or `BS_Error` (`BlueprintCompilationManager.cpp:1660,1675,1853-1859`) and may reinstance classes/dependent instances, broadcast compile/reinstance events, and collect garbage (`:342-491,1702-1722`). The tool reads none of that status or event data.
- **Widget generated class:** `FWidgetBlueprintCompiler::CanCompile` accepts any `UWidgetBlueprint` (`WidgetBlueprintCompiler.cpp:147-150`). Compilation duplicates/replaces the generated widget tree (`:844-938`), sanitises editor bindings and emits runtime bindings (`:711-747,1044-1053`), clones animations, rebuilds named slots, and runs `UWidgetCompilerRule::ExecuteRule` plus extension callbacks (`:705-708,1103-1107`). These are project-supplied hooks, not merely data formatting.
- **Possible package write:** `CompileSynchronouslyImpl` adds compiled packages to `PackagesToSave` when `UBlueprintEditorSettings::SaveOnCompile` is `Always` or `SuccessOnly`, then calls `FEditorFileUtils::PromptForCheckoutAndSave(..., bCheckDirty=true)` (`BlueprintCompilationManager.cpp:458-490`). The enum default is `SoC_Never` (`BlueprintEditorSettings.h:41-45`), with a legacy `bSaveOnCompile` migration to `SuccessOnly` (`BlueprintEditorSettings.cpp:100-105`). The project's effective setting was not found in the worktree; no auto-save is assumed.
- **Dirty reset:** the manager records `bPackageWasDirty` before compiling and restores that flag afterward (`BlueprintCompilationManager.cpp:524-538,1904-1907`). Source-side widget binding/graph work can therefore coexist with a clean package flag; the tool's `saved` text is not proof that the compiler's in-memory source edits reached disk.
- **No transaction:** neither the tool nor the compiler path begins an undo transaction for compiler-side source edits. The Edit toolset's one-undo contract is not met by this implementation path.
- **Common UI/Input:** the source proves only `UBlueprint`/`UWidgetBlueprint` acceptance. No fixture tests Common UI or Input; inheritance is not evidence of support.

### Findings

- **`R04-COMPILE-01` (confirmed source, conditional; compile).** Compile can trigger an unreported package write or source-control checkout through `SaveOnCompile`. The tool passes `EBlueprintCompileOptions::None` (`TurboMCPCompile.cpp:225`), so `CompileSynchronouslyImpl` may build `PackagesToSave` and call `PromptForCheckoutAndSave` (`BlueprintCompilationManager.cpp:458-490`). No test controls the setting; the effective project value is unverified. Compile-only and compile+save are not distinguished.
- **`R04-COMPILE-02` (confirmed source; compile).** Widget compile can mutate source-side bindings/graphs while restoring the prior dirty flag. `CreateFunctionList` can create `__Get` graphs and update `EditorBinding.FunctionName` (`WidgetBlueprintCompiler.cpp:238-298`); `SanitizeBindings` removes stale entries and changes `PropertyBindings` (`:711-747`); the manager restores `bPackageWasDirty` (`BlueprintCompilationManager.cpp:1904-1907`). The tool then reads that flag for `saved`. No transaction/rollback exists for these compiler-side source changes.
- **`R04-COMPILE-03` (confirmed source; compile).** Compiler failures and engine diagnostics are not separately named; `Blueprint->Status` is unread. `bHasErrors` is only `CompileLog.NumErrors` (`TurboMCPCompile.cpp:227`). `bHasWarnings` combines compiler warnings and captured engine warnings; `bEngineLoggedSomething` includes captured engine errors, but `ErrorCount` counts compiler-error assets only and `WarningCount` can mean compiler warnings, engine warnings, engine errors or a mix (`:236-254`). The tool deliberately prints a distinct `not clean, the compiler reported nothing but the engine log did` status for engine-only diagnostics (`:264-270`) and separate engine warning/error counts (`:286-298`); it does not declare compiler failure for every engine `Error`. The summary label `with errors` counts compiler-error assets, not engine diagnostics (`:301-329`). The existing ensure-handling contract must remain. Whether every engine-log Error should become a compile failure is an explicit owner decision; status-error/empty-log consistency is a separate unproven probe.
- **`R04-COMPILE-04` (confirmed source; compile).** Widget compile has generated-class, binding, reinstancing and project-code effects beyond the result text. The compiler duplicates the widget tree, writes runtime bindings and named slots, runs custom widget compiler rules/extensions and may reinstance live classes/instances (`WidgetBlueprintCompiler.cpp:705-708,844-938,1006-1107`; `BlueprintCompilationManager.cpp:1702-1722`). The tool reports only counts and dirty text; no existing test compiles `WBP_NodeTest` or observes these effects.
- **`R04-COMPILE-05` (contract gap; compile).** Common UI and Input support is unverified and cannot be inferred from `UBlueprint`/`UWidgetBlueprint` inheritance. The plugin accepts any `UBlueprint` (`TurboMCPCompile.cpp:76-81,206-215`); the widget compiler accepts any `UWidgetBlueprint` (`WidgetBlueprintCompiler.cpp:147-150`). `WBP_NodeTest` is plain UMG; there is no plugin-specific fixture or compile test. The error text "Only Blueprints and widgets compile" is broader than the tested path.
- **`R04-COMPILE-06` (confirmed source; compile).** Partial and all-error batches are summarized as `changed: compiled N of M`. `CompiledCount` includes `ErrorCount` (`TurboMCPCompile.cpp:301-322`); errors continue the loop and are counted alongside clean attempts. There is no aggregate success/failure/partial outcome and no distinction between attempted, succeeded, warned, errored and refused in one machine-readable result.
- **`R04-COMPILE-07` (contract gap; compile).** Compile success, package write and durable persistence are not separately established. `saved`/`not saved yet` is `Package->IsDirty()` (`:272-277`); compile may auto-save under `SaveOnCompile` or restore the pre-compile dirty flag (`BlueprintCompilationManager.cpp:458-490,1904-1907`). The tool never reloads, hashes or compares the package. No ARC02 outcome/code/operation id exists. The proposed restart is also not expressible by the current suite helpers: `run-suites.ps1:643` sets `TURBOMCP_RUNNER_OWNS_FIXTURES=1`; `tmcp_editor.py:81-82` makes `stop_editor` a successful no-op, `:203-217` makes `ensure_editor` reuse an answering server, and `start-editor.ps1:130-131` refuses a direct start under the runner. A supervisor-owned restart is an implementation prerequisite, not an existing capability.

### Bounded repair proposals

All proposals are unapproved and do not authorize implementation or a new test hook.

#### `R04-REPAIR-28` — prevent or report hidden compile-time package writes

- **Finding / owner:** `R04-COMPILE-01`; owner R04.
- **Exact files:** `Plugins/TurboMCP/Source/TurboMCP/TurboMCPCompile.cpp`; `Tools/test-widget-compile-save-reopen.py` (proposed new file).
- **Preserved:** compile result/per-asset rows; clean/warning/error/refused text; one attempt per input; explicit save remains the write operation.
- **Synthetic probe:** set `SaveOnCompile` to Never and SuccessOnly in a disposable fixture; compile `WBP_NodeTest`; record dirty flag, file hash and source-control operations.
- **Expected:** compile passes `SkipSave` and never writes, or reports the package write/checkout explicitly; compile-only and compile+save are distinct; the effective setting is recorded.
- **Negative control:** Never produces no file/source-control change; explicit `EditTurboMCPSave` still writes; a clean no-op compile does not claim a write.
- **Fixture gap:** no setting control, hash-around-compile or source-control observer; proposed test file does not exist.
- **Validation:** proposed setting/hash test; no current assertion.
- **Rollback:** revert the compile file and proposed test only.

#### `R04-REPAIR-29` — report compiler-side source changes and dirty state truthfully

- **Finding / owner:** `R04-COMPILE-02`; owner R04.
- **Exact files:** `Plugins/TurboMCP/Source/TurboMCP/TurboMCPCompile.cpp`; `Tools/test-widget-compile-save-reopen.py`.
- **Preserved:** compile result; explicit save; ordinary package dirty semantics; no automatic project rollback.
- **Synthetic probe:** compile `WBP_NodeTest` with a property binding/stale binding; snapshot `Bindings`, `PropertyBindings`, generated class and dirty state before/after.
- **Expected:** compiler-side source changes are reported and left dirty for explicit save, or restored under a documented transaction; `saved` is never based only on a restored pre-compile dirty flag.
- **Negative control:** a clean compile with no binding mutation remains no-change; explicit save writes; plain BP control unaffected.
- **Fixture gap:** no binding/stale-binding WBP fixture or binding reader; proposed test file does not exist.
- **Validation:** proposed binding/dirty test; no current assertion.
- **Rollback:** revert the compile file and proposed test only.

#### `R04-REPAIR-30` — name compiler failures and engine diagnostics separately

- **Finding / owner:** `R04-COMPILE-03`; owner R04.
- **Exact files:** `Plugins/TurboMCP/Source/TurboMCP/TurboMCPCompile.cpp`; `Tools/test-compile-ensure.py`.
- **Preserved:** compiler errors remain compiler failures; engine-log-only errors remain `not clean` and are not silently called compiler success; separate compiler message and captured engine warning/error text/counts; per-asset clean/warning/error shape and six-message bound; the existing ensure-handling contract and `test-compile-ensure` checks.
- **Synthetic probe:** compile disposable fixtures with a compiler warning, a compiler error, an engine-log-only Error with no compiler error, a clean no-message asset and, if producible, a Blueprint status error with an empty compiler log. Record `CompilerErrorAssets`, `CompilerWarningAssets`, engine diagnostic warnings/errors and `Blueprint->Status` separately.
- **Expected:** the report names each count and its unit: compiler-error assets (`CompileLog.NumErrors > 0`), compiler-warning assets, engine-diagnostic warnings/errors, refused assets and Blueprint status. An engine-only Error stays `not clean` but is not called a compiler failure; whether every engine Error becomes a compile failure requires an explicit owner decision. A status-error/empty-log case is a separate probe.
- **Negative control:** compiler error remains compiler error; warning-only remains warning; clean remains clean/zero errors; engine-only Error remains not clean without entering the compiler-error count; the ensure fixture still records its engine diagnostics.
- **Fixture gap:** no engine-only, compiler-error, warning-only or status-error/empty-log fixture exists. `test-compile-ensure` covers ensure shape, not these count units.
- **Validation:** proposed `test-compile-ensure`/error-matrix extension; no current assertion distinguishes compiler-error assets from engine-diagnostic counts.
- **Rollback:** revert `TurboMCPCompile.cpp` and `test-compile-ensure.py` only.

#### `R04-REPAIR-31` — cover Widget Blueprint generated-class and binding effects

- **Finding / owner:** `R04-COMPILE-04`; owner R04.
- **Exact files:** `Plugins/TurboMCP/Source/TurboMCP/TurboMCPCompile.cpp`; `Tools/test-widget-compile-save-reopen.py`; `Tools/test-widget-tree.py`.
- **Preserved:** plain WBP compile/result; generated class creation; source widget tree; existing WBP save/read behavior.
- **Synthetic probe:** compile `WBP_NodeTest`; read source tree with `ReadTurboMCPWidgetTree`, generated class identity with `ReadTurboMCPAssetDescribe`, and a controlled binding/tree change; compare before/after and check live instances.
- **Expected:** report names generated class and binding effects where a reader exists and records unobservable binding/reinstancing effects as missing; project compiler-rule execution is not inferred as safe.
- **Negative control:** `BP_NodeTest` compile still reports its status; unchanged WBP tree text matches.
- **Fixture gap:** no WBP compile test, binding reader, custom rule fixture or live-instance observer; proposed test file does not exist.
- **Validation:** proposed WBP compile/save/reopen suite; no current WBP compile assertion.
- **Rollback:** revert the compile file, proposed test and any `test-widget-tree.py` addition only.

#### `R04-REPAIR-32` — state the Common UI/Input support boundary

- **Finding / owner:** `R04-COMPILE-05`; owner R04.
- **Exact files:** `Plugins/TurboMCP/Source/TurboMCP/TurboMCPCompile.cpp`; `Tools/test-widget-compile-save-reopen.py`.
- **Preserved:** plain UMG WBP compile; generic Blueprint acceptance; no new authoring feature.
- **Synthetic probe:** compile plain `WBP_NodeTest`, then an approved Common UI or Input widget fixture if one exists; do not infer support from inheritance.
- **Expected:** claims only the tested plain UMG path; Common UI/Input is documented unsupported or unverified until an owned fixture/test exists; a stable refusal is preferable to an inheritance-based promise.
- **Negative control:** plain WBP still compiles; a non-widget BP follows its actual compiler.
- **Fixture gap:** no Common UI/Input asset in tracked fixtures; proposed test file and approved plugin-specific fixture do not exist.
- **Validation:** proposed fixture/test only; no current assertion.
- **Rollback:** revert the compile wording/guard and proposed test only.

#### `R04-REPAIR-33` — distinguish partial and all-error batches

- **Finding / owner:** `R04-COMPILE-06`; owner R04.
- **Exact files:** `Plugins/TurboMCP/Source/TurboMCP/TurboMCPCompile.cpp`; `Tools/test-compile-ensure.py`.
- **Preserved:** per-asset lines/messages; refused rows; clean/warning/error counts; continue after a failure.
- **Synthetic probe:** compile `WBP_NodeTest`/`BP_NodeTest` with a compiler-error asset and an invalid name; add all-error and duplicate batches.
- **Expected:** headline/counts distinguish attempted, clean, warning, error and refused; error compiles are not successful "compiled" assets; mixed batches have a partial outcome; all-refused is unchanged and all-error is not success.
- **Negative control:** single clean asset remains one compiled/clean; unknown name refuses; duplicates remain visible pending policy.
- **Fixture gap:** no compiler-error asset or mixed/all-error batch fixture; duplicates/malformed batches untested.
- **Validation:** proposed `test-compile-ensure` extension; no current partial/error headline assertion.
- **Rollback:** revert the compile file and test only.

#### `R04-REPAIR-34` — separate compile, explicit save and durable reopen

- **Finding / owner:** `R04-COMPILE-07`; owner R04.
- **Exact files:** `Plugins/TurboMCP/Source/TurboMCP/TurboMCPCompile.cpp`; `Tools/test-widget-compile-save-reopen.py` (proposed); `Tools/test-widget-tree.py`; `Tools/test-widgets-as-vars.py`; `Tools/run-suites.ps1`; `Tools/tmcp_editor.py`; `Tools/start-editor.ps1`; `Tools/check-mcp-server.py`.
- **Preserved:** compile status/dirty text; explicit save; no automatic reload over unsaved editor state; existing WBP read/save semantics; supervisor ownership of the editor, fixture snapshot/restore and unrelated editors/unsaved state; reversible `IsVariable` edit through the existing `EditTurboMCPWidgetsAsVars`.
- **Synthetic probe:** two trials on restored `WBP_NodeTest`. Trial A: read baseline tree; `EditTurboMCPWidgetsAsVars` sets `TitleText=true`; compile; explicit save; supervisor-owned restart with old-PID-dead/new-PID-alive/server-owner proof; `ReadTurboMCPWidgetTree` must contain a `(widget TitleText ...)` line carrying `:variable`, and `ReadTurboMCPBlueprintLoad` must report `TitleText` variable. Trial B: same edit and compile with `SaveOnCompile` observed as Never, no save; the same restart must show `TitleText` not marked. File hashes are supplementary only.
- **Expected:** a reversible, observable edit persists after a verified new editor read the package from disk; an otherwise identical unsaved control loses it. Compile success, explicit package write and durable reopen are separate claims. A dirty package with unchanged serialized content need not change its hash; hash inequality is not required. The tool result does not call a dirty flag durable proof.
- **Negative control:** the no-save trial loses the `TitleText` variable flag after restart; the saved trial retains it. If `SaveOnCompile` is not demonstrably Never, the control is invalid and must be reported blocked, not passed. A failed compile/save never claims persistence; a clean no-op remains no changes.
- **Fixture gap:** `WBP_NodeTest`/`TitleText` and the existing `EditTurboMCPWidgetsAsVars`, `ReadTurboMCPWidgetTree` and `ReadTurboMCPBlueprintLoad` path exist; missing are a supervisor-owned restart request, observed `SaveOnCompile=Never`, and the proposed test file. No per-asset reload MCP tool or generated-class/binding reader exists.
- **Implementation prerequisite:** owner integration-owner/R02 supervisor scope; files `Tools/run-suites.ps1`, `Tools/tmcp_editor.py`, `Tools/start-editor.ps1`, `Tools/check-mcp-server.py`. The runner must accept a per-suite restart request, stop only its owned editor, prove the old PID exited, start and record a new owned PID, and pass `check-mcp-server.py --editor-pid` with project/engine identity. A suite must not call `stop_editor`/`ensure_editor`, clear `TURBOMCP_RUNNER_OWNS_FIXTURES`, or invoke `start-editor` directly.
- **Validation:** proposed dual-trial WBP compile/save/restart test with old/new PID and server-owner proof; no current compile persistence assertion.
- **Rollback:** revert the compile file, proposed test, any `test-widget-tree.py`/`test-widgets-as-vars.py` addition and the supervisor prerequisite files only.

### Minimal proposed Widget Blueprint compile -> save -> reopen plan

This is a proposed acceptance sequence, not an executed result. It uses only existing owned fixtures and tools; the new test file is proposed, not present. The reopen step is blocked on the supervisor-owned restart prerequisite below: the current `tmcp_editor.stop_editor`/`ensure_editor` pair cannot force a new editor under the suite runner.

| Step | Existing fixture/tool | Assertion |
| --- | --- | --- |
| 1 | `/Game/Tests/WBP_NodeTest` (`TestProject/Content/Tests/WBP_NodeTest.uasset`); `ReadTurboMCPWidgetTree`; `ReadTurboMCPAssetDescribe`; `ReadTurboMCPBlueprintLoad` | Snapshot/restore the owned fixture; record the baseline; confirm `TitleText` is not a variable and record loaded/dirty/on-disk fields. |
| 2 | `EditTurboMCPWidgetsAsVars` with `asset=WBP_NodeTest`, `widgets=TitleText`, `bIsVariable=true`; `ReadTurboMCPWidgetTree`; `ReadTurboMCPBlueprintLoad` | Apply a reversible, observable edit; require a `(widget TitleText ...)` line carrying `:variable` and `ReadTurboMCPBlueprintLoad` reporting `TitleText` variable, plus a dirty package; this is the discriminating change, not a no-op. |
| 3 | `EditTurboMCPCompile` with `assets=/Game/Tests/WBP_NodeTest` | Require one per-asset line, a recognised clean/warning/error verdict, saved/dirty text, and the observed `SaveOnCompile` setting. |
| 4 | `EditTurboMCPSaveWithoutThumbnail` (or `EditTurboMCPSave` only when thumbnail/design-time execution is allowed) | Require one saved/no-change outcome. A file hash is supplementary evidence; a dirty package with unchanged serialized content need not change its hash. |
| 5 | Supervisor-owned restart (new prerequisite, not `tmcp_editor.stop_editor`/`ensure_editor`) | Prove the old owned PID exited, a new owned PID started, and `check-mcp-server.py --editor-pid` passes with project/engine identity. |
| 6 | New `Session.call`; `ReadTurboMCPAssetDescribe`; `ReadTurboMCPWidgetTree`; `ReadTurboMCPBlueprintLoad` | Require loaded/on disk/no unsaved and the `TitleText` variable flag. |
| 7 | Restored baseline; `EditTurboMCPWidgetsAsVars` same edit; compile with `SaveOnCompile` observed as Never; no save; same supervisor restart | Require `TitleText` not marked after reopen. This is the memory-only control. If `SaveOnCompile` is not demonstrably Never, mark the control blocked, not passed. |
| 8 | Both trials | Preserve the old/new PID, server-owner, setting, compile/save text and post-reopen state. Do not use hash inequality as a universal save requirement. |

**Existing assertions:** `Tools/test-save-thumbnail-option.py:92-94,187-197` uses `WBP_NodeTest` for save; `Tools/test-widget-tree.py:28` reads it; `Tools/test-widgets-as-vars.py:27,57-75` applies/reads the `TitleText`/`TitleText2` variable flag through `ReadTurboMCPBlueprintLoad`; `Tools/test-compile-ensure.py:77-79,173` compiles `BP_NodeTest` only; `Tools/test-uncovered-tools.py:138-150` compiles `BP_NodeTest`; `Tools/test-graph-knots.py:286-300` compiles `TurboMCPAcceptance` before/after a knot edit. No current test compiles `WBP_NodeTest` or reopens a saved WBP.

**Missing seams:** no supervisor-owned per-suite restart request (`run-suites.ps1:643` sets `TURBOMCP_RUNNER_OWNS_FIXTURES=1`; `tmcp_editor.py:81-82` no-ops stop; `:203-217` reuses an answering server; `start-editor.ps1:130-131` refuses a direct start under the runner). No controlled `SaveOnCompile=Never` seam exists. No per-asset unload/reload MCP tool and no reader for generated-class bindings/reinstanced instances exist. The proposed test file does not exist.

**Manual fallback:** a supervised operator must ask the runner owner to perform the same restart through the supervisor and supply old-PID-exit, new-PID/server-owner and new-session read evidence. Do not call `start-editor` directly, clear `TURBOMCP_RUNNER_OWNS_FIXTURES`, stop a foreign editor or discard unrelated unsaved state. If the supervisor cannot perform the restart, report the persistence step blocked/unverified.

**Required candidate-bound evidence:** the proposed dual-trial `Tools/test-widget-compile-save-reopen.py` must run against the exact candidate under a supervisor that can restart its owned editor, with an owned `WBP_NodeTest` restore, observed `SaveOnCompile`, request commit/tree digest, old/new PID and server-owner proof, compile/save text, the `TitleText` variable flag after restart and its absence in the no-save control. No such engine evidence exists in this checkpoint; only the offline profile is run.

### Existing assertions vs unrun probes

- `Tools/test-compile-ensure.py` (engine, T-STRUCT-7) compiles `BP_NodeTest` through `EditTurboMCPCompile` and checks shape/saved/verdict and the C++ ensure fixture; it does not compile a Widget Blueprint, inspect generated class/bindings, control `SaveOnCompile`, or reopen.
- `Tools/test-uncovered-tools.py:138-150` compiles `BP_NodeTest` and checks a per-asset line, saved/unsaved text and unknown-name refusal.
- `Tools/test-graph-knots.py:286-300` compiles `TurboMCPAcceptance` before/after a knot edit and compares a substring verdict; no package write/persistence assertion.
- `Tools/test-save-thumbnail-option.py:92-94,187-197` saves `WBP_NodeTest`, not compile.
- `Tools/test-widget-tree.py:28` reads `WBP_NodeTest`, not compile.
- `Tools/test-widgets-as-vars.py:27,57-75` applies and reads the reversible `TitleText`/`TitleText2` variable flag, but does not compile, save/restart or persist it.
- No test covers `EditTurboMCPCompile` omitted/empty/duplicate/malformed input, all-error or mixed batches, widget compiler rules, Common UI/Input, `SaveOnCompile`, generated bindings or reopen.
- `Tools/run-suites.ps1:643` + `Tools/tmcp_editor.py:81-82,203-217` do not expose a per-suite restart request; the proposed supervisor boundary is an implementation prerequisite.

### Checkpoint 5 validation

| Command | Result |
| --- | --- |
| `python Tools/audit-inventory.py --check` | exit 0; 101 definitions; 95 `parsed_call`, 6 `name_reference_only` |
| `python Tools/test-result-contract.py` | exit 0; PASS (C03–C08) |
| `python Tools/test-client-retry.py` | exit 0; PASS (C01/C02) |
| `python Tools/execute-tests.py --profile offline --execute` | exit 0 on the pre-commit correction tree (request commit `5da62367`): report `.local/executions/4c41e7fec10b4c6393c7d65037eb2ad3/report.json`, tree `3cf0270b…`; `Tools/validate.py` passed. Candidate-bound evidence is obtained after this correction commit and returned separately. |

### Checkpoint 5 unresolved questions

- Whether `EditTurboMCPCompile` should pass `EBlueprintCompileOptions::SkipSave` or report auto-save/checkout side effects; the effective `SaveOnCompile` setting is unverified.
- Whether compiler-side widget binding/graph changes should be restored, left dirty, or reported as source changes; no transaction/undo policy is approved.
- How to represent `Blueprint->Status`, compiler-log errors and engine-log errors consistently without a new public schema.
- How to expose or verify Widget Blueprint generated-class/binding/reinstancing effects; no current reader exists.
- Whether Common UI/Input widget classes are supported; no fixture/test exists, and inheritance is not evidence.
- How to define partial/all-error batch outcomes and whether `changed` is ever correct for an error-only compile.
- How to distinguish compile success, auto-save, explicit package write and durable reopen without a new result contract.
- Whether the supervisor owner will implement a runner-owned per-suite restart request and an observed SaveOnCompile seam; without both, the WBP persistence acceptance plan is blocked, not passed.

Correction after review: this commit preserves candidate `5da62367fc473d0edce262a08b66c6da9a7372b9` and addresses request `7074def1-119b-4017-a8aa-163e8ed71ce6` (verified report SHA-256 `44b2d56a520cca97167122a8e29bae722e0e18fb6641a6cface7d1fd9e152502`). The reopen plan now requires a supervisor-owned restart with old-PID-exit, new-PID and server-owner proof; the `tmcp_editor.stop_editor`/`ensure_editor` pair is documented as ineffective under the runner. The persistence plan now uses a reversible `TitleText` `IsVariable` edit and an otherwise identical no-save control with `SaveOnCompile` observed as Never, with hashes only supplementary. `R04-COMPILE-03`/`R04-REPAIR-30` now keep compiler-error assets, compiler-warning assets and engine-diagnostic counts separate; treating every engine Error as a compile failure remains an explicit owner decision. Prior findings and all repairs remain unapproved.

## Integration review

Checkpoint 1 was accepted as a source-only checkpoint at candidate `12d515ac46b58e42e83fffb9b577181fb4e68b55`. That acceptance covered four reviewed tools and 97 not reviewed; all repair rows remain unapproved proposals. The toggle end-after-disable exception and any state/header expansion require an explicit reviewed lifecycle contract before implementation. Checkpoint 2 is accepted at candidate `f10dc74ca4569ca449c0e598fe76abe042a0e7b7`: it adds three source-reviewed rows, for a cumulative seven reviewed and 94 not reviewed. Checkpoint 3 was accepted at candidate `39c229306f1e5d56cf156be9de3b7891af7382e8`: it adds three VCS rows, for ten reviewed and 91 not reviewed. Checkpoint 4 was accepted at candidate `8bd3a449d178bdcc266df9eaf3067e22462719fd`: it adds two save rows, for twelve reviewed and 89 not reviewed. Checkpoint 5 is pending review: it adds the compile row, for a current total of thirteen reviewed and 88 not reviewed. R04 remains incomplete. No runtime outcome or repair is certified.
