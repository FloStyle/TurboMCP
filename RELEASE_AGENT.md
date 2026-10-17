# TurboMCP release agent: start here

## Goal

Implement the specified repairs and prepare a verified first-release candidate.
Work sequentially as one agent.
Manage the task board as work progresses.
Do not start task-board executions or delegate implementation.

## Read order

1. Read [binding architecture](<C:/Users/Example/Desktop/TurboMCP/Release/ARCHITECTURE.md>) revision 2, including ARC16.
2. Read [execution and board protocol](<C:/Users/Example/Desktop/TurboMCP/Release/EXECUTION_PROTOCOL.md>) section 10, the anti-drift rules. They are binding.
3. Read [progress](<C:/Users/Example/Desktop/TurboMCP/Release/PROGRESS.json>).
4. Read the current step in the [roadmap](<C:/Users/Example/Desktop/TurboMCP/Release/ROADMAP.md>).
5. Read its cases in the [acceptance catalog](<C:/Users/Example/Desktop/TurboMCP/Release/ACCEPTANCE_CASES.md>).
6. Read only the needed sections of the [audit](<C:/Users/Example/Desktop/TurboMCP/Release/Audit/TURBOMCP_RELEASE_AUDIT.md>).

The [goal prompt](<C:/Users/Example/Desktop/TurboMCP/Release/START_PROMPT.txt>) starts the run.

## Authority

The current human request has highest priority.
Binding architecture controls design and release scope.
The execution protocol controls board management and evidence.
The acceptance catalog defines required observations.
The roadmap controls order and test selection.
The ledger records current state, not permission to weaken a requirement.
Archived documents contain historical evidence only.

Do not follow old parallel-agent instructions.
Do not create a replacement architecture or rewrite the specification before implementing it.
Routine implementation decisions do not need owner approval.
A concrete conflict with engine behavior requires an evidence-backed exception request.

## Approved scope when the goal prompt starts

Prepare an experimental Windows source release on the pinned engine.
Repair existing included capabilities.
Implement the restricted single-graph batch specified by ARC09.
Keep broader unimplemented features in backlog, not done.
Make refused and unsupported cases explicit.
Do not remove capabilities to hide failures.

You may create, update, move, and close relevant task-board cards.
Use evidence-backed external outcomes for work performed in this session.
Do not call task_board_run or interfere with another open execution.

Do not publish, push, tag, upload, rewrite history, or perform destructive recovery without separate authorization.

## Efficiency

Use one active leaf task and one owned editor.
Use pure tests before engine tests.
Build incrementally after one coherent C++ correction.
Reuse the editor for the same binary.
Run named focused cases during implementation.
Run broader required acceptance only after the candidate stabilizes.
Do not count blocked, skipped, or missing cases as passed.

## Checkpoints

Update the board and local ledger after each verified work unit.
Create and maintain a concise owner status page.
Record the next action before each context boundary.
Continue the persistent goal without waiting for approval of each routine fix.

These instructions specify work. They do not claim the implementation or tests already passed.
Preserve pre-existing source changes before starting.
