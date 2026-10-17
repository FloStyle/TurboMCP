# Epic and TurboMCP comparison

[Documentation](README.md) · [Tool selection](design/tool-selection.md) · [Engine compatibility](engine-compatibility.md)

## Evidence and scope

Baseline: Epic source 5.8.3 at `396c9f059903aed5fec78ecd3d437a40c6415368`. Forward target: Epic 5.9 development at `5789fc07f00643fb37f85ede9894cd08f1cebcef`. TurboMCP: approved base `1aea14e402bcdedce85a25381916e06d707c20e7`, plugin tree `590661ec45c829264ea1743fccea994edb748092`. The upstream development head (`refs/heads/ue5-main`) was read once on 2026-10-17 and matched the forward target. The development target is not a released compatibility promise.

The tables distinguish source declarations from runtime capability. A tool present in Epic source is not necessarily enabled in a default editor or shipped in an installed distribution. The native header scan covers AICallable declarations in headers under Experimental/Toolsets. R13 also found 598 candidate Python tool declarations in 29 files across 7 toolset plugins at 5.8.3, plus legacy MCP tool libraries, direct tool registration, `MCPClientToolset`, AgentSkill, tool-search meta-tools and 5.9 baked manifests. The 5.8.3 engine-shipped Python toolset modules are absent at 5.9. A missing entry is not proof that Epic lacks a feature.

All 101 TurboMCP tools are accounted for below. No tool is currently labeled as a verified replacement for an Epic tool. R13 records source-level equivalence and limitations for the five deferred areas in [native overlap](audit/native-overlap.md); R04 establishes runtime behavior, and R17 publishes evidence-backed routing guidance. All routing preferences remain unverified.

## Where TurboMCP aims to add value

| Workflow | Epic 5.8.3 source evidence | TurboMCP intended value | Epic 5.9 source evidence | Decision / proof needed |
| --- | --- | --- | --- | --- |
| Blueprint graph workflows | Complete native equivalent not resolved by this header scan | Compact graph inspection, explicit identity, coordinated edits and layout | Full native graph parity still to inspect | Compare exact reads, supported nodes, failed-edit state and output size; no superiority claim yet |
| Widget authoring | UMGToolSet declares creation, tree edits and compilation | Tree representation, targeted workflows and preview | Existing UMG tools plus WidgetAnimationToolset | Retain TurboMCP widget workflows; adopt native animation lifecycle and bindings; defer track/key authoring |
| Enums and structs | These specific authoring headers are absent from the scanned subtree | Existing authoring workflows on baseline | Dedicated enum and struct toolsets | Retain TurboMCP one-call creation (not atomic), type change and limited loaded-asset reader diagnostic; adopt native editing and variable flags; see R13 decisions |
| Source control | Dedicated SourceControlToolset header absent from scan | Explicit project asset state | Revision and changelist interfaces | Retain checkout and provider diagnostics; adopt native history and changelists; native state fails open and TurboMCP freshness is unproven |
| Capture and editor interaction | EditorApp capture and SlateInspector tools | Graph framing, layout diagnostics and widget preview | Both tool families remain | Measure framing and result handling; screenshot existence alone is not proof |
| Materials | Complete native equivalent unresolved | Material inspection, reroute workflows and compact output | Native parity unresolved | Compare concrete material workflows before declaring an advantage |
| Connection and protocol | Epic MCP server and registry | Tool extensions, help and outcome conventions | Registry/media changes and native stdio proxy | Adopt the native 5.9 proxy; do not build a competing proxy; preserve unknown-outcome semantics |

## R13 source-comparison decisions

The five deferred areas were compared at the pinned revisions. Full evidence, including registration routes, availability labels and maintenance costs, is in [native overlap](audit/native-overlap.md) and its [JSON record](audit/native-overlap.json).

| Area | Decision | Demonstrated reason |
| --- | --- | --- |
| Enums | Retain TurboMCP named one-call workflow; adopt native editing on 5.9; thin adapter for preview only if requested | Native creates empty enums and cannot set internal enumerator names; native editing covers add, remove, move, display name and bitflags. TurboMCP's one call is not atomic; see [214d6e7a](../todo/214d6e7a.md). |
| Structs | Retain TurboMCP creation, type change, limited loaded-asset reader diagnostic and preview; evaluate native rename and variable flags | Native has no field-list creation, type-change or preview; TurboMCP has no rename or editable/savegame flags. The reader diagnostic misses graph-only make/break use and unloaded assets; see [7bd81b84](../todo/7bd81b84.md). |
| Source control | Retain TurboMCP checkout and provider diagnostics; adopt native history, revision load and changelists | Native has no checkout; its single-asset state fails open. TurboMCP handles a null state but does not prove `ForceUpdate` freshness, so this is diagnostic detail, not stale-proof measurement. |
| Widget animation | Adopt native lifecycle and bindings on 5.9; defer track and key authoring | TurboMCP has no animation tool; the 5.9 header's Sequencer toolsets were not found in the pinned tree. |
| MCP stdio proxy | Adopt the native 5.9 proxy; do not build a competitor | The bridge exists only at 5.9 and covers install, reconnect and tool-list change notifications. |

