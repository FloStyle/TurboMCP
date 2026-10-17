# Sequential roadmap to a verified release candidate

Follow the [binding architecture](<C:/Users/Example/Desktop/TurboMCP/Release/ARCHITECTURE.md>) before this roadmap.
Use the [acceptance catalog](<C:/Users/Example/Desktop/TurboMCP/Release/ACCEPTANCE_CASES.md>) for exact observations.
Manage tasks through the [execution protocol](<C:/Users/Example/Desktop/TurboMCP/Release/EXECUTION_PROTOCOL.md>).
Do not redesign the product during execution.

## 1. Operating contract

Work alone. Do not delegate or start board execution sessions.
Keep exactly one roadmap step active.
Follow the listed order unless a concrete prerequisite blocks a step.
Record that blocker before selecting the earliest independent step.
Do not bypass a blocked prerequisite for dependent work.
Use one owned editor and one build at a time.
A managed background process is allowed only when required by that step.
Wait for its result before starting dependent work.

Use absolute paths for file operations and commands.
Resolve the repository root again if this checkout moves.
Do not read the entire repository repeatedly.
Use symbol, dependency, and test indexes when available.
If an index is empty, record that fact and use focused searches.

Preserve unrelated and pre-existing changes.
Do not use destructive reset, checkout, clean, or stash operations.
Do not alter engine source to make the plugin appear compatible.
Propose engine changes separately if they are necessary.

Do not stop unrelated processes.
Do not restore fixtures over a user's assets.
Do not commit unrelated work.
If commits are not authorized, maintain a patch fingerprint instead.

## 2. Completion means evidence, not activity

Use these work states:

- `pending`: no work started.
- `in_progress`: the only active implementation step.
- `implemented_unverified`: source changed, acceptance incomplete.
- `verified`: required behavior passed on the recorded artifact.
- `blocked`: a concrete prerequisite prevents verification.

Use these finding dispositions:

- `open`: unresolved.
- `fixed_verified`: repair and required verification complete.
- `disproved`: current code and evidence contradict the audit finding.
- `accepted_limitation`: owner approved a precise limitation.
- `excluded_approved`: owner approved removal from release scope.
- `deferred_by_scope`: ARC01 or ARC09 excludes an unbuilt feature from this release. This is not done.
- `duplicate`: another ledger item owns the same issue.
- `blocked`: unresolved and prevents completion when release-relevant.

A duplicate must name its canonical item.
An approved disposition must cite the owner's decision or a boundary explicitly approved through the launch prompt.
A skipped test is not a verified result.
An audit recommendation is not owner approval.

Account for all board cards without implementing every historical wishlist automatically.
Apply ARC01 and ARC09 to existing feature requests.
Ask only for exclusions beyond those fixed boundaries or proven architectural contradictions.
Continue independent safe work while exceptions remain pending.
Do not claim release readiness while required decisions remain unresolved.

## 3. Resume protocol

At every session start:

1. Read [progress](<C:/Users/Example/Desktop/TurboMCP/Release/PROGRESS.json>).
2. Check Git status and the current revision.
3. Check owned editor and build processes.
4. Compare source fingerprints with the last checkpoint.
5. Revalidate evidence affected by new changes.
6. Resume the recorded active step.

If the host supports persistent goals, create or resume this release objective.
Follow the host's rules for goal continuation and blocking.
Do not create a second goal for each fix.

At each checkpoint, update the progress ledger, owner status page, and active board card.
Close verified leaf tasks with evidence-backed external outcomes.
Do not start task-board execution sessions.
Record the next concrete action.
Do not rely on conversation history as the only record.

## 4. Small-test policy

### Test levels

| Level | Use | Required evidence |
|---|---|---|
| L0 | Formatting, JSON, static declarations, documentation links | Exit code and checked scope |
| L1 | Pure parsing, state machines, result classification, transport mocks | Behavior assertions and a negative case |
| L2 | Affected C++ build | Build identity and expected artifact existence |
| L3 | Focused Unreal behavior | Actual state before and after, exact assertions, loaded artifact |
| L4 | Final release regression and clean installation | Complete required manifest with zero blocked or failed required cases |

Static source checks do not prove Unreal behavior.
A compile does not prove safe undo or correct rendering.
A node count does not prove correct topology.
A dirty flag does not prove durable persistence.

