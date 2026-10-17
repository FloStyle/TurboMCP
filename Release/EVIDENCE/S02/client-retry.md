# S02 evidence — remove ambiguous mutation replay

Step: S02 — Remove ambiguous mutation replay.
Covers: audit finding R02; roadmap step S02; acceptance cases C01, C02.
Date: 2026-10-16 (same run as S00 and S01).

## Observed failure before the correction

`Release/Audit/audit-verification.json`:

```
{"name":"client ambiguous retry isolated reproduction","result":"DEFECT_CONFIRMED",
 "details":"Real Session.rpc with mocked network. One simulated effect followed by response timeout caused
            reconnect and a second effect. No editor or network calls."}
```

Read in `Tools/tmcp.py`: `rpc` caught every exception, cleared `self.id`, called `connect()`, and then
repeated `_rpc_once` with the original request — for every method, including `tools/call` for a tool that had
already changed an asset. The response is lost in exactly the case where the request may have been applied, so
the replay is a second mutation, not a recovery.

The repository's own probe recorded the defect and asserted it (`assert p.effects == 2`).

## Correction

`Tools/tmcp.py`:

| Change | Detail |
|---|---|
| `UnknownOutcomeError` | New exception carrying the tool name and the transport failure. Its message states that the outcome is UNKNOWN, that the request was NOT repeated, and what to do instead — read the asset or the dirty-package list back, or inspect the `operationId` in the result line. |
| `is_mutation(tool, override=None)` | The classification. `Edit` and `Preview` prefixes are mutations; `Read`, `Look`, `List`, `Get`, `Find`, `Search` and `Describe` prefixes are reads; the MCP envelope methods are not themselves mutations; **an unrecognised name is a mutation**, because wrongly replaying a write costs more than wrongly refusing to replay a read. `--mutates yes|no` overrides it. |
| `Session.rpc(..., mutates=None)` | The reconnect still happens for every method, because a stale session id would break every later call. The **repeat** happens only for a read. A mutation raises `UnknownOutcomeError` after the reconnect. |
| `Session.call(..., mutates=None)` | Passes the classification through to `rpc`. Every existing caller is unchanged: the new parameter has a default. |
| `--mutates` CLI flag and `EXIT_UNKNOWN = 2` | The CLI prints `outcome_unknown: …` rather than `error:`, because the edit may well have applied, and exits 2 — distinct from 1, which means a refusal or failure that WAS established. |
| `tier_of_return` | Kept, documented as reserved. It describes the distinction between a transport failure and a malformed request, which is the next thing to tighten; it is deliberately not wired into the retry, because the retry now decides by mutation rather than by error code. |

`Release/Audit/audit_retry_probe.py` was updated to the repaired expectation (`effects == 1`, one reconnect,
an unknown outcome reported) and keeps its role as the historical evidence for the finding. Deleting it would
have erased what was measured.

## Test results

Command: `python C:\Users\Example\Desktop\TurboMCP\Tools\test-client-retry.py`
Exit code: **0**. Result line: `RESULT: PASS (one lost response means one effect, and a read still reconnects)`. 20 checks, 0 failed.

| Case | Observation |
|---|---|
| C01 | A mutation whose response was lost produced **exactly one** effect and **one** request, raised `UnknownOutcomeError`, and still performed **one** reconnect. The message contains `UNKNOWN` and `NOT repeated` and names the tool. The audit probe's old expectation was two effects; it is now one. |
| C01 negative control | The same loss on a `Read` request was **still retried** (2 attempts) and the retry succeeded. A fix that simply deleted the retry would fail this line. |
| C01 classification | `Edit…` mutation, `Preview…` mutation (because R05 records that the Preview class does not enforce read-only behaviour), `Read…` read, unknown name mutation, explicit override wins, MCP envelope not a mutation. |
| C02 | A reconnect after a lost read response re-sent only that read; a reconnect after a lost mutation response did **not** re-send the mutation (`['EditTurboMCPGraphRepoint']`), the mutation was counted once, the reconnect still happened, and the outcome was reported as unknown. |
| C02 exit code | `EXIT_UNKNOWN == 2`, distinct from 1. |

Historical probe: `python Release\Audit\audit_retry_probe.py` → exit 0, `simulated_effects: 1`,
`reconnects: 1`, `unknown_outcome: true`. It asserted `2` before the repair.

Static suite set through the real runner:

```
pwsh -NoProfile -File Tools\run-suites.ps1 -Suites audit-suite-quality,check-includes,check-plugin,check-probe-graphs,test-client-retry,test-coverage-audit,test-lookup-guards,test-runner-verdicts,test-traps-plugin-identity,test-widget-tree-nested
```

Exit code **0**, 140.3 s, **10 passed, 0 failed, 0 blocked, 0 timed out**.

## Independent state compared

The suite counts **effects on the fake server**, not calls made by the client, so a client that silently
retried would be caught by the effect count rather than by inspecting its own control flow. The request order
is recorded by the fake transport, and the assertion is on that order.

## Build

Not required: S02 changes Python only. No C++ file was touched.

## Review of the diff, in a separate pass

- The reconnect is preserved for every method; only the repeat is conditional. Losing the reconnect would
  reintroduce the six-round failure that `run-suites.ps1`'s own comment records.
- The classification is static rather than derived from the server, because it must be correct precisely when
  the server is not answering.
- The bias is stated in the code and in this record: unknown names are mutations.
- `UnknownOutcomeError` is raised only after the reconnect attempt, so the session is left usable.
- A reconnect that itself fails does not replace the unknown-outcome report: the request's outcome was already
  unknown before the reconnect was tried.
- No caller in `Tools/` or in `Release/` was broken: the new parameters are keyword-with-default, and the full
  static set passes.

## Unverified limits

- No live editor test. C01/C02 are L1 with a mocked transport, which is what the acceptance catalog specifies.
- Whether the server itself deduplicates a retried request is NOT claimed. `rpc`'s docstring and the error
  message both say the caller must establish the outcome independently; the release does not claim safe
  automatic retries.
- The `operationId` the message refers to is not yet present in every tool's result line. Until S04 migrates
  the result contract (`tmcp-result-v1`), a caller's fallback is reading the asset or the dirty-package list.

## Evidence paths

- `Release/EVIDENCE/S02/client-retry.md` (this record)
- `Tools/test-client-retry.py`
- `Release/Audit/audit_retry_probe.py`
- `.agent/suite-logs/manifest.json`

## Next action

S03 — make the environment and builds safe (E01–E07): ownership checks before anything is stopped, no
process-name termination on operational paths, mount and dependency validation, pinned engine identity, and a
build that fails when the artifact is missing.
