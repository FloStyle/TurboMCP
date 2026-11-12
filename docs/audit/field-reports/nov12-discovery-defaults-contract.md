# Optional discovery arguments

Readiness: ready. Owner: integrator. Task: NOV09-10.

Repair the confirmed signature/default gaps in nov12-discovery-reconciliation.md. Nonempty reflected string defaults must normalize to legacy empty semantics: GraphLoad Revision=first and EntryFilter=*; BlueprintLoad GraphFilter=* with manifest/read/60000 defaults; ToolHelp *=all toolsets or list tools, with no arguments listing toolsets. Preserve explicit empty calls. Later GraphLoad pages still require a real matching revision; first cannot bypass this. Document literal * as all, first as initial revision.

Keep public parameter names to preserve clients; explicitly document that ToolHelp uses ToolsetName/ToolName while ToolSchema uses Toolset/Tool. Do not alter generic engine schema generation. Exact four source files in worker-scopes.json. No other reader behavior or filtering repair in this task. One incremental compile and focused omitted-argument/schema check after the batch; no full regression.