### Per-fix loop

1. Read the finding and current implementation.
2. Identify the smallest observable failure.
3. Add or select one focused regression case.
4. Reproduce the failure before changing behavior when practical.
5. Record why reproduction is unavailable if it cannot run.
6. Make the narrowest coherent correction.
7. Run the focused regression.
8. Run direct dependent tests only when the change affects them.
9. Review the diff for missing failure paths and unrelated edits.
10. Record evidence and the next step.

Prefer one success case, the reported failure case, and one boundary case.
Add undo and failure-injection cases for mutations.
Use small scratch assets with deterministic topology.
Reuse fixtures when their isolation is proven.
Do not regenerate the complete fixture kit for each assertion.

### Builds and editor use

Use incremental builds by default.
Do not run a clean engine rebuild after each plugin fix.
Build after a coherent C++ change, before interpreting runtime results.
Do not accumulate unrelated uncompiled changes across roadmap steps.

Header, reflection, descriptor, and module changes require a real rebuild and editor restart.
Do not use Live Coding as release evidence.
Close only the owned editor before replacing its loaded module.
Start it once after the build.
Reuse it for the focused tests of that unchanged artifact.

Do not execute a suite directly until its lifecycle and fixture behavior are known.
Some suites restart the editor or regenerate assets.
Use the repaired runner for these suites.
Do not use unsafe fast mode merely to obtain a passing result.

### Test selection

Search the supplied test map for each changed source file first.
Check the selected test's assertions before trusting its name.
Record absent coverage rather than inventing a test command.
Add a case selector if a large suite prevents focused verification.
Reject unknown selectors and zero selected cases.
Preserve the suite's existing all-cases entry point.

Record elapsed time for each build and test.
Use those measurements to improve selection.
Do not impose an arbitrary short timeout on Unreal compilation or shader work.

### Evidence reuse

Reuse evidence only when its implementation and relevant dependencies remain unchanged.
Record source hashes, fixture identity, engine revision, and artifact identity.
A changed test oracle invalidates its previous result.
A common rollback, parser, resolver, or formatter change invalidates affected dependent evidence.

After an unrelated documentation edit, rerun link and consistency checks only.
After a common graph-core change, run the graph family, not every plugin suite.

### Final run

Run L4 once when the candidate stabilizes.
Include all required behavior for every exposed capability.
Use a committed, inspectable selection manifest.
Do not remove failing tests to make that manifest pass.

After a final-run failure, fix the defect and run its affected subset.
Reuse unaffected results only with documented dependency and artifact reasoning.
Rerun the whole final manifest if a shared dependency changes or isolation is uncertain.
A final package change requires a fresh installation smoke test.

## 5. Required living artifacts

Maintain the supplied architecture and acceptance catalog. Do not rewrite them as seven new specifications.
Create only the remaining generated manifests and implementation evidence.

- `Release/CAPABILITIES.json`: reflected operations, support boundaries, effects, release disposition, required tests.
- `Release/ARCHITECTURE.md`: supplied binding contracts. Record exceptions rather than silently rewriting decisions.
- `Release/TEST_MANIFEST.json`: supplied case inventory. Add real selectors, dependencies, commands, and evidence.
- `Release/DECISIONS.md`: owner decisions and unresolved questions.
- `Release/EVIDENCE/`: logs and one concise record per verified fix.
- `Release/CANDIDATE.md`: final candidate identity, install instructions, limitations, and release verdict.

Architecture and the initial test manifest are supplied. Other artifacts are implementation deliverables.
Use null commands and pending status until a real test entry point exists.
Update [progress](<C:/Users/Example/Desktop/TurboMCP/Release/PROGRESS.json>) after every verified fix.
Do not duplicate detailed logs inside the progress ledger.

## 6. Ordered execution

Read only the listed audit sections and direct code dependencies for each step.
The finding identifiers use namespaces `G`, `D`, and `R` from the technical reports.

### S00 — Record baseline and reconcile scope

**Inputs:** all audit headings, current declarations, current board metadata.

- Record plugin revision, engine revision, dirty files, and owned processes.
- Preserve the six dirty source files identified by the original audit if they still exist.
- Recount declarations and suites. Do not trust historical counts.
- Refresh relevant board metadata if the board is available.
- Otherwise use the supplied snapshot and mark it stale.
- Map every finding and card to a canonical ledger item.
- Apply ARC01 and ARC09 without reopening their scope decisions.
- Request approval only for additional exclusions.
- Create or reuse the release tracking root and the first bounded leaf card.
- Create the minimal capability and decision manifests.

