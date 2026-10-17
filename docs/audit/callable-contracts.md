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

Reviewed tools:

- `ReadTurboMCPCallCost`
- `ReadTurboMCPCallLog`
- `EditTurboMCPCallLog`
- `EditTurboMCPCallLogClear`

## Reviewed contracts

| Tool | Declaration | Wrapper / implementation | Parameters and defaults | Effects and result |
| --- | --- | --- | --- | --- |
| `ReadTurboMCPCallCost` | `TurboMCPToolsets.h:464-468`; `AICallable` | `FRecordingToolset` at `TurboMCPCallRecorder.cpp:103-130`; implementation `TurboMCPCallCostTools.cpp:80-437`; timeline `TurboMCPCallTimeline.cpp:49-142` | `Since`/`Filter` empty = no bound/all; `Cap` empty = 10, max 50. All are `FString`; no C++ defaults. | Reads in-memory records, merges them into calls, ranks tokens/duration separately, totals by tool/asset. Its own start record is included. No ARC02 result line. |
| `ReadTurboMCPCallLog` | `TurboMCPToolsets.h:425-429`; `AICallable` | Same recorder; implementation `TurboMCPCallLogTools.cpp:34-200` | `Filter` empty = all; `Limit` empty = 50, max 500; `Since` empty = no bound. No C++ defaults. | Reads raw records newest-first, groups sessions, shows phase/source/tool and up to 200 chars of args/result. Its own start record is visible to its own filter. No ARC02 result line. |
| `EditTurboMCPCallLog` | `TurboMCPToolsets.h:1982-1983`; `AICallable` | Same recorder; implementation `TurboMCPCallLogTools.cpp:202-247` | `Enabled` accepts `true/on/yes/1` and `false/off/no/0` (trimmed, case-insensitive); anything else refused. Empty is refused. | Writes `bEnableCallLog` and calls `SaveConfig`. Returns changed or unchanged text. No ARC02 result line. |
| `EditTurboMCPCallLogClear` | `TurboMCPToolsets.h:1992-1993`; `AICallable` | Same recorder; implementation `TurboMCPCallLogTools.cpp:249-266` | `Confirm` accepts `yes` or `true` (trimmed, case-insensitive); anything else refused. Empty is refused. | Clears only the in-memory `Entries` array; file is not touched. Returns dropped count and path. No ARC02 result line. |

### Recording boundary

`FRecordingToolset` wraps each engine registry handler and creates `FTurboMCPCallLog::FScopedCall` (`TurboMCPCallRecorder.cpp:103-130`). The constructor writes a `start` record (`TurboMCPCallLog.cpp:254-270`); `Finish` writes an `end` record with duration, full result length, token estimate and error flag (`:272-305`). `Write` is a no-op when logging is off (`:216-234`). The `start` is therefore conditional on the setting at call entry, and the `end` is conditional on the setting at call exit.

### Numeric parsing

Both `Limit` and `Cap` use `FString::IsNumeric()` followed by `FCString::Atoi()` (`TurboMCPCallLogTools.cpp:53-67`; `TurboMCPCallCostTools.cpp:126-144`). `IsNumeric` accepts an optional sign and at most one dot (`Engine/Source/Runtime/Core/Public/Misc/CString.h:148-175`); `Atoi` converts the integer prefix (`CString.h:1219-1222`). This is not whole-number validation. `"1.5"` passes and becomes `1`; `"+5"` is accepted; `1e3` and `1..2` are refused. Overflow is delegated to the platform conversion and is not checked.

### Empty and populated logs

- `ReadTurboMCPCallLog` validates `Limit` before reading entries. Empty logs still get a limit refusal for `banana` or `0`.
- `ReadTurboMCPCallCost` returns its empty-log report before parsing `Cap`. With an empty log, `cap=banana` or `cap=0` returns the empty report instead of the cap error.
- With logging OFF, no start/end records are written for ordinary calls. Reads and edits made while OFF are therefore absent.
- The log is in-memory bounded at 1000 entries; the file rotates at 4 MiB with one `.1` file. `FScopedCall::Finish` stores at most 4000 result characters but measures the full result; the readers display at most 200 characters of args/result.

