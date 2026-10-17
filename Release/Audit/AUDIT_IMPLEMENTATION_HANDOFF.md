> Archived audit evidence from 2026-10-16. Follow the [sequential roadmap](<C:/Users/Example/Desktop/TurboMCP/Release/ROADMAP.md>) for execution. Historical parallel-agent instructions do not apply.

# Audit handoff: decisions, documents, and bounded tasks

Date: 2026-10-16. This is a proposed work package. No task below ran as an implementation task.

## 1. Rules for every implementation agent

1. Read the current source before changing it.
2. Treat this audit as a dated finding, not permanent truth.
3. Record the starting commit and existing worktree changes.
4. Preserve unrelated changes.
5. Use a separate worktree for independent source changes.
6. Obtain the integration owner's lease before building or using an editor.
7. Do not start another editor for the same project.
8. Do not stop a process that the task does not own.
9. Do not edit a shared fixture concurrently.
10. Do not weaken a test to obtain a passing result.
11. Do not remove a capability without an approved release-scope decision.
12. Do not mark runtime acceptance complete after a source-only test.
13. Report blocked acceptance as blocked, not passed.
14. Give every result its source commit, artifact identity, test command, exit code, and log path.
15. Do not publish, push, or change board status unless the task authorizes that action.

These instructions use short technical English. They do not claim certified ASD dictionary compliance.

## 2. Owner decisions required before more feature work

| Decision | Recommended choice | Alternative and cost |
|---|---|---|
| Release identity | Experimental source preview, with a verified capability list | Stable 1.0 requires all advertised safety guarantees and clean-install acceptance |
| Engine support | Windows and the exact tested source-engine revision | Broader versions require independent build and runtime rows |
| Mutation scope | Exclude unsafe or unverified writers from the release registration | Keep them only after independent rollback, undo, and persistence tests |
| Batch scope | Exclude unfinished batch tools | Design and test cross-operation dependencies and disk failure semantics first |
| Execution tools | Treat widget preview and play as code execution | A non-executing preview needs a separate implementation and narrower output |
| Disk atomicity | Report package-by-package persistence separately from memory undo | Cross-file atomicity requires staging, recovery records, and recovery tests |
| Public history | Review the public-only commit before deciding publication strategy | Do not infer permission to force-push from old documentation |

## 3. Produce these documents in this order

### D1 — Release capability manifest

One machine-readable row per tool or supported operation kind:

- Stable capability ID and reflected tool name.
- Release disposition: included, experimental, excluded, or deferred.
- Supported engine revision and platforms.
- Supported asset and node families.
- Known limitations and explicit refusal conditions.
- Side effects: asset, dirty state, UI, script execution, disk, network.
- Test IDs and last accepted commit.
- Evidence status: not tested, source checked, runtime checked, or release accepted.

Generate the public capability table and tool counts from this manifest. Do not maintain duplicate handwritten totals.

### D2 — Mutation and persistence contract

Specify these states separately: planned, applied in memory, compiled, verified, saved, rolled back, rollback failed, and outcome unknown.

Define ownership of transactions, compilation, saving, and independent comparison. Define what one undo restores. State whether warnings trigger rollback. Define behavior when the asset already contains compile errors.

Define recovery after save failure, editor crash, client disconnect, and source-control refusal. Do not describe disk writes as reversible through editor undo.

### D3 — Graph representation contract

Separate a readable summary from a lossless edit representation. Name the lossless mode explicitly.

Specify node identity, pin identity, all link endpoints, shared pure nodes, strings, text localization, object defaults, split pins, delegates, macros, and unsupported nodes.

A summary must not serve as destructive replacement input. An incomplete representation must carry a machine-readable completeness flag.

### D4 — Result and schema contract

Specify one result envelope with a schema version. Include outcome, diagnostics, effects, verification, persistence, recovery, and truncation fields.

Define transport success separately from operation success. Define optional input values, booleans, numbers, action enums, and examples. Specify retry policy and operation identity.

### D5 — Release verification and environment ownership

Define the build/editor lease. Define fixture ownership and recovery. Define required suites and nonzero exit conditions.

Record source and artifact identity. Define a clean-clone installation test. Separate source guards from runtime acceptance.

### D6 — User quickstart and limitations

Keep the main procedure short. Include prerequisites, installation, client configuration, first read, first supported edit, verification, undo, save, and recovery.

State the local trust boundary. Warn about project-code execution and retained logs. Name unsupported node families and operating systems.

### D7 — Architecture decisions and archived evidence

Move historical investigations out of current instruction fields. Keep links and dates. Retain useful evidence without making old conclusions normative.

Do not regenerate every historical document. Replace their current-status role with a dated archive notice and the manifest link.

## 4. Bounded implementation tasks

Each task needs a separate implementation review and an independent acceptance result. Task IDs below are audit IDs, not existing board IDs.

### A01 — Make the suite runner fail closed

**Priority:** release blocker. **Dependencies:** none. **Scope:** [runner](<C:/Users/Example/Desktop/TurboMCP/Tools/run-suites.ps1>) and new pure harness tests.

