# Task review

[Roadmap](../../ROADMAP.md) | [Task index](../../TODO.md) | [Repair contracts](repair-contracts.md)

All 110 task records were reviewed against the current implementation boundaries, tests and architecture at private main `6aee9dfd92a4c06a73e89c091973f982d715b7a2`. This includes 85 imported records, seven foundation/follow-up records and 18 roadmap tasks. Original IDs and historical requirements are preserved.

The review is complete as backlog triage and specification review. It is not a claim that every historical symptom was reproduced, every feature was audited, or every task is ready for implementation. Each task now states a current finding, its remaining question or acceptance condition, the owning roadmap task, source references and real verification commands.

## What an agent may do

R13 has completed its bounded source comparison after integration review. R16 is next, for integration constraints. The queue order is unchanged. Give one task to one worker through the claim workflow. Imported records remain evidence for the assigned roadmap investigation, not permission to implement the whole backlog. R14 is now unready until its replacement API and adapter boundaries are specified.

A source-confirmed defect needs a failing regression and a reviewed repair scope before integration. The repair contracts below are the starting specifications; they are not a blanket implementation assignment. Runtime investigations may finish with explicitly unresolved evidence, but a product repair cannot be marked done on that basis.

## Decisions

| Decision | Records | Meaning |
| --- | ---: | --- |
| completed | 4 | Bounded foundation and R13 source comparison completed; broader runtime claims stay open. |
| confirmed | 17 | A specific current source path supports a defect or contract gap; runtime acceptance is still required. |
| deferred | 18 | New scope or compatibility work needs a native comparison, design or unavailable target. |
| duplicate | 6 | Another named record owns the change; these requirements remain linked. |
| investigate | 17 | The symptom or design conflict still needs a targeted experiment or decision. |
| partial | 18 | Some historical claims are stale or already implemented; remaining checks are named. |
| plan | 15 | The roadmap or release contract is actionable within its dependencies and stated limits. |
| reference | 6 | Useful historical regression evidence, not an implementation task. |
| requires spec | 1 | Implementation must wait for an exact reviewed specification. |
| superseded | 7 | Old process instructions are replaced by the current workflow. |
| withdrawn | 1 | The stated diagnosis is contradicted by current code; do not implement its proposed fix. |

## Observed validation during review

Pinned 5.8.3 run `80d3adda842046f1ae27c282836158f9` built successfully, verified the loaded plugin identity and restored 16 fixture files. `test-refs` passed; its cases do not send explicit direction `both` against a nonzero edge set. `test-bp-lint` failed one assertion: the actual reader refusal was `Unknown tool EditTurboMCPBPLint`. This is a separate refusal-expectation problem, not a reproduction of the cast warning. The overall engine run failed and remains recorded as failed. Raw evidence stays in `.local/executions/`.

The compile-ensure native test is in `TestProject/Source/TurboMCPTest/Private/TurboMCPTestCompileEnsureTest.cpp`. Its induced ensure errors need classification; searching only the shipping plugin would incorrectly call the fixture missing. No new compile-ensure run was claimed by this review.

## Main corrections

- Explicit reference direction `both` validates but skips both query branches. Existing tests miss it.
- Cast lint uses a guessed output pin name. The pinned engine provides a typed accessor.
- Patch and Replace retain empty-name first-graph fallback, contrary to D-008.
- Restricted batch calls a supposedly reusable core that starts its own transaction. Strict scope, sequencing, compile and restoration remain required.
- Table application groups removals before sets. Enum output counts requested names. Widget reads omit all object-reference overrides.
- Save success currently relies on the dirty flag in a branch that does not require the write result. ARC05 requires stronger evidence.
- Several historical fixes already exist: order-independent asset filters, propagated spawn notes, measured-geometry refusal in Tidy and real-wire cycle adjacency. Their broader regressions remain separate.
- The old multi-asset batch, default-delete cleanup and automatic dirty-flag cleanup proposals must not override current safety contracts.

## Record-by-record register

