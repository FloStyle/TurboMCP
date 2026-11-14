# NOV09-01 Tidy response review

The original report recorded a roughly 451,000-character blocks-mode preview for 65 selected nodes in a 450-node graph. That graph is unavailable locally.

The scoped source candidate `caa77c3b` passed `Tools/check-worker-scope.py` at base `2e849234ee53e9082ec21ed768598a08536a6c9a`. Source review found that preview now begins with `WOULD APPLY`, `WOULD ROLL BACK`, or `ALREADY TIDY`, retains the compact scores, omits full geometry by default, offers `GeometryOffset` and `GeometryLimit` pages in blocks mode, and bounds the assembled response. Edit retains compact scores and no longer emits full geometry. The contract was corrected to describe preview-only pagination; a repeated edit is not a safe way to page evidence.

The supervised stock Unreal 5.8.4 build passed at `caa77c3b` (local execution `3f2bc1a4458c433fb9248f96eb77386c`). The focused `test-graph-tidy` run at `9fe78409` passed doctor and build, then was **blocked before test assertions**: the editor did not bind MCP port 8000 within the startup deadline, including the runner's one recovery attempt. The runner recorded zero suites passed, zero failed, and one blocked; it restored 16 fixture files (local execution `dc91f5e085ab43e2b6597c9dedb3b879`). This is not runtime acceptance of the Tidy output change.

The added focused checks cover summary-first default output, a bounded response without rectangle rows, two consecutive numbered geometry pages, and invalid-pagination refusal. Run that suite once the editor is healthy. The original 450-node response and real client behavior remain field follow-ups. Keep implementation marked done with validation blocked, separately from any field acceptance.
