# Native-tool overlap and investment decisions

[Documentation](../README.md)

Checked 2026-10-17. Comparison identity:

- Baseline: Epic source 5.8.3 at `396c9f059903aed5fec78ecd3d437a40c6415368` (local engine checkout, `release` branch).
- Pinned forward target: Epic 5.9 development at `5789fc07f00643fb37f85ede9894cd08f1cebcef`.
- Upstream development head read once at task start: `refs/heads/ue5-main` resolved to `5789fc07f00643fb37f85ede9894cd08f1cebcef`; it matched the pin, so no target drift was found.
- TurboMCP: approved base `1aea14e402bcdedce85a25381916e06d707c20e7`, plugin tree `590661ec45c829264ea1743fccea994edb748092`.

The forward target is a development snapshot, not a released compatibility promise or an installation guarantee. All findings below are source-interface observations. Runtime discovery, runtime behavior and installed-distribution availability remain unverified.

## Evidence labels

- **Source-present**: the declaration or registration route exists at the named revision.
- **Runtime-discovered**: the editor actually exposed the toolset in a running MCP session. No runtime probe was run for R13.
- **Enabled-by-default**: `EnabledByDefault` is explicitly true, or the plugin is a project plugin. Engine plugins whose field is absent or false are not enabled by default (`PluginManager.cpp:421-435` at 5.8.3).
- **Installed-distribution availability**: what an installed engine ships. R13 did not validate an installed engine; R16 owns that evidence.
- **Unverified**: not demonstrated by this source pass. It is not a synonym for absent.

## Investment decisions

