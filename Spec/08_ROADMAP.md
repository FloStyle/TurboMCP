# Roadmap

Build in this order. Each phase ends when its tests in `06_ACCEPTANCE.md`
pass and are recorded. Start each phase only when the one before it is
done, except where noted.

| Phase | Capabilities | Why this order |
|---|---|---|
| 0. Base | Plugin, module, toolset registration, the shared formats (result layout, node IDs, asset path resolver, token estimate), the editor window with server row and tool list, the test asset kit | Every tool depends on these. Build the formats once |
| 1. Agent help | R-TOOL-INDEX, R-TOOL-SCHEMA with the traps file, R-ASSET-SEARCH, R-ASSET-DESCRIBE, R-TYPE-FIND, P-NODE-TYPE, P-NODE-PINS, R-DIRTY-LIST, E-COMPILE, E-SAVE | Agents lose time on these in every session, with the engine's tools alone. Small and high value |
| 2. Blueprint reading | R-BP-LOAD, R-GRAPH-LOAD (both formats), R-REFS, R-OVERRIDES, R-NODE-RESOLVE, R-NODE-FIND, R-DATATABLE, R-COMMENT-LIST | The base for everything after. The `write` format is the bridge to the engine writer until phase 7 |
| 3. Looking | L-LAYOUT-REPORT, L-CAPTURE, L-WIDGET-PREVIEW | UI work is blind without them |
| 4. Structure | P/E-VARS-ADD, P/E-ENUM-ADD, E-WIDGETS-AS-VARS, P/E-OVERRIDE-ADD, E-FUNCTION-ADD and remove, P/E-WIDGET-RETARGET | Gaps with no engine tool at all |
| 5. Call log | E-CALL-LOG, R-CALL-LOG | Makes every later phase easier to debug |
| 6. Layout | P/E-GRAPH-FIX-BACKWARD, P/E-GRAPH-DEAD, P/E-COMMENT, P/E-GRAPH-TIDY with nested boxes, P/E-GRAPH-KNOTS, P/E-LOCAL-VARS | Local repair first: it helps mostly good graphs, where full layout tends to refuse |
| 7. Graph writing | P-GRAPH-CHECK, then Append, then Replace, then Patch | Largest phase. The round trip test is the gate. Start the parser early, in parallel with phase 3 or later, if there is capacity |
| 8. Materials | R-MAT-LABELS, R-MAT-LOAD, R-MAT-TRACE, R-MAT-BLOCKS, then P/E-MAT-LABEL-PROMOTE, P/E-MAT-TIDY, P/E-MAT-BLOCKS (confirm D-17 with the owner first), then material writing | Readers must print every input before any write |
| 9. Runtime | R-PLAY-CHECK, E-TRACE and R-TRACE, then breakpoint research | Needs a play session; most risk |

Material reading (the first four in phase 8) can move earlier if the
owner asks; it does not depend on phases 3 to 7.