## Source findings

| ID | Status | Scope | Finding and exact source evidence | Existing assertions |
| --- | --- | --- | --- | --- |
| `R04-CALL-01` | confirmed source | cost, log | `IsNumeric` + `Atoi` accepts fractional/signed strings and does not detect overflow. `1.5` becomes 1. Sources: `CString.h:148-175,1219-1222`; `TurboMCPCallLogTools.cpp:53-67`; `TurboMCPCallCostTools.cpp:126-144`. | `test-call-log.py:400-408` and `test-call-cost.py:344-353` test only `banana` and `0`. |
| `R04-CALL-02` | confirmed source | cost, log | Cost returns the empty-log report before cap validation (`TurboMCPCallCostTools.cpp:104-116` vs `:126-145`); log validates limit first (`TurboMCPCallLogTools.cpp:53-69`). An empty log + invalid cap is accepted by one reader and refused by the other. | Invalid caps are tested only with populated logs. |
| `R04-CALL-03` | confirmed source | log | When `Total > Selected`, the reader prints `since=<newest shown>` to page forward (`TurboMCPCallLogTools.cpp:185-189`), but selection is newest-first (`:104-110`) and `since` excludes `Timestamp <= SinceText` (`:79`). The omitted rows are older, so the echoed value cannot retrieve them. | `test-call-log.py:262-280` proves `since` excludes older rows; it does not test the truncation sentence. |
| `R04-CALL-04` | contract gap | all four | The four tools return plain `FString` and do not call `FTurboMCPResult::AppendTo`; the only production caller is `TurboMCPGraphRepointTools.cpp:27`. `Tools/tmcp.py:422-431` assumes every `TurboMCP.*` toolset is migrated, so `call_structured` without `requires_result=False` raises for these four. Whether to migrate them or declare them compatibility-only is unresolved. | `test-result-contract.py` covers GraphRepoint and a native tool, not these four. Existing call-log suites use `Session.call`, not `call_structured`. |
| `R04-CALL-05` | confirmed source | edit | Switching the log OFF from ON writes a start (`FScopedCall` constructor) and suppresses the end (`Finish` -> `Write` sees disabled), leaving a start-only record that the timeline renders as running/missing-end. Sources: `TurboMCPCallLog.cpp:216-234,254-305`; `TurboMCPCallLogTools.cpp:202-247`. | `test-call-log.py` tests off/on transitions but not the resulting record shape. |

`R04-CALL-04` is recorded as a **contract gap**, not a reproduced runtime failure. No engine suite was run, so none of these findings is claimed as runtime-observed.

## Bounded repair proposals

These are proposals only. They do not authorize implementation, schema change or a new public result format.