**Tests:** L0 only. Do not run a baseline full suite.

**Exit:** every supplied finding and board card has an owner step or a tracked decision.

### S01 — Repair verification verdicts

**Covers:** A01, R01, R04.

- Preserve child exit codes and complete logs.
- Fail required blocked, timed-out, missing, and empty selections.
- Reject unknown suite names, including partial mismatches.
- Fix default-parameter parsing in the coverage audit.
- Correct false positives for repoint and swap assertion styles.
- Keep source guards separate from runtime acceptance.

**Tests:** L1 fake children and tiny temporary inputs.
Test PASS followed by exit 7, blocked-only, zero matches, mixed valid/invalid names, timeout, and normal success.

**Exit:** the harness cannot approve incomplete or failed required work.

### S02 — Remove ambiguous mutation replay

**Covers:** A02, R02.

- Separate reconnect from request replay.
- Return an uncertain outcome after ambiguous mutation failure.
- Do not replay project-code execution as if it were a pure read.
- Document recovery without claiming server-side deduplication.

**Tests:** L1 fake server or mocked transport.
Apply once, discard the response, and verify exactly one effect.

**Exit:** no automatic duplicate mutation after response loss.

### S03 — Make the environment and builds safe

**Covers:** A11 baseline, A12, R08–R11.

- Verify editor, project, port, fixture, and build ownership.
- Remove global process-name termination from operational paths.
- Preserve recovery snapshots until restoration passes.
- Validate mount targets and declared plugin dependencies.
- Pin the supported engine identity.
- Remove token-in-URL instructions.
- Fail a missing plugin artifact after a successful build command.

**Tests:** L1 process and filesystem fakes first.
Then L2 and one L3 registration smoke on the current candidate.
Use real ownership checks before running existing lifecycle scripts.

**Exit:** later steps can build and test without damaging unrelated sessions.

### S04 — Implement the fixed shared contracts

**Covers:** DOC1–DOC5, R05–R07, G05, G07, G11, G17.

- Implement the typed internal result and ARC02 public formatter/parser.
- Keep legacy text rendering through the compatibility client method.
- Implement the small shared mutation coordinator from ARC03.
- Add fingerprint-bound diagnostic baselines from ARC04.
- Add owned transaction and rollback verification boundaries from ARC05.
- Use existing engine APIs rather than a new general editing framework.
- Compile helpers now. Migrate each writer during its following family step.
- Do not claim unmigrated writers satisfy the coordinator contract.

**Tests:** L1 result/diagnostic cases and L2 shared-helper build.
The first migrated real writer must pass M cases before its leaf closes.
Complete result migration for every exposed tool by S14.

**Exit:** later fixes use the selected contracts instead of choosing their own architecture.

### S05 — Repair graph identity and representation

**Covers:** A07, G10, G13–G16, related grammar and reader cards.

- Preserve stable identity through collisions and accepted ID forms.
- Emit shared pure-node definitions where the format requires them.
- Correct reversible quoting and pin endpoint representation.
- Refuse destructive use of incomplete summaries.
- Refuse ambiguous type resolution and unsupported creation.
- Handle wildcard resolution without silent guessing.
- Implement the edit export header and explicit deletion directives from ARC07.
- Apply matching-fingerprint checks before destructive edits.

**Tests:** L1 parser fixtures first.
Then L2 and focused L3 graph check/read/round-trip cases.
Start with one tiny graph per defect.
Compare engine topology independently from the serializer under test.

**Exit:** supported representation contracts pass. Unsupported cases refuse explicitly.

### S06 — Remove preview mutations

**Covers:** A09 graph/material work, G01, D1, R05.

- Move graph probes out of live graphs.
- Remove material recompilation from non-mutating planning paths.
- Check tidy, blocks, and label planning separately.
- Preserve already-dirty assets rather than clearing their flags blindly.

**Tests:** focused L3 accepted and refused previews.
Compare graph state, fields, dirty flags, undo history, and relevant UI state.

**Exit:** included non-mutating previews match their declared effect contract.

### S07 — Fix patch semantics

**Covers:** A04, G02, G03, relevant G05/G11 paths.