| Task | Decision | Follow-up owner |
| --- | --- | --- |
| [Validate call-cost caps without hiding invalid input](../../todo/019cf7af.md) | partial | R04 |
| [Map trace events to actual executing nodes](../../todo/02674939.md) | confirmed | R09 |
| [Use consistent getter-placement rules](../../todo/02ba0f24.md) | confirmed | R07 |
| [Preserve useful output while reducing noise](../../todo/04111899.md) | investigate | R10 |
| [Exercise material creation failures and verify restoration](../../todo/048107d9.md) | partial | R05 |
| [Retire obsolete public-branch reconciliation](../../todo/081329d2.md) | superseded | R15 |
| [Specify complete node-link and widget-detail reads](../../todo/0b84b1d8.md) | investigate | R06, R08 |
| [Verify focused search and parameter help](../../todo/0e4658ad.md) | partial | R10 |
| [Resolve omitted macro, asset and session requirements](../../todo/100b52a6.md) | partial | R06, R13 |
| [Report editable widget reference overrides](../../todo/1256778b.md) | confirmed | R08 |
| [Compare missing widget writes with native tools](../../todo/1388b93a.md) | deferred | R13 |
| [Shorten successful checkout reports](../../todo/1db73082.md) | investigate | R10 |
| [Distinguish struct dependents from proven breakage](../../todo/1f5fe672.md) | investigate | R08 |
| [Verify enum names after creation](../../todo/214d6e7a.md) | confirmed | R08 |
| [Keep bound handlers out of unused-code claims](../../todo/261bb8db.md) | duplicate | R06 |
| [Preserve ten positive regression workflows](../../todo/2f066e6c.md) | investigate | R11 |
| [Retain play-check observations without a clean bill of health](../../todo/37d3d49e.md) | reference | R09 |
| [Identify side effects of reader and preview calls](../../todo/3be553c8.md) | investigate | R05 |
| [Separate reader correctness findings by domain](../../todo/3eae39b1.md) | investigate | R08, R09 |
| [Make explicit both references equal in plus out](../../todo/484d5638.md) | confirmed | R09 |
| [Describe token estimates without universal claims](../../todo/48608da1.md) | confirmed | R10 |
| [Define supported reader-to-writer graph forms](../../todo/4de12720.md) | investigate | R06 |
| [Defer curve authoring until a workflow gap is shown](../../todo/4ec9e1cc.md) | deferred | R13 |
| [Verify append node structure and compile outcomes](../../todo/4f1bfd61.md) | partial | R06, R05 |
| [Retain the historical unused-symbol survey as reference](../../todo/4ff1c6d8.md) | reference | R04 |
| [Retire the old in-progress suite sweep](../../todo/5013737a.md) | superseded | R11 |
| [Verify session-report completeness and stable IDs](../../todo/51678b9f.md) | partial | R10 |
| [Strengthen window-list assertions](../../todo/5301f48e.md) | confirmed | R04 |
| [Keep audit counts tied to explicit scope](../../todo/53fb9672.md) | reference | R04 |
| [Compare bulk widget authoring before expansion](../../todo/5b06fa2e.md) | deferred | R13 |
| [Repair the restricted single-graph batch contract](../../todo/5b6a8e45.md) | confirmed | R05 |
| [Retire the legacy release-run container](../../todo/5f2cee78.md) | superseded | R11, R15 |
| [Defer a session change ledger pending semantics](../../todo/602feaab.md) | deferred | R13 |
| [Compare function creation and signature gaps](../../todo/63d5c9e2.md) | deferred | R13, R08 |
| [Specify safe asset operations only for native gaps](../../todo/66d97dc8.md) | deferred | R13 |
| [Withdraw the unused OutNote diagnosis](../../todo/701a6c3d.md) | withdrawn | R10 |
| [Treat failure-wording scans as advisory](../../todo/71e2e86a.md) | partial | R04 |
| [Replace the stale comment-audit count with exact findings](../../todo/745637f4.md) | investigate | R05, R08, R10 |
| [Preserve override-reader regression evidence](../../todo/77b738a1.md) | reference | R08 |
| [Compare live widget-instance inspection](../../todo/7ae3dd76.md) | deferred | R13, R08 |
| [Avoid false unnecessary-cast warnings](../../todo/7bbe9c3c.md) | confirmed | R06 |
| [Include struct make/break usage in removal checks](../../todo/7bd81b84.md) | confirmed | R08 |
| [Separate result decoding from compile-fixture verdicts](../../todo/7de8d185.md) | partial | R04, R05 |
| [Correct the widget-token delimiter comment](../../todo/81cbc063.md) | confirmed | R10 |
| [Deduplicate unsafe-write and comment findings](../../todo/8af56e37.md) | duplicate | R05, R10 |
| [Retain trap-loader behavior and clarify replacement semantics](../../todo/8beb2deb.md) | reference | R10 |
| [Provide valid help examples for graph and trace tools](../../todo/9263bd95.md) | duplicate | R10 |
| [Distinguish fixture serialization drift from semantic drift](../../todo/9935c0ea.md) | investigate | R03, R11 |
| [Design explicit unsaved-asset revert separately](../../todo/9c47994b.md) | deferred | R13, R05 |
| [Consolidate editor presentation requirements under R18](../../todo/9d4624ff.md) | partial | R18 |
| [Verify node purity and declared-name aliases](../../todo/9db83209.md) | partial | R06 |
| [Make task-to-tool discovery bounded and evidence-based](../../todo/9e5d5c9a.md) | investigate | R17, R10 |
| [Make usage analysis conservative about delegates and self](../../todo/a28a09fd.md) | confirmed | R06 |
| [Separate recorder availability from obsolete member cleanup](../../todo/a312b008.md) | partial | R04, R16 |
| [Separate table creation from nested row correctness](../../todo/a4eed2e1.md) | deferred | R13, R08 |
| [Retain pure-helper regression evidence](../../todo/a6a86af7.md) | reference | R04 |
| [Make layout counts name their populations](../../todo/a8341453.md) | investigate | R07 |
| [Define field syntax without changing value semantics](../../todo/abdd2c08.md) | partial | R04, R08 |
| [Extract a genuinely transaction-free repoint core](../../todo/b230e508.md) | confirmed | R05 |
| [Preserve run-start fixtures without resetting user work](../../todo/b4d297e2.md) | duplicate | R03, R11 |
| [Defer broad material authoring after reader verification](../../todo/b6fa0cf7.md) | deferred | R13, R08 |
| [Defer runtime input automation without a complete design](../../todo/ba518a2a.md) | deferred | R13 |
| [Make table previews and application follow caller order](../../todo/ba6df3c8.md) | confirmed | R08 |
| [Keep general multi-asset batch outside v1 scope](../../todo/bbb63d1e.md) | deferred | R13, R05 |
| [Verify geometry provenance across all layout tools](../../todo/be9c2edb.md) | partial | R07 |
| [Enforce explicit graph selection and asset-wide reads](../../todo/c1e04796.md) | confirmed | R06 |
| [Consolidate release approval under current gates](../../todo/c2865829.md) | superseded | R11, R15 |
| [Verify captured pixels match reported framing](../../todo/c421299d.md) | investigate | R07 |
| [Verify override removal preserves unrelated overrides](../../todo/c4485ba2.md) | confirmed | R08, R05 |
| [Resolve layout objectives before changing the solver](../../todo/c5c54253.md) | investigate | R07 |
| [State graph search coverage explicitly](../../todo/ca5bbbfc.md) | investigate | R06 |
| [Distinguish checkout state from checkout provenance](../../todo/cdbf31c9.md) | partial | R13, R10 |
| [Retire the old write-format plan-count request](../../todo/d03a170f.md) | superseded | R06, R10 |
| [Specify ordered node previews within the graph contract](../../todo/d0f909a3.md) | deferred | R06, R13 |
| [Preserve all graph nodes and link endpoints in reads](../../todo/d3897f07.md) | confirmed | R06 |
| [Split new asset, component and listener proposals](../../todo/d4433d0e.md) | deferred | R13 |
| [Use one agent entry point and shipped trap source](../../todo/e35faf67.md) | partial | R10, R17 |
| [Verify Tidy cycle reports with real wire graphs](../../todo/e6fc78bd.md) | partial | R07 |
| [Use canonical identity with unambiguous display aliases](../../todo/e866ba0d.md) | partial | R06 |
| [Split helper proposals and restrict source-file access](../../todo/e9a28b8d.md) | deferred | R13, R07, R08 |
| [Retire the parallel-fix batch verifier](../../todo/ee4fde11.md) | superseded | R11 |
| [Keep intermittent post-generation swap failures open](../../todo/efa76989.md) | investigate | R03, R06 |
| [Defer destructive cleanup helpers until scoped](../../todo/f1a04ef1.md) | deferred | R13, R06 |
| [Verify exact unconnected material constants](../../todo/f434ffba.md) | partial | R08 |
| [Specify a complete pin-level graph read](../../todo/ff18311b.md) | investigate | R06 |
| [Track installed-engine verification through R16 and R12](../../todo/foundation-engine-matrix.md) | duplicate | R16, R12 |
| [Retain shared-ownership completion evidence](../../todo/foundation-shared-lease.md) | superseded | R02 |
| [Maintain the source-backed task review](../../todo/foundation-task-triage.md) | plan | R04 |
| [Require accepted evidence before fresh-history publication](../../todo/public-cutover.md) | plan | R15 |
| [R01: Repair test-audit evidence classifications](../../todo/roadmap-r01.md) | completed | R01 |
| [R02: Enforce shared engine ownership](../../todo/roadmap-r02.md) | completed | R02 |
| [R03: Verify fixture recovery and binary identity](../../todo/roadmap-r03.md) | completed | R03 |
| [R04: Audit callable behavior and result contracts](../../todo/roadmap-r04.md) | plan | R04 |
| [R05: Specify mutation and restricted-batch repairs](../../todo/roadmap-r05.md) | plan | R05 |
| [R06: Specify graph correctness repairs](../../todo/roadmap-r06.md) | plan | R06 |
| [R07: Specify geometry and capture repairs](../../todo/roadmap-r07.md) | plan | R07 |
| [R08: Specify domain read-back and restoration repairs](../../todo/roadmap-r08.md) | plan | R08 |
| [R09: Specify truthful references and runtime observations](../../todo/roadmap-r09.md) | plan | R09 |
| [R10: Align human docs and help with verified behavior](../../todo/roadmap-r10.md) | plan | R10 |
| [R11: Complete the pinned baseline acceptance matrix](../../todo/roadmap-r11.md) | plan | R11 |
| [R12: Validate installed-engine packages on the exact target](../../todo/roadmap-r12.md) | plan | R12 |
| [R13: Compare native capabilities before expansion](../../todo/roadmap-r13.md) | completed | R16, R04, R14 |
| [R14: Implement only an approved compatibility adapter](../../todo/roadmap-r14.md) | requires spec | R14 |
| [R15: Activate gated private CI and release preparation](../../todo/roadmap-r15.md) | plan | R15 |
| [R16: Establish installed-engine integration constraints](../../todo/roadmap-r16.md) | plan | R16 |
| [R17: Expose verified tool preferences](../../todo/roadmap-r17.md) | plan | R17 |
| [R18: Design the editor panel before rewriting it](../../todo/roadmap-r18.md) | plan | R18 |
| [Keep one owner for task review](../../todo/task-reconciliation.md) | duplicate | R04 |
| [Verify forward protocol changes on pinned source and runtime](../../todo/ue59-protocol.md) | deferred | R13, R14 |
| [Gate recorder adaptation on exact registry API evidence](../../todo/ue59-recorder.md) | deferred | R13, R14 |

## Evidence limits and maintenance

Source links remain relative so they work in a fresh public clone. The review revision and source hashes identify the inspected snapshot; displayed line numbers may move later. The machine-readable register records source hashes and commands so a later reviewer can locate the original evidence; hashes are not a claim that the entire referenced file was audited. Re-review affected decisions when source changes. Do not update the review revision without checking the new behavior.

The original task review did not execute the native comparison. R13 subsequently completed the bounded source investigation; its completion evidence records the accepted candidate and validation. Installed-engine certification, full 101-tool behavioral audit and 97-case release acceptance remain open under R16/R12 and R04-R11. Missing workplace reports are not prerequisites: reproduce with synthetic fixtures or record the observation as unresolved. No proprietary report, raw crash log or engine implementation body is added to the public-intended tree.
