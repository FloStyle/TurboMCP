# Troubleshooting

[Documentation](../README.md) · [Installation](installation.md) · [Tool reference](tools.md)

## Connection and discovery

If the client cannot connect, check that the intended project is open, the engine's MCP server is enabled and the client uses that running editor's connection settings. Keep the endpoint local. Follow the engine connection guide linked from [installation](installation.md); do not guess a port or reuse credentials from another setup.

If connection works but TurboMCP tools are missing:

1. Confirm the TurboMCP project plugin is enabled and its module loads without dependency errors in Unreal's Output Log.
2. Open **Edit > Editor Preferences > Plugins > TurboMCP** and check **Enable TurboMCP**. This separate per-user/project preference defaults to **OFF** unless a saved choice overrides it. Do not delete user settings to activate the plugin.
3. If you change the preference, restart the editor. Changes do not activate or deactivate tools in the current session.
4. Check the `LogTurboMCP` startup message. When the preference disables TurboMCP, it explicitly reports that no TurboMCP tools, panel or hooks were installed; Unreal's MCP server remains independent.
5. Refresh tool discovery after restart. `ReadTurboMCPStatus` reports registration state only when TurboMCP is active. Its absence while disabled is expected, so use the preference and startup log for diagnosis.

A client may expose tool search rather than listing every tool upfront. Ask it to discover the TurboMCP toolsets and read the current schemas. Rebuild and restart the editor after changing reflected tool declarations.

## Build and plugin loading

Check the exact source-engine revision against [engine.lock.json](../../engine.lock.json). A matching version label alone is insufficient. From a development checkout with `workspace.local.json` configured, run:

```powershell
python Tools/doctor.py
```

This checks engine identity and test-project discovery; it does not build the plugin or prove which binary a running editor loaded.

For missing dependency errors, compare the available engine plugins with [TurboMCP.uplugin](../../Plugins/TurboMCP/TurboMCP.uplugin). Installed-engine compatibility is still under investigation. Do not remove dependencies just to make a build proceed.

For duplicate or stale plugin behavior, check project-local and engine-wide plugin copies. The repository test project uses `AdditionalPluginDirectories` to find this checkout. Verify paths before removing old copies or links. Follow [workspace setup](../workspace.md), and never stop another worker's editor to resolve your own build problem.

## Large Blueprint reads and argument errors

`ReadTurboMCPBlueprintLoad` accepts 1..8 Blueprint paths in `assets`, separated by commas or pipes. Its `tokenBudget` is a character budget for each asset's document, with a default of 60000. Several asset documents and the response headings can together exceed that value. If the combined response exceeds your client's capacity, split the list into smaller calls or read one asset per call.

For a per-asset budget refusal, use the reported required character count to set `tokenBudget` as a decimal string, such as `"120000"`. You can also narrow `graphFilter` or use `ReadTurboMCPGraphLoad` to page one graph. BlueprintLoad preserves the requested detail when it checks the budget. Check each asset's result because one call can contain both successful reads and refusals.

For a raw JSON-RPC call through Unreal's `call_tool` bridge, put `toolset_name`, `tool_name` and the inner `arguments` object inside `params.arguments`. BlueprintLoad fields such as plural `assets`, `graphFilter` and `tokenBudget` belong in that inner object. Confirm their spelling with the discovered schema. An envelope validation error can occur before the plugin receives the call. The protocol guide includes the complete [Unreal call envelope](../design/protocol.md#arguments-for-the-unreal-call_tool-bridge).

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


## Interrupted MCP requests

A timeout, fetch failure or unavailable server does not prove that an edit failed.
Do not repeat the edit. Reconnecting restores communication; it does not determine
what the original request changed.

The bundled client reports an UNKNOWN outcome and a bounded Incident record:
UTC time, tool, failure class, HTTP status when available, and reconnect result.
It does not include arguments or raw session IDs. Python callers can retain
UnknownOutcomeError.diagnostic or Session.last_failure; the CLI prints the record.
A failed reconnect does not erase the original uncertainty.

Before a debugging session, retain ReadTurboMCPStatus so the loaded engine and
plugin binary identity are available if communication stops. On recurrence:

1. Preserve the Incident record and the editor log window around its UTC time,
   including engine MCP/server messages, shutdown lines and any crash report.
   A client transport error alone does not identify the server-side cause.
2. Compare the startup build-identity line with the last status output. Compiler
   and binary timestamps are identifiers, not source-revision proof.
3. Once connected, inspect the exact asset or operation and ReadTurboMCPDirtyList.
   Compare with the intended change before deciding whether another edit is needed.
4. Retain any partial changes or uncertain outcome in the issue report. Do not
   infer success from a later successful call or clear evidence by saving first.

ReadTurboMCPEditorLog reads only the current enabled session's retained memory.
After an editor restart, use the previous editor log rather than expecting the
old records in the new session. Review logs before sharing proprietary information.

The Oct27 X-115 field disconnect remains unexplained: no pinned build, server log
or crash evidence was supplied. These diagnostics support a future recurrence;
they are not a claim that its historical cause has been repaired.
