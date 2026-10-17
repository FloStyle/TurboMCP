# Troubleshooting

[Documentation](../README.md) · [Installation](installation.md) · [Tool reference](tools.md)

## Connection and discovery

If the client cannot connect, check that the intended project is open, the engine's MCP server is enabled and the client uses that running editor's connection settings. Keep the endpoint local. Follow the engine connection guide linked from [installation](installation.md); do not guess a port or reuse credentials from another setup.

If connection works but TurboMCP tools are missing, confirm TurboMCP is enabled and its module loaded in the editor. Refresh tool discovery in the client. When available, `ReadTurboMCPStatus` reports registration state. Inspect Unreal's Output Log for loading or dependency errors.

A client may expose tool search rather than listing every tool upfront. Ask it to discover the TurboMCP toolsets and read the current schemas. Rebuild and restart the editor after changing reflected tool declarations.

## Build and plugin loading

Check the exact source-engine revision against [engine.lock.json](../../engine.lock.json). A matching version label alone is insufficient. From a development checkout with `workspace.local.json` configured, run:

```powershell
python Tools/doctor.py
```

This checks engine identity and test-project discovery; it does not build the plugin or prove which binary a running editor loaded.

For missing dependency errors, compare the available engine plugins with [TurboMCP.uplugin](../../Plugins/TurboMCP/TurboMCP.uplugin). Installed-engine compatibility is still under investigation. Do not remove dependencies just to make a build proceed.

For duplicate or stale plugin behavior, check project-local and engine-wide plugin copies. The repository test project uses `AdditionalPluginDirectories` to find this checkout. Verify paths before removing old copies or links. Follow [workspace setup](../workspace.md), and never stop another worker's editor to resolve your own build problem.

## Results and edits

Read the operation's result metadata as well as the transport response. A request can reach the server successfully while the operation is refused, fails or has an uncertain outcome. If metadata is missing or malformed, record it as a protocol problem rather than assuming success.

Check the exact asset and graph, current editor state, reported compile result and save outcome. An in-memory edit is distinct from disk persistence. If the tool reports partial changes or failed rollback, stop further mutations and preserve evidence for recovery. Do not blindly retry a potentially applied edit.

The [protocol design](../design/protocol.md) explains result interpretation; [mutation design](../design/mutations.md) covers lifecycle and recovery requirements. A test failure against these contracts is a defect to investigate, not permission to weaken the contract.

## Report a useful issue

Use the repository's bug-report template and include:

- Plugin commit, exact engine revision, operating system and client.
- The tool name and sanitized arguments.
- Starting asset state, expected behavior and actual result.
- The smallest reproduction you ran, relevant log excerpts and whether it repeats.

A suspected cause is optional. State which observations are confirmed and which are guesses. Avoid uploading proprietary assets, tokens or full logs containing private data. For vulnerabilities, use [the security policy](../../SECURITY.md) instead of a public issue.
