# R04 callable contracts — checkpoints 1, 2 and 3

Source/offline investigation only. Checkpoint 1 audits four call-log tools; checkpoint 2 adds status/help/schema; checkpoint 3 adds source-control state and checkout. None completes R04.

- **Checkpoint 1 base:** `1180c6d43655195dc2772b6d1f0b80588e4b466a`
- **Checkpoint 1 claim:** `ab2cf6535c584bc6a9419c79e85d0b04` (`dsh-r04-01`)
- **Checkpoint 2 base:** `b353fdd8b44f405c84ef9a67fde9db679f8c4804`
- **Checkpoint 2 claim:** `350b288bbc2d4f5ab0e408d760bf2dfb` (`dsh-r04-02`)
- **Checkpoint 3 base:** `fc46e01845c4fbd6814c07f5f228d946aeaef6b9`
- **Checkpoint 3 claim:** `c44cc6b13e824f5682e53c43bee3cd5d` (`dsh-r04-vcs-01`)
- **Plugin tree:** `590661ec45c829264ea1743fccea994edb748092`
- **Engine:** 5.8.3 at `396c9f059903aed5fec78ecd3d437a40c6415368`
- **Machine-readable inventory:** [callable-contracts.json](callable-contracts.json)

## Scope and coverage

`docs/audit/capabilities.json` contains **101 unique tool IDs**. Checkpoint 1 created the four-row baseline; checkpoint 2 added three; checkpoint 3 adds the three VCS rows.

| Audit status | Count |
| --- | ---: |
| `reviewed_source` | 10 |
| `not_reviewed` | 91 |
| **Total** | **101** |

Checkpoint 1 snapshot: 4 reviewed/97 not reviewed. Checkpoint 2: 7/94. Checkpoint 3 adds `ReadTurboMCPVcsState`, `PreviewTurboMCPVcsCheckout` and `EditTurboMCPVcsCheckout`. The 91 unreviewed rows retain their capability source/test references and are explicitly marked. No conclusion is implied for them.

Reviewed tools: `ReadTurboMCPCallCost`, `ReadTurboMCPCallLog`, `EditTurboMCPCallLog`, `EditTurboMCPCallLogClear`, `ReadTurboMCPStatus`, `ReadTurboMCPToolHelp`, `ReadTurboMCPToolSchema`, `ReadTurboMCPVcsState`, `PreviewTurboMCPVcsCheckout`, `EditTurboMCPVcsCheckout`.

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

Resolution accepts short names, package/object paths and generated class paths; short names cover `/Game` only. Invalid, ambiguous, multiple-asset-package and missing-file inputs are refused per row. An unsaved package can resolve in memory but has no verified file, so `FileForPackagePath` refuses it (`TurboMCPSourceControl.cpp:54-85`). Files are verified with `FileExists` before a provider is asked. Duplicate inputs remain duplicate rows, duplicate `GetState` calls and potentially duplicate checkout operations.

### Provider and state trace

`Plan` asks `ISourceControlModule::IsEnabled` and, only when enabled, `IsAvailable` and the provider capability flags (`TurboMCPSourceControl.cpp:184-245`). A disabled/unavailable/module-missing run produces one refusal string per resolved asset and does not call `GetState`. The refusal strings are the engine's own `SourceControlHelpers.cpp:67,76` or a module-load reason.

For each surviving row, `Plan` calls:

`Provider.GetState(Row.File, EStateCacheUsage::ForceUpdate)` (`TurboMCPSourceControl.cpp:293`).

That reaches the single-file helper (`ISourceControlProvider.cpp:50-60`), which discards the `ECommandResult` and returns the state only when the array call succeeded and returned a nonempty state. The pinned Perforce provider's array `GetState` (`PerforceSourceControlProvider.cpp:423-433`) executes `FUpdateStatus` when `ForceUpdate` is requested, ignores that operation's result, reads `GetStateInternal` from its cache and returns `ECommandResult::Succeeded`. A cached state can therefore be reported after a failed refresh. R13 recorded the same limit (`docs/audit/native-overlap.md:35,88`; `docs/audit/technical-debt.md:52-53`); no stale-proof claim is made here.

`Plan` checks `State.IsValid()` only (`TurboMCPSourceControl.cpp:295`). A null state becomes `UNKNOWN` with the reason. It does not call `ISourceControlState::IsUnknown()`. Perforce's default `FPerforceSourceControlState` is `DontCare` (`PerforceSourceControlState.h:52-56`); `IsUnknown()` is true for it (`PerforceSourceControlState.cpp:356-359`), and `IsSourceControlled()` is also true (`:326-329`). A non-null cached unknown state can therefore print `under source control: true` with provider text `Unknown`, rather than the unknown-state branch.

