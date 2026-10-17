# S03 evidence — environment and build safety

Step: S03 — Make the environment and builds safe.
Covers: R08, R09, R10, R11; acceptance cases E01–E07.
Date: 2026-10-16.

**State: S03 remains OPEN, but E01 now passes and the first L3 observation of this release was taken.**

## E01 — the ownership case now passes (41 of 41 checks)

`Tools/test-ownership.py` → **exit 0**, 41 checks, 0 failed, 10.2 s:
`RESULT: PASS (a foreign editor is never owned, the lease is exclusive, a foreign mount is refused, and a
missing artifact fails the build)`.

### The cause, finally measured

The assertion was instrumented, and both operands appeared in one line:

```
DBG2 rc= 0  raw= '{"Owned":true,"Reason":"process 40260 is this project\'s editor"}\n'
                last= {'Owned': True, ...}
  ok   E01 this repository's editor (positional project) is owned
```

`owns(...)` returns `ps_json(...)`, i.e. `(exit_code, output)`. The call sites read

```python
payload, out = owns(ours, editor_exe, repo_project, engine)
```

so **`payload` was the exit code — the integer 0 — and `out` was the JSON**. `field(payload, "Owned")`
therefore returned `None` for a correct `{"Owned": true}` sitting in `out`, and seven assertions reported a
module defect that did not exist. The correction is one line per call site: unpack `(rc, output)` and parse the
output with `last_json`.

This is recorded with its full history because four earlier attempts each fixed a *plausible* cause without
moving the count. Three of those were real defects (a host-executable fallback, a shared probe body that
hard-coded the host path, and `last_json` taking the last parseable line); the fourth, `$global:PID`, was a
guess that changed nothing. What found the cause was printing both operands at the failing assertion, which is
what the previous record said to do next.

## R08 — the declared-dependency gap is closed, and now checked

`TurboMCP.uplugin` declared neither `EditorScriptingUtilities` nor `PythonScriptPlugin`, while
`TurboMCP.Build.cs` depends on both. The audit recorded why it survived: the test project enables Python
explicitly, so the missing declaration is invisible until an installation does not happen to enable it.

`Tools/check-plugin.py` gained `check_declared_dependencies`, in **both** directions:

| Direction | Rule |
|---|---|
| source → descriptor | an include of `Engine/Plugins/<…>/Source/<Module>/…` names an engine plugin that must appear in the descriptor's `Plugins` array |
| descriptor → engine | every declared plugin must have an engine descriptor, found by a **recursive** glob |

Both new plugins are declared, and `check-plugin.py` reports **RESULT: PASS (13 checks)** — up from 11.
A false positive was caught and fixed while writing it: the first non-recursive glob reported
`SlateInspectorToolset` as missing its descriptor, while it is at
`Engine/Plugins/Experimental/Toolsets/SlateInspectorToolset/`. A blocking gate that fails healthy code is a
reason to bypass the gate, so the depth is no longer assumed.

## R09 — engine version mismatch is refused

`setup-engine.ps1` used to print the engine version and continue with whatever it found. It now refuses a
version other than 5.8.3 with a named cause, and `-AllowOtherEngineVersion` is the explicit override. That is
the same shape as R11: a fact stated and then not acted on.

## E06 — build, start, and loaded module identity (the first L3 observation of this run)