| # | Deferred proposal | 5.8.3 source | 5.9 source | Decision | Demonstrated gap and maintenance cost |
| --- | --- | --- | --- | --- | --- |
| 1 | Create an enum with named values in one call | Not source-present | `UUserDefinedEnumToolset::Create` plus `AddEnumerator` | `retain-turbomcp` | Native `Create` makes an empty enum and `AddEnumerator` sets a display name, not the internal enumerator name; N values need 1+N calls and there is no preview. TurboMCP validates identifiers and `_MAX`, refuses overwrite, and covers the named-creation workflow in one call. That call is **not an atomic mutation**: a failed `SetEnums` can leave the asset present with unverified names, and a failed display-name setter only reduces the reported count; neither failure rolls creation back. The existing read-back/partial-failure repair is [214d6e7a](../../todo/214d6e7a.md). Maintenance: one C++ file and header coupled to `FEnumEditorUtils` and `UEnum::SetEnums`. |
| 2 | Edit an existing enum (add, remove, move, display name, bitflags) | Not source-present | `UUserDefinedEnumToolset` edit functions | `adopt-native` | TurboMCP has no edit-existing operation; the 5.9 toolset covers it. Native cannot choose or rename internal enumerator names, and the inspected file has no explicit transaction or save call. Maintenance avoided; track the experimental toolset. |
| 3 | Preview native enum edits | Not source-present | No native preview function | `thin-adapter` | A read-only diff over `GetEnumerators` would be small, but no workflow currently requires it. Do not build until a concrete request exists; a version-specific wrapper must track the native struct. |
| 4 | Create a struct from a compact field list with preview | Not source-present | `UUserDefinedStructToolset::Create` plus `Add*Variable` | `retain-turbomcp` | Native creation installs one default bool; each variable is a separate call and there is no field-list input or preview. TurboMCP creates the field list in one transaction, removes the creator bool, sets names/defaults/tooltips/categories, reads back and refuses overwrite. Maintenance: high; a large C++ file coupled to `FStructureEditorUtils`. |
| 5 | Expand variable authoring to flags, rename, containers and object/struct types | Not source-present | 14 struct functions including flags and typed adds | `adopt-native` | TurboMCP lacks rename and the editable-on-instance/savegame flags; native covers them and accepts container and object/struct types. TurboMCP already parses container and reference types, so this is a coverage gap, not a total one. Do not duplicate native flags. |
| 6 | Change a variable type, report loaded-asset readers, or preview field changes | Not source-present | No native type-change, reader-count or preview function | `retain-turbomcp` | Native variable functions add, remove, rename and reorder but do not change an existing variable's type; TurboMCP does, with a loaded-asset reader diagnostic and one preview/edit plan. That diagnostic scans loaded Blueprint `NewVariables` and DataTable row structs only; it does not cover graph-only make/break use or unloaded assets, so a zero count is not proof of safe removal. The existing reader-coverage repair is [7bd81b84](../../todo/7bd81b84.md). Maintenance: high and engine-utility dependent. |
| 7 | Explicit asset checkout with a preview | Python asset tools expose no asset checkout; `write_file` checks out text files only | No checkout function | `retain-turbomcp` | The 5.9 source-control toolset returns state and history but never checks out. TurboMCP issues the provider operation, reports the provider's own result and refuses per asset. Maintenance: two C++ files and a header that repeat provider gating. |
| 8 | Revision history, historical revision load, changelist creation | Not source-present | Three asynchronous source-control functions | `adopt-native` on 5.9; `defer` on 5.8.3 | Native 5.9 covers all three; TurboMCP has no equivalent. Do not implement a duplicate. Maintenance avoided; the feature remains unavailable on the 5.8.3 baseline. |
| 9 | Simple single-asset editability or checkout state | `AssetTools.can_edit_asset`, `is_checked_out` | `GetAssetSourceControlState` | `adopt-native` for simple checks | Native is a single-asset check and, in 5.9, fails open: a disabled provider or failed refresh returns the default "not tracked, editable" state instead of an unknown. Use it only where that collapse is acceptable. |
| 10 | Batch diagnostic source-control state | No batch equivalent | No batch equivalent | `retain-turbomcp` | TurboMCP reports provider capabilities, per-asset refusals and a null-state report separately; native 5.9 is single-asset and fail-open. A null state is handled explicitly, but `ForceUpdate` does not prove a successful refresh: the pinned Perforce provider can ignore the update result and return cached state. This is diagnostic detail, not stale-proof measurement. The failed-refresh follow-up belongs under the R04 tool audit. The cost is a long text response. |
| 11 | Widget-animation lifecycle and bindings | No UMG-specific animation tool | 10 functions in `UWidgetAnimationToolset` | `adopt-native` on 5.9; `defer` on 5.8.3 | Native covers create, list, find, rename, label, length, remove and bindings, with transactions. The plugin is explicitly off by default. TurboMCP has no animation tool, so nothing is removed. |
| 12 | Widget-animation track and key authoring | Python `AnimationAssistantToolset` candidates exist | No `SequencerTools`, `SequencerKeyframingTools` or `SequencerConditionTools` found in the pinned tree | `defer` | The 5.9 header claims those toolsets work on widget animations, but they are absent from the pinned 5.9 tree and the 5.8.3 Python animation toolsets are gone. Exact 5.9 parity is unresolved; do not build on the header comment. |
| 13 | Competing stdio proxy | Not source-present | `Extras/Proxy` with install and reconnect behavior | `adopt-native`; do not build | The native proxy answers `initialize` locally, reconnects, and emits `notifications/tools/list_changed`. It rewrites `.mcp.json`, requires manual install, and does not document replay of in-flight mutations. Preserve unknown-outcome semantics; never assume reconnect authorizes replay. |
| 14 | New MCP server or registration route | Engine MCP server plus registry | Engine MCP server plus registry and baked manifests | `adopt-native` | Reuse the engine transport and registry. TurboMCP must not add a second server or a duplicate discovery route. |

These are investment recommendations for roadmap planning, not live tool preferences. A live preference requires target availability plus accepted runtime and safety evidence. Every one of the 101 routing preferences remains `unverified`. For workflows outside the five areas compared here, R04 owns the remaining behavioral/workflow audit and R14 owns forward runtime certification; R13 is not their pending owner.

## Availability at a glance

