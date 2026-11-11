# NOV10-01: asset search folder scope

Readiness: ready
Integrator-approved repair, 2026-11-11. User requests Sol High, quick fixes first and minimal tests.

Current review: ReadTurboMCPAssetSearch accepts only Query, and path: uses substring matching. A separate path argument cannot constrain this schema. Implement an optional explicit folder/path argument with segment-boundary matching and recursive control; preserve existing Query-only calls and document legacy path: semantics accurately. Reconcile the original Nov10 report before choosing the parameter name. Do not silently ignore folder scope. Reject invalid full package folders clearly. Remain registry-only: no loads, saves, checkout, map load or PIE.

Review behavior, help, English diagnostics and comments together. Use an existing focused asset-search suite if available, with a sibling-folder case; otherwise provide a narrow fixture proposal to integrator for registration. Batch implementation, compile once and run only directly relevant verification. No full regression. Report exact evidence and limitations; do not mark speculative causes fixed.

Exact scope is assignment nov11-asset-search-scope. Parent owns trackers, catalog, integration and push. Preserve the completed widget-inventory candidate on this branch. No engine edits, no DSH. After submitting this repair, read-only rank remaining Nov10 issues from simple to complex with exact next-scope proposals; do not implement outside scope. Parent will dispatch subsequent assignments.
