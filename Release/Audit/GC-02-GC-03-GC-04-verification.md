> Archived audit evidence from 2026-10-16. Follow the [sequential roadmap](<C:/Users/Example/Desktop/TurboMCP/Release/ROADMAP.md>) for execution. Historical parallel-agent instructions do not apply.

# GC-02 / GC-03 / GC-04 verification record

## Focused baseline

Before changes, `python C:\Users\Example\Desktop\TurboMCP\Tools\test-graph-check.py` passed. It proved the focused checker contract for existing graph text: exact findings, real pin names, valid-text acceptance, and no mutation.

The tree measurement found:

- **GC-02:** `FTurboMCPGraphCheck::ResolveTypeId` searched only instantiated graph nodes. Lookup and writers used `FTurboMCPNodeType`'s action database, so a valid new type could be found/created but rejected by check.
- **GC-03:** `TurboMCPGraphTextLex::ExtractHeadTypeId` reads to whitespace and does not treat `[` or `]` as delimiters. Bracketed IDs already retain their complete head token; no parser rewrite was necessary.
- **GC-04:** There was no local-label parser, form field, link-resolution map, or help example.

## Implemented changes

- `FTurboMCPNodeType::CollectEntries` is now the sole action-database/filter walk. Lookup, checker probes, append spawning, and replace/patch through append use it. Static count: **one** `FBlueprintActionFilter` and **one** `FBlueprintActionDatabase::Get` call remain in `TurboMCPNodeType.cpp`.
- Checker tries `BuildProbeNode` before it reports a type unknown.
- Graph append accepts `; label Name` on a new form, resolves `(-> Name:Pin)` in the submitted block, and refuses duplicate labels by name.
- Graph-append help documents `; label First` and `(-> First:then)`.
- `Tools/test-graph-append.py` adds an edit-and-readback local-label link test.

## Verification

1. Baseline focused checker: **PASS**.
2. Earlier focused plugin build: **PASS** before the final single-resolver extraction.
3. Final static focused checks: **PASS** — `git diff --check`, `python -m py_compile Tools\test-graph-append.py`, and the one-filter/one-database callsite count above.
4. Post-build focused runtime attempt: **INCONCLUSIVE**. `test-graph-append.py` passed its new-type resolver/append checks through sections 1–3, then its MCP connection reset before the local-label section. A clean editor restart then exited during engine initialization before MCP bound port 8000.
5. The final build was stopped after it expanded to a **3,673-action full engine rebuild**, which exceeds the requested focused-verification scope. A later `-Module=TurboMCP` attempt still attached to a 3,439-action engine rebuild, so its process tree was stopped as well. Neither produced a focused result.

No full suite was run. It would not establish more than the focused graph/parser proof and is inappropriate while the editor cannot stay alive long enough to run that proof.
