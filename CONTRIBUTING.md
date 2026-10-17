# Contributing

Read [AGENTS.md](AGENTS.md), [contributor workflow](docs/contributing.md), [design index](docs/design/README.md) and the selected task in [TODO.md](TODO.md). These instructions apply to humans and any agent harness.

A ready task is self-contained: problem, current evidence, scope, design, dependencies, acceptance and exact verification commands. An imported finding is not automatically ready. Preserve its ID while reconciling it. Use one branch/worktree per bounded task and keep specification, implementation and test changes together.

Run `python Tools/validate.py` before integration, then the affected engine checks. Record missing evidence honestly. Never weaken tests to obtain a pass. Every tracked file is intended for the public source tree; credentials, local configuration and private recovery material stay outside Git.
