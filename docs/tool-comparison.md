# Epic and TurboMCP comparison

[Documentation](README.md) · [Tool selection](design/tool-selection.md) · [Engine compatibility](engine-compatibility.md)

## Evidence and scope

Baseline: Epic source 5.8.3 at `396c9f059903aed5fec78ecd3d437a40c6415368`. Forward target: Epic 5.9 development at `5789fc07f00643fb37f85ede9894cd08f1cebcef`. TurboMCP plugin tree: `2513a0bbb42604fd3f6427bc7536e61c7822c912`. The development target is not a released compatibility promise.

The tables distinguish source declarations from runtime capability. A tool present in Epic source is not necessarily enabled in a default editor or shipped in an installed distribution. The native scan covers AICallable declarations in headers under Experimental/Toolsets; Python, data-defined tools and other engine locations still need R13 discovery review. A missing entry is not proof that Epic lacks a feature.

All 101 TurboMCP tools are accounted for below. No tool is currently labeled as a verified replacement for an Epic tool. R13 completes the source comparison, R04 establishes behavior, and R17 publishes evidence-backed routing guidance.

## Where TurboMCP aims to add value

| Workflow | Epic 5.8.3 source evidence | TurboMCP intended value | Epic 5.9 source evidence | Decision / proof needed |
| --- | --- | --- | --- | --- |
| Blueprint graph workflows | Complete native equivalent not resolved by this header scan | Compact graph inspection, explicit identity, coordinated edits and layout | Full native graph parity still to inspect | Compare exact reads, supported nodes, failed-edit state and output size; no superiority claim yet |
| Widget authoring | UMGToolSet declares creation, tree edits and compilation | Tree representation, targeted workflows and preview | Existing UMG tools plus WidgetAnimationToolset | Retain demonstrated workflow benefits; defer broad animation duplication |
| Enums and structs | These specific authoring headers are absent from the scanned subtree | Existing authoring workflows on baseline | Dedicated enum and struct toolsets | Compare naming, validation, read-back and failure state before expansion |
| Source control | Dedicated SourceControlToolset header absent from scan | Explicit project asset state | Revision and changelist interfaces | Prefer native primitives where they meet requirements; verify provider semantics |
| Capture and editor interaction | EditorApp capture and SlateInspector tools | Graph framing, layout diagnostics and widget preview | Both tool families remain | Measure framing and result handling; screenshot existence alone is not proof |
| Materials | Complete native equivalent unresolved | Material inspection, reroute workflows and compact output | Native parity unresolved | Compare concrete material workflows before declaring an advantage |
| Connection and protocol | Epic MCP server and registry | Tool extensions, help and outcome conventions | Registry/media changes and native stdio proxy | Reuse Epic transport; isolate version differences; do not build a competing proxy |

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

Every row has a definition and static-reference audit in the [capability inventory](audit/capabilities.json). Family grouping routes investigation; it is not a mapping to an equivalent native function. All runtime comparisons and preference decisions remain pending.