**Change:** preserve process exit codes. Reject unknown suite names. Reject empty selections. Return nonzero for required blocked or timed-out suites. Persist complete logs.

**Acceptance:** printed PASS followed by exit 7 fails. Blocked-only execution fails. One valid plus one unknown suite fails. Zero matches fails. Normal success passes.

**Non-goals:** no fixture rewrite, editor startup redesign, or application feature work.

### A02 — Prevent automatic replay of uncertain mutations

**Priority:** release blocker. **Dependencies:** D4 retry decision. **Scope:** [client](<C:/Users/Example/Desktop/TurboMCP/Tools/tmcp.py>) and pure transport tests.

**Change:** distinguish reconnect from replay. Do not replay a mutation after ambiguous response loss. Return an explicit uncertain outcome.

**Acceptance:** use a fake server that applies a request and drops its response. Count exactly one mutation. Check recovery guidance. Check safe reconnect behavior separately.

**Non-goals:** do not invent server-side deduplication in the client.

### A03 — Exclude unfinished batch registration or complete its contract

**Priority:** release blocker if included. **Dependencies:** owner release decision and D2. **Scope:** [batch implementation](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPBatchEditTools.cpp>) and its public declarations.

**Recommended release action:** exclude the unfinished batch capability. Preserve its development work outside the release surface.

**If implementation continues:** validate operation dependencies. Compile each affected Blueprint after all operations. Compare independent state. Distinguish memory rollback from disk persistence.

**Acceptance:** operation three fails after two writes. Every affected asset equals its initial state. A rollback failure must not report unchanged.

**Non-goals:** no new operation kinds until the coordinator passes failure injection.

### A04 — Correct patch value capture and link semantics

**Priority:** release blocker if patch ships. **Dependencies:** D2 and D3. **Scope:** [patch implementation](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPGraphPatch.cpp>) and its focused suite.

**Change:** capture transactional state before modifying values. Use schema validation without prior raw assignment. Apply requested links or refuse them before mutation.

**Acceptance:** one undo restores the first changed literal. An invalid literal changes nothing. A mixed literal-and-link request applies both changes or neither.

**Non-goals:** no full parser replacement in this task.

### A05 — Make graph replacement one atomic memory edit

**Priority:** release blocker if replacement ships. **Dependencies:** D2 and transaction-free append core. **Scope:** [replacement implementation](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPGraphReplace.cpp>) and the minimum append-core extraction.

**Change:** one outer transaction owns additions and removals. Do not call an independently committed public writer inside replacement.

**Acceptance:** one undo restores nodes, links, defaults, positions, and initial dirty state. Inject failure after additions and during removals. Compare complete state, not counts alone.

**Non-goals:** no multi-asset batch or new graph grammar.

### A06 — Preserve semantics during swap and repoint

**Priority:** release blocker if these tools ship. **Dependencies:** D2. **Scope:** [swap implementation](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPGraphSwapCall.cpp>) and [repoint implementation](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPGraphRepoint.cpp>).

**Split before parallel assignment:** one task owns swap. One task owns repoint. A single integrator owns shared declarations.

**Acceptance:** preserve unlinked literal, object, and text defaults. Test occupied data inputs. Test multiple outputs and exec connections. Refuse source deletion that destroys unrelated behavior. Verify all edges and one undo.

**Non-goals:** no batch feature expansion.

### A07 — Establish a lossless graph corpus

**Priority:** release blocker for lossless claims and destructive round trips. **Dependencies:** D3. **Scope:** reader, lexer, checker, and writer adapters.

**Change:** use one canonical representation for supported mutations. Preserve read-only representations for unsupported node families. Refuse destructive use of incomplete text.

**Acceptance corpus:** shared pure nodes, multiple source pins, fan-out, escaped strings, localized text, object defaults, own-function calls, delegates, split structs, macros, latent nodes, cycles, and large graphs.

**Comparison:** extract topology and values independently from engine objects. Read, parse, apply to a duplicate, and compare. Do not compare only the reader with itself.

**Non-goals:** support every Unreal node family before release. Explicitly exclude unproved families instead.

### A08 — Correct trace attribution and play lifecycle truth

**Priority:** high, release blocker if these tools remain advertised as reliable. **Dependencies:** none for source work, editor lease for acceptance.

**Split:** trace and play-check are separate tasks.

**Trace acceptance:** arm two distinct nodes in one function. Execute only one. Attribute the exact node using the real code offset. Distinguish no execution from unmapped execution.

**Play acceptance:** delay startup past the budget. Refuse play startup. End play early. Report actual observed duration. Never report played when no play session began.

**Non-goals:** breakpoint stepping, input injection, or a menu-solving agent.

### A09 — Enforce an honest preview effect contract

**Priority:** high. **Dependencies:** D1 and D2. **Scope:** widget preview, material planning, graph planning, and affected capture paths. Split these into separate implementation tasks.

**Change:** distinguish rendering from project-code execution. Require explicit opt-in for execution. Document that transient instances do not isolate external effects. Do not recompile a real material or spawn probes into a live graph during a declared non-mutating preview.

