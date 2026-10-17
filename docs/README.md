# Documentation

[Repository home](../README.md)

## Use TurboMCP

- [Installation and connection](user/installation.md): prerequisites, project setup and first use.
- [Tool reference](user/tools.md): tools declared by the plugin, grouped by toolset.
- [Troubleshooting](user/troubleshooting.md): connection, plugin loading and unexpected results.
- [Project status](status.md): release and compatibility limits.

## Contribute

Start with [Contributing](../CONTRIBUTING.md), then [workspace setup](workspace.md). For assigned work, read the task and its linked design rather than the entire documentation tree.

- [Roadmap](../ROADMAP.md): priorities and approved execution order.
- [Task index](../TODO.md): queued work and historical findings.
- [Contributor workflow](contributing.md): ownership, review and completion evidence.
- [Test guide](../Tests/README.md): test layers and commands.
- [Tooling guide](../Tools/README.md): repository scripts.

- [Governance](governance.md), [security policy](../SECURITY.md) and [community conduct](../CODE_OF_CONDUCT.md).
- [English-language policy](language.md): rules, automated checks and review limits.

- [Reusable agent skills](agent-skills.md): field reporting, session checks and maintainer triage.

## Understand the design

| Document | Question it answers |
| --- | --- |
| [Product scope](product.md) | What are we building, and for whom? |
| [Architecture contracts](architecture.md) | What behavior must the plugin guarantee? |
| [Design index](design/README.md) | How are responsibilities divided? |
| [Acceptance requirements](acceptance.md) | What must be demonstrated before release? |
| [Repository design](repository-design.md) | Where do source, fixtures and local files belong? |
| [Audit findings](audit/findings.md) | What gaps have been identified? |
| [Capability inventory](audit/capabilities.md) | Where is each declared tool implemented and referenced by tests? |
| [Technical decisions](audit/technical-decisions.md) | Which implementation choices need attention? |

Architecture and design describe required behavior. The audit and acceptance evidence establish what the current implementation actually does.

- [Implementation handoff](agent-handoff.md): next assignment and review evidence.
- [Tool selection](design/tool-selection.md) and [editor experience](design/editor-experience.md): planned discovery and UI behavior.

## Compatibility and releases

- [Epic 5.8.3 / TurboMCP / Epic 5.9 comparison](tool-comparison.md).
- [Engine compatibility](engine-compatibility.md) and [native feature comparison](audit/native-overlap.md).
- [Installed-engine investigation](installed-engine.md).
- [Release guide](release.md) and [release manager checklist](release-manager.md).

Agent workflow is defined in [AGENTS.md](../AGENTS.md). Historical task evidence does not override current contracts or authorize additional work.
