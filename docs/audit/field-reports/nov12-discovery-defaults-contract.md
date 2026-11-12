# Optional discovery arguments

Readiness: ready. Owner: integrator. Task: NOV09-10.

Repair the confirmed signature/default gaps in nov12-discovery-reconciliation.md. Nonempty reflected string defaults must normalize to legacy empty semantics: GraphLoad Revision=first and EntryFilter=*; BlueprintLoad GraphFilter=* with manifest/read/60000 defaults; ToolHelp *=all toolsets or list tools, with no arguments listing toolsets. Preserve explicit empty calls. Later GraphLoad pages still require a real matching revision; first cannot bypass this. Document literal * as all, first as initial revision.

Keep public parameter names to preserve clients; explicitly document that ToolHelp uses ToolsetName/ToolName while ToolSchema uses Toolset/Tool. Do not alter generic engine schema generation. Exact four source files in worker-scopes.json. No other reader behavior or filtering repair in this task. One incremental compile and focused omitted-argument/schema check after the batch; no full regression.

## Focused follow-up

Readiness: ready. nov12-discovery-check adds AssetSearch Path=* normalization (legacy empty preserved) and omitted-argument calls to the existing asset metadata suite. Initial run b3b30a14ab5540ee83af192ae0c6ad43: help passed, metadata failed before assertions because Path was required despite optional help. Build 97b59764d6554981b8233c06231290f2 passed. Retain both results. Verify ToolHelp no args, BlueprintLoad asset-only, GraphLoad first page without optional args and rejection of later pages without a real revision. No broader regression.
