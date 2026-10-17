# Implementation handoff and review

[Documentation](README.md) · [Roadmap](../ROADMAP.md)

## Next assignment: R01

Give the implementation agent this instruction:

> Read AGENTS.md, docs/contributing.md and todo/roadmap-r01.md. Implement R01 only. Claim the task with your recorded owner in an isolated worktree. Preserve plugin behavior, engine configuration and roadmap order. Run the specified regression and offline checks. Return the base commit, commit IDs, changed files, failing-before/passing-after evidence, test commands and exit codes, limitations and task-status changes. Do not mark reviewed completion or push to main yourself.

R01 is infrastructure work and needs offline parser evidence. It does not claim a runtime plugin change or require an engine rebuild. Plugin behavior changes require the exact 5.8.3 baseline build and owned-editor evidence; 5.9 source review never substitutes for that evidence.

## Review packet for every agent

- Task ID, base commit and submitted commits; one bounded change.
- Problem, observable before/after behavior and affected contracts.
- Commands, exits, focused regression evidence and offline results.
- For engine work: engine revision, plugin binary hash, loaded plugin identity, fixture/reset evidence and editor ownership.
- Changes to schemas, dependencies, tool preference, UI or packaging called out explicitly.
- Unrun checks and unresolved risks; no claim that source inspection is runtime evidence.
- Suggested task status. The reviewer checks acceptance and approves completion and integration.

The reviewer inspects the diff, verifies evidence, checks scope and regressions, and either accepts or returns a bounded correction request. Only reviewed work enters private main. Concurrent workers need explicitly independent scopes and worktrees; the default selector is serial. Do not launch multiple workers against the same engine outputs or test editor.

## Sequence and priorities

R01 repairs evidence classification. R13 completes the three-way source comparison. R16 establishes the integration and packaging constraints early. R02-R09 provide ownership, fixtures and behavioral audit evidence. R17 makes verified preferences discoverable; R18 produces the UI design; R10 reconciles help and documentation. R11 is baseline acceptance and R15 release automation. Confirmed fixes discovered by investigations require owner-approved bounded repair tasks before their acceptance gate.

R12 separately validates installed-engine packages. R14 handles the 5.9 adapter. Neither optional target inherits baseline certification. Prefer useful native functionality over duplication; preserve TurboMCP workflows where evidence demonstrates a worthwhile improvement.

Review continues when the owner provides a commit, branch or review packet. This document does not install a background monitor or automatically start agents.
