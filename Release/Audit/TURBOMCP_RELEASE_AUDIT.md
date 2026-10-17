> Archived audit evidence from 2026-10-16. Follow the [sequential roadmap](<C:/Users/Example/Desktop/TurboMCP/Release/ROADMAP.md>) for execution. Historical parallel-agent instructions do not apply.

# TurboMCP first-release audit

**Date:** 2026-10-16  
**Decision:** **Do not release the current tree as a stable, broadly safe 1.0.**  
**Possible alternative:** a narrow experimental source preview, after the release gates below pass.

## 1. Executive judgment

TurboMCP addresses a real problem. Its combination of compact inspection, stable references, targeted edits, and editor integration is useful. The repository contains substantial implementation and regression work. This is not an empty prototype.

However, the product's strongest promises are not reliable contracts yet:

- Read and Preview do not always avoid mutation or execution.
- Some writes do not preserve one-undo behavior.
- Some edits can lose requested changes or existing values.
- Compilation is not consistently a checked acceptance gate.
- A successful response does not consistently mean the operation succeeded.
- Test and board status can overstate verification.

**The central problem is confidence, not feature count.** Adding more tools before repairing these contracts will increase the amount of behavior that agents can misuse.

A less capable implementation agent needs smaller, stronger contracts than the current documents provide. More historical explanation will not compensate for contradictory states, ambiguous results, or missing failure tests.

### What I would do next

1. Freeze new feature registration.
2. Select a release subset explicitly.
3. Repair test verdicts and unsafe retries first.
4. Fix or exclude mutation paths that violate undo, rollback, or preservation guarantees.
5. Establish truthful read and execution results.
6. Run one serialized clean-install acceptance sequence.
7. Publish only the capability claims that sequence proves.

Do not spend the week completing the whole board. Do not use a beta label as permission to ship known destructive behavior without a boundary.

## 2. What this audit establishes

### Baseline

| Item | Observed state |
|---|---|
| Plugin commit | `f045c1313f1db04ddcac269d6910be9ad6befb11` |
| Engine commit | `396c9f059903aed5fec78ecd3d437a40c6415368` |
| Local engine version | 5.8.3, source tree, Windows |
| Plugin worktree | Five modified tracked files and one untracked batch implementation |
| Reflected declarations in working tree | 101: Read 30, Look 4, Preview 29, Edit 38 |
| Discovered suites | 74 |
| Board snapshot | Revision 1632, 111 non-archived cards |
| Board columns | 59 todo, 20 backlog, 32 done |
| Done cards with failed latest execution | 6 |
| Done cards with verification bypass | 9 |

Counts describe this snapshot. They are not quality scores.

### Method and limits

This was a broad, risk-based audit of implementation, focused tests, engine integration, public claims, and board evidence. Two independent reviewers examined graph safety and the remaining tool domains. The lead examined release engineering, API behavior, security boundaries, and verification.

I ran static checks and isolated control-flow probes. I did not build the current tree or start an editor. No full runtime suite, fresh-clone install, cross-platform build, penetration test, or performance benchmark ran.

Therefore, this report establishes concrete source defects and release-evidence gaps. It does **not** certify every tool or prove every engine-dependent failure at runtime. “Exhaustive” here means broad audit scope with explicit coverage limits, not a claim of complete defect discovery.

No implementation code or board state was intentionally changed. Existing dirty work was preserved. Audit artifacts reside outside the code repository.

## 3. Evidence package

