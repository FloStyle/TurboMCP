# TurboMCP

More context and more tools for Unreal Editor MCP workflows.

TurboMCP is an addon to stock Unreal MCP. It helps an AI assistant inspect complex Blueprints, understand project data, make focused edits and investigate failures. It keeps Unreal's native toolset structure and uses the editor's APIs.

**1.0.0 stable:** Windows with the pinned Unreal Engine 5.8.4 source build (`f45729c0`). Launcher builds and Unreal 5.9 are not certified yet. See the [1.0.0 release](https://github.com/FloStyle/TurboMCP/releases/tag/v1.0.0).

**[Website and getting started](https://flostyle.github.io/TurboMCP/)** | [Releases](https://github.com/FloStyle/TurboMCP/releases) | [Open issues](https://github.com/FloStyle/TurboMCP/issues) | [1.0.0 plan](docs/release-1.0.0.md) | [User wiki](https://github.com/FloStyle/TurboMCP/wiki) | [Documentation](docs/README.md)

## What it adds

| Area | Examples |
| --- | --- |
| Read and understand | Blueprint graphs, nodes, pins, references, asset search and diagnostics |
| View | Graph captures, widget previews and editor state |
| Edit | Targeted graph changes, layout, supported batch edits and cleanup |
| Data | Data Assets, class defaults, DataTables and row editing |
| UI and materials | Widget trees, material parameters and related asset information |
| Debug and guide | Tool help, call logs, contextual errors and bounded play-input checks |

Read, view and edit are workflow groups, not permission levels. Some calls execute project code or change editor state. Discover the current schemas and read the tool's effects before using it. The [tool reference](docs/user/tools.md) describes the available tools; registration does not mean every behavior has release acceptance.

## Install and connect

1. Use Windows, the C++ toolchain and a source engine matching [engine.lock.json](engine.lock.json).
2. Install the plugin one of two ways:
   - **From the 1.0.0 ZIP.** Download the package from the [1.0.0 release](https://github.com/FloStyle/TurboMCP/releases/tag/v1.0.0) and extract it so that `Plugins/TurboMCP` sits inside your project. The prebuilt binary matches only the engine build that produced it, identified by its BuildId `ad399df4-fb25-4fc8-af97-38e7d74715d2`. On your own 5.8.4 source build Unreal skips that binary and rebuilds TurboMCP from the included `Source` folder with your C++ toolchain, so the first editor build takes longer.
   - **From source.** Copy `Plugins/TurboMCP` from a checkout of this repository into your project's `Plugins` directory and build your editor target.
3. Open **Edit > Editor Preferences > Plugins > TurboMCP**, enable **TurboMCP**, then restart the editor. This per-user preference defaults to off.
4. Enable Unreal's local MCP server and use its generated connection settings in your client.
5. Ask the client to discover the toolsets and inspect a known asset before making edits.

The plugin requires Unreal's `ModelContextProtocol` and `ToolsetRegistry` plugins. See [installation](docs/user/installation.md), [troubleshooting](docs/user/troubleshooting.md) and [security](SECURITY.md). Launcher installation is a separate, unfinished compatibility check.

Example first request:

> Discover the TurboMCP tools and read the Blueprint at `/Game/MyProject/BP_Example`. Summarize its graphs and missing information. Do not edit, compile, save or start play mode.

Use version control. Check the reported operation outcome and read the result back after edits. Treat compilation, saving and rollback as separate outcomes.

## Branches and releases

| Branch | Role |
| --- | --- |
| `main` | Active development and reviewed fixes. Successful pushes can produce daily source prereleases. |
| `stable` | Accepted stable releases. It starts at 1.0.0 (`ef1d1e1f`). |
| `codex/ue59-preparation` | Unreal 5.9 compatibility candidate. Separate engine and tests; no support claim yet. |

Temporary work branches are merged and removed. The [Git workflow](docs/git-workflow.md) explains the three long-lived roles.

Daily prereleases use `daily-YYYY-MM-DD`, based on the Europe/Paris date. Successful `main` pushes update that day's snapshot after validation. Stable `v1.0.0` is a manual release with its own acceptance gates. See [release process](docs/release.md).

## Feedback

Use the [shared feedback form](https://github.com/FloStyle/TurboMCP/issues/new?template=feedback.md) for a **bug**, **tool improvement** or **new feature**. Feedback from a tool call and a work session follows the same process: search, describe, review and submit. Add evidence to an existing issue when it matches.

The [reporting skill](skills/turbomcp-session-report/SKILL.md) and [report format](skills/turbomcp-session-report/references/report-format.md) help prepare a sanitized local draft. Review it before posting. See [Contributing](CONTRIBUTING.md) for the complete workflow.

## Development

Use Python 3.13, Git and PowerShell. Start with [AGENTS.md](AGENTS.md), the [contributor workflow](docs/contributing.md) and an approved task. Keep Unreal in a separate checkout.

```powershell
python Tools/validate.py
python Tools/next-task.py
```

For engine work, configure ignored `workspace.local.json`, then follow the [workspace guide](docs/workspace.md) and [test guide](Tests/README.md). Offline checks do not replace native acceptance.

[Current issue map](docs/github-issues.md) | [Project status](docs/status.md) | [Roadmap](ROADMAP.md) | [Architecture](docs/architecture.md) | [Native tool comparison](docs/tool-comparison.md)

## Repository layout

`Plugins/TurboMCP` contains the plugin. `TestProject` contains synthetic fixtures. `Tools` and `Tests` contain validation tools and acceptance data. `docs`, `todo` and `release` record design, work and release evidence. `site` contains the GitHub Pages landing page.

The [static website](https://flostyle.github.io/TurboMCP/) provides a human-readable introduction, installation links and release information. It is linked in the repository About panel and updated after successful main validation.

Licensed under [MPL-2.0](LICENSE).
