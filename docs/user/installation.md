# Install and connect

[Documentation](../README.md) · [Tool reference](tools.md)

## Prerequisites

Use Windows and the Unreal Engine 5.8.3 source revision pinned in [engine.lock.json](../../engine.lock.json). The plugin requires the engine's ModelContextProtocol, ToolsetRegistry and the other dependencies listed in [TurboMCP.uplugin](../../Plugins/TurboMCP/TurboMCP.uplugin).

Installed-engine packages and Unreal 5.9 are not validated yet. See [project status](../status.md).

## Install in a project

1. Copy `Plugins/TurboMCP` from this repository into your project's `Plugins` directory.
2. Enable TurboMCP in the project.
3. Build the project's editor target with the matching engine and toolchain.
4. Open the project in that editor.

TurboMCP is editor-only. Only the plugin folder is installed into the project; repository tests, tasks and build tools remain development files. To develop the plugin itself, use [workspace setup](../workspace.md).

## Connect an MCP client

Enable Unreal's MCP server for the local editor and use its generated client configuration. Follow [Epic's connection guide](https://dev.epicgames.com/documentation/en-us/unreal-engine/unreal-mcp-in-unreal-editor?application_version=5.8) for the engine setup.

Keep the listener local. Logging can include project paths, arguments and results, so enable it only when appropriate for your project.

## First use

Have the client discover the available toolsets and schemas, then consult the [tool reference](tools.md). Test edits in a version-controlled project and do not send overlapping mutation calls. Inspect the returned outcome metadata: an HTTP success response alone does not confirm that an edit succeeded.

Adding reflected tool declarations during development requires a rebuild and editor restart.

For connection, build or result problems, see [Troubleshooting](troubleshooting.md).

## Disable TurboMCP for yourself

Open **Edit > Editor Preferences > Plugins > TurboMCP** and clear **Enable TurboMCP**, then restart the editor. To enable it again, select the same checkbox and restart. The preference defaults to enabled and is stored in Unreal's `EditorPerProjectUserSettings` for your user and project. It does not modify the shared project descriptor or `DefaultEditor.ini`.

When disabled at startup, TurboMCP does not register its tools, panel, recording/analytics hooks, focus guard or native-tool safety filters. Unreal's MCP server and unrelated toolsets are left alone. This preference does not unload the plugin binary; use Unreal's plugin manager if you need to prevent the module from loading altogether.

Changes require an editor restart. Until then the current session keeps its previous activation state. While enabled, `ReadTurboMCPStatus` reports both the current state and the saved preference, including a pending restart. When disabled, that tool is absent too; use Editor Preferences or the `LogTurboMCP` startup message to identify the reason.

**Record every tool call** is now a personal preference on the same page, shared with the panel checkbox. Old project-wide call-log configuration no longer controls this per-user setting; check the checkbox after upgrading. Logging still defaults to enabled.