- Call Modify before changing state.
- Validate defaults without first assigning raw text.
- Apply links or refuse the entire request before writing.
- Check compile and rollback results.

**Tests:** focused L3 literal change, invalid literal, mixed link/literal, one undo, and injected failure.

**Exit:** all requested changes apply together or the original state returns exactly.

### S08 — Fix replacement transaction ownership

**Covers:** A05, G04, G05, G07, G11.

- Extract a transaction-free append core where necessary.
- Give replacement one outer transaction.
- Check readiness before any mutation.
- Compile and compare before success.
- Report rollback failure truthfully.

**Tests:** focused L3 additions plus removals, one undo, and failure between phases.
Run direct append regressions after the shared-core extraction.

**Exit:** replacement is one reversible memory operation.

### S09 — Preserve swap semantics

**Covers:** A06 swap, G06, G11.

- Preserve unlinked literal, object, and text defaults.
- Preserve exact links and supported pin semantics.
- Refuse unsupported conversions before mutation.
- Verify the standalone compile policy after the dirty refactor.

**Tests:** focused L3 same-type swap with custom defaults, incompatible pins, undo, and failure rollback.

**Exit:** a supported swap does not silently change unrelated behavior.

### S10 — Preserve repoint semantics

**Covers:** A06 repoint, G08, G09.

- Handle occupied data inputs through the correct schema response.
- Preserve unrelated source outputs and exec links.
- Refuse unsafe source deletion.
- Check plan freshness before applying.

**Tests:** focused L3 occupied input, fan-out, multiple outputs, keep/delete modes, and undo.

**Exit:** the exact requested readers move without unrelated deletion.

### S11 — Correct data and field semantics

**Covers:** A15, D6–D8, relevant signature/type cards.

Work on table order, variable access, and struct defaults one at a time.

- Preserve documented operation order for repeated table rows.
- Use Blueprint private metadata instead of EditConst.
- Check default setters and compare actual defaults.
- Report or roll back partial writes according to the contract.
- Check dependent assets for type/signature changes when applicable.

**Tests:** focused L3 set/remove permutations, external access, invalid defaults, and later-operation failure.
Use L1 parsing cases before rebuilding when applicable.

**Exit:** requested field semantics match the resulting asset.

### S12 — Verify remaining mutation and persistence paths

**Covers:** A10, D9, G17, material/data/widget/source-control risks in the domain report.

- Review one writer family at a time.
- Check transaction participants before mutations.
- Compare material topology and properties, not only expression keys.
- Check rollback return values everywhere touched by the repair.
- Verify supported save paths through reload or equivalent durable evidence.
- Keep source-control failure distinct from compile and save failure.

**Tests:** smallest L3 fixture for each changed family.
Include a failure after the first mutation and one undo.
Do not claim provider support from refusal tests alone.

**Exit:** each included writer has the required preservation evidence.

### S13 — Correct references, trace, and play evidence

**Covers:** A08, D3–D5.

Fix references, trace, and play one at a time.

- Make literal both agree with explicit in/out queries.
- Use the executing bytecode location for trace attribution.
- Distinguish unmapped execution from no execution.
- Require observed PIE startup before reporting played.
- Report actual observed duration and explicit startup failure.

**Tests:** one known reference edge, two trace nodes, and delayed/refused PIE startup.
Use L1 lifecycle tests where extraction preserves real production behavior.
Use focused L3 for engine mapping and play witnesses.

**Exit:** runtime evidence cannot report success without the required observation.

### S14 — Make execution effects, help, and logs truthful

**Covers:** A09 widget work, A13, D2, R05–R07, R12, R13.

- Separate widget rendering from project-code execution.
- Add and enforce Execution=deny/allow exactly as ARC10 specifies.
- Validate consent before widget construction, not only before a named callback.
- Do not claim transient allocation provides security isolation.
- Add bounded discovery and schema-derived help.
- Preserve operation failure status through public results and the supplied client.
- Check logging failures and document retention/privacy.
- Verify local binding and document trusted-client limits.

**Tests:** L1 schema/result/log cases first.
Use focused L3 widget side-effect and help integration cases.
Verify actual listening address once on the candidate.

**Exit:** agents can determine outcomes and side effects without guessing from prose.

### S15 — Resolve remaining domain gaps and board cards