These are investment recommendations, not live tool preferences. A live preference requires target availability plus accepted runtime and safety evidence. All 101 routing preferences remain `unverified`. For workflows outside the five areas, R04 owns the remaining behavioral/workflow audit and R14 owns forward runtime certification.

## Native interface inventory

These are candidate declaration counts, not feature scores. References link to Epic source and require Epic repository access. The machine-readable [inventory](audit/epic-tool-inventory.json) contains function identifiers and source locations without implementation bodies.

| Native header | Epic 5.8.3 | Epic 5.9 development |
| --- | --- | --- |
| AutomationTestToolset.h | [7 declarations](https://github.com/EpicGames/UnrealEngine/blob/396c9f059903aed5fec78ecd3d437a40c6415368/Engine/Plugins/Experimental/Toolsets/AutomationTestToolset/Source/AutomationTestToolset/Public/AutomationTestToolset.h) | [7 declarations](https://github.com/EpicGames/UnrealEngine/blob/5789fc07f00643fb37f85ede9894cd08f1cebcef/Engine/Plugins/Experimental/Toolsets/AutomationTestToolset/Source/AutomationTestToolset/Public/AutomationTestToolset.h) |
| ClothAssetToolset.h | [6 declarations](https://github.com/EpicGames/UnrealEngine/blob/396c9f059903aed5fec78ecd3d437a40c6415368/Engine/Plugins/Experimental/Toolsets/ChaosClothAssetToolset/Source/ChaosClothAssetToolset/Private/ChaosClothAsset/ClothAssetToolset.h) | [6 declarations](https://github.com/EpicGames/UnrealEngine/blob/5789fc07f00643fb37f85ede9894cd08f1cebcef/Engine/Plugins/Experimental/Toolsets/ChaosClothAssetToolset/Source/ChaosClothAssetToolset/Private/ChaosClothAsset/ClothAssetToolset.h) |
| CheatsToolsetDefinition.h | Not found by scan | [2 declarations](https://github.com/EpicGames/UnrealEngine/blob/5789fc07f00643fb37f85ede9894cd08f1cebcef/Engine/Plugins/Experimental/Toolsets/CheatsToolset/Source/CheatsToolset/Private/CheatsToolsetDefinition.h) |
| ConfigSettingsToolset.h | [8 declarations](https://github.com/EpicGames/UnrealEngine/blob/396c9f059903aed5fec78ecd3d437a40c6415368/Engine/Plugins/Experimental/Toolsets/ConfigSettingsToolset/Source/ConfigSettingsToolset/Private/ConfigSettingsToolset.h) | [8 declarations](https://github.com/EpicGames/UnrealEngine/blob/5789fc07f00643fb37f85ede9894cd08f1cebcef/Engine/Plugins/Experimental/Toolsets/ConfigSettingsToolset/Source/ConfigSettingsToolset/Private/ConfigSettingsToolset.h) |
| DataRegistryTools.h | [7 declarations](https://github.com/EpicGames/UnrealEngine/blob/396c9f059903aed5fec78ecd3d437a40c6415368/Engine/Plugins/Experimental/Toolsets/DataRegistryToolset/Source/DataRegistryToolset/Private/DataRegistryTools.h) | [7 declarations](https://github.com/EpicGames/UnrealEngine/blob/5789fc07f00643fb37f85ede9894cd08f1cebcef/Engine/Plugins/Experimental/Toolsets/DataRegistryToolset/Source/DataRegistryToolset/Private/DataRegistryTools.h) |
| DataflowAgentToolset.h | [22 declarations](https://github.com/EpicGames/UnrealEngine/blob/396c9f059903aed5fec78ecd3d437a40c6415368/Engine/Plugins/Experimental/Toolsets/DataflowAgent/Source/DataflowAgent/Private/DataflowAgentToolset.h) | [22 declarations](https://github.com/EpicGames/UnrealEngine/blob/5789fc07f00643fb37f85ede9894cd08f1cebcef/Engine/Plugins/Experimental/Toolsets/DataflowAgent/Source/DataflowAgent/Private/DataflowAgentToolset.h) |
| EditorDataStorageToolset.h | Not found by scan | [24 declarations](https://github.com/EpicGames/UnrealEngine/blob/5789fc07f00643fb37f85ede9894cd08f1cebcef/Engine/Plugins/Experimental/Toolsets/EditorDataStorageToolset/Source/EditorDataStorageToolset/Public/EditorDataStorageToolset.h) |
| EditorDataStorageUIToolset.h | Not found by scan | [13 declarations](https://github.com/EpicGames/UnrealEngine/blob/5789fc07f00643fb37f85ede9894cd08f1cebcef/Engine/Plugins/Experimental/Toolsets/EditorDataStorageToolset/Source/EditorDataStorageToolset/Public/EditorDataStorageUIToolset.h) |
| EditorAppToolset.h | [21 declarations](https://github.com/EpicGames/UnrealEngine/blob/396c9f059903aed5fec78ecd3d437a40c6415368/Engine/Plugins/Experimental/Toolsets/EditorToolset/Source/EditorToolset/Private/EditorAppToolset.h) | [40 declarations](https://github.com/EpicGames/UnrealEngine/blob/5789fc07f00643fb37f85ede9894cd08f1cebcef/Engine/Plugins/Experimental/Toolsets/EditorToolset/Source/EditorToolset/Private/EditorAppToolset.h) |
| LogsToolset.h | [4 declarations](https://github.com/EpicGames/UnrealEngine/blob/396c9f059903aed5fec78ecd3d437a40c6415368/Engine/Plugins/Experimental/Toolsets/EditorToolset/Source/EditorToolset/Private/LogsToolset.h) | [4 declarations](https://github.com/EpicGames/UnrealEngine/blob/5789fc07f00643fb37f85ede9894cd08f1cebcef/Engine/Plugins/Experimental/Toolsets/EditorToolset/Source/EditorToolset/Private/LogsToolset.h) |
| SourceControlToolset.h | Not found by scan | [4 declarations](https://github.com/EpicGames/UnrealEngine/blob/5789fc07f00643fb37f85ede9894cd08f1cebcef/Engine/Plugins/Experimental/Toolsets/EditorToolset/Source/EditorToolset/Private/SourceControlToolset.h) |
| UserDefinedEnumToolset.h | Not found by scan | [8 declarations](https://github.com/EpicGames/UnrealEngine/blob/5789fc07f00643fb37f85ede9894cd08f1cebcef/Engine/Plugins/Experimental/Toolsets/EditorToolset/Source/EditorToolset/Private/UserDefinedEnumToolset.h) |
| UserDefinedStructToolset.h | Not found by scan | [14 declarations](https://github.com/EpicGames/UnrealEngine/blob/5789fc07f00643fb37f85ede9894cd08f1cebcef/Engine/Plugins/Experimental/Toolsets/EditorToolset/Source/EditorToolset/Private/UserDefinedStructToolset.h) |
| AbilitySystemInspectorToolset.h | [4 declarations](https://github.com/EpicGames/UnrealEngine/blob/396c9f059903aed5fec78ecd3d437a40c6415368/Engine/Plugins/Experimental/Toolsets/GASToolsets/Source/GASToolsets/Private/AbilitySystemInspectorToolset.h) | [4 declarations](https://github.com/EpicGames/UnrealEngine/blob/5789fc07f00643fb37f85ede9894cd08f1cebcef/Engine/Plugins/Experimental/Toolsets/GASToolsets/Source/GASToolsets/Private/AbilitySystemInspectorToolset.h) |
| AttributeSetToolset.h | [2 declarations](https://github.com/EpicGames/UnrealEngine/blob/396c9f059903aed5fec78ecd3d437a40c6415368/Engine/Plugins/Experimental/Toolsets/GASToolsets/Source/GASToolsets/Private/AttributeSetToolset.h) | [2 declarations](https://github.com/EpicGames/UnrealEngine/blob/5789fc07f00643fb37f85ede9894cd08f1cebcef/Engine/Plugins/Experimental/Toolsets/GASToolsets/Source/GASToolsets/Private/AttributeSetToolset.h) |
| GameplayCueToolset.h | [8 declarations](https://github.com/EpicGames/UnrealEngine/blob/396c9f059903aed5fec78ecd3d437a40c6415368/Engine/Plugins/Experimental/Toolsets/GASToolsets/Source/GASToolsets/Private/GameplayCueToolset.h) | [8 declarations](https://github.com/EpicGames/UnrealEngine/blob/5789fc07f00643fb37f85ede9894cd08f1cebcef/Engine/Plugins/Experimental/Toolsets/GASToolsets/Source/GASToolsets/Private/GameplayCueToolset.h) |
| GameFeaturesToolset.h | [7 declarations](https://github.com/EpicGames/UnrealEngine/blob/396c9f059903aed5fec78ecd3d437a40c6415368/Engine/Plugins/Experimental/Toolsets/GameFeaturesToolset/Source/GameFeaturesToolset/Private/GameFeaturesToolset.h) | [12 declarations](https://github.com/EpicGames/UnrealEngine/blob/5789fc07f00643fb37f85ede9894cd08f1cebcef/Engine/Plugins/Experimental/Toolsets/GameFeaturesToolset/Source/GameFeaturesToolset/Private/GameFeaturesToolset.h) |
| GameplayTagsToolset.h | [6 declarations](https://github.com/EpicGames/UnrealEngine/blob/396c9f059903aed5fec78ecd3d437a40c6415368/Engine/Plugins/Experimental/Toolsets/GameplayTagsToolset/Source/GameplayTagsToolset/Private/GameplayTagsToolset/GameplayTagsToolset.h) | [8 declarations](https://github.com/EpicGames/UnrealEngine/blob/5789fc07f00643fb37f85ede9894cd08f1cebcef/Engine/Plugins/Experimental/Toolsets/GameplayTagsToolset/Source/GameplayTagsToolset/Private/GameplayTagsToolset/GameplayTagsToolset.h) |
| LiveCodingToolset.h | [1 declarations](https://github.com/EpicGames/UnrealEngine/blob/396c9f059903aed5fec78ecd3d437a40c6415368/Engine/Plugins/Experimental/Toolsets/LiveCodingToolset/Source/LiveCodingToolset/Public/LiveCodingToolset.h) | [1 declarations](https://github.com/EpicGames/UnrealEngine/blob/5789fc07f00643fb37f85ede9894cd08f1cebcef/Engine/Plugins/Experimental/Toolsets/LiveCodingToolset/Source/LiveCodingToolset/Public/LiveCodingToolset.h) |
| MVVMToolset.h | [9 declarations](https://github.com/EpicGames/UnrealEngine/blob/396c9f059903aed5fec78ecd3d437a40c6415368/Engine/Plugins/Experimental/Toolsets/MVVMToolset/Source/MVVMToolset/Private/MVVMToolset/MVVMToolset.h) | [14 declarations](https://github.com/EpicGames/UnrealEngine/blob/5789fc07f00643fb37f85ede9894cd08f1cebcef/Engine/Plugins/Experimental/Toolsets/MVVMToolset/Source/MVVMToolset/Private/MVVMToolset/MVVMToolset.h) |
| MovieRenderPipelineToolset.h | Not found by scan | [1 declarations](https://github.com/EpicGames/UnrealEngine/blob/5789fc07f00643fb37f85ede9894cd08f1cebcef/Engine/Plugins/Experimental/Toolsets/MovieRenderPipelineToolset/Source/MovieRenderPipelineToolset/Private/MovieRenderPipelineToolset.h) |
| NiagaraToolset_Assets.h | [3 declarations](https://github.com/EpicGames/UnrealEngine/blob/396c9f059903aed5fec78ecd3d437a40c6415368/Engine/Plugins/Experimental/Toolsets/NiagaraToolsets/Source/NiagaraToolsets/Private/NiagaraToolset_Assets.h) | [4 declarations](https://github.com/EpicGames/UnrealEngine/blob/5789fc07f00643fb37f85ede9894cd08f1cebcef/Engine/Plugins/Experimental/Toolsets/NiagaraToolsets/Source/NiagaraToolsets/Private/NiagaraToolset_Assets.h) |
| NiagaraToolset_Blueprint.h | [2 declarations](https://github.com/EpicGames/UnrealEngine/blob/396c9f059903aed5fec78ecd3d437a40c6415368/Engine/Plugins/Experimental/Toolsets/NiagaraToolsets/Source/NiagaraToolsets/Private/NiagaraToolset_Blueprint.h) | [2 declarations](https://github.com/EpicGames/UnrealEngine/blob/5789fc07f00643fb37f85ede9894cd08f1cebcef/Engine/Plugins/Experimental/Toolsets/NiagaraToolsets/Source/NiagaraToolsets/Private/NiagaraToolset_Blueprint.h) |
| NiagaraToolset_Component.h | [4 declarations](https://github.com/EpicGames/UnrealEngine/blob/396c9f059903aed5fec78ecd3d437a40c6415368/Engine/Plugins/Experimental/Toolsets/NiagaraToolsets/Source/NiagaraToolsets/Private/NiagaraToolset_Component.h) | [4 declarations](https://github.com/EpicGames/UnrealEngine/blob/5789fc07f00643fb37f85ede9894cd08f1cebcef/Engine/Plugins/Experimental/Toolsets/NiagaraToolsets/Source/NiagaraToolsets/Private/NiagaraToolset_Component.h) |
| NiagaraToolset_Info.h | [1 declarations](https://github.com/EpicGames/UnrealEngine/blob/396c9f059903aed5fec78ecd3d437a40c6415368/Engine/Plugins/Experimental/Toolsets/NiagaraToolsets/Source/NiagaraToolsets/Private/NiagaraToolset_Info.h) | [1 declarations](https://github.com/EpicGames/UnrealEngine/blob/5789fc07f00643fb37f85ede9894cd08f1cebcef/Engine/Plugins/Experimental/Toolsets/NiagaraToolsets/Source/NiagaraToolsets/Private/NiagaraToolset_Info.h) |
| NiagaraToolset_System.h | [46 declarations](https://github.com/EpicGames/UnrealEngine/blob/396c9f059903aed5fec78ecd3d437a40c6415368/Engine/Plugins/Experimental/Toolsets/NiagaraToolsets/Source/NiagaraToolsets/Private/NiagaraToolset_System.h) | [46 declarations](https://github.com/EpicGames/UnrealEngine/blob/5789fc07f00643fb37f85ede9894cd08f1cebcef/Engine/Plugins/Experimental/Toolsets/NiagaraToolsets/Source/NiagaraToolsets/Private/NiagaraToolset_System.h) |
| PCGSpatialToolset.h | [1 declarations](https://github.com/EpicGames/UnrealEngine/blob/396c9f059903aed5fec78ecd3d437a40c6415368/Engine/Plugins/Experimental/Toolsets/PCGToolset/Source/PCGToolset/Public/PCGSpatialToolset.h) | [1 declarations](https://github.com/EpicGames/UnrealEngine/blob/5789fc07f00643fb37f85ede9894cd08f1cebcef/Engine/Plugins/Experimental/Toolsets/PCGToolset/Source/PCGToolset/Public/PCGSpatialToolset.h) |
| PCGToolset.h | [30 declarations](https://github.com/EpicGames/UnrealEngine/blob/396c9f059903aed5fec78ecd3d437a40c6415368/Engine/Plugins/Experimental/Toolsets/PCGToolset/Source/PCGToolset/Public/PCGToolset.h) | [30 declarations](https://github.com/EpicGames/UnrealEngine/blob/5789fc07f00643fb37f85ede9894cd08f1cebcef/Engine/Plugins/Experimental/Toolsets/PCGToolset/Source/PCGToolset/Public/PCGToolset.h) |
| GpuProfilerToolset.h | Not found by scan | [1 declarations](https://github.com/EpicGames/UnrealEngine/blob/5789fc07f00643fb37f85ede9894cd08f1cebcef/Engine/Plugins/Experimental/Toolsets/PerfToolset/Source/PerfToolset/Private/GpuProfilerToolset.h) |
| StatsToolset.h | Not found by scan | [2 declarations](https://github.com/EpicGames/UnrealEngine/blob/5789fc07f00643fb37f85ede9894cd08f1cebcef/Engine/Plugins/Experimental/Toolsets/PerfToolset/Source/PerfToolset/Private/StatsToolset.h) |
| PhysicsAssetToolset.h | [17 declarations](https://github.com/EpicGames/UnrealEngine/blob/396c9f059903aed5fec78ecd3d437a40c6415368/Engine/Plugins/Experimental/Toolsets/PhysicsToolsets/Source/PhysicsToolsets/Private/PhysicsToolsets/PhysicsAssetToolset.h) | [17 declarations](https://github.com/EpicGames/UnrealEngine/blob/5789fc07f00643fb37f85ede9894cd08f1cebcef/Engine/Plugins/Experimental/Toolsets/PhysicsToolsets/Source/PhysicsToolsets/Private/PhysicsToolsets/PhysicsAssetToolset.h) |
| PluginToolset.h | [17 declarations](https://github.com/EpicGames/UnrealEngine/blob/396c9f059903aed5fec78ecd3d437a40c6415368/Engine/Plugins/Experimental/Toolsets/PluginToolset/Source/PluginToolset/Private/PluginToolset.h) | [17 declarations](https://github.com/EpicGames/UnrealEngine/blob/5789fc07f00643fb37f85ede9894cd08f1cebcef/Engine/Plugins/Experimental/Toolsets/PluginToolset/Source/PluginToolset/Private/PluginToolset.h) |
| RewindDebuggerToolset.h | Not found by scan | [9 declarations](https://github.com/EpicGames/UnrealEngine/blob/5789fc07f00643fb37f85ede9894cd08f1cebcef/Engine/Plugins/Experimental/Toolsets/RewindDebuggerToolset/Source/RewindDebuggerToolset/Public/RewindDebuggerToolset.h) |
| SemanticSearchToolset.h | [2 declarations](https://github.com/EpicGames/UnrealEngine/blob/396c9f059903aed5fec78ecd3d437a40c6415368/Engine/Plugins/Experimental/Toolsets/SemanticSearchToolset/Source/SemanticSearchToolset/Private/SemanticSearchToolset.h) | Not found by scan |
| SlateInspectorToolset.h | [14 declarations](https://github.com/EpicGames/UnrealEngine/blob/396c9f059903aed5fec78ecd3d437a40c6415368/Engine/Plugins/Experimental/Toolsets/SlateInspectorToolset/Source/SlateInspectorToolset/Public/SlateInspectorToolset.h) | [14 declarations](https://github.com/EpicGames/UnrealEngine/blob/5789fc07f00643fb37f85ede9894cd08f1cebcef/Engine/Plugins/Experimental/Toolsets/SlateInspectorToolset/Source/SlateInspectorToolset/Public/SlateInspectorToolset.h) |
| UMGToolSet.h | [23 declarations](https://github.com/EpicGames/UnrealEngine/blob/396c9f059903aed5fec78ecd3d437a40c6415368/Engine/Plugins/Experimental/Toolsets/UMGToolSet/Source/UMGToolSet/Private/UMGToolSet.h) | [23 declarations](https://github.com/EpicGames/UnrealEngine/blob/5789fc07f00643fb37f85ede9894cd08f1cebcef/Engine/Plugins/Experimental/Toolsets/UMGToolSet/Source/UMGToolSet/Private/UMGToolSet.h) |
| WidgetAnimationToolset.h | Not found by scan | [10 declarations](https://github.com/EpicGames/UnrealEngine/blob/5789fc07f00643fb37f85ede9894cd08f1cebcef/Engine/Plugins/Experimental/Toolsets/WidgetAnimationToolset/Source/WidgetAnimationToolset/Private/WidgetAnimationToolset.h) |
| WorldConditionTools.h | [2 declarations](https://github.com/EpicGames/UnrealEngine/blob/396c9f059903aed5fec78ecd3d437a40c6415368/Engine/Plugins/Experimental/Toolsets/WorldConditionsToolset/Source/WorldConditionsToolset/Private/WorldConditionTools.h) | [2 declarations](https://github.com/EpicGames/UnrealEngine/blob/5789fc07f00643fb37f85ede9894cd08f1cebcef/Engine/Plugins/Experimental/Toolsets/WorldConditionsToolset/Source/WorldConditionsToolset/Private/WorldConditionTools.h) |

## TurboMCP coverage and comparison status

Every row has a definition and static-reference audit in the [capability inventory](audit/capabilities.json). Family grouping routes investigation; it is not a mapping to an equivalent native function. R13 records source-level equivalence for the five deferred areas below; runtime comparisons and preference decisions remain pending. Rows outside those areas remain unverified: R04 owns the remaining behavioral/workflow audit and R14 owns forward runtime certification. R13 is not their pending owner.

| TurboMCP tool | Family | Epic 5.8.3 parity | Epic 5.9 parity | Routing status |
| --- | --- | --- | --- | --- |
| `EditTurboMCPBatchEdit` | asset-and-data | Pending R04 | Pending R04/R14 | No replacement claim |
| `EditTurboMCPCallLog` | schema-and-observability | Pending R04 | Pending R04/R14 | No replacement claim |
| `EditTurboMCPCallLogClear` | schema-and-observability | Pending R04 | Pending R04/R14 | No replacement claim |
| `EditTurboMCPComment` | graph | Pending R04 | Pending R04/R14 | No replacement claim |
| `EditTurboMCPCompile` | asset-and-data | Pending R04 | Pending R04/R14 | No replacement claim |
| `EditTurboMCPDataTableRows` | asset-and-data | Pending R04 | Pending R04/R14 | No replacement claim |
| `EditTurboMCPEnumAdd` | asset-and-data | No native tool found (source) | Create/AddEnumerator; no name control (source) | Retain; preference unverified |
| `EditTurboMCPFunctionAdd` | asset-and-data | Pending R04 | Pending R04/R14 | No replacement claim |
| `EditTurboMCPFunctionRemove` | asset-and-data | Pending R04 | Pending R04/R14 | No replacement claim |
| `EditTurboMCPGraphAppend` | graph | Pending R04 | Pending R04/R14 | No replacement claim |
| `EditTurboMCPGraphCollapse` | graph | Pending R04 | Pending R04/R14 | No replacement claim |
| `EditTurboMCPGraphCopy` | graph | Pending R04 | Pending R04/R14 | No replacement claim |
| `EditTurboMCPGraphDead` | graph | Pending R04 | Pending R04/R14 | No replacement claim |
| `EditTurboMCPGraphFixBackward` | graph | Pending R04 | Pending R04/R14 | No replacement claim |
| `EditTurboMCPGraphKnots` | graph | Pending R04 | Pending R04/R14 | No replacement claim |
| `EditTurboMCPGraphPatch` | graph | Pending R04 | Pending R04/R14 | No replacement claim |
| `EditTurboMCPGraphReplace` | graph | Pending R04 | Pending R04/R14 | No replacement claim |
| `EditTurboMCPGraphRepoint` | graph | Pending R04 | Pending R04/R14 | No replacement claim |
| `EditTurboMCPGraphSwapCall` | graph | Pending R04 | Pending R04/R14 | No replacement claim |
| `EditTurboMCPGraphTidy` | graph | Pending R04 | Pending R04/R14 | No replacement claim |
| `EditTurboMCPLocalVars` | graph | Pending R04 | Pending R04/R14 | No replacement claim |
| `EditTurboMCPMaterialBlocks` | material | Pending R04 | Pending R04/R14 | No replacement claim |
| `EditTurboMCPMaterialInstanceSet` | material | Pending R04 | Pending R04/R14 | No replacement claim |
| `EditTurboMCPMaterialLabelPromote` | material | Pending R04 | Pending R04/R14 | No replacement claim |
| `EditTurboMCPMaterialTidy` | material | Pending R04 | Pending R04/R14 | No replacement claim |
| `EditTurboMCPOverrideAdd` | asset-and-data | Pending R04 | Pending R04/R14 | No replacement claim |
| `EditTurboMCPOverrideRemove` | asset-and-data | Pending R04 | Pending R04/R14 | No replacement claim |
| `EditTurboMCPSave` | asset-and-data | Pending R04 | Pending R04/R14 | No replacement claim |
| `EditTurboMCPSaveWithoutThumbnail` | asset-and-data | Pending R04 | Pending R04/R14 | No replacement claim |
| `EditTurboMCPStructFields` | asset-and-data | No native tool found (source) | Rename+flags; no type change (source) | Retain type change/category/preview; native rename+flags are source-only candidates (unverified) |
| `EditTurboMCPStructNew` | asset-and-data | No native tool found (source) | Create/AddVariable; no field list (source) | Retain; preference unverified |
| `EditTurboMCPTrace` | schema-and-observability | Pending R04 | Pending R04/R14 | No replacement claim |
| `EditTurboMCPVarsAdd` | asset-and-data | Pending R04 | Pending R04/R14 | No replacement claim |
| `EditTurboMCPVcsCheckout` | asset-and-data | No native checkout (source) | No native checkout (source) | Retain; preference unverified |
| `EditTurboMCPWidgetBindAdd` | widget | Pending R04 | Pending R04/R14 | No replacement claim |
| `EditTurboMCPWidgetRetarget` | widget | Pending R04 | Pending R04/R14 | No replacement claim |
| `EditTurboMCPWidgetsAsVars` | widget | Pending R04 | Pending R04/R14 | No replacement claim |
| `LookTurboMCPCapture` | asset-and-data | Pending R04 | Pending R04/R14 | No replacement claim |
| `LookTurboMCPLayoutReport` | graph | Pending R04 | Pending R04/R14 | No replacement claim |
| `LookTurboMCPToolset` | schema-and-observability | Pending R04 | Pending R04/R14 | No replacement claim |
| `LookTurboMCPWidgetPreview` | widget | Pending R04 | Pending R04/R14 | No replacement claim |
| `PreviewTurboMCPBatchEdit` | asset-and-data | Pending R04 | Pending R04/R14 | No replacement claim |
| `PreviewTurboMCPComment` | graph | Pending R04 | Pending R04/R14 | No replacement claim |
| `PreviewTurboMCPDataTableRows` | asset-and-data | Pending R04 | Pending R04/R14 | No replacement claim |
| `PreviewTurboMCPEnumAdd` | asset-and-data | No native tool found (source) | GetEnumerators only (source) | Retain; preference unverified |
| `PreviewTurboMCPGraphAppend` | graph | Pending R04 | Pending R04/R14 | No replacement claim |
| `PreviewTurboMCPGraphCheck` | graph | Pending R04 | Pending R04/R14 | No replacement claim |
| `PreviewTurboMCPGraphCollapse` | graph | Pending R04 | Pending R04/R14 | No replacement claim |
| `PreviewTurboMCPGraphCopy` | graph | Pending R04 | Pending R04/R14 | No replacement claim |
| `PreviewTurboMCPGraphDead` | graph | Pending R04 | Pending R04/R14 | No replacement claim |
| `PreviewTurboMCPGraphFixBackward` | graph | Pending R04 | Pending R04/R14 | No replacement claim |
| `PreviewTurboMCPGraphKnots` | graph | Pending R04 | Pending R04/R14 | No replacement claim |
| `PreviewTurboMCPGraphPatch` | graph | Pending R04 | Pending R04/R14 | No replacement claim |
| `PreviewTurboMCPGraphReplace` | graph | Pending R04 | Pending R04/R14 | No replacement claim |
| `PreviewTurboMCPGraphRepoint` | graph | Pending R04 | Pending R04/R14 | No replacement claim |
| `PreviewTurboMCPGraphSwapCall` | graph | Pending R04 | Pending R04/R14 | No replacement claim |
| `PreviewTurboMCPGraphTidy` | graph | Pending R04 | Pending R04/R14 | No replacement claim |
| `PreviewTurboMCPLocalVars` | graph | Pending R04 | Pending R04/R14 | No replacement claim |
| `PreviewTurboMCPMaterialBlocks` | material | Pending R04 | Pending R04/R14 | No replacement claim |
| `PreviewTurboMCPMaterialLabelPromote` | material | Pending R04 | Pending R04/R14 | No replacement claim |
| `PreviewTurboMCPMaterialTidy` | material | Pending R04 | Pending R04/R14 | No replacement claim |
| `PreviewTurboMCPNodePins` | graph | Pending R04 | Pending R04/R14 | No replacement claim |
| `PreviewTurboMCPNodeType` | graph | Pending R04 | Pending R04/R14 | No replacement claim |
| `PreviewTurboMCPOverrideAdd` | asset-and-data | Pending R04 | Pending R04/R14 | No replacement claim |
| `PreviewTurboMCPStructFields` | asset-and-data | No native tool found (source) | ListVariables only (source) | Retain; preference unverified |
| `PreviewTurboMCPStructNew` | asset-and-data | No native tool found (source) | ListVariables only (source) | Retain; preference unverified |
| `PreviewTurboMCPToolset` | schema-and-observability | Pending R04 | Pending R04/R14 | No replacement claim |
| `PreviewTurboMCPVarsAdd` | asset-and-data | Pending R04 | Pending R04/R14 | No replacement claim |
| `PreviewTurboMCPVcsCheckout` | asset-and-data | State booleans only (source) | State only; no checkout (source) | Retain; preference unverified |
| `PreviewTurboMCPWidgetRetarget` | widget | Pending R04 | Pending R04/R14 | No replacement claim |
| `ReadTurboMCPAssetCost` | asset-and-data | Pending R04 | Pending R04/R14 | No replacement claim |
| `ReadTurboMCPAssetDescribe` | asset-and-data | Pending R04 | Pending R04/R14 | No replacement claim |
| `ReadTurboMCPAssetSearch` | asset-and-data | Pending R04 | Pending R04/R14 | No replacement claim |
| `ReadTurboMCPBPLint` | asset-and-data | Pending R04 | Pending R04/R14 | No replacement claim |
| `ReadTurboMCPBlueprintLoad` | asset-and-data | Pending R04 | Pending R04/R14 | No replacement claim |
| `ReadTurboMCPCallCost` | schema-and-observability | Pending R04 | Pending R04/R14 | No replacement claim |
| `ReadTurboMCPCallLog` | schema-and-observability | Pending R04 | Pending R04/R14 | No replacement claim |
| `ReadTurboMCPCommentList` | graph | Pending R04 | Pending R04/R14 | No replacement claim |
| `ReadTurboMCPDataTable` | asset-and-data | Pending R04 | Pending R04/R14 | No replacement claim |
| `ReadTurboMCPDirtyList` | asset-and-data | Pending R04 | Pending R04/R14 | No replacement claim |
| `ReadTurboMCPEditQueue` | asset-and-data | Pending R04 | Pending R04/R14 | No replacement claim |
| `ReadTurboMCPGraphLoad` | graph | Pending R04 | Pending R04/R14 | No replacement claim |
| `ReadTurboMCPMaterialBlocks` | material | Pending R04 | Pending R04/R14 | No replacement claim |
| `ReadTurboMCPMaterialInstance` | material | Pending R04 | Pending R04/R14 | No replacement claim |
| `ReadTurboMCPMaterialLabels` | material | Pending R04 | Pending R04/R14 | No replacement claim |
| `ReadTurboMCPMaterialLoad` | material | Pending R04 | Pending R04/R14 | No replacement claim |
| `ReadTurboMCPMaterialTrace` | material | Pending R04 | Pending R04/R14 | No replacement claim |
| `ReadTurboMCPNodeFind` | graph | Pending R04 | Pending R04/R14 | No replacement claim |
| `ReadTurboMCPNodeResolve` | graph | Pending R04 | Pending R04/R14 | No replacement claim |
| `ReadTurboMCPOverrides` | asset-and-data | Pending R04 | Pending R04/R14 | No replacement claim |
| `ReadTurboMCPPlayCheck` | schema-and-observability | Pending R04 | Pending R04/R14 | No replacement claim |
| `ReadTurboMCPPureChecks` | asset-and-data | Pending R04 | Pending R04/R14 | No replacement claim |
| `ReadTurboMCPRefs` | asset-and-data | Pending R04 | Pending R04/R14 | No replacement claim |
| `ReadTurboMCPStatus` | schema-and-observability | Pending R04 | Pending R04/R14 | No replacement claim |
| `ReadTurboMCPToolHelp` | schema-and-observability | Pending R04 | Pending R04/R14 | No replacement claim |
| `ReadTurboMCPToolIndex` | schema-and-observability | Pending R04 | Pending R04/R14 | No replacement claim |
| `ReadTurboMCPToolSchema` | schema-and-observability | Pending R04 | Pending R04/R14 | No replacement claim |
| `ReadTurboMCPTrace` | schema-and-observability | Pending R04 | Pending R04/R14 | No replacement claim |
| `ReadTurboMCPTypeFind` | asset-and-data | Pending R04 | Pending R04/R14 | No replacement claim |
| `ReadTurboMCPVcsState` | asset-and-data | can_edit_asset/is_checked_out (source) | GetAssetSourceControlState (source) | Retain diagnostics; preference unverified |
| `ReadTurboMCPWidgetTree` | widget | Pending R04 | Pending R04/R14 | No replacement claim |

## Required comparison for a preference decision

For each workflow record exact discovered native and TurboMCP names, required inputs, side effects, supported scope, output completeness, refusals, undo/rollback/save behavior, call count, measured latency/output bytes, native media handling and clean fixture evidence. Use the same fixture and goal on the same 5.8.3 build. Separate 5.9 source observations from 5.9 runtime tests. Record a benefit only when measured; retain native tools where they are the better fit.

Do not advertise safe rollback, lower token cost or broader coverage based only on header descriptions. The reviewer approves each preference row and its evidence before it reaches agent-visible help.