| Report | Scope |
|---|---|
| [Graph safety review](<C:/Users/Example/Desktop/TurboMCP/Release/Audit/audit-graph-safety.md>) | Graph representation, append, patch, replace, swap, repoint, compile, rollback, save |
| [Tool-domain review](<C:/Users/Example/Desktop/TurboMCP/Release/Audit/audit-tool-domains.md>) | Widgets, materials, capture/layout, trace/play, data, references, source control |
| [Release engineering review](<C:/Users/Example/Desktop/TurboMCP/Release/Audit/audit-release-engineering.md>) | API, client, tests, installation, security, logging, documentation |
| [Implementation handoff](<C:/Users/Example/Desktop/TurboMCP/Release/Audit/AUDIT_IMPLEMENTATION_HANDOFF.md>) | Seven document specifications, fifteen bounded work packages, acceptance rules |
| [Board reconciliation](<C:/Users/Example/Desktop/TurboMCP/Release/Audit/AUDIT_BOARD_RECONCILIATION.md>) | Metadata triage for all 111 non-archived cards |
| [Board evidence snapshot](<C:/Users/Example/Desktop/TurboMCP/Release/Audit/audit-board-snapshot.json>) | Exact card descriptions and execution records from the snapshot |
| [Verification record](<C:/Users/Example/Desktop/TurboMCP/Release/Audit/audit-verification.json>) | Executed checks, results, and tests not performed |
| [Source fingerprints and link check](<C:/Users/Example/Desktop/TurboMCP/Release/Audit/audit-artifact-validation.json>) | SHA-256 fingerprints of dirty sources and report-link validation |

## 4. Highest-impact findings

“Blocker” means fix the behavior or exclude the affected capability from the release. It does not mean every tool must ship next week.

| Finding | Impact | Evidence and release action |
|---|---|---|
| Test runner can return exit zero with blocked suites | A release gate can approve untested behavior | Confirmed source. It also discards child exit codes. Fix before trusting aggregate results. |
| Client automatically retries failed requests | A lost response can cause a write twice | Confirmed control flow. Mocked I/O produced two effects after one lost response. Disable ambiguous mutation replay. |
| Graph replacement uses two transactions | One undo does not reverse the complete replace operation | Confirmed source. Addition completes before removal begins. Fix or exclude replacement. |
| Patch assigns a value before `Modify()` | Undo can capture the new value instead of the old value | Confirmed source. Fix ordering and independently compare restoration. |
| Patch skips requested links | A mixed patch can apply literals while dropping requested rewiring | Confirmed source review. Apply the links or refuse before any write. |
| Swap migrates links but not unlinked defaults | A same-type replacement can change graph behavior | Confirmed source review. Preserve literal/object/text defaults or refuse. |
| Preview can change real editor objects | “Safe to preview” is not a trustworthy assumption | Append planning spawns into the real graph. Material tidy planning recompiles and dirties the material. |
| Explicit reference direction `both` skips both query branches | The reader reports false zero references | Confirmed source. The empty-string default works differently. Fix and test both spellings. |
| Trace uses a fixed code offset of zero | Runtime evidence can identify the wrong node or miss execution | Confirmed source and engine comparison. Do not interpret unmatched events as “never ran.” |
| Play-check can report success without observed play startup | A validation tool can certify execution that did not occur | Confirmed lifecycle logic. Require observed startup and measured duration. |
| Batch edit returns changed without final compile/save verification | A new public feature exposes unfinished safety semantics | Dirty working-tree implementation. Exclude from the release candidate. |
| Data-table operations are reordered | `set R; remove R` can recreate the row and report success | Confirmed source. Honor documented order or reject conflicting operations before mutation. |
| Private-variable and struct-default results are misleading | Requested access restrictions or defaults may not be applied | Confirmed source. Use the engine's private metadata and verify default setter results. |
| Broad safety and completion claims are stale | Users and agents make unsafe decisions from the documentation | Current counts differ. Several done cards lack a successful latest verification record. |
| Compile and rollback results are not consistently enforced | A changed result can conceal an invalid graph or failed restoration | Graph writers often ignore compiler status or rollback return values. Centralize enforcement before claiming safe edits. |

### Direct evidence for four central findings