| Property | 5.8.3 | 5.9 |
| --- | --- | --- |
| Enum, struct and source-control C++ toolsets | Absent | Source-present in `EditorToolset` |
| Widget-animation toolset | Absent | Source-present in `WidgetAnimationToolset` |
| Engine-shipped Python toolsets | Source-present; 598 candidate tool declarations under `Experimental/Toolsets` | The engine-shipped `Content/Python` toolset modules are absent from `ToolsetRegistry`, `EditorToolset` and `AnimationAssistantToolset` |
| Enabled by default | No for `UMGToolSet`, `ToolsetRegistry`, `ModelContextProtocol`; `EditorToolset` and `AnimationAssistantToolset` omit the field and are engine plugins, so they are not enabled by default | No for `WidgetAnimationToolset`, `ToolsetRegistry` and `ModelContextProtocol`; `EditorToolset` also omits the field and is not enabled by default |
| Runtime-discovered | Unverified | Unverified; plugin enablement and MCP server startup are prerequisites |
| Installed distribution | Unverified; R16 owns it | Unverified; R16 owns it |

`NoRedist: true` appears on `ToolsetRegistry` and `ModelContextProtocol` descriptors, but no runtime enforcement of that descriptor field was found in this source pass. It is evidence of a packaging convention, not proof that an installed engine lacks the plugins.

## Area evidence

### Enum authoring

