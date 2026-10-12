# Tools inventory

*** GENERATED FROM THE DIRECTORY BY `Tools/make-tools-inventory.py`, SO NO TOOL CAN BE UNDOCUMENTED. *** The description on each line is the **first line of the script's own header** - what its author wrote, not a summary invented here. [`README.md`](README.md) keeps the prose: why each one exists and the order they are used in.

## Nothing, static (17)

| Tool | What its own header says |
|---|---|
| `apply-license-header.ps1` | Puts the MPL-2.0 notice at the top of every .h, .cpp and .cs in the plugin. |
| `audit-suite-quality.py` | Audit whether each suite can actually FAIL, without running anything. |
| `audit-tool-coverage.py` | Audit that EVERY MCP tool has a testable call, without needing the editor. |
| `build-test-graph.py` | Build the Phase 2 test graph, using the ENGINE's own writer. |
| `check-includes.py` | Verify that every header the plugin includes actually exists in the engine. |
| `check-plugin.py` | Pre-build health check for the TurboMCP plugin. |
| `check-probe-graphs.py` | Prove the acceptance asset's probe graphs contain the structures the specs need. |
| `check-line-cites.py` | Check line-number citations into Markdown files against the text that is actually there. |
| `check-refs.py` | Check every cross-reference in the documentation against the file it points at. |
| `check-suite-shapes.py` | Check the quality audit's own detection against the suites, independently. |
| `count-plan.py` | Count the plan's rows by their LAST FIELD, the way the plan itself says to count them. |
| `measure-overlap.py` | Measure what two documents actually share, so a claim about overlap is a reading rather than an estimate. |
| `common.ps1` | Finds the repository root, the test project and the engine install, WITHOUT A PATH OF ANYONE'S MACHINE. |
| `license-header.txt` | The MPL-2.0 notice itself, as text. `apply-license-header.ps1` reads it, so the notice has one spelling rather than one per script. |
| `make-questions-index.py` | Build the status index at the top of QUESTIONS.md from the questions' own text. |
| `make-tools-doc.py` | Generate TOOLS.md from the tool declarations in the plugin header. |
| `tmcp_paths.py` | Finds the repository root and the Unreal Engine install, for every Python tool in this directory. |

## A running editor (55)