| ID | Finding / owner | Allowed files | Preserved behavior | Synthetic probe | Expected / negative control | Validation / rollback |
| --- | --- | --- | --- | --- | --- | --- |
| `R04-REPAIR-01` | `R04-CALL-01`, owner R04 | `TurboMCPCallLogTools.cpp`, `TurboMCPCallCostTools.cpp`, `test-call-log.py`, `test-call-cost.py` | Empty defaults, positive values, 500/50 maxima, existing error prefixes, totals over all selected records | Populated log; `1.5`, `+1`, `.5`, `1e3`, large decimal, valid `1` | One whole-integer policy; refuse truncated/overflowing values by parameter name. Negative: valid `1` still works. | Engine suites `test-call-log,test-call-cost`; revert the two source and two test files. |
| `R04-REPAIR-02` | `R04-CALL-02`, owner R04 | `TurboMCPCallCostTools.cpp`, `test-call-cost.py` | Valid empty-log wording, OFF vs ON-and-empty, cap 10/50, filter/since counts | Empty log (logging OFF, entries cleared); `cap=banana`, `0`, `1`; then ON-and-empty | Invalid cap refused before emptiness; valid cap still reports emptiness. Negative: `cap=1` returns the empty report. | `test-call-cost`; revert the cost file and test. |
| `R04-REPAIR-03` | `R04-CALL-03`, owner R04 | `TurboMCPCallLogTools.cpp`, `test-call-log.py` | Newest-first, pre-cut denominator, `since` excludes `<=`, 200-char display | >Limit distinct timestamps; limit 1; use echoed value; compare omitted set | Continuation can actually retrieve the omitted rows, or says raise the limit/cap; no false since claim. Negative: older `since` returns rows. | `test-call-log`; revert the log tool and test. |
| `R04-REPAIR-04` | `R04-CALL-05`, owner R04 | `TurboMCPCallLog.cpp`, `TurboMCPCallLogTools.cpp`, `test-call-log.py` | Settings persistence, OFF suppresses new records, normal ON calls keep pairs, clear semantics, genuine unfinished calls still show running | ON -> `EditTurboMCPCallLog(false)` -> read while OFF; repeat ON->ON, OFF->OFF, OFF->ON | No start-only record from a setting change; symmetric or absent toggle record. Negative: normal ON pair, OFF no record, real unfinished call still running. | `test-call-log`; revert log implementation/recorder and test. |
| `R04-REPAIR-05` | `R04-CALL-04`, owner R04 | `Tools/tmcp.py`, `Tools/test-result-contract.py` | GraphRepoint result line, native no-line behavior, truly missing migrated metadata still fails, compatibility call | `call_structured` on the four with default and explicit `requires_result=False`; compare GraphRepoint/native | Migration decision is explicit per tool or tools gain valid result lines; no silent native certification. Negative: GraphRepoint missing/invalid line still raises. | `test-result-contract.py`, `test-client-retry.py`; revert client and test only. |

## Existing assertions vs unrun probes

Existing engine suites are assertions, not results for this checkpoint:

- `Tools/test-call-cost.py` (T-HELP-6): off/empty, populated token and duration rankings, per-tool agreement, no-match filter, cap row boundary, `banana`/`0` cap refusals.
- `Tools/test-call-log.py` (T-DBG-1, T-LOG-1/2/3): start off/persistence, empty wording, filter, self-match, `since` exclusion, clear confirmation/file untouched, 4 MiB rotation, bad boolean, `banana`/`0` limit refusals.
- `Tools/test-result-contract.py`: C03–C08; passes, but no case calls these four tools.
- `Tools/test-client-retry.py`: C01/C02; passes, but no call-log case.

Not run: engine suites, editor, build, direct MCP calls, packaging, recovery. Probes still needed: fractional/signed/overflow, empty-log invalid cap, 50/500 and 4000/200 boundaries, truncated paging, ON→OFF record shape, clear-with-log-on shape, and `call_structured` for these four.

## Validation

| Command | Result |
| --- | --- |
| `python Tools/audit-inventory.py --check` | exit 0; 101 definitions; 95 `parsed_call`, 6 `name_reference_only` |
| `python Tools/test-result-contract.py` | exit 0; PASS (C03–C08) |
| `python Tools/test-client-retry.py` | exit 0; PASS (C01/C02) |
| `python Tools/execute-tests.py --profile offline --execute` | exit 0; PASS at base commit `1180c6d`; report `.local/executions/e08918e140e4430aa2d550140999f441/report.json`, tree `fa4e4265…` |

## Limits and unresolved questions

- Source paths and line numbers are from base `1180c6d`; the plugin tree is `590661ec…`.
- No runtime behavior is certified. The four tools were not called in an editor.
- `R04-CALL-04` needs a policy decision: migrate these tools to ARC02 result lines or make the client's migration requirement explicit per tool.
- The 97 unreviewed rows remain unreviewed; their inclusion is inventory coverage, not certification.
- Imported finding 019cf7af's stale fallback diagnosis is not repeated: current cost/log code refuses nonnumeric and nonpositive values after `Atoi`. Its remaining concern about strict integer validation and empty-log ordering is carried into `R04-CALL-01` and `R04-CALL-02`.
- Imported finding 7de8d185's compile-ensure runtime work is outside this checkpoint. Its result-decoding concern is reflected in `R04-CALL-04`; no wrapper was weakened.
