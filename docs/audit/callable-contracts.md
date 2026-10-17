# R04 call-log contracts — first checkpoint

Source/offline investigation only. This checkpoint audits four call-log tools and does not complete R04.

- **Task:** R04, first dispatch checkpoint
- **Approved base:** `1180c6d43655195dc2772b6d1f0b80588e4b466a`
- **Plugin tree:** `590661ec45c829264ea1743fccea994edb748092`
- **Engine:** 5.8.3 at `396c9f059903aed5fec78ecd3d437a40c6415368`
- **Claim:** `ab2cf6535c584bc6a9419c79e85d0b04` (`dsh-r04-01`)
- **Machine-readable inventory:** [callable-contracts.json](callable-contracts.json)

## Scope and coverage

`docs/audit/capabilities.json` contains **101 unique tool IDs**. This checkpoint creates all 101 rows.

| Audit status | Count |
| --- | ---: |
| `reviewed_source` | 4 |
| `not_reviewed` | 97 |
| **Total** | **101** |

The 97 `not_reviewed` rows retain their capability source/test references and are explicitly marked. No conclusion is implied for them.

Reviewed tools: `ReadTurboMCPCallCost`, `ReadTurboMCPCallLog`, `EditTurboMCPCallLog`, `EditTurboMCPCallLogClear`.

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

The recorder sets `bError = Result.HasError()` (`TurboMCPCallRecorder.cpp:126-128`). A tool body that returns `error: ...` for invalid arguments is therefore a **successful value** to the recorder: no `ERROR` row, not counted in the cost report's failed count, and not matched by `filter=error`. A genuine invocation/transport failure is `bError=true`. See `R04-CALL-06`.

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
| `R04-CALL-06` | confirmed source | all four | Ordinary domain refusals (`error:` strings returned by the tool body) are reflected as successful values, so `bError=false`; they are absent from ERROR rows, failed counts and `filter=error`. Genuine unknown-tool/pre-invocation/script errors are `HasError` and recorded `bError=true`. Evidence: `FunctionLibraryToolset.cpp:134-197`; `ObjectFunctionToolCall.cpp:105-162,390-521`; `TurboMCPCallRecorder.cpp:126-128`; `TurboMCPCallLog.cpp:272-305`; `TurboMCPCallCostTools.cpp:237-240`; `TurboMCPCallLogTools.cpp:95`. | Bad-boolean text is asserted; the record's `bError`, failed count and invocation-error control are not. |

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
| `R04-REPAIR-06` | `R04-CALL-06`, owner R04 | `Tools/test-call-log.py`; `Tools/test-call-cost.py` | No prose parsing; invocation errors still `bError=true`; ordinary refusal text unchanged | ON: one ordinary refusal (`Enabled=banana`) and one genuine invocation error (unknown tool or pre-body bad parameter); inspect bError, ERROR and failed count/filter | Record the current truth: refusal `bError=false`; invocation error `bError=true`. Production fix deferred until a structured outcome exists. Negative: invocation error must still be true. | Engine suites; test-only, no production rollback. |

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
