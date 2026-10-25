# Vision workflow review

2026-10-25. Reproducible suite: `python Tools/execute-tests.py --profile engine --suites test-vision-workflow --execute`. Fixture authoring is native setup, not evidence for graph authoring tools. All assets are disposable and unsaved; tracked fixture code recreates them.

## Five workflows

1. Dense Blueprint: 254 nodes, 12 requests and 12 OnDestroyed handlers, three event graphs, sequence/branch/print operations, comments and a 48-operation calculation function. Capture overview, node, neighbours, comment and region. Use reader context before images. Whole-graph images are maps, not readable code review at this density.
2. Widget: render known text at 640x240 and 1920x720, then inspect its live designer. This does not simulate input.
3. Material: open/capture a disposable constant-color material editor. Explicit Target=fit now frames the measured graph; an empty Target keeps the current view. Shader state is now reported separately from displayed-frame freshness. The constant-color sphere is visually confirmed after normal active-editor ticks; material instance/function coverage is still planned.
4. Editor: capture live UI and then open another asset editor. This reproduced a Slate hit-test assertion, repaired by capturing through Slate's own window renderer rather than repainting live widgets into a temporary window/grid.
5. Play: start an owned eight-second PlayCheck, capture the active play window, poll until stopped. The test project now uses CommonGameViewportClient. The owned play window completes with zero warnings/errors; this is not general gameplay acceptance.

## Evidence and quality

Candidate 9108d1b; scope gate passed. Native 6257458f771049da8ff56197d8901bf9: fixture compilation, 21 images, PNG decoding/dimensions, serialized graph preservation and owned play completion. Native test accepts only the exact stock UMG duplicate shortcut warning and preserves it in evidence; other warnings fail. The wrapper intentionally retains PlayCheck's runtime error as a separate finding rather than asserting gameplay correctness. Offline 7a1e0f454c0442f7a1cf9eb1606bb59b: 22/26; existing structure, language, client-retry and lookup-guard failures.

Initial 8f96e8caf22a4a998541ec23c008bee0 failed on SameSize(AppendedGrid.Get()) after screen capture. Repair uses TakeScreenshot for live window/editor widgets; graph scratch rendering and offscreen transient widget rendering stay separate. Actual screenshot dimensions drive output and optional downscale. No engine modification. Earlier fixture build errors (header location and event delegate accessor) were corrected. The first graph preservation reader budget was invalid; final preservation uses full serialized graph snapshots, with a valid separately recorded reader call.

Visually inspected dense overview and close-ups, function overview/detail, material editor, designer and play captures. Targeted graph nodes are readable; overview text is small. The initial material image was cropped; the follow-up full-graph fit is visually checked. The initial screen output mislabeled PIE as an editor window; actual active-window title reporting now fixes this. Neither change adds a dedicated viewport capability. Changed code comments explain hit-test ownership. Existing helper prose contains historical implementation detail and Size overloads; these are recorded for the next focused quality pass, not declared fixed.

Canonical findings and remaining scenarios are in vision-workflow-review.json. Five workflows are registered in workflow-tool-coverage.json. Reusing tools does not increase the unique reviewed count: 38/125 remains. Animation/Niagara/material-instance/function/DataTable/HDR/DPI extremes remain planned, not tested. The local image gallery is generated evidence under the workspace outputs/vision-review folder; repository review data is durable and shared.

## Window identity and material framing follow-up

Native bf10e7592ece4810b906900c78c3b4c3 passed the final candidate. Actual editor and PIE window titles were recorded. The material fixture uses separated nodes; its full fit image was visually inspected, and original expression/output positions and connection were preserved. Unsupported fit and invalid framing refuse. Slate zoom presets are respected before centering. The standard Material Editor working copy is used, with an editor identity guard. Updated helper/help English documents fit effects and shader-readiness uncertainty.

Offline 2d230dafffe2474a91fcf0320b9ef59c: 22/26, the same four baseline failures. VISION-02 is done; VISION-03 is partial: full-graph fit done, shader readiness and targeted capture still open. The sphere still showed a fallback appearance. CommonUI configuration error remains in play evidence. Unique tool coverage remains 38/125.

## Preview readiness and CommonUI repair

Final candidate 1fdcde2; native 509881025b9244918ebbc5547c64f6a7 passed build, vision assertions and owned play (zero warnings/errors). The 23-image sequence includes initial material compilation, shader availability, and another capture after three seconds of normal active-editor ticks. The final image visibly shows the expected green sphere. The first available-shader capture can still show an older fallback frame: output now explicitly distinguishes these facts and tells the caller to retry after editor ticks. No blocking shader compile or engine modification was added. Material instance editors report unknown rather than an invented state.

Capture helper output no longer claims FWidgetRenderer offscreen rendering: live captures use Slate. The test-project CommonUI prerequisite is fixed; VISION-05 is done. VISION-03 now contains only the remaining targeted subgraph feature. VISION-04 and VISION-06 remain open. Unique bounded tool reviews remain 38/125. Offline 65936db52d854ae08e287ed0246b0a2d passed 26/26 before the final delayed-capture and prose changes; the final native run covers those changes. Historical runs above retain their original failures.

## Live PIE menu loop

See [play/menu workflow](play-menu-workflow.md). Native d64216f0036647cfaa62701131f443fd passed explicit PIE capture and the complete Settings/Back/Purchase loop with independent callback assertions, four visual states and zero runtime warnings/errors. Existing native Slate input tools are reused. Dedicated editor/camera/multiple-player targeting and CommonUI/controller tests remain open.

Enhanced Input foundation integration: native 8463aa977fd34b429ff769f1c1aebc4e and offline 56b3d64da8ab420f8037ab1454cf4556 (26/26) passed. Stock engine unchanged. VISION-08 is done for the bounded scenario; VISION-09 tracks actual CommonUI controller/tab/back navigation. Full tool review remains 38/127.

CommonUI follow-up: native 83b71dfcc8764dff9cd82b7b212a8a46 passed; see [workflow evidence](commonui-play-workflow.md). VISION-09 is done for this bounded scenario. VISION-10 tracks broader navigation and production CommonUI diagnostics.

Focus/diagnostics integration: native 3c78d90b08224fe69e055a985ae23708 and offline 821fc9cbfc244511bd3aa86bbf308757 (26/26) passed. VISION-11 records the fixed focus-loss bug; VISION-10 retains broader unverified runtime cases. Full-review count remains 38/127.
