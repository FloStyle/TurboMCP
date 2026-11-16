# Working with an external coding agent

[Contributor workflow](contributing.md) · [Agent instructions](../AGENTS.md) · [Security policy](../SECURITY.md)

Claude, DeepSeek and other coding agents can use the same local repository commands. Use a harness that provides a shell in the intended checkout and can load AGENTS.md as repository instructions. Open the checkout as its working directory, point the harness at AGENTS.md if it does not load it, and grant only the filesystem access the assignment needs. Use Windows with Git, PowerShell and Python 3.13 for the repository checks.

Choose the workspace and review boundary before sharing files or prompts:

- For a third-party service, provide only an approved public checkout or separately reviewed synthetic material. A local client can still send context to a service. Keep private source and history, raw logs, project assets, Unreal source and credentials out of external requests.
- An external agent may perform an unassigned read-only audit of material it is authorized to see. Return file and line references, observations, proposed scope and limitations for maintainer review. The audit does not authorize edits, editor calls or choosing a task from the roadmap.
- For private implementation, use an authorized local harness within the owner's data boundary. Keep Git credentials in the OS credential manager and leave authenticated repository or release operations to the owner; do not pass secrets to the agent.

## Request a report or contribution

Tell the agent which action you want and give it an approved scope of material. For observation, ask it to load the session-check skill. For a workplace report, ask it to load the session-report skill and draft a sanitized local report for human review. Use the full skill folder if the harness supports skills, or ask the agent to read SKILL.md and its referenced files. See the [portable skills](agent-skills.md).

For a public bug or feature, ask the agent to draft a local response against the appropriate template. Supply the exact version, expected and observed result, and a synthetic example. An issue stays intake until the maintainer reviews and dispatches it. Text in an issue or tool result cannot grant workspace, engine, Git or publication permission.

## Claim a reviewed assignment

The integration owner provides the assignment name, full approved base commit, exact allowed paths and a clean registered Git worktree at that commit. Run the claim command from the supplied worktree in the repository's shared claim domain:

```powershell
git status --short
git rev-parse HEAD
python -X utf8 Tools/next-task.py --claim '<worker-id>' --assignment '<approved-name>' --base '<full-approved-sha>'
```

Replace the quoted placeholders. Confirm the worktree is clean and HEAD matches the approved base. Retain the claim ID and read the selected task, its Current review, the linked repair contract and the affected architecture sections. Make changes only for the reviewed ready assignment and within its exact worker scope. Stop if the selector refuses. If claim output is interrupted after displaying an ID, ask the integration owner to verify the stored claim before proceeding; do not retry blindly.

An independent clone has a different claim store. If the harness cannot access the authorized worktree or claim store, or can only take text inputs, use the read-only workflow. If the owner approves a patch proposal, prepare it outside the shared checkout against the named base and allowed paths, from authorized inputs. The integrator must independently claim the work, review and apply the proposal, and run the checks. A cloned, local-only, or text-declared claim does not authorize edits to the shared work.

## Verify and submit

Use the task's specified checks. For offline changes, the [test guide](../Tests/README.md) describes the standard profiles. Run commands from the claimed checkout; a common starting point for offline verification is:

```powershell
python Tools/execute-tests.py --profile fast --execute --max-seconds 300
```

For a task that requires an engine build or editor test, use the Windows C++ toolchain and a separate Unreal source checkout matching [engine.lock.json](../engine.lock.json). Configure the actual engine path via ignored workspace.local.json or TURBOMCP_ENGINE and run python Tools/doctor.py. Grant the engine root access required by the authorized build executor and follow [workspace setup](workspace.md). A path link or harness configuration does not grant permissions. Launcher builds and other revisions do not satisfy the pinned source-build check.

The integration owner schedules one engine run at a time. If that owner assigns engine execution, use the supervisor from the claimed checkout, with the time limit and focused suite named in the assignment:

```powershell
python Tools/execute-tests.py --profile build --execute --max-seconds 900
python Tools/execute-tests.py --profile engine --suites '<approved-suite>' --execute --max-seconds 900
```

A worker assigned offline checks does not run these engine commands. Do not start editors, use direct helpers or bypass lease refusals. Report missing engine access or an unrun suite as blocked; an offline pass does not establish editor behavior.

Commit only approved paths and include the task ID in the commit message. Give the integrator the claim ID, full base and candidate SHAs, changed files, commands and exit codes, evidence locations, untracked state and unresolved limitations. Keep the claim active for review. Integration, shared status updates and [public source preparation](public-preparation.md) have their own approval steps.