The structured flags are `IsSourceControlled`, `IsCheckedOut`, `IsAdded`, `IsCurrent`, `IsModified`, `IsIgnored` and `CanCheckout` (`TurboMCPSourceControl.cpp:309-317`). `IsCheckedOutOther` and `IsUnknown` are not read; a filesystem read-only attribute is not probed. Those conditions appear only through provider display text or `CanCheckout()`.

### Read, Preview and Edit side effects

- `Read` and `Preview` call `Plan`, so they can issue per-asset `GetState(ForceUpdate)` provider refresh/cache/network work even though they never issue `FCheckOut` or write an asset file.
- `Preview` renders checkout intent from `UsesCheckout`, `bCheckedOut`/`bAdded`, `bStateRead` and `CanCheckout`; it does not execute a source-control operation.
- `Edit` issues one `FCheckOut` for each eligible row (`TurboMCPSourceControl.cpp:491-492`). It skips plan-refused rows, unreadable states and rows already checked out/added; it still attempts when `CanCheckout()` is false. On a `UsesCheckout() == false` provider it states the no-op expectation and still executes to record the provider's answer.
- All three are wrapped by `FRecordingToolset` (`TurboMCPCallRecorder.cpp:103-130`); logging ON adds start/end records and can rotate the log.
- No tool writes or saves an asset. `Edit` can change provider/depot lock state. There is no rollback, aggregate transaction or atomic multi-asset behavior.

### Checkout execution and result limits

`Edit` calls `Plan`, renders the state half, then loops the rows. It does not re-run `GetState` before or after `Provider.Execute`; skips are based on the plan's stale fields. A file whose state changes between Plan and Execute can be skipped or attempted on the wrong condition. `ECommandResult::Succeeded` is the provider operation's answer only; no post-operation state read proves that the file is checked out. The result fields `bAttempted`/`bSucceeded`/`ProviderAnswer` in `TurboMCPSourceControl.h:101-109` are not populated by this path.

A multi-asset call continues after a failed row. The output has per-row results but no aggregate count, partial-success marker, retry or rollback. `Preview` and `Edit` each call `Plan` separately; sharing the same function does not preserve one state snapshot across two calls, so a state change between Preview and Edit can legitimately/incorrectly change the answer.

All three tools return plain `FString` with no ARC02 result line. `Tools/tmcp.py` `call_structured` requires one for `TurboMCP.*` unless `requires_result=False`; this is the prior `R04-CALL-04` contract gap. Text refusals are values to the recorder, so `bError=false` (`R04-CALL-06`); they are not failed-call counts.

### Findings

| ID | Status | Scope | Finding and evidence | Existing assertions |
| --- | --- | --- | --- | --- |
| `R04-VCS-01` | confirmed limitation | all three | `ForceUpdate` does not prove refresh success. Single-file helper discards the command result (`ISourceControlProvider.cpp:50-60`); Perforce ignores `FUpdateStatus` failure and returns cached state with `Succeeded` (`PerforceSourceControlProvider.cpp:423-433`). R13 recorded the same at `native-overlap.md:35,88`. | No failed-refresh/cached-state test; Perforce only tested unavailable. |
| `R04-VCS-02` | confirmed source | all three | Non-null state is never checked with `IsUnknown` (`TurboMCPSourceControl.cpp:295-317`). Perforce `DontCare` is `IsUnknown` true but `IsSourceControlled` true (`PerforceSourceControlState.cpp:326-329,356-359`); it can print `under source control: true` with provider text `Unknown`. | No non-null unknown-state test. |
| `R04-VCS-03` | confirmed source | all three | `SplitList` preserves duplicates (`TurboMCPAssetResolver.cpp:182-206`); Plan loops every row (`TurboMCPSourceControl.cpp:252-320`) and Checkout can execute per duplicate (`:443-527`). Duplicate inputs inflate `assets:` and can duplicate state queries/checkouts. | `test-vcs.py` uses two distinct fixtures and checks `len(rows)==2`; no duplicate/separator case. |
| `R04-VCS-04` | confirmed source | preview, edit | Checkout uses plan state without revalidation (`TurboMCPSourceControl.cpp:455-492`), never reads state after Execute, does not populate the `bSucceeded` fields, and has no aggregate/partial/rollback result. Preview/Edit each call Plan separately; shared code is not a shared snapshot. | `test-vcs.py` only covers no-provider, Git no-op and Perforce-unavailable; no successful live checkout or post-state. |
| `R04-VCS-05` | confirmed source | all three | Flags at `TurboMCPSourceControl.cpp:134-142,309-317` omit `IsCheckedOutOther` and `IsUnknown`; read-only is not probed. Those states are only in provider text/`CanCheckout`. | No test for checked-out-other, unknown non-null or read-only. |