| Observation | Value |
|---|---|
| Artifact | `Binaries\Win64\UnrealEditor-TurboMCP.dll`, 3 680 256 bytes, 2026-10-16 22:14:17 |
| Artifact SHA-256 | `07B277CA56FB194EEF31C7FFA3FB66EACA13004D67081CD5D024B69FE620FB6A` |
| Plugin mount | `Engine\Plugins\Marketplace\TurboMCP` → `C:\Users\Example\Desktop\TurboMCP` |
| **Mounted artefact SHA-256** | `07B277CA56FB194EEF31C7FFA3FB66EACA13004D67081CD5D024B69FE620FB6A` — **identical to the built artifact** |
| Loaded module path | `C:\ExampleUnrealengine\Engine\Plugins\Marketplace\TurboMCP\Binaries\Win64\UnrealEditor-TurboMCP.dll` |
| Editor start | `start-editor.ps1 -NullRHI -Offscreen -LowPriority` → `RESULT: PASS`, exit 0, 16.4 s |
| Editor PID | 39780 |
| Ownership verdict for that PID | `owned=True :: process 39780 is this project's editor` |
| MCP server | `PASS all 4 TurboMCP toolsets are registered, of 28 toolsets served` (`check-mcp-server.py --editor-pid 39780`, exit 0) |
| Live tool call | `ReadTurboMCPStatus` answered `read: TurboMCP status, 4 toolsets` and listed all four |
| Turndown | `start-editor.ps1 -Stop` → `stopping 1 editor process(es) that own this project`, `RESULT: PASS`, exit 0; **0 editors remain** |

The loaded module's bytes are the artifact's bytes, which is what E06 asks for, and the stop proves the new
ownership path refuses nothing it owns and stops what it does.

## Focused L3: `test-function.py` — a defect found, a regression I caused, and a limit found

This suite was already failing before this round's work, and running it produced three separate findings.

| Finding | Detail |
|---|---|
| **A public-contract defect** | `EditTurboMCPFunctionAdd` declares `Inputs` and `Outputs` with defaults, and the engine's MCP adapter **refuses** a call that omits them: `input param "inputs" is required by the function input schema Json, but is missing`. A documented default an agent cannot rely on. Applies to five tools, not one. |
| **A regression I caused** | This session's earlier edit added calls that supply the two parameters to only *some* call sites, so 27 checks failed on the adapter's schema complaint before reaching the behaviour under test. Fixed: every call supplies both. **27 → 5 failures.** |
| **A resolver limit** | `FString` is not an accepted pin type: `input pin 'Label' has invalid type 'FString': 'FString' is not a type or a loadable class`. Measured which types resolve: `float`, `int`, `bool`, `String`, `Name`, `Vector` are accepted; `FString` and `Object` are not. |

**Still failing (5):** the signature case itself. `Score: float, Weight: int` / `Total: float` is refused or
the pins are not reported, and one assertion compares against a stale `before` list captured while a leftover
probe existed. The graph fixture WAS left clean and saved: `graphs now: EventGraph, ReceiveBeginPlay,
TLAY13Probe, UserConstructionScript`, `nothing to save`.

Two `check()` calls also passed a list as their detail argument, which raised `TypeError` and hid the result.
Fixed, and it is the same shape as the rest of this round: a test defect reported as a product defect.

## Honest limitations

- **The signature case still fails.** Not accepted, and it is the only reason `test-function.py` is not green.
- No other L3 suite was run this round.
- The `inputs`/`outputs` defect applies to five tools (`EditTurboMCPFunctionAdd`, `ReadTurboMCPTypeFind`, and
  the other three found by the same schema check); the shared contract is S04's `tmcp-result-v1` work, and the
  parameter-requiredness question is recorded for it rather than patched here.
- `Tools/` still contains several `patch_*.py` one-shot scripts written in this session. They are developer
  scripts, not product code, and S17 must keep them out of the user package.

## Evidence paths

- `Release/EVIDENCE/S03/environment-safety.md` (this record)
- `Tools/ownership.ps1`, `Tools/test-ownership.py`, `Tools/check-plugin.py`, `Tools/test-function.py`
- `TurboMCP.uplugin`, `Tools/setup-engine.ps1`

## Next action

Fix the signature case in `Tools/test-function.py` (capture `before` after any leftover probe is removed, and
establish which type/name combination the resolver accepts for a two-input signature), then run the focused
graph family for S05 and record the `inputs`/`outputs` requiredness finding against S04's shared contract.