- [Patch mutation order:398–402](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPGraphPatch.cpp#L398-L402>).
- [Replacement addition phase:275–286](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPGraphReplace.cpp#L275-L286>) and [separate removal transaction:324–328](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPGraphReplace.cpp#L324-L328>).
- [Reference direction predicates:166–175](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPRefs.cpp#L166-L175>).
- [Trace source lookup:123–127](<C:/Users/Example/Desktop/TurboMCP/Source/TurboMCP/TurboMCPTrace.cpp#L123-L127>).

The append preview's persistent side effects depend on the chosen node spawner. The source definitely spawns and removes a node in the live graph. This audit did not measure every spawner's residual effects.

## 5. State of the product by area

| Area | Assessment | First-release boundary |
|---|---|---|
| Tool registration and editor integration | Substantial working structure. Static reflection check passes. | Verify current binary and clean installation. Do not equate reflected declarations with working tools. |
| Blueprint and graph reading | Valuable breadth, but fidelity is not uniformly lossless. | Mark summaries, truncation, unsupported nodes, and incomplete topology explicitly. |
| Graph checking and text | Shared helpers exist, but reader/checker/writer contracts still differ. | Do not use a readable summary as destructive replacement input. |
| Append, patch, replace | Safety invariants remain inconsistent. | Fix or exclude each writer separately. Require exact topology and value comparisons. |
| Swap and repoint | New, narrow tests miss important semantic cases. Dirty refactoring adds uncertainty. | Treat as experimental until defaults, occupied inputs, deletion scope, compile, and undo pass. |
| Materials | Useful reads and specialized operations. Preview and rollback require attention. | Verify side effects and failure restoration. Do not advertise full material authoring. |
| Widgets | Useful tree inspection and previews. General authoring remains incomplete. | Separate asset inspection, live instance inspection, rendering, and script execution. |
| Layout and capture | Useful specialized features, with geometry and UI-state constraints. | State measured versus estimated geometry. Verify images, not just file creation. |
| Data tables, structs, enums, functions | Meaningful authoring support with unequal result and rollback strength. | Test multi-field operations, dependent assets, type changes, and exact restoration. |
| References and cost | Good product direction, but false zeros are dangerous. | Fix literal `both`. Report loaded/on-disk scope and cost provenance. |
| Trace and play | Currently unsuitable as authoritative runtime proof. | Fix lifecycle and attribution, or exclude from the verified subset. |
| Source control | Provider-dependent and environment-sensitive. | Publish provider-specific evidence. Refusal testing is not successful provider integration. |
| Batch coordination | Incomplete working-tree feature. | Exclude from this release. |
| Documentation and task board | Rich history, weak current-state authority. | Replace competing status narratives with manifests and explicit evidence states. |

Not every incomplete roadmap item is a release blocker. Missing curve editing, widget animation authoring, broad asset operations, input injection, or full material graph authoring can remain deferred. False success and silent semantic changes cannot be treated the same way.

## 6. Conceptual issues to resolve before scaling implementation

### 6.1 Labels are not capability boundaries

Read, Look, Preview, and Edit are useful categories. They do not prevent a C++ function from calling mutable editor APIs.

A transient widget is not an isolated process. Its callback can modify other objects or invoke external effects. A preview that recompiles a material is not a pure read.

Describe effects explicitly. Test the actual boundary. Remove claims of structural impossibility that the architecture does not enforce.

### 6.2 “One safe edit” combines several different guarantees

These are separate properties:

1. Validation refuses before mutation.
2. One undo restores memory state.
3. Failure rollback restores memory state.
4. Compilation introduces no unacceptable diagnostics.
5. Independent comparison proves the intended change.
6. Saving persists the intended package state.
7. Multiple package saves recover safely after partial failure.

An editor transaction does not prove all seven. A successful compile does not prove semantic preservation. A matching node count does not prove matching links or defaults.

The next architecture document must separate these states instead of repeating “safe edit.”

### 6.3 Human-readable output and serialization have different jobs

Compact output can omit details deliberately. A destructive writer cannot infer that omitted details should disappear.

Keep a readable summary if it saves context. Add a distinct canonical representation for supported round trips. Include pin identity and all edges. Preserve unsupported nodes as explicit read-only data.

An incomplete result must be impossible to mistake for a complete replacement plan.

### 6.4 Autonomous agents need stable outcomes, not prose conventions

Current string results mix `error`, `unchanged`, `refused`, warnings, and partial success. The MCP layer can classify a normal returned error string as transport success.

Use explicit outcome fields. Separate changed, saved, verified, rolled back, rollback failed, and outcome unknown. Do not ask a weaker agent to infer these states from a paragraph.

### 6.5 Preview is not approval of an immutable plan

Shared Plan code reduces duplication. It does not prove that a later Edit applies the same state the user inspected.

For sensitive operations, include a graph or asset revision fingerprint. Refuse stale plans. Prefer stable operation IDs and explicit target sets. Avoid raw-pointer plans that survive untracked mutations.

This is an architectural recommendation. It is not a claim that this audit reproduced a concurrent edit race.

### 6.6 The implementation process repeats the product's failure pattern

The product tries to prevent guesses, silent partial changes, and false success. The board and test process still permit those same outcomes.

Many cards mix diagnosis, history, unresolved questions, and implementation instructions. Priorities are inconsistent across generations. Some old P0 cards concern formatting, while current semantic corruption sits beside ordinary feature work.

Reprioritize by user impact and release exposure. Do not preserve an old priority label merely because it was written emphatically.

## 7. Test evidence: what is real and what is not

### Checks executed in this audit

- Plugin static check: **PASS**, 11 checks, 1606 includes, 101 tool declarations.
- Tool coverage audit: **FAIL**. Two new batch tools have no callers. FunctionAdd also triggers a default-parameter parser defect.
- Suite-quality audit: **FAIL**, two candidates. Both are heuristic false positives for empty tests, not proof of missing assertions.
- Renamed-plugin source regression: **PASS**.
- Nested-widget source regression: **PASS**.
- Git whitespace check: **PASS**.
- Isolated runner classifier: accepted PASS followed by traceback text.
- Isolated client retry: one response loss caused two simulated effects.

### What these checks do not prove

They do not prove current compilation, live tool registration, safe undo, failure rollback, clean installation, or correct rendering. The source-string tests cannot replace an actual renamed install or nested widget runtime fixture.

Do not repair the quality gate by forcing every test to use one preferred variable shape. Improve its parser or scope. Preserve real semantic assertions.

### Required acceptance structure

For each included writer, record:

- Successful operation and independent state comparison.
- Refusal before mutation.
- Failure after the first mutation.
- Compile failure and pre-existing diagnostic behavior.
- Exact rollback, including initial dirty state.
- One undo and redo where supported.
- Save failure and source-control refusal.
- Repeat call and ambiguous response-loss behavior.

For readers, test completeness, empty state, missing state, unsupported state, and truncation separately. For runtime tools, distinguish no events from events that could not be matched.

## 8. Board reconciliation and prioritization

The complete board matrix is a metadata inventory, not 111 independently verified implementation verdicts. Archived cards include unrelated projects and are not part of that active matrix.

### Consolidate these overlapping groups

| Canonical group | Existing overlapping work | Recommended treatment |
|---|---|---|
| Graph fidelity and grammar | T05 remainder, T13, GL-09, GL-10–13, GC-06/07, GC-10–12 | One contract and fixture corpus. Preserve feedback IDs as aliases. |
| Graph mutation safety | GC-09, T12 and its prerequisite, swap/repoint work, this audit's patch/replace findings | Safety before new operation kinds. Exclude unfinished batch. |
| Geometry and capture | T16–T21, X-20/TD-04/CP-05, FT-30/31/TD-05 | Separate measurement truth, layout behavior, and image verification. |
| Data and signatures | T11, T22–T26, SF-04/VA-02/FA-04, DT-06/SV-07 | Separate type syntax, mutation atomicity, dependent refresh, and persistence. |
| Read-only effects | X-18/X-21/X-22, preview findings, FT-32–36 | Do not blindly clear dirty flags. Identify and prevent the mutation. |
| Help and discovery | T32, AS-01/BL-07/TH-02/03, FT-43, TH-04–07 | One bounded schema-driven discovery design. |
| Usage and runtime evidence | T30, BL-09/LT-03/LT-06, LT-04, TR-01/02 | Distinguish suggestions from proofs. Avoid unsafe deletion recommendations. |
| Release verification | Release gate, batch verifier, T-KEEP, fixture work | One final candidate and one evidence manifest. |

### Six done cards need explicit acceptance reconciliation

- T05 remainder: GC-02/03/04.
- F3/F4/X-3: widget traversal and plugin traps.
- DT-06/SV-07: data-table writes and save reporting.
- GL-09: graph reader node support.
- GC-06/GC-07: grammar and insertion.
- FT-30/FT-31/TD-05: scoped layout and preview consistency.

Their latest executions failed. Later independent evidence may exist. Find that evidence before reopening or closing anything. Do not infer implementation failure solely from the board record.

### What to defer

Defer broad widget authoring, full material writing, curve editing, asset CRUD expansion, input injection, generalized batch editing, and cosmetic panel work. Keep narrow corrections required by an included capability.

Archive “zero defects found” and completed historical investigation cards as evidence notes. They are not future implementation tasks.

## 9. A realistic next-week release path

This is a sequence of decision gates, not a promise that all fixes fit five days.

### Day 1: freeze and establish truth

Choose the included capabilities. Freeze new registration. Record the candidate source and engine revisions. Repair runner verdicts and retry behavior. Exclude the unfinished batch surface.

If no reliable test verdict exists at the end of this stage, do not call the release candidate verified.

### Days 2–3: fix or exclude exposed safety defects

Prioritize patch, replacement, swap/repoint, preview side effects, reference `both`, and runtime false claims. Use independent state comparisons and failure injection.

If a mutation tool cannot meet its contract, remove it from the included subset. Do not disguise the gap with softer wording alone.

### Day 4: clean installation and integration

Use one integration owner. Build the final candidate once. Verify the loaded artifact. Test a fresh installation in a minimal project. Run the required regression sequence without shared-editor contention.

### Day 5: evidence review and publication decision

Review every required result, exclusion, warning, and recovery instruction. Generate the release documentation from the accepted manifest. Review public history and licensing boundaries before publication.

If the gates do not pass, release a development snapshot only if its scope and risks are explicit. Otherwise postpone. A week of delay costs less than a first release that damages a user's Blueprint.

## 10. Minimum release gates

- [ ] One immutable candidate and artifact identity.
- [ ] No unexplained dirty implementation in the candidate.
- [ ] Required blocked, missing, or failed tests cause nonzero release status.
- [ ] No automatic replay of uncertain writes.
- [ ] Included writers pass undo, rollback, and exact-preservation tests.
- [ ] Included previews meet their declared effect contract.
- [ ] Reference `both` agrees with explicit in/out queries.
- [ ] Trace/play claims are verified or excluded.
- [ ] Fresh minimal-project installation passes with declared dependencies.
- [ ] No lifecycle script kills unrelated editor processes.
- [ ] Documentation names actual limitations and supported engine revision.
- [ ] Board completion links to current acceptance evidence.
- [ ] Public-branch reconciliation and release publication receive owner approval.

## 11. Final assessment

The project has enough useful work to justify a first public preview. It does not yet have enough consistent safety enforcement and verification to justify its current broad 1.0 claims.

The strongest next investment is not another feature batch. It is a small set of enforced contracts: truthful outcomes, exact preservation, explicit side effects, reliable test verdicts, and reproducible release evidence.

Produce those contracts first. Then assign the [bounded implementation tasks](<C:/Users/Example/Desktop/TurboMCP/Release/Audit/AUDIT_IMPLEMENTATION_HANDOFF.md>). That will make less capable agents more useful without asking them to reconstruct the project's history or guess what “safe” means.