**Acceptance:** a widget callback attempts to change another object. The selected policy must prevent the call or expose the effect. Check dirty state, selection, open tabs, and generated files. Material and graph previews must preserve initial state on both acceptance and refusal paths. Test assets that were already dirty.

**Non-goals:** promise a security sandbox inside an unrestricted editor process.

### A10 — Audit rollback across all advertised writers

**Priority:** high. **Dependencies:** D2 and A01. **Scope:** one writer family per task, not one repository-wide edit.

**Acceptance dimensions:** transaction begins before mutation, all touched objects participate, failure restores exact state, rollback result is checked, one undo works, save outcome is separate.

Start with graph mutation, materials, struct changes, and data-table rows. Use deterministic failure injection at the second mutation and at compile/save boundaries.

**Non-goals:** search-and-replace every transaction call. Different asset types have different restoration rules.

### A11 — Repair installation and artifact provenance

**Priority:** release gate. **Dependencies:** owner engine-support decision. **Scope:** plugin descriptor, build rules, setup, and build result handling.

**Change:** declare required plugins or separate fixture-only modules. Validate existing mount targets. Pin the supported engine revision. Remove token-in-URL advice. Fail when the artifact is missing.

**Acceptance:** fresh checkout in a path with spaces, renamed folder, minimal host project, disabled optional plugins, and documented Python/PowerShell versions. Record artifact hashes and loaded module identity.

**Non-goals:** claim Launcher, Linux, or macOS support without testing them.

### A12 — Serialize editor and fixture ownership

**Priority:** high. **Dependencies:** D5. **Scope:** lifecycle scripts and integration scheduling.

**Change:** use one lease for engine build, project editor, MCP port, and fixtures. Stop only owned process trees. Preserve recovery snapshots until restoration passes.

**Acceptance:** foreign editor present, lease contention, owner crash, timed-out test, commandlet failure, and interrupted restore. Foreign processes and assets must remain untouched.

**Non-goals:** parallel live tests against one shared project.

### A13 — Make discovery and result interpretation predictable

**Priority:** high for less capable agents. **Dependencies:** D4. **Scope:** bounded discovery, generated help, and shared result formatting.

**Acceptance:** task-word query finds the intended operation within a fixed output budget. Each parameter shows accepted values and required status. Refusals and rollback failures have unambiguous outcomes.

**Non-goals:** rewrite all tool signatures at once. Introduce compatible adapters in small groups.

### A14 — Reconcile board evidence and release scope

**Priority:** release gate. **Dependencies:** D1, D5, and current integration results.

**Input:** [all 111 active cards](<C:/Users/Example/Desktop/TurboMCP/Release/Audit/AUDIT_BOARD_RECONCILIATION.md>).

**Change:** assign one canonical capability or defect to each card. Merge overlapping scopes. Preserve original feedback IDs. Separate historical notes from work. Require evidence before giving release credit.

**Acceptance:** every included capability has a current test result. Every known unresolved defect has an owner or an explicit exclusion. No done card relies only on a failed execution record.

**Non-goals:** automatically reopen every done card. Some later evidence may prove completion.

### A15 — Correct data-operation order and field semantics

**Priority:** release blocker for affected advertised behavior. **Dependencies:** D2 and the chosen field syntax. **Split:** three independent tasks for table order, variable access, and struct defaults.

**Scope:** [table row operations](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPDataTableRows.cpp>), [variable creation](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPVarsAdd.cpp>), and [struct fields](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPStruct.cpp>).

**Table acceptance:** set then remove leaves no row. Remove then set leaves the final value. Repeated sets retain the final requested value. A conflicting batch must either follow documented order or refuse before mutation.

**Variable acceptance:** a private variable uses the engine's Blueprint access metadata. An external Blueprint cannot access it as public. Do not substitute editor read-only flags for access control.

**Struct acceptance:** an invalid default produces a refusal or explicit partial result. A valid default is read back and compared. Failure in a later field does not conceal changes to earlier fields.

**Non-goals:** no new nested-value parser or general schema migration in these tasks.

## 5. Parallelism plan

- Lane 1: A01 and A02 use pure tests. They can proceed independently.
- Lane 2: A04, A05, and A06 require coordinated graph-core ownership. Do not edit shared cores concurrently.
- Lane 3: A08 trace and play work can use separate source branches.
- Lane 4: A11 packaging work and D1–D7 documentation can proceed independently.
- Integration lane: one owner merges, builds, starts the editor, and runs live acceptance.
- Acceptance lane: an independent reviewer checks behavior after integration. The author cannot substitute a source-string test for runtime proof.

## 6. Required task result format

```text
Task ID:
Starting commit:
Final commit or patch hash:
Files changed:
Approved scope changes:
Build identity:
Tests executed:
Exit code per test:
Behavior verified:
Behavior not verified:
Failure injection result:
Undo result:
Rollback result:
Persistence result:
Evidence paths:
Remaining risks:
Verdict: implemented-unverified | verified | blocked | failed
```

Do not use `done` as a synonym for `implemented`.
