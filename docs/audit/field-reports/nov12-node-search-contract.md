# Bounded node search completion

Readiness: ready. Owner: integrator. Priorities: NOV10-07, overlapping NOV09-08 and NOV09-19.

Current review: NodeFind loads one Blueprint, matches every split word, omits delegate function identity and literal values, caps output at 40 without continuation. NodeContext requires a separate exact graph name even when a qualified node ID contains one. Existing node reader tests provide the narrow validation entry point.

Implement asset, folder and project scopes with deterministic registry enumeration and explicit coverage. Preserve legacy AND matching by default; add explicit any/all term matching, optional literal matching and node-class filtering. Search stored delegate identities. Return bounded rows, totals/count-only, continuation and skipped/refused assets. Never describe an incomplete scan or page as exhaustive. Full node IDs emitted by Find must resolve in Context; conflicting explicit asset or graph must refuse, never silently retarget. Improve contextual names and plain-English help.

No save, checkout, map load or PIE. Use the existing bounded reader loading precautions: registry readiness, top-level Blueprint classification, map/redirector and hard-dependency checks, compilation-disabled loading; disclose load-hook effects. A project search must offer continuation through all eligible assets, not permanently cap at a small first subset. Define bounded per-call scan work and response size separately; count-only must report its scan coverage honestly. Keep inherited assets distinct, no inferred runtime dispatch.

Review source before running candidate code. Batch changes then run only the node reader workflow and focused new synthetic assertions, one incremental build. Preserve wrong-identity refusals and legacy readers. Parent owns catalog/tracker changes. The shared header requires exclusive scope; DSH stays paused and engine source remains stock.