Pinned 5.9 interfaces: [UserDefinedEnumToolset.h](https://github.com/EpicGames/UnrealEngine/blob/5789fc07f00643fb37f85ede9894cd08f1cebcef/Engine/Plugins/Experimental/Toolsets/EditorToolset/Source/EditorToolset/Private/UserDefinedEnumToolset.h). The eight functions are `Create`, `GetEnumerators`, `AddEnumerator`, `RemoveEnumerator`, `SetDisplayName`, `MoveEnumerator`, `IsBitflags` and `SetBitflags`. `GetEnumerators` returns `FUserDefinedEnumerator` entries with index, internal name, value and display name.

Source inspection shows that `Create` checks the asset registry and uses the enum factory. `AddEnumerator` appends one enumerator, optionally sets its display name, and removes it again if the display name is rejected. `RemoveEnumerator`, `SetDisplayName` and `MoveEnumerator` validate indices and raise script errors. The file contains no explicit transaction or save call.

The 5.8.3 scan found no enum authoring header under `Experimental/Toolsets` and no Python enum tool. This is an absence in the inspected routes, not proof that no project code or engine utility can create an enum.

TurboMCP counterpart: `EditTurboMCPEnumAdd` and `PreviewTurboMCPEnumAdd` in [TurboMCPEnumAdd.cpp](../../Plugins/TurboMCP/Source/TurboMCP/TurboMCPEnumAdd.cpp). It accepts `Name` or `Name=Display Name` values, validates identifier rules and duplicates, refuses an existing asset, creates through `FEnumEditorUtils`, sets names through `UEnum::SetEnums`, and does not save. The one-call workflow is not a rollback guarantee: a failed `SetEnums` returns an error after the asset has been created, and failed display-name setters leave creation in place and only lower the reported count. Read-back of the qualified names and display names remains the existing repair [214d6e7a](../../todo/214d6e7a.md).

Output cost: native `GetEnumerators` returns structured entries with four fields each; TurboMCP returns compact text with one line per value. No byte or latency measurements were taken, so no token-saving claim is made.

### Struct authoring

Pinned 5.9 interfaces: [UserDefinedStructToolset.h](https://github.com/EpicGames/UnrealEngine/blob/5789fc07f00643fb37f85ede9894cd08f1cebcef/Engine/Plugins/Experimental/Toolsets/EditorToolset/Source/EditorToolset/Private/UserDefinedStructToolset.h). The 14 functions cover create, tooltip, list, add primitive/struct/object variables, remove, rename, default value, tooltip, editable-on-instance, savegame and move.

`AddVariableWithSpec` applies its fields and rolls the new variable back if a later field fails. `ListVariables` returns name, element type, container type, default, tooltip, editable flag and savegame flag. The inspected file contains no explicit transaction, compile or save call; whether the engine utility compiles or dirties implicitly was not established.

The 5.8.3 scan found no struct authoring toolset. TurboMCP `EditTurboMCPStructNew`, `EditTurboMCPStructFields` and their previews are described in [TurboMCPStruct.cpp](../../Plugins/TurboMCP/Source/TurboMCP/TurboMCPStruct.cpp). They support a compact `Name: Type` dialect, containers and typed references, type changes, defaults, tooltips, categories, reorder, removal with a loaded-asset reader diagnostic, one-transaction creation, explicit compile and package dirtying, and a preview that shares the edit plan. The reader diagnostic scans loaded Blueprint `NewVariables` and DataTable row structs only. It does not inspect graph-only make/break use or unloaded assets, and its count is per loaded asset rather than per field or node. A zero count therefore does not establish that removal is safe; the exact gap and the required make/break check are [7bd81b84](../../todo/7bd81b84.md).

The overlap is real but partial: native has rename and variable flags; TurboMCP has type change, category, the limited loaded-asset reader diagnostic and preview. Neither is a superset.

### Source control

Pinned 5.9 interfaces: [SourceControlToolset.h](https://github.com/EpicGames/UnrealEngine/blob/5789fc07f00643fb37f85ede9894cd08f1cebcef/Engine/Plugins/Experimental/Toolsets/EditorToolset/Source/EditorToolset/Private/SourceControlToolset.h). `GetAssetSourceControlState` returns tracked, can-edit and checked-out-by; `ListAssetRevisions` returns revision metadata; `LoadAssetRevision` loads a read-only historical package; `CreateChangelist` moves pending assets into a new changelist.

In the 5.9 implementation, state refresh failure or a disabled provider returns the default state rather than an error. That collapse makes "not under source control" and "could not measure" indistinguishable.

The 5.8.3 Python `AssetTools` module exposes `can_edit_asset`, `is_checked_out`, `is_dirty` and `save_assets`; `write_file` checks out a text file before overwriting it. It exposes no batch diagnostic state, no asset checkout and no revision history.

TurboMCP counterpart: `ReadTurboMCPVcsState`, `PreviewTurboMCPVcsCheckout` and `EditTurboMCPVcsCheckout`. The implementation reports the provider name and capability flags, per-asset state, a refusal or null-state report, and the provider's own checkout result. It never writes an asset file and handles a null `GetState` result explicitly instead of reporting "not tracked". It does not independently prove that the `ForceUpdate` refresh succeeded: in the pinned Perforce provider, `GetState(..., ForceUpdate)` ignores the `Execute(FUpdateStatus)` result and returns cached states with `Succeeded`. The report is therefore useful provider diagnostics, not provider-independent protection against stale or unknown measurements. Failed-refresh coverage is a bounded follow-up under the R04 tool audit. The trade-off is a long text response and duplicated provider gating.

### Widget animation

Pinned 5.9 interfaces: [WidgetAnimationToolset.h](https://github.com/EpicGames/UnrealEngine/blob/5789fc07f00643fb37f85ede9894cd08f1cebcef/Engine/Plugins/Experimental/Toolsets/WidgetAnimationToolset/Source/WidgetAnimationToolset/Private/WidgetAnimationToolset.h). Ten functions cover lifecycle and bindings. The implementation uses `FScopedTransaction` for create, rename, label, length, remove, add-widget and remove-binding, and compiles the Widget Blueprint when a binding needs a generated class.

The header says the Sequencer toolsets work on `UWidgetAnimation` because it is a `UMovieSceneSequence`. No `SequencerTools`, `SequencerKeyframingTools` or `SequencerConditionTools` toolset was found in the pinned 5.9 tree; the 5.8.3 Python `AnimationAssistantToolset` had 319 candidate tool declarations but its `Content/Python` tree is absent at 5.9. Track and key parity is therefore unresolved, not proven.

TurboMCP has no widget-animation tool, so there is no feature to remove. Its widget tools address trees, bindings, retargeting, variables and previews, which native `WidgetAnimationToolset` does not replace.

### MCP stdio proxy

Pinned 5.9 evidence: [Proxy README](https://github.com/EpicGames/UnrealEngine/blob/5789fc07f00643fb37f85ede9894cd08f1cebcef/Engine/Plugins/Experimental/ModelContextProtocol/Extras/Proxy/README.md). The proxy is a Go stdio bridge with `install` and `uninstall`, an optional explicit `.mcp.json` path, local `initialize` answers, upstream reconnect, `notifications/tools/list_changed`, and clean JSON-RPC errors when the editor is unreachable.

It is not a plugin and is not runtime-discovered. It rewrites the client configuration, so an editor integration that also rewrites `.mcp.json` can clobber it. The README does not state whether an in-flight mutation is replayed after reconnect. Preserve the client's unknown-mutation-outcome rule; reconnect is not authorization to retry.

The 5.8.3 tree has no `Extras/Proxy`. TurboMCP has no proxy implementation, so nothing is removed by adopting the native bridge.

## Registration routes

The 38-header inventory is a C++ declaration scan, not an exhaustive native MCP inventory. The routes found at the pinned revisions are:

| Route | 5.8.3 | 5.9 | Consequence |
| --- | --- | --- | --- |
| `UToolsetDefinition` subclass with `AICallable` UFUNCTIONs, registered by `UToolsetRegistry::RegisterToolsetClass` | Present | Present | C++ toolsets are live-registered by plugin modules; a toolset only appears when its plugin is enabled. |
| Engine Python `toolset_registry` classes and `@toolset_registry.tool_call` | Present; 598 candidate declarations in 29 files across 7 toolset plugins | Engine-shipped modules absent | Header-only scans miss a large part of 5.8.3 native coverage. A project-defined Python toolset route may remain through reflected APIs, but no engine helper was found; unverified. |
| Legacy `UModelContextProtocolToolLibrary` Blueprint and asset tools | Present, deprecated | Present, deprecated | A legacy data-defined route that bypasses `ToolsetRegistry`. It is not part of the 38-header scan. |
| Direct `IModelContextProtocolTool` registration | Present | Present | A C++ module can add a tool without a toolset class. Not counted by the header scan. |
| `MCPClientToolset` external-server adapter | Present | Present | Imports tools from local or private MCP servers into the registry. It is another source of discoverable tools, not a TurboMCP toolset. |
| `UAgentSkillToolset` and AgentSkill assets | Present | Present | Skills are instructions, not tools; they are registered separately from the toolset classes. |
| Tool-search meta-tools (`list_toolsets`, `describe_toolset`, `call_tool`) | Present; `bEnableToolSearch = true` | Present; `bEnableToolSearch = true` | By default `tools/list` exposes the meta-tools, not every toolset tool. TurboMCP tools are reached through `call_tool`; this is runtime discovery behavior, not an absence of tools. |
| Baked `UToolsetRegistryManifest` data asset | Absent | Present | Cooked metadata route for toolsets. It supplements, and does not replace, live registration. |
| MCP server autostart | `bAutoStartServer = false` | `bAutoStartServer = false` | Source-present tools are not reachable until the server is started or autostart is enabled. |

The 5.8.3 Python candidates are not a certified tool count: the scan matches decorated methods and excludes tests. They are evidence that the header scan alone is incomplete and that the 5.9 removal of engine Python toolsets is a material availability change.

## Adjacent defects for review

These are recorded, not repaired:

1. The 5.9 `WidgetAnimationToolset` header refers to Sequencer toolsets that are absent from the pinned 5.9 tree; exact track and key coverage is unresolved.
2. The 5.9 `GetAssetSourceControlState` fails open when source control is disabled or refresh fails, collapsing an unknown measurement into "not tracked, editable".
3. The 5.9 removal of the 5.8.3 engine Python toolsets removes `save_assets`, `is_dirty`, `can_edit_asset`, `is_checked_out` and the write-file checkout path; no replacement save or checkout tool was found in the pinned C++ toolsets.
4. This document's predecessor cited 8 struct functions; the pinned header declares 14. The incomplete list is superseded here.
5. `docs/tool-comparison.md` cited plugin tree `2513a0b...`; the approved base builds plugin tree `590661e...`. The comparison was corrected to the approved base.
6. `NoRedist` has no runtime enforcement in the inspected engine source. Installed-engine absence must be measured by R16, not inferred from the descriptor.
7. TurboMCP enum creation is one call but not atomic. A failed `SetEnums` can leave the asset present, and display-name failures do not roll creation back. Read-back and truthful partial-failure reporting remain the existing repair [214d6e7a](../../todo/214d6e7a.md).
8. TurboMCP struct removal counts loaded Blueprint `NewVariables` and DataTable row structs only. Graph-only make/break use and unloaded assets are outside the scan, so zero readers are not proof of safe removal. The existing repair is [7bd81b84](../../todo/7bd81b84.md).
9. TurboMCP VCS state handles a null `GetState`, but does not prove refresh freshness. The pinned Perforce provider can ignore the `FUpdateStatus` result and return cached state. Failed-refresh coverage is a bounded follow-up under the R04 tool audit.
10. The offline runner is environment-sensitive: `test-ownership` E03 captured a PowerShell diagnostic that Python decoded with the wrong encoding (byte `0x90`), so an assertion failed even though the same test passed directly and an independent full offline run passed. The precise trigger is not established. An integrator-owned repair must define producer/consumer encoding, reproduce non-ASCII diagnostics and retain the unreadable-lease blocking control. R13 does not change the test, runner, assertions or shell configuration.

## Refresh triggers

- `refs/heads/ue5-main` changes from `5789fc07f00643fb37f85ede9894cd08f1cebcef`.
- Any of the five pinned headers, their plugin descriptors, or the Python `Content` trees change.
- `bEnableToolSearch`, `bAutoStartServer`, `EnabledByDefault` or the MCP server registration changes.
- The `UToolsetRegistryManifest` bake format or `RegisterToolsetClass` lifecycle changes.
- Perforce `ForceUpdate`/`GetState` behavior or another provider's refresh contract changes.
- A 5.9 runtime becomes available; then run R14 probes instead of inferring behavior from source.
- The TurboMCP plugin tree changes from `590661ec45c829264ea1743fccea994edb748092`.
- An installed engine is inspected; R16 owns that matrix.

## Unresolved

- No 5.9 runtime or installed engine was available, so all runtime discovery, undo, save, media and failure-state claims remain unverified.
- Native enum internal-name control: only display names are settable through the inspected 5.9 toolset.
- Enum creation partial failure: `SetEnums` and display-name failures are not rolled back; read-back remains open under [214d6e7a](../../todo/214d6e7a.md).
- Struct reader coverage: graph-only make/break use and unloaded assets remain outside the diagnostic; safe removal is not established by a zero count.
- VCS freshness: whether `ForceUpdate` established a successful refresh is provider-dependent and unverified; failed-refresh coverage is assigned to R04.
- Native struct compile and dirty behavior after edits: no explicit call was found; engine utility behavior was not traced.
- Track and key authoring for 5.9 widget animations: the referenced Sequencer toolsets were not found.
- Proxy behavior for an in-flight mutation across reconnect: not documented.
- Whether a project-defined Python toolset can still be registered in 5.9 through reflected APIs: unverified, and not used as a parity claim.

## Integration resolution

The integrator repaired the ownership-test encoding boundary separately in
`00ac2b8`. A synthetic non-ASCII diagnostic reproduced the byte 0x90 failure
before the change. Strict UTF-8 producer/consumer handling and exact stdout,
stderr and false-verdict regression controls now pass. Full offline run
`3121fd3995424c49ae8317227915c0d0` passed with `PYTHONUTF8=0`. Defect 10 above
is retained as historical evidence of the worker submission, not an open request
to alter tests. Product and compatibility limitations remain open.
