# Implementation handoff and review

[Documentation](README.md) · [Roadmap](../ROADMAP.md)

## Handoff is paused

The integration owner is completing the infrastructure before implementation
agents resume. See the [infrastructure review](audit/infrastructure-review.md).
Do not dispatch the previous R01 assignment or choose a product task. The bounded
offline executor is available; unattended engine execution remains disabled.

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

R01 repairs evidence classification. R02 and R03 establish ownership and fixture recovery before any handoff. R13 then completes the source comparison and R16 establishes integration and packaging constraints. R04-R09 provide behavioral audit evidence. R17 makes verified preferences discoverable; R18 produces the UI design; R10 reconciles help and documentation. R11 is baseline acceptance and R15 release automation. Confirmed fixes discovered by investigations require owner-approved bounded repair tasks before their acceptance gate.

R12 separately validates installed-engine packages. R14 handles the 5.9 adapter. Neither optional target inherits baseline certification. Prefer useful native functionality over duplication; preserve TurboMCP workflows where evidence demonstrates a worthwhile improvement.

Review continues when the owner provides a commit, branch or review packet. This document does not install a background monitor or automatically start agents.
