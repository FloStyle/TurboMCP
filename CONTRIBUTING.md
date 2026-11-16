# Contributing

[Repository home](README.md) · [Documentation](docs/README.md)

## Propose a contribution

Bug reports, tests, documentation corrections and focused fixes are welcome. Use an issue to describe the problem and a minimal reproduction; discuss new features before implementing them. Maintainers turn accepted work into repository tasks. See [governance](docs/governance.md) for review responsibilities and [community conduct](CODE_OF_CONDUCT.md).

Use the bug or feature issue template and include only synthetic or approved public examples. Report vulnerabilities privately through [SECURITY.md](SECURITY.md). You remain responsible for reviewing and verifying AI-assisted contributions.

## Choose work

Follow the [roadmap](ROADMAP.md). To find the next approved task:

```powershell
python Tools/next-task.py
```

Read the returned task and its linked design. Stop if the selector reports a blocker. Historical findings in [TODO.md](TODO.md) need reconciliation before implementation. An issue or a selector preview does not assign work. Get the reviewed assignment, exact scope, approved base and durable claim before editing. See the [external agent workflow](docs/external-agent-workflow.md) for the contributor CLI and portable skill entry points.

## Make and verify a change

1. Follow [AGENTS.md](AGENTS.md), which defines the shared workflow for humans and agents.
2. Configure the [workspace](docs/workspace.md) and use the assigned branch and worktree. Follow the [Git workflow](docs/git-workflow.md) for the development, release and compatibility branches.
3. Keep code, regression tests and affected documentation together.
4. Run `python Tools/validate.py` and the task's focused checks. Record unavailable engine tests as blocked.
5. Submit the candidate commit, exact base, changed files, checks and limitations. The integrator reviews the submission and records shared task evidence, status and index updates.

See the [contributor workflow](docs/contributing.md) for ownership, specification review and completion requirements.

## Write useful documentation

Write in English, with a clear purpose and short, direct sentences. Explain what a reader needs to do before describing implementation details. Use working examples and descriptive links. Keep requirements, observed behavior and unresolved questions distinct.

Maintain one source of truth for each topic. Link to it rather than repeating its rules. Preserve stable task and contract IDs. Update a generator when changing generated documentation, then regenerate its output. Historical evidence may be collapsed for readability but must remain available.

Every tracked file is intended for the future public source repository. Keep credentials, machine configuration and private recovery material outside Git.