| Tool | What its own header says |
|---|---|
| `check-mcp-server.py` | Probe the engine's MCP server and report whether TurboMCP registered its toolsets. |
| `test-agent-help.py` | Exercise the Phase 1 agent-help tools against the running server. |
| `test-asset-cost.py` | `R-ASSET-COST`: the size map and hard references of one asset, split by provenance. |
| `test-asset-tools.py` | Exercise the phase 1 asset and type tools against the running editor. |
| `test-batch-tools.py` | Drive the tools added in the batch round: the schema reader, the node type readers, the material |
| `test-bp-lint.py` | `R-BP-LINT`: the six checks, their denominators, and the filter that names them. |
| `test-call-cost.py` | Exercise `R-CALL-COST`. |
| `test-call-log.py` | Exercise E-CALL-LOG, R-CALL-LOG and E-CALL-LOG-CLEAR against the running editor. |
| `test-capture-asset.py` | Exercise L-CAPTURE-ASSET: capture an ASSET EDITOR, not just a graph. |
| `test-capture-selectors.py` | Exercise the four framing selectors added to L-CAPTURE by WORKING_PLAN row 2.1. |
| `test-capture.py` | Exercise L-CAPTURE against the running editor. |
| `test-comment-boxes.py` | Exercise P-COMMENT and E-COMMENT against the running editor. |
| `test-comments.py` | Exercise R-COMMENT-LIST against the running editor. |
| `test-datatable-rows.py` | `P/E-DATATABLE-ROWS`: add, change and remove rows in one call, proved by reading back through a DIFFERENT tool. |
| `test-datatable.py` | Exercise R-DATATABLE against the running editor. |
| `test-enum-add.py` | Exercise the P/E-ENUM-ADD pair against the running editor. |
| `test-function.py` | Exercise E-FUNCTION-ADD and E-FUNCTION-REMOVE against the running editor. |
| `test-graph-append.py` | `P/E-GRAPH-APPEND` (`T-WRITE-2`): a text block is added to a graph, and nothing is removed. |
| `test-graph-check.py` | `P-GRAPH-CHECK` (`T-WRITE-1`): graph text is checked, and NOTHING is written. |
| `test-graph-dead.py` | Exercise P-GRAPH-DEAD and E-GRAPH-DEAD against the running editor. |
| `test-graph-fix-backward.py` | Drive P/E-GRAPH-FIX-BACKWARD: preview, edit, the roll-back, and the cross-tool read-back. |
| `test-graph-knots.py` | Drive `P-GRAPH-KNOTS` and `E-GRAPH-KNOTS` against the acceptance asset. |
| `test-graph-patch.py` | `P/E-GRAPH-PATCH` (`T-WRITE-5`): changing two lines changes exactly two nodes. |
| `test-graph-replace.py` | `P/E-GRAPH-REPLACE` (`T-WRITE-3`, `T-WRITE-4`): a body is rewritten, the entry is REUSED, and what was |
| `test-graph-structure.py` | `P/E-GRAPH-COPY` and `P/E-GRAPH-COLLAPSE`. |
| `test-graph-tidy.py` | Drive `P/E-GRAPH-TIDY` against the acceptance fixture. |
| `test-json.py` | Check that R-BP-LOAD's JSON carries the same facts as its text. |
| `test-kit-ta17-ta19.py` | TA-17 built and loadable, TA-18 and TA-19 refused by name. |
| `test-layout.py` | Exercise L-LAYOUT-REPORT against the running editor. |
| `test-local-vars.py` | Exercise P-LOCAL-VARS and E-LOCAL-VARS against the running editor. |
| `test-mat-inst-load.py` | Prove R-MAT-INST-LOAD reports the parent chain, the overrides, and the inherited parameters. |
| `test-material-blocks.py` | `P/E-MAT-BLOCKS`: make labels follow blocks. |
| `test-material-label-promote.py` | `P/E-MAT-LABEL-PROMOTE`: give values names - create the declaration, rewire the readers. |
| `test-material-tidy.py` | `P/E-MAT-TIDY`: lay out a material, one comment box per block. |
| `test-material.py` | Drive the four material readers against the fixture built for them. |
| `test-override-add.py` | Exercise the P/E-OVERRIDE-ADD pair against the running editor. |
| `test-overrides.py` | Exercise R-OVERRIDES against the running editor. |
| `test-panel-reach.py` | Prove that EVERY tool is reachable the way the PANEL reaches it. |
| `test-panel.py` | Probe the TurboMCP panel's tool runner, through the commandlet that reaches it. |
| `test-play-check.py` | `R-PLAY-CHECK`: play the project briefly and report what it complained about. |
| `test-pure-checks.py` | Exercise the two PURE functions behind `R-CALL-COST`, with LITERAL inputs. |
| `test-refs.py` | Exercise R-REFS against the running editor. |
| `test-struct.py` | Exercise P/E-STRUCT-NEW and P/E-STRUCT-FIELDS against the running editor. |
| `test-trace.py` | `E-TRACE` and `R-TRACE`: arming a trace point, reading it, and clearing it. |
| `test-uncovered-tools.py` | Exercise the tools that NOTHING has ever called. |
| `test-vars-add.py` | Exercise the P/E-VARS-ADD pair against the running editor. |
| `test-vcs-write.py` | `E-SAVE` and `E-STRUCT-FIELDS`: the automatic checkout a write tool performs before it writes. |
| `test-vcs.py` | `E-VCS-CHECKOUT`: state per asset, and a checkout through the project's own provider. |
| `test-widget-preview.py` | Exercise L-WIDGET-PREVIEW against the running editor. |
| `test-widget-retarget.py` | Exercise P/E-WIDGET-RETARGET against the running editor. |
| `test-widget-tree-sync.py` | Exercise `R-WIDGET-TREE`, the slot-ordered tree reader behind `P/E-WIDGET-TREE`. |
| `test-widget-tree.py` | Exercise the widget tree section of R-BP-LOAD. |
| `test-widgets-as-vars.py` | Exercise E-WIDGETS-AS-VARS against the running editor. |
| `tmcp.py` | Call a TurboMCP tool through the running MCP server and print its answer. |
| `tmcp_editor.py` | One owner of the editor lifecycle. |

## Nothing else running (14)

| Tool | What its own header says |
|---|---|
| `build.ps1` | Builds the plugin's editor target, with NO PATH OF ANYONE'S MACHINE in it. |
| `make-acceptance-asset.ps1` | Rebuilds the acceptance asset from its commandlet. |
| `make-kit-assets.ps1` | Rebuilds the hand-made acceptance kit, TA-1 to TA-13. |
| `make-material-asset.ps1` | Rebuilds the material fixture that the four material readers need. |
| `make-test-assets.py` | Create a minimal test asset in the project, to prove scripted asset |
| `make-test-blueprint.py` | Create the Phase 2 Blueprint scaffolding for node-reading tests. |
| `make-test-map-blueprint.py` | Build the Class-Based Blueprint the PIE test map needs, with a graph that EXECUTES. |
| `make-test-map.ps1` | Builds the PIE test map: a small level containing a placed actor whose code runs during Play-In-Editor. |
| `make-test-widget.py` | Create and save the Phase 3 Widget Blueprint test asset. |
| `make-tools-inventory.py` | Generate the Tools/ inventory from the files, so no tool is undocumented. |
| `run-suites.ps1` | Runs every acceptance suite in sequence, keeping the editor alive per suite. |
| `setup-engine.ps1` | Set up Unreal Engine 5.8.3 from source and build the TurboMCP editor plugin. |
| `start-editor.ps1` | Start the TurboMCP test editor and prove it reached a working state. |
| `test-suite-timeout.ps1` | The per-suite deadline, as a standalone script, so it can be exercised without the harness around it. |

*** 86 files, all listed. ***
