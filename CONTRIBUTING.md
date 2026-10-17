# Contributing

[Repository home](README.md) · [Documentation](docs/README.md)

## Choose work

Follow the [roadmap](ROADMAP.md). To find the next approved task:

```powershell
python Tools/next-task.py
```

Read the returned task and its linked design. Stop if the selector reports a blocker. Historical findings in [TODO.md](TODO.md) need reconciliation before implementation.

## Make and verify a change

1. Follow [AGENTS.md](AGENTS.md), which defines the shared workflow for humans and agents.
2. Configure the [workspace](docs/workspace.md). Use one branch and worktree per task.
3. Keep code, regression tests and affected documentation together.
4. Run `python Tools/validate.py` and the task's focused checks. Record unavailable engine tests as blocked.
5. Record evidence in the task, regenerate the index with `python Tools/update-todo.py`, and submit the change for integration.

See the [contributor workflow](docs/contributing.md) for ownership, specification review and completion requirements.

## Write useful documentation

Write in English, with a clear purpose and short, direct sentences. Explain what a reader needs to do before describing implementation details. Use working examples and descriptive links. Keep requirements, observed behavior and unresolved questions distinct.

Maintain one source of truth for each topic. Link to it rather than repeating its rules. Preserve stable task and contract IDs. Update a generator when changing generated documentation, then regenerate its output. Historical evidence may be collapsed for readability but must remain available.

Every tracked file is intended for the future public source repository. Keep credentials, machine configuration and private recovery material outside Git.
