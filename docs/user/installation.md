# Installation and use

This source snapshot targets Windows and the pinned Unreal Engine 5.8.3 source build. It requires the engine's ModelContextProtocol, ToolsetRegistry and other dependencies declared in TurboMCP.uplugin.

Copy Plugins/TurboMCP into your Unreal project's Plugins directory, enable it in the project and build the project editor target with the matching engine/toolchain. The plugin is editor-only. Do not copy private development tasks or engine source into your project.

Enable the engine MCP server for your local editor and connect your client using the engine-generated client configuration. Keep the listener local. Logging may contain project paths, arguments and results; enable it only when appropriate for your project.

Tool-search clients discover toolsets and schemas before invoking a tool. Do not issue overlapping mutation calls. Adding reflected tool declarations requires a rebuild and editor restart.

This snapshot is experimental. Validate changes on a version-controlled project and inspect returned outcome metadata; a successful HTTP response alone does not establish that an edit succeeded.

See [Epic's MCP documentation](https://dev.epicgames.com/documentation/en-us/unreal-engine/unreal-mcp-in-unreal-editor?application_version=5.8) for the engine connection workflow.
