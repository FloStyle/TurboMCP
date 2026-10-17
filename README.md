# TurboMCP

TurboMCP extends Unreal Engine's MCP tooling with editor tools for Blueprint graphs, materials, widgets, inspection, capture and diagnostics. The plugin is currently **experimental, 1.0.0-dev**. The v1 acceptance work is unfinished.

Development baseline: Windows and the exact Unreal Engine 5.8.3 source revision in engine.lock.json. Installed-engine and 5.9 support are separate validation targets.

## Start here

- Using the plugin: [installation](docs/user/installation.md) and [declared tools](docs/user/tools.md).
- Planning and next work: [ROADMAP.md](ROADMAP.md); run `python Tools/next-task.py`.
- Contributing or running an agent: [AGENTS.md](AGENTS.md), [contributor workflow](docs/contributing.md), then [TODO.md](TODO.md).
- Understanding the product: [requirements](docs/product.md), [technical contracts](docs/architecture.md) and [design index](docs/design/README.md).
- Setting up a checkout: [workspace setup](docs/workspace.md) and [test layers](Tests/README.md).
- Preparing a release: [release process](docs/release.md).

## Quick validation

From the repository root, with Python 3.13, Git and PowerShell available:

```powershell
python Tools/validate.py
```

For engine work, copy workspace.example.json to ignored workspace.local.json, configure the engine root, then run:

```powershell
python Tools/doctor.py
pwsh -File Tools/build.ps1 -Engine <engine-root>
```

Offline checks do not certify runtime behavior. Follow the selected task's engine and fixture verification requirements.

## Source layout

| Directory | Purpose |
| --- | --- |
| Plugins/TurboMCP | Installable editor plugin source, content and resources |
| TestProject | Integration host and versioned fixtures |
| Tools | Build, test, documentation and release commands |
| Tests | Test catalog and product acceptance specifications |
| docs | User guides, product requirements, technical contracts and designs |
| todo | Bounded tasks and historical findings awaiting reconciliation |
| release | Release gates and complete-tree publication policy |

The private development tree and future public source tree have the same files and paths. Public publication starts fresh Git history from a verified copy of this complete tree. Plugin binary packages are separate release assets. Unreal source, credentials, generated outputs and private recovery archives do not belong in the tracked tree.

Licensed under [MPL-2.0](LICENSE). See [repository design](docs/repository-design.md) for boundaries.
