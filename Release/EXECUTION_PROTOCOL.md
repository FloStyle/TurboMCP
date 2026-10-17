# Execution protocol for one autonomous agent

## 1. Start and resume

Read the entry instructions and binding architecture once at session start.
Read the active step, its acceptance cases, and its direct source dependencies.
Do not reread every archived document for each fix.

Create or resume one persistent release goal if the host supports it.
Do not create parallel agents or task-board execution sessions.
Do not call task_board_run.
Do not switch models or invoke external implementation agents.

Before work, check the current worktree and owned processes.
Preserve pre-existing dirty changes.
Record a patch fingerprint if commits are not authorized.

## 2. Board setup
Refresh the board before using its snapshot.
Identify the TurboMCP workspace and relevant cards by workspace and scope.
Do not modify unrelated project cards.

Create one root tracking card if no suitable release tracking card exists:

```text
TurboMCP release candidate — sequential implementation and acceptance
```

Use it as a progress container, not as a task runner.
Move it to running while this release effort is active.
Give the root a link to the local progress ledger and evidence directory.
Do not claim a Host execution exists for a manually moved card.

Use one active leaf card at a time.
The root may remain running while its one leaf runs.
The agent is the only implementation worker.

## 3. Reuse or create a leaf card

Call task_board_get before modifying a card.
Reuse an existing card only when its entire requested scope matches the current work unit.
Do not mark a broad card done after implementing a subset.

If scopes differ, create a bounded child under the release root.
Use this title format:

```text
S07 / G02 — Preserve the original pin value through undo
```

Its description contains:

- Finding IDs and original board IDs.
- Exact behavior to change.
- Relevant architecture decision IDs.
- Acceptance case IDs.
- Source files or symbols.
- Non-goals.
- Evidence destination.

Do not expand a single card to several unrelated fixes.
Do not create duplicate cards on resume.
Record every reused or created card ID in the progress ledger.

## 4. Mark work in progress

Before source edits, move the selected leaf to running.
Record its ID and current action locally.
If it has an open Host execution, do not move, settle, or duplicate that execution.
Record contention and continue only unrelated safe work.
Never settle another session merely to obtain a task.

Keep the visible status concise:

```text
Step S07 / G02
Current: add the regression for undo capture
Next: patch mutation order
Tests: H01 verified, GP01 pending
Blockers: none
```

Update visible progress after each test/build result and at each checkpoint.
Do not stream every source read as a progress event.

## 5. Completion and closing

Close a leaf only after every listed acceptance case passes.
Use task_board_manage with action record-external-outcome when this session performed the work.
Set result to succeeded and initiatedBy to the current agent/session identity.
Include evidence paths, source fingerprint, commands, exit codes, and remaining limitations in the summary.
This creates a real external outcome record instead of a bare done declaration.

If the task genuinely fails, record failed with the concrete failure.
A blocked environment is not a successful implementation.
For a transient block, return the manually managed leaf to todo and record the blocker locally.
Do not fabricate a failed execution for work that never ran.

An existing card may already be done with stale or failed evidence.
If current acceptance passes, record the new external outcome.
If current acceptance fails, move the card to todo when no Host execution is open.
Link the new defect or verification card. Do not silently erase old evidence.

Do not alter verification-bypass flags to force completion.
Do not use move-done as a substitute for a verified implementation outcome.

## 6. Deferred and duplicate cards

Feature work excluded by ARC01 or ARC09 goes to backlog with disposition deferred_by_scope.
Keep the original feature card visible. Do not mark it implemented or done.
Record the architecture decision that defines the release boundary.

For a duplicate, record the canonical card ID locally.
Do not claim the duplicate's acceptance conditions passed until the canonical task proves them all.
A history-only note may remain archived. Do not erase its evidence.

Some started cards have immutable descriptions.
Do not repeatedly attempt prohibited edits.
Use an outcome summary or a linked new tracking card for current evidence.

## 7. Local progress contract

Maintain [PROGRESS.json](<C:/Users/Example/Desktop/TurboMCP/Release/PROGRESS.json>) after each work unit.
Maintain a short `Release/STATUS.md` for the owner.
Create STATUS.md on the first checkpoint.

The status page contains:

- Current step and active board card.
- Verified findings / total findings.
- Deferred features and blocked items, counted separately.
- Last build and test results with elapsed time.
- Next concrete action.
- Release verdict.

Never count open, deferred, blocked, or duplicate work as a verified fix.
For a disproved finding, link the evidence that disproves it.