| TurboMCP tool | Family | Epic 5.8.3 parity | Epic 5.9 parity | Routing status |
| --- | --- | --- | --- | --- |
| `EditTurboMCPBatchEdit` | asset-and-data | Pending R13/R04 | Pending R13 | No replacement claim |
| `EditTurboMCPCallLog` | schema-and-observability | Pending R13/R04 | Pending R13 | No replacement claim |
| `EditTurboMCPCallLogClear` | schema-and-observability | Pending R13/R04 | Pending R13 | No replacement claim |
| `EditTurboMCPComment` | graph | Pending R13/R04 | Pending R13 | No replacement claim |
| `EditTurboMCPCompile` | asset-and-data | Pending R13/R04 | Pending R13 | No replacement claim |
| `EditTurboMCPDataTableRows` | asset-and-data | Pending R13/R04 | Pending R13 | No replacement claim |
| `EditTurboMCPEnumAdd` | asset-and-data | Pending R13/R04 | Pending R13 | No replacement claim |
| `EditTurboMCPFunctionAdd` | asset-and-data | Pending R13/R04 | Pending R13 | No replacement claim |
| `EditTurboMCPFunctionRemove` | asset-and-data | Pending R13/R04 | Pending R13 | No replacement claim |
| `EditTurboMCPGraphAppend` | graph | Pending R13/R04 | Pending R13 | No replacement claim |
| `EditTurboMCPGraphCollapse` | graph | Pending R13/R04 | Pending R13 | No replacement claim |
| `EditTurboMCPGraphCopy` | graph | Pending R13/R04 | Pending R13 | No replacement claim |
| `EditTurboMCPGraphDead` | graph | Pending R13/R04 | Pending R13 | No replacement claim |
| `EditTurboMCPGraphFixBackward` | graph | Pending R13/R04 | Pending R13 | No replacement claim |
| `EditTurboMCPGraphKnots` | graph | Pending R13/R04 | Pending R13 | No replacement claim |
| `EditTurboMCPGraphPatch` | graph | Pending R13/R04 | Pending R13 | No replacement claim |
| `EditTurboMCPGraphReplace` | graph | Pending R13/R04 | Pending R13 | No replacement claim |
| `EditTurboMCPGraphRepoint` | graph | Pending R13/R04 | Pending R13 | No replacement claim |
| `EditTurboMCPGraphSwapCall` | graph | Pending R13/R04 | Pending R13 | No replacement claim |
| `EditTurboMCPGraphTidy` | graph | Pending R13/R04 | Pending R13 | No replacement claim |
| `EditTurboMCPLocalVars` | graph | Pending R13/R04 | Pending R13 | No replacement claim |
| `EditTurboMCPMaterialBlocks` | material | Pending R13/R04 | Pending R13 | No replacement claim |
| `EditTurboMCPMaterialInstanceSet` | material | Pending R13/R04 | Pending R13 | No replacement claim |
| `EditTurboMCPMaterialLabelPromote` | material | Pending R13/R04 | Pending R13 | No replacement claim |
| `EditTurboMCPMaterialTidy` | material | Pending R13/R04 | Pending R13 | No replacement claim |
| `EditTurboMCPOverrideAdd` | asset-and-data | Pending R13/R04 | Pending R13 | No replacement claim |
| `EditTurboMCPOverrideRemove` | asset-and-data | Pending R13/R04 | Pending R13 | No replacement claim |
| `EditTurboMCPSave` | asset-and-data | Pending R13/R04 | Pending R13 | No replacement claim |
| `EditTurboMCPSaveWithoutThumbnail` | asset-and-data | Pending R13/R04 | Pending R13 | No replacement claim |
| `EditTurboMCPStructFields` | asset-and-data | Pending R13/R04 | Pending R13 | No replacement claim |
| `EditTurboMCPStructNew` | asset-and-data | Pending R13/R04 | Pending R13 | No replacement claim |
| `EditTurboMCPTrace` | schema-and-observability | Pending R13/R04 | Pending R13 | No replacement claim |
| `EditTurboMCPVarsAdd` | asset-and-data | Pending R13/R04 | Pending R13 | No replacement claim |
| `EditTurboMCPVcsCheckout` | asset-and-data | Pending R13/R04 | Pending R13 | No replacement claim |
| `EditTurboMCPWidgetBindAdd` | widget | Pending R13/R04 | Pending R13 | No replacement claim |
| `EditTurboMCPWidgetRetarget` | widget | Pending R13/R04 | Pending R13 | No replacement claim |
| `EditTurboMCPWidgetsAsVars` | widget | Pending R13/R04 | Pending R13 | No replacement claim |
| `LookTurboMCPCapture` | asset-and-data | Pending R13/R04 | Pending R13 | No replacement claim |
| `LookTurboMCPLayoutReport` | graph | Pending R13/R04 | Pending R13 | No replacement claim |
| `LookTurboMCPToolset` | schema-and-observability | Pending R13/R04 | Pending R13 | No replacement claim |
| `LookTurboMCPWidgetPreview` | widget | Pending R13/R04 | Pending R13 | No replacement claim |
| `PreviewTurboMCPBatchEdit` | asset-and-data | Pending R13/R04 | Pending R13 | No replacement claim |
| `PreviewTurboMCPComment` | graph | Pending R13/R04 | Pending R13 | No replacement claim |
| `PreviewTurboMCPDataTableRows` | asset-and-data | Pending R13/R04 | Pending R13 | No replacement claim |
| `PreviewTurboMCPEnumAdd` | asset-and-data | Pending R13/R04 | Pending R13 | No replacement claim |
| `PreviewTurboMCPGraphAppend` | graph | Pending R13/R04 | Pending R13 | No replacement claim |
| `PreviewTurboMCPGraphCheck` | graph | Pending R13/R04 | Pending R13 | No replacement claim |
| `PreviewTurboMCPGraphCollapse` | graph | Pending R13/R04 | Pending R13 | No replacement claim |
| `PreviewTurboMCPGraphCopy` | graph | Pending R13/R04 | Pending R13 | No replacement claim |
| `PreviewTurboMCPGraphDead` | graph | Pending R13/R04 | Pending R13 | No replacement claim |
| `PreviewTurboMCPGraphFixBackward` | graph | Pending R13/R04 | Pending R13 | No replacement claim |
| `PreviewTurboMCPGraphKnots` | graph | Pending R13/R04 | Pending R13 | No replacement claim |
| `PreviewTurboMCPGraphPatch` | graph | Pending R13/R04 | Pending R13 | No replacement claim |
| `PreviewTurboMCPGraphReplace` | graph | Pending R13/R04 | Pending R13 | No replacement claim |
| `PreviewTurboMCPGraphRepoint` | graph | Pending R13/R04 | Pending R13 | No replacement claim |
| `PreviewTurboMCPGraphSwapCall` | graph | Pending R13/R04 | Pending R13 | No replacement claim |
| `PreviewTurboMCPGraphTidy` | graph | Pending R13/R04 | Pending R13 | No replacement claim |
| `PreviewTurboMCPLocalVars` | graph | Pending R13/R04 | Pending R13 | No replacement claim |
| `PreviewTurboMCPMaterialBlocks` | material | Pending R13/R04 | Pending R13 | No replacement claim |
| `PreviewTurboMCPMaterialLabelPromote` | material | Pending R13/R04 | Pending R13 | No replacement claim |
| `PreviewTurboMCPMaterialTidy` | material | Pending R13/R04 | Pending R13 | No replacement claim |
| `PreviewTurboMCPNodePins` | graph | Pending R13/R04 | Pending R13 | No replacement claim |
| `PreviewTurboMCPNodeType` | graph | Pending R13/R04 | Pending R13 | No replacement claim |
| `PreviewTurboMCPOverrideAdd` | asset-and-data | Pending R13/R04 | Pending R13 | No replacement claim |
| `PreviewTurboMCPStructFields` | asset-and-data | Pending R13/R04 | Pending R13 | No replacement claim |
| `PreviewTurboMCPStructNew` | asset-and-data | Pending R13/R04 | Pending R13 | No replacement claim |
| `PreviewTurboMCPToolset` | schema-and-observability | Pending R13/R04 | Pending R13 | No replacement claim |
| `PreviewTurboMCPVarsAdd` | asset-and-data | Pending R13/R04 | Pending R13 | No replacement claim |
| `PreviewTurboMCPVcsCheckout` | asset-and-data | Pending R13/R04 | Pending R13 | No replacement claim |
| `PreviewTurboMCPWidgetRetarget` | widget | Pending R13/R04 | Pending R13 | No replacement claim |
| `ReadTurboMCPAssetCost` | asset-and-data | Pending R13/R04 | Pending R13 | No replacement claim |
| `ReadTurboMCPAssetDescribe` | asset-and-data | Pending R13/R04 | Pending R13 | No replacement claim |
| `ReadTurboMCPAssetSearch` | asset-and-data | Pending R13/R04 | Pending R13 | No replacement claim |
| `ReadTurboMCPBPLint` | asset-and-data | Pending R13/R04 | Pending R13 | No replacement claim |
| `ReadTurboMCPBlueprintLoad` | asset-and-data | Pending R13/R04 | Pending R13 | No replacement claim |
| `ReadTurboMCPCallCost` | schema-and-observability | Pending R13/R04 | Pending R13 | No replacement claim |
| `ReadTurboMCPCallLog` | schema-and-observability | Pending R13/R04 | Pending R13 | No replacement claim |
| `ReadTurboMCPCommentList` | graph | Pending R13/R04 | Pending R13 | No replacement claim |
| `ReadTurboMCPDataTable` | asset-and-data | Pending R13/R04 | Pending R13 | No replacement claim |
| `ReadTurboMCPDirtyList` | asset-and-data | Pending R13/R04 | Pending R13 | No replacement claim |
| `ReadTurboMCPEditQueue` | asset-and-data | Pending R13/R04 | Pending R13 | No replacement claim |
| `ReadTurboMCPGraphLoad` | graph | Pending R13/R04 | Pending R13 | No replacement claim |
| `ReadTurboMCPMaterialBlocks` | material | Pending R13/R04 | Pending R13 | No replacement claim |
| `ReadTurboMCPMaterialInstance` | material | Pending R13/R04 | Pending R13 | No replacement claim |
| `ReadTurboMCPMaterialLabels` | material | Pending R13/R04 | Pending R13 | No replacement claim |
| `ReadTurboMCPMaterialLoad` | material | Pending R13/R04 | Pending R13 | No replacement claim |
| `ReadTurboMCPMaterialTrace` | material | Pending R13/R04 | Pending R13 | No replacement claim |
| `ReadTurboMCPNodeFind` | graph | Pending R13/R04 | Pending R13 | No replacement claim |
| `ReadTurboMCPNodeResolve` | graph | Pending R13/R04 | Pending R13 | No replacement claim |
| `ReadTurboMCPOverrides` | asset-and-data | Pending R13/R04 | Pending R13 | No replacement claim |
| `ReadTurboMCPPlayCheck` | schema-and-observability | Pending R13/R04 | Pending R13 | No replacement claim |
| `ReadTurboMCPPureChecks` | asset-and-data | Pending R13/R04 | Pending R13 | No replacement claim |
| `ReadTurboMCPRefs` | asset-and-data | Pending R13/R04 | Pending R13 | No replacement claim |
| `ReadTurboMCPStatus` | schema-and-observability | Pending R13/R04 | Pending R13 | No replacement claim |
| `ReadTurboMCPToolHelp` | schema-and-observability | Pending R13/R04 | Pending R13 | No replacement claim |
| `ReadTurboMCPToolIndex` | schema-and-observability | Pending R13/R04 | Pending R13 | No replacement claim |
| `ReadTurboMCPToolSchema` | schema-and-observability | Pending R13/R04 | Pending R13 | No replacement claim |
| `ReadTurboMCPTrace` | schema-and-observability | Pending R13/R04 | Pending R13 | No replacement claim |
| `ReadTurboMCPTypeFind` | asset-and-data | Pending R13/R04 | Pending R13 | No replacement claim |
| `ReadTurboMCPVcsState` | asset-and-data | Pending R13/R04 | Pending R13 | No replacement claim |
| `ReadTurboMCPWidgetTree` | widget | Pending R13/R04 | Pending R13 | No replacement claim |

## Required comparison for a preference decision

For each workflow record exact discovered native and TurboMCP names, required inputs, side effects, supported scope, output completeness, refusals, undo/rollback/save behavior, call count, measured latency/output bytes, native media handling and clean fixture evidence. Use the same fixture and goal on the same 5.8.3 build. Separate 5.9 source observations from 5.9 runtime tests. Record a benefit only when measured; retain native tools where they are the better fit.

Do not advertise safe rollback, lower token cost or broader coverage based only on header descriptions. The reviewer approves each preference row and its evidence before it reaches agent-visible help.
