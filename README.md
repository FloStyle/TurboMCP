# TurboMCP

TurboMCP connects Unreal Editor workflows to MCP clients, including AI coding assistants. It extends Unreal's MCP tooling with ways to inspect Blueprints, edit graphs, work with materials and widgets, and examine editor diagnostics.

You work through your MCP client while Unreal Editor hosts the plugin and the project. TurboMCP supplies editor tools; your client supplies the conversation and decides which tools to request.

**Development preview:** Windows with the pinned Unreal Engine 5.8.3 source build is the development baseline. There is no approved stable release yet. Installed-engine and Unreal 5.9 support are still being evaluated. See [project status](docs/status.md) for the current limits.

[Get started](#get-started) · [First use](#first-use) · [Troubleshooting](#troubleshooting) · [Documentation](docs/README.md) · [Contributing](CONTRIBUTING.md)

## What can I use it for?

The plugin declares tools for these workflows. Individual behaviors are still being audited; this overview describes the available tool families, not completed release acceptance.

| Workflow | Examples |
| --- | --- |
| Understand a project | Find assets, inspect Blueprints and graphs, examine references and types |
| Work on graphs | Inspect nodes and connections, plan edits, apply graph changes and adjust layout |
| Inspect materials | Read material outputs, parameters, named reroutes and instance overrides |
| Work with widgets | Inspect widget trees and request previews |
| Investigate behavior | Inspect diagnostics, run play checks and examine trace information |
| Inspect editor state | Check tool registration, unsaved assets, source-control state and graph captures |

Browse the [tool reference](docs/user/tools.md) for exact function names. Ask the connected client to discover the current schemas before making calls. Some operations execute project code or modify assets; a toolset name alone does not establish its effects.

## Get started

### What you need

- Windows and a source build matching [engine.lock.json](engine.lock.json).
- A working Unreal project and the C++ toolchain required to build its editor target.
- An MCP client able to connect to Unreal's MCP server.
- A version-controlled project in which to try experimental editor operations.

TurboMCP requires engine plugins including `ModelContextProtocol` and `ToolsetRegistry`; the full dependency list is in [TurboMCP.uplugin](Plugins/TurboMCP/TurboMCP.uplugin). A Launcher installation is not currently a validated replacement for the pinned source build. See [installed-engine support](docs/installed-engine.md).

### Install and connect

1. Obtain this source checkout. Copy `Plugins/TurboMCP` into your project's `Plugins` directory.
2. Enable TurboMCP and build the project's editor target with the matching engine and toolchain.
3. Open the project in that editor.
4. Enable Unreal's local MCP server and configure your client using the engine-generated connection settings.
5. Have the client discover the available toolsets and schemas.

Follow [Installation and connection](docs/user/installation.md) for the detailed guide and Epic's engine connection instructions. Keep the MCP listener local. TurboMCP runs inside the editor with its permissions; see the [security policy](SECURITY.md).

If you want to modify TurboMCP itself, use the [development setup](#develop-turbomcp) below. The repository's test project discovers the plugin directly from the checkout.

## First use

Start by checking the connection and inspecting one known asset. For example, give your connected assistant this request, replacing the asset path with one from your project:

> Discover the TurboMCP tools and their schemas. Check the plugin's registration status, then inspect the Blueprint at `/Game/MyProject/BP_Example`. Summarize its graphs and any missing information. Do not edit, compile, save, start play mode or execute project code.

This is an example request, not a recorded test result. The client should use discovered schemas rather than guessing argument names. The reference includes `ReadTurboMCPStatus`, `ReadTurboMCPAssetSearch` and `ReadTurboMCPToolHelp` as starting points for discovery and inspection.

Before requesting an edit:

- Identify the exact asset and graph, and state the intended change.
- Ask for a plan and check the tool's effects and refusal conditions.
- Run one mutation at a time and inspect the reported outcome and resulting editor state.
- Treat saving to disk as a separate decision. An HTTP success response does not prove that an edit, compile, rollback or save succeeded.

For the required result and mutation behavior, see [protocol design](docs/design/protocol.md) and [mutation design](docs/design/mutations.md). These define the contracts being verified for release.

## Troubleshooting

| Symptom | Start here |
| --- | --- |
| The client cannot connect or find TurboMCP tools | Check the running project, enabled plugin, local MCP connection settings and tool discovery. See [connection troubleshooting](docs/user/troubleshooting.md#connection-and-discovery). |
| The plugin does not build or load | Check the exact engine revision, required engine plugins and which plugin copy the project discovers. See [build troubleshooting](docs/user/troubleshooting.md#build-and-plugin-loading). |
| A call returns success but the asset looks unchanged | Inspect the operation outcome and target, not only the transport response. See [results and edits](docs/user/troubleshooting.md#results-and-edits). |
| A newer engine or Launcher build behaves differently | Consult [engine compatibility](docs/engine-compatibility.md) and [installed-engine support](docs/installed-engine.md). |

For a bug report, include the plugin commit, engine version and revision, client, tool arguments, expected result and a minimal reproduction. Remove credentials and private project data. Use the repository's bug-report template; report vulnerabilities through [SECURITY.md](SECURITY.md).

## Report field-test feedback

Use the [session-report skill](docs/agent-skills.md) to prepare a local, sanitized report from a work session. It records reproduction steps, observed results and limitations without proprietary code or project identifiers. Reports require human review before transfer; the field tester does not contribute code or submit issues automatically.

## Develop TurboMCP

Use Python 3.13, Git and PowerShell for repository tooling. Keep Unreal Engine in a separate checkout. From the TurboMCP repository root:

```powershell
python Tools/validate.py
python Tools/next-task.py
```

The first command runs offline validation. The second prints the next approved task; it does not execute or claim it.

For engine work, copy `workspace.example.json` to ignored `workspace.local.json` and set `engineRoot` to your source-engine checkout. Then run:

```powershell
python Tools/doctor.py
pwsh -File Tools/build.ps1 -Engine <engine-root>
```

Replace `<engine-root>` with your actual engine path. Follow [workspace setup](docs/workspace.md) for harness access and build ownership, and the [test guide](Tests/README.md) for engine checks. Offline checks alone do not establish runtime correctness.

Contributors and implementation agents should read [CONTRIBUTING.md](CONTRIBUTING.md) and [AGENTS.md](AGENTS.md), then work from the selected task and linked design. The [roadmap](ROADMAP.md) defines execution order; the [task index](TODO.md) also preserves historical findings that need reconciliation.

## Find the right documentation

| Question | Guide |
| --- | --- |
| Where is the complete documentation index? | [Documentation home](docs/README.md) |
| How does TurboMCP compare with Epic tools? | [Three-way comparison](docs/tool-comparison.md) |
| What tools are declared? | [Tool reference](docs/user/tools.md) |
| What is supported and what remains unfinished? | [Project status](docs/status.md) and [roadmap](ROADMAP.md) |
| What is the intended product scope? | [Product requirements](docs/product.md) |
| How should tools behave internally? | [Architecture contracts](docs/architecture.md) and [design index](docs/design/README.md) |
| How are changes verified? | [Test guide](Tests/README.md) and [acceptance requirements](docs/acceptance.md) |
| Which scripts should I use? | [Tooling guide](Tools/README.md) |
| How will source and plugin packages be released? | [Release guide](docs/release.md) |

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

## Community and license

Contributions and project discussions use English. See [Contributing](CONTRIBUTING.md), [Governance](docs/governance.md), [Security](SECURITY.md) and [Community conduct](CODE_OF_CONDUCT.md).

TurboMCP is licensed under [MPL-2.0](LICENSE).
