# Technical decision review

Review scope: the 16 normative ARC contracts and repository execution/publication model, 2026-10-17. Each decision below is a planning judgment with an explicit validation obligation. Existing implementation is not presumed compliant.

| Contract/choice | Decision | Reason and obligation |
| --- | --- | --- |
| ARC01 release boundary | Keep pinned 5.8.3 source baseline; separate optional matrices | Existing safety fixes need a stable target; no adjacent-version support inference |
| ARC02 FString + typed final result | Keep current public signatures for v1 | Avoid a wholesale schema migration; prove one authoritative envelope, invalid-input handling and unknown outcomes |
| ARC03 mutation coordinator | Keep one owner and fail-fast reentrancy | Prevent nested transactions and competing edits; verify all affected callers and transaction end count |
| ARC04 compile baseline | Keep baseline-relative diagnostics | Prevent pre-existing errors from becoming false new failures; never hide ensures or weaken capture to pass |
| ARC05 rollback versus persistence | Keep explicit separate states | Editor undo is not filesystem atomicity; test restored state and partial save |
| ARC06 stable identities/stale plans | Keep explicit resolution and fingerprints | Ambiguous/changed targets must refuse, not select first match |
| ARC07 graph representation | Keep summaries separate from lossless edit formats | Prevent destructive round trips from incomplete reports |
| ARC08 exact edit semantics | Keep bounded operation contracts | Independent read-back must establish links/properties actually changed |
| ARC09 restricted batch | Keep graphRepoint-only single-graph release scope | Current implementation is unfinished; broad multi-asset work would multiply risk and maintenance |
| ARC10 preview/execution | Keep explicit execution disclosure | Widget construction and play may run project code; toolset names are not a safety boundary |
| ARC11 observation truth | Keep unknown/unmatched distinct from absent | Prevent false dead-code/never-ran/zero-reference reports |
| ARC12 testing | Keep layers and negative controls; repair static audit false positives | A test reference or source guard cannot establish runtime correctness |
| ARC13 build/package | Keep separate source and binary artifacts; design test-module extraction before implementing | Full public source includes tests/tools; installed package must not drag fixture-only dependencies unnecessarily |
| ARC14 local trust/logging | Keep trusted loopback deployment and bounded logs | Do not invent remote auth/server work for this release; verify actual bind and log limits |
| ARC15 repository workflow | One complete tracked tree, one ordered task queue, fresh public history | Avoid private/public documentation drift and hidden contributor prerequisites |
| ARC16 D-008/D-009 | Keep writer refusal/explicit reader all-graphs and unchanged diagnostic safety | Empty graph cannot mean arbitrary first graph; failing acceptance does not authorize narrowing safety |
| Shared engine ownership | Replace per-checkout check-then-write as the concurrency guarantee | Needs actual atomic cross-worktree/port tests; until then use one integration owner |
| Feature investment | Native comparison before expansion | Keep value in safety, bounded workflows and truthful output; do not duplicate Epic's new primitives without a demonstrated gap |
| Release automation | Automate preparation and verification; explicitly authorize publication | No development push should publish incomplete work or private history |

Every new technical choice beyond these boundaries belongs in its bounded task: alternative considered, observable benefit, engine assumption, failure behavior, test and maintenance cost. Do not create architecture prose solely to justify a speculative feature.
