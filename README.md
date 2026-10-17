# TurboMCP

Editor tools for working with Unreal Engine through MCP clients: inspect Blueprints, edit graphs, work with materials and widgets, and capture editor diagnostics.

**Development preview.** The baseline is Windows with the pinned Unreal Engine 5.8.3 source build. There is no approved stable release yet. Installed-engine and 5.9 compatibility are being evaluated; see [project status](docs/status.md).

## Start here

| I want to… | Start with |
| --- | --- |
| Try the plugin | [Installation](docs/user/installation.md), then the [tool reference](docs/user/tools.md) |
| Understand the project | [Documentation](docs/README.md) and [product scope](docs/product.md) |
| Contribute a fix | [Contributing](CONTRIBUTING.md) |
| Assign work to an agent | [Agent instructions](AGENTS.md) and the [roadmap](ROADMAP.md) |
| Prepare a release | [Release guide](docs/release.md) |

## Development quick start

Use Python 3.13, Git and PowerShell. From the repository root:

```powershell
python Tools/validate.py
python Tools/next-task.py
```

For builds and editor tests, follow [workspace setup](docs/workspace.md). Unreal Engine lives in a separate checkout; `engine.lock.json` identifies the baseline. Offline validation checks repository and test infrastructure. Each implementation task also specifies its required engine tests.

## Repository layout

| Directory | Contents |
| --- | --- |
| `Plugins/TurboMCP` | Plugin source, content and resources |
| `TestProject` | Integration project and fixtures |
| `Tools` | Build, test and release commands |
| `Tests` | Test catalog and acceptance requirements |
| `docs` | Guides, specifications, designs and audit results |
| `todo` | Implementation tasks and historical findings |
| `release` | Release readiness and export policy |

The future public repository will contain this complete tracked tree with fresh Git history. Installable plugin packages are separate release assets. See [repository design](docs/repository-design.md).

## Community and security

[Contributing](CONTRIBUTING.md) · [Governance](docs/governance.md) · [Security policy](SECURITY.md) · [Community conduct](CODE_OF_CONDUCT.md)

## License

[MPL-2.0](LICENSE).