**Covers:** A14, R14, all unmatched audit risks and board items.

- Read the coverage matrix in each technical report.
- Check remaining capture, geometry, widget traversal, material-instance, cost, resolver, enum, and signature risks.
- Fix release-relevant defects with focused tests.
- Reconcile all six historical done/failed cards against current evidence.
- Treat bypassed verification as missing evidence unless independent proof exists.
- Map duplicates without losing their acceptance conditions.
- Apply the fixed backlog boundaries from ARC01/ARC09.
- Request approval only for additional limitations or architectural exceptions.
- Close verified cards and leave deferred feature cards in backlog.

**Tests:** only the focused cases needed for new repairs or unsupported completion claims.
Do not repeat proven unchanged cases merely because a card mentions them.

**Exit:** no finding or card lacks an explicit, justified disposition.

### S16 — Implement the restricted single-graph batch

**Covers:** A03, G12, R03, the release subset of T12.

- Implement ARC09, not general multi-asset editing.
- Accept only graphRepoint operations in one Blueprint graph.
- Refuse cross-asset, cross-graph, unknown-kind, and unknown-field requests before mutation.
- Use one transaction and one final compile.
- Re-resolve operations after preceding changes.
- Verify full rollback after any failure.
- Do not save from the batch.
- Keep broad T12 in backlog and close only the verified subset card.

**Tests:** B01–B05 against a small graph fixture.

**Exit:** the restricted batch passes its complete contract.

### S17 — Produce release documentation and package

**Covers:** A11 final installation, DOC6/DOC7 documents, R08/R09/R14.

- Generate counts and support tables from the current capability manifest.
- Replace stale current-state claims with verified facts.
- Preserve historical documents as dated evidence.
- Write a short installation and recovery procedure.
- Produce the intended local distributable or source candidate.
- Include licenses and required plugin content.
- Exclude credentials, local machine artifacts, and private evidence from public packaging.
- Do not distribute engine source or binaries without checking applicable permissions.

**Tests:** L0 links, schema consistency, package contents, and secret-pattern checks.
Perform one clean minimal-project installation smoke.
Test the intended folder naming and path-with-spaces case together when possible.

**Exit:** the package installs through the documented procedure.

### S18 — Final acceptance and release verdict

**Covers:** all included capabilities and all release gates.

- Freeze the candidate revision or patch fingerprint.
- Record engine, toolchain, artifact hashes, and loaded module identity.
- Run the required final manifest once.
- Confirm no required failure, block, unknown selection, or missing evidence.
- Review the diff in a separate pass.
- Check every finding and board disposition.
- Prepare release notes and exact publication commands.
- Stop before publication unless the owner explicitly authorized it.

**Exit:** report RELEASE_CANDIDATE_READY or NOT_READY.
Name every remaining blocker if not ready.
Do not call an unverified candidate released.

## 7. Evidence record

Use this compact structure per fix:

```text
Step and finding IDs:
Source revision or patch fingerprint:
Observed failure:
Correction:
Changed files:
Test IDs and exact commands:
Exit codes and elapsed time:
Engine/artifact identity, when applicable:
Independent state compared:
Undo and rollback result, when applicable:
Persistence result, when applicable:
Evidence paths:
Unverified limits:
Review result:
Next action:
```

## 8. Stop conditions

Stop mutation when ownership, fixture safety, or recovery is uncertain.
Do not kill another session to resolve contention.
Record a concrete blocker and continue only independent safe work.

Do not stop merely because the first defect is difficult.
Do not stop after writing a plan when implementation is authorized.
Do not repeatedly run a failing full suite without a new hypothesis.
Do not weaken contracts to meet an arbitrary calendar date.

## 9. Release-ready checklist

- Every supplied finding has a valid disposition.
- Every relevant board card maps to canonical work or an approved deferral.
- Verified work has an evidence-backed board outcome. Deferred features remain backlog, not done.
- Every exposed capability has current acceptance evidence.
- Required test failures and blocks produce nonzero release status.
- Uncertain writes do not replay automatically.
- Included writers satisfy their undo, rollback, compile, and persistence contracts.
- Previews satisfy their declared effects.
- Runtime reports require actual observations.
- The candidate installs in the documented minimal environment.
- Documentation matches the manifest and the artifact.
- Existing user work remains intact.
- Publication remains a separate authorized action.
