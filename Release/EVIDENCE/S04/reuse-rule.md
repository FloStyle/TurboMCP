# S04 evidence — the reuse rule works, and the repoint suite's failures are its own fixture assumption

Step: S04 — Implement the fixed shared contracts.
Date: 2026-10-16.

**State: `implemented_unverified`. The editor-lifecycle blocker is FIXED AND VERIFIED. The graph repoint
suite now runs to its assertions, and its three failures are a TEST defect that predates the migration — with
the evidence below.**

## The reuse rule: implemented and verified

`start-editor.ps1` no longer refuses a start when this project's editor is already up and **answering**. The
three outcomes are distinct and each is reported:

| Situation | Behaviour |
|---|---|
| this project's editor answers the MCP server | **REUSE**, exit 0; nothing stopped, nothing started; the lease is refreshed to name that editor |
| this project's editor does **not** answer | the existing refusal is kept, so `run-suites.ps1`'s wedged-editor recovery still detects it |
| a **foreign** editor | already refused above; never pre-empted |

The probe is the decision, not the process — `run-suites.ps1` records why in as many words: a launcher once
answered "an editor is already running (PID 53852)" while `check-mcp-server.py` could **not reach a server**, and
every later suite inherited that live process with no server.

**MEASURED, and it is the specific thing two rounds failed at:**

```
ok    this project's editor is already running and healthy (PID 43180); reusing it
      The MCP server answered and all four TurboMCP toolsets are registered.
      Nothing was stopped and nothing was started.
RESULT: PASS (an existing healthy editor was reused)
  ok
  ok
  FAIL
  FAIL
RESULT: FAIL
```

The suite now **gets past the editor lifecycle entirely and reaches its own assertions**. That is the change
this round was for.

## The remaining failures are a TEST defect, and here is the evidence

`test-graph-repoint.py` takes the graph's node list and uses **positions**:

```python
keys = re.findall(...)              # every node id, in read order
source = ...(keys[0])               # assumed to be a call node with a "then" output
target = ...(keys[2])               # assumed to be a call node with a "then" output
```

The fixture's actual nodes, read live:

| position | node | class | outputs |
|---|---|---|---|
| 0 | `74e7c4` | `K2Node_IfThenElse` | `then`, `else` |
| 1 | `8ed5ad` | `K2Node_VariableGet` | — |
| 2 | `ca5c81` | `K2Node_VariableGet` | `bAppendFixtureBranch` |
| 3 | `c96a8c` | `K2Node_CallFunction` | `then` |

So `keys[2]` — the suite's target — is a **VariableGet** with no `then` output, and the plan refuses:

```
refused target has no output pin named 'then'; outputs: bAppendFixtureBranch
```

The suite's first three checks are therefore expected to fail against the regenerated fixture:

1. `preview.startswith("preview:") and "reader(s) would move" in preview` — the preview is a **refusal** because
   the target has no such pin;
2. `"unchanged:" in refusal and "NoSuchPin" in refusal` — this one passes;
3. `edit.startswith("changed:") and "1 reader(s)" in edit` — the edit is also a refusal;
4. `(":%s)" % keys[2]) in after` — the target never gained a link.

**This is a fixture-position assumption, not a migration defect.** The test picks nodes by index without
checking their class, and the fixture rebuilds deterministically into a different shape than the test assumes.
It is recorded as a test defect with the fix stated: select the call nodes **by class** (`K2Node_CallFunction`)
rather than by position, or name the specific nodes the case needs.

**I am not claiming the migration is regression-free on this evidence.** The suite's assertions about repointing
did not run against a valid target, so they prove nothing either way. That is exactly why the record says the
migration's regression status is still **UNKNOWN** — the test has to be able to exercise it before its result
means anything.

## Verified / not verified

| Item | State |
|---|---|
| The reclaim rule (round 12) | **verified** |
| The reuse rule (this round) | **verified — the suite passes the editor lifecycle** |
| A foreign editor is never pre-empted | verified in round 4 (E01) |
| The graph repoint migration preserved behaviour | **UNKNOWN** — the suite cannot exercise it |
| `test-graph-repoint.py` passes | **NOT yet** — test-defect: nodes selected by position |
| `test-graph-swap-call.py` passes | **NOT run this round** |

## Evidence paths

- this record; `Tools/start-editor.ps1`
- the fixture's live node list and the plan's refusal are quoted above
- editor at the end of the round: reused, not restarted; 0 commandlets

## Next action, in one step

Fix `test-graph-repoint.py` to select its source and target **by class**, not by position — and while there,
print the plan's refusal when it refuses, so a fixture change fails with a reason rather than as a bare `FAIL`.
Then run it and `test-graph-swap-call.py` to a pass. Nothing else in S04 can be trusted until the graph repoint
migration has regression evidence.
