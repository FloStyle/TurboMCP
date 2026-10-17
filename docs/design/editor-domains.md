# Editor domain contracts

[Documentation](../README.md)

Each domain uses the common result and mutation contracts. Domain-specific acceptance must compare actual Unreal state with the requested outcome.

| Domain | Main implementation files | Required behavior |
| --- | --- | --- |
| Material graphs/instances | TurboMCPMaterial.cpp, TurboMCPMaterialInstance.cpp, MaterialBlocks/LabelsPromote/Tidy | Preserve expression identity and links; distinguish inherited defaults from overrides; retain unconnected input constants; verify restoration |
| Widgets | TurboMCPWidgetTree.cpp, WidgetTreeSync, WidgetRetarget, WidgetPreview | Distinguish class defaults from instance overrides; preserve tree/slot/binding identity; report complete requested detail; disclose project-code execution |
| Tables/types/functions | DataTableRows, Struct, EnumAdd, VarsAdd, Function, Overrides | Validate before mutation; read back names/values; report actual affected assets and unsupported cases |
| References/cost/readers | Refs, AssetCost, BlueprintRead, BPLint | Normalize accepted asset forms; direction both combines actual in/out results; distinguish unknown/unsearched from zero; obey detail/budget contracts |
| Capture/layout | Capture, CaptureAsset, CaptureSelector, Layout | Identify selection/geometry, disclose estimates, verify framing and return accessible media |
| Runtime observations | Trace, PlayCheck, CallRecorder | Distinguish no observations, unmatched identity and observed absence; retain or explicitly clear lifecycle state |
| Persistence/source control | SourceControl, SourceControlWrite, Compile | Report checkout, compile, memory state and saved bytes separately; name partial failures |

All filenames above are under Plugins/TurboMCP/Source/TurboMCP with the TurboMCP prefix. This is a navigation map, not a release capability matrix.

Read-only operations must not silently mutate, save or check out assets. A restoration strategy may restore only state the operation owns; it must not erase a user's pre-existing dirty state. Widget preview and play can execute project code and must be documented as such. Observation tools must not claim that a node never ran when event-to-node matching was unavailable.

New authoring capabilities require their own bounded design and refusal semantics. Keep the existing supported scope intact while fixing false success and data loss. Each task identifies exact fixtures and test selectors; source-only checks cannot prove editor-state behavior.