If board tools fail, save the intended updates in a boardSyncQueue field.
Continue local work when safe.
Retry synchronization at the next checkpoint, not in a busy loop.
Do not claim the board is current while updates remain queued.
Board synchronization failure does not falsify technical test results.

## 8. Verification cadence

For one coherent fix:

1. Select the named acceptance cases.
2. Reproduce the failing behavior when practical.
3. Implement the correction.
4. Run pure cases first.
5. Build once if C++ changed.
6. Run the named focused engine cases.
7. Review the diff separately.
8. Save evidence and close the leaf.
9. Proceed to the next work unit.

Do not build after every line edit.
Do not combine unrelated fixes to avoid their individual acceptance.
Reuse the owned editor for the same binary.
Use the smallest owned fixtures and case selectors.

A failure with no new hypothesis does not justify another full run.
Read the diagnostic, identify the failing boundary, and run the smallest discriminating case.

## 9. Fault injection and false-positive control

Acceptance must exercise the implementation under test.
A hand-written fake implementation is not proof of the production algorithm.
A source-string search is not proof of runtime behavior.

Each safety repair needs one negative control.
The reported original defect can serve as that control if reproduced.
Otherwise use a deterministic failpoint or a deliberately invalid input with an exact expected refusal.

Record what would make the test fail.
Do not count assertions that merely check response text exists.
Do not accept matching node counts instead of matching edges and defaults.
Do not bless a changed golden file without checking the intended semantic change.

## 10. Anti-drift rules — added after a release run lost its way

That run spent its middle third on an invented data-loss defect. It proposed eleven mechanisms, disproved all
eleven, and narrowed a safety rule to make a suite pass. These rules are short on purpose. They are checked, not
aspirational.

### 10.1 Never narrow a safety rule to make a case pass

A safety rule exists because a caller can be told something false. If the rule refuses valid work, the rule or the
fixture is wrong, and the owner decides which.

Narrowing a rule changes the product. Report it to the owner. Never do it because a suite is red.
A narrowed rule must never read `compile=passed`, `changed=true`, or "verified" while it excludes the evidence that
would have decided the outcome.

### 10.2 Measure the subject, not a rendering of the subject

To learn what a tool DOES, read its objects, return values, and recorded state.
To learn what a tool SHOWS, read its output.
Never use the second as evidence for the first.

A regex over a tool's human-readable answer measures the answer. It is not the asset, the graph, or the side effect.

### 10.3 Every guard needs a positive control

A check that cannot fail is the most repeated defect in this repository. For each rule that can refuse, show one
input that it MUST refuse and one that it must accept.

State what would make the assertion fail. Record it next to the assertion.

### 10.4 Stop at two disproved mechanisms

Two failed hypotheses on one question means the next step is a measurement, not a third hypothesis.

Write down the smallest reading that separates the surviving candidates. Take that reading before any code changes.
Do not propose a mechanism from reading source and treat it as evidence.

### 10.5 A single run cannot attribute a cause

A suite with a known order dependence cannot settle a question in one run.
Reproduce the failing condition first. Then fix. Then repeat the run.

If the failure does not reproduce, say so. Do not explain it away.

### 10.6 Finish one step before starting the next

The unit of work is one roadmap step.
Do not leave a step "partly verified" and begin a different family.

A step closes when its named cases pass and its evidence exists. Not when a summary says it is close.

### 10.7 Bookkeeping is a real work unit

When context is short, update the ledger, the status page, and the board. Then stop.

Do not begin a multi-file change you cannot verify. A build with no verification budget ships defects.

### 10.8 Never claim more than the evidence shows

Use these exact words for these exact states:

| State | Say |
|---|---|
| Case observed passing, repeatable | verified |
| Implemented, not exercised | implemented, unverified |
| Refusal path unreachable by any fixture | implemented, unexercised |
| Hypothesis disproved | disproved, with the measurement |
| Rule narrowed for any reason | weakened, with the owner decision |

Corrections are expected and are cheap. A wrong claim left standing is expensive.

## 11. End of run

Run final acceptance only after implementation stabilizes.
Reconcile all release-relevant cards and evidence.
Close the root through record-external-outcome only when all required work is verified.
Deferred feature cards remain backlog items.

Report RELEASE_CANDIDATE_READY or NOT_READY.
Give the local package path, evidence manifest, limitations, and blockers.
Stop before remote publication unless the owner separately authorized it.
