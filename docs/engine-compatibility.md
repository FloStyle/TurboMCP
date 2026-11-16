# Unreal 5.9 planning

[Documentation](README.md)

Current baseline: 5.8.4, commit f45729c0b6f097038e72b52d366a62baeca3275b (engine.lock.json).
The source comparison below was originally made against 5.8.3 at 396c9f059903aed5fec78ecd3d437a40c6415368; it must be refreshed against the current baseline before adapter approval.
Forward inspection: ue5-main at 5789fc07f00643fb37f85ede9894cd08f1cebcef (Build.version says 5.9.0). This is a development snapshot, not a supported release promise or a full engine-wide migration audit.

## Confirmed source differences that matter

1. **Call-recorder compile break:** FToolset::GetToolsetClass() is removed. TurboMCPCallRecorder.cpp overrides it and calls it to match toolsets. Replace that identity mechanism through a small version adapter, with recorder registration/unregistration tests. Do not substitute a private manifest API without verifying its contract.
2. **Plugin dependency discovery:** the SemanticSearchToolset descriptor is absent at its old Experimental/Toolsets location, while TestProject explicitly enables it. Locate the supported replacement/move or make the test-only dependency conditional before building 5.9; deletion at the old path does not prove the entire feature vanished.
3. **MCP media envelope:** the editor adapter extracts schema-marked FToolsetImage values to native image content blocks and replaces structured-content slots with references. Test screenshot/capture handling and preserve non-text blocks in harness clients; do not assume every response is one FString/text block.
4. **Null values:** ReferenceConverter emits JSON null for null UObject references instead of the old None string path. Add explicit null fixtures wherever wrappers or schema consumers relied on string sentinels.
5. **Child actors:** SetObjectProperties adds a guard against unsupported per-instance child actor edits. Expect actionable refusals and target the supported template when appropriate; do not work around safety checks.
6. **Tool registry and overlap:** manifests, async result types and new engine toolsets expand the surface. Reconcile TurboMCP discovery/help/trap data with actual 5.9 schemas and compare native enum/struct/source-control/widget-animation capabilities before implementing duplicate roadmap features.

The compared subtrees contain 63 changed files in ToolsetRegistry, 41 in ModelContextProtocol and 128 in Experimental/Toolsets. Counts include additions, removals and nonfunctional changes; they are not counts of breaking APIs. For example, the inspected SlateInspectorToolset public header difference was documentation-only.

Keep 5.8.4 pinned for current fixes. Use a separate engine checkout and compatibility branch for 5.9, share no build outputs, and run compiler, discovery/schema, result-envelope, rollback/undo/dirty-state and fixture acceptance checks before declaring support.

Sources: [pinned engine tree](https://github.com/EpicGames/UnrealEngine/tree/5789fc07f00643fb37f85ede9894cd08f1cebcef), [registry API removal](https://github.com/EpicGames/UnrealEngine/commit/1abce413e7), [MCP media adapter](https://github.com/EpicGames/UnrealEngine/commit/258762f0f19db17c85b9ab282ff824d4365bd0ab), [child actor guard](https://github.com/EpicGames/UnrealEngine/commit/8e6196da2c790c6842b38ce5f183b08fd6a43ad1). Epic repository access is required.

## Preparation queue — 2026-11-14

Reuse R13's completed historical source audit and R14's pending adapter task; do not count these as new field-report fixes. R14 remains needs_reconciliation until exact API replacements and file scopes are reviewed. The target above is the recorded snapshot, not a claim about today's Epic branch head.

| Order | Preparation | Completion evidence |
| --- | --- | --- |
| 1 | Refresh the source comparison from pinned 5.8.4 to the selected immutable 5.9 commit. Verify Epic branch identity and record any proposed target change before changing the lock. | Exact engine commits, public API signatures, dependency locations, and adopt/adapter/retain decisions; no inferred runtime parity. |
| 2 | Specify the recorder identity replacement and resolve SemanticSearchToolset dependency discovery. | Supported replacement contract, exact files/version boundaries, lifecycle negative controls and one reviewed bounded assignment per adapter. |
| 3 | Check client image blocks, structured references and UObject null handling. | Focused synthetic result-contract fixtures, including mixed text/image responses and actual JSON null; retain 5.8.4 behavior. |
| 4 | Reconcile native tool schemas and safety behavior. | Discovery/help differences, child-actor refusal behavior and native overlap decisions; preserve stock Unreal and existing guards. Recheck applicable engine proposals without assuming they are fixed upstream. |
| 5 | Compile the plugin and run focused fixtures on the separate target checkout. | Independent engine/artifact identity; recorder lifecycle, discovery, media/null and relevant undo/dirty-state results. Serialize scarce build/editor resources; no full engine rebuild unless required. |
| 6 | Review packaging and support eligibility with R12/R16. | Separate source-build and installed-engine evidence. Keep supported=false until the required matrix passes; record blocked cases explicitly. |

No 5.9 build, runtime test, new Epic branch fetch or support certification was performed for this preparation update. Current bug fixes continue on 5.8.4; DSH stays paused. Historical release evidence is not rebased by changing its recorded engine commit.