### Bounded repair proposals

All proposals are unapproved.

| ID | Finding / owner | Exact allowed files | Preserved behavior | Probe | Expected / negative control | Validation / rollback |
| --- | --- | --- | --- | --- | --- | --- |
| `R04-REPAIR-13` | `R04-VCS-01`, owner R04 | `Plugins/TurboMCP/Source/TurboMCP/TurboMCPSourceControl.cpp`; `Tools/test-vcs.py` | Per-asset state rows, provider/module facts, refusal strings, no checkout in Read/Preview | Cached state after failed `FUpdateStatus` via test provider | State is labeled provider-reported/freshness-unproven, never stale-proof; provider text remains. Negative: changed successful state still changes. Fixture gap: no failed-refresh provider fixture. | No current suite; proposed provider fixture; rollback both files. |
| `R04-REPAIR-14` | `R04-VCS-02`, owner R04 | `Plugins/TurboMCP/Source/TurboMCP/TurboMCPSourceControl.cpp`; `Tools/test-vcs.py` | Null-state branch, provider text, per-asset rows/order, no checkout in Read/Preview | Non-null `IsUnknown` state plus a known state | Unknown row is marked UNKNOWN and does not claim true/false source control. Negative: known state still prints known flags. Fixture gap: no unknown-state provider fixture. | No current suite; proposed fixture; rollback both files. |
| `R04-REPAIR-15` | `R04-VCS-03`, owner R04 | `Plugins/TurboMCP/Source/TurboMCP/TurboMCPSourceControl.cpp`; `Tools/test-vcs.py` | Comma/pipe, trimming, order, distinct assets, per-asset refusals | `TA01,TA01`, `TA01|TA01`, separator-only | Duplicates collapsed or explicitly refused; distinct assets/order unchanged. Policy is unresolved. Negative: two distinct assets still two rows. | Add cases to test-vcs.py; revert both files. |
| `R04-REPAIR-16` | `R04-VCS-04`, owner R04 | `Plugins/TurboMCP/Source/TurboMCP/TurboMCPSourceControl.cpp`; `Tools/test-vcs.py` | Per-asset refusals, no-op Git behavior, provider messages, no asset write | State change between Plan and Execute; post-state read; one success/failure/skip | Revalidate or state snapshot; read back state and only claim verified checkout; summarize partial results. Negative: stable success still succeeds; Git no-op never claims lock. Fixture gap: no state-changing provider fixture. | No current suite; proposed provider fixture; rollback both files. |
| `R04-REPAIR-17` | `R04-VCS-05`, owner R04 | `Plugins/TurboMCP/Source/TurboMCP/TurboMCPSourceControl.cpp`; `Tools/test-vcs.py` | Existing flags/provider text/order/refusals | Checked-out-other, non-null unknown, read-only states | New structured status or explicit inability; no false true/false. Negative: normal checked-out/current flags unchanged. Fixture gap: no such provider states. | No current suite; proposed fixture; rollback both files. |

### Checkpoint 3 validation

| Command | Result |
| --- | --- |
| `python Tools/audit-inventory.py --check` | exit 0; 101 definitions; 95 `parsed_call`, 6 `name_reference_only` |
| `python Tools/test-result-contract.py` | exit 0; PASS (C03–C08) |
| `python Tools/test-client-retry.py` | exit 0; PASS (C01/C02) |
| `python Tools/execute-tests.py --profile offline --execute` | exit 0; report `.local/executions/9ebf217b4f3f4564b78a55bc77059f40/report.json`, tree `8486fa8e…` |

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

## Integration review

Checkpoint 1 was accepted as a source-only checkpoint at candidate `12d515ac46b58e42e83fffb9b577181fb4e68b55`. That acceptance covered four reviewed tools and 97 not reviewed; all repair rows remain unapproved proposals. The toggle end-after-disable exception and any state/header expansion require an explicit reviewed lifecycle contract before implementation. Checkpoint 2 is accepted at candidate `f10dc74ca4569ca449c0e598fe76abe042a0e7b7`: it adds three source-reviewed rows, for a cumulative seven reviewed and 94 not reviewed. Checkpoint 3 is pending review: it adds three VCS rows, for a current total of ten reviewed and 91 not reviewed. R04 remains incomplete. No runtime outcome or repair is certified.
