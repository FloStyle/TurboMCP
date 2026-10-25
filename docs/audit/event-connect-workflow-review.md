# Multi-event connection repair workflow

Status: scenario checked. Data-input repair is provisionally accepted; this is not full GraphConnect certification.

## Scenario WF-BP-03-EVENT-CONNECT

A disposable Blueprint contains Purchase and Refund custom-event flows in EventGraph, plus an Audit event in another event graph. Each flow uses similar arithmetic and output nodes. Only Purchase has a wrong numeric source connection. Read overview, graph content, search and event context before choosing exact source/target IDs. Repair only that connection; never select by title alone.

## Acceptance

Review source, comments, schema and clear-English output for GraphConnect preview/edit. Preview must identify the intended endpoints and displaced connection without editing the live graph. Apply must replace only the chosen data input source, preserve nodes, positions, defaults and every other connection across all event flows and graphs. Repeating the same connection must report no edit needed. Missing pins, wrong graph IDs and execution/data mismatches must refuse without mutation. Compile explicitly; invoke each event on a disposable object and check independent expected values. Inspect a native capture. No save or workplace assets. Failure restoration and arbitrary node types remain separate coverage.

## Usage tracking

See workflow-tool-coverage.json for executed versus planned tool usage. Reuse of a tool in a new scenario adds evidence, not another unique tool to the coverage count.

## Evidence and result

Candidate 0850faa52724924be1dc9c5336b37fa6ac310289 passed the scope gate. Supervised build and final test-event-connect-workflow passed in 3281be0d2c084b2f853c05d18e08e205. Earlier baseline run 3976d6e1cd554523892c242784f9b61e also passed. Both tools were actually called through their native production entry points; the transcript retains discovery/context, positive results and refusals.

Purchase now produces 42; Refund still produces 30 and Audit still produces 300. The exact only-changed-edge comparison normalizes that edge and compares both graphs, including node IDs, positions, defaults and every connection. Correct and incorrect requests preserve the expected state; repeated valid connection reports no edit needed. The original capture left the old unused arithmetic node behind. The user correctly rejected that as an incomplete cleanup workflow; the follow-up below supersedes that visual acceptance.

The source candidate is initially disconnected. GraphLoad lists its unreachable ID; NodeFind finds it, and NodeContext exposes its values (40 and 2). The target context must name AddEvent|Custom|Purchase, not Refund. The test checks the exact source/target IDs occur in reader/search output. An identically named arithmetic node in AuditEvents is refused when supplied to PurchaseEvents.

## Code and text review

Connect resolves exact pins in one graph, refuses mixed execution/data or implicit conversion, preflights on a transient copy and verifies the resulting reciprocal link. Data replacement touches the target input; exec replacement uses a different contract, retained but not positively covered here. Live failure uses owned undo and honestly reports unverified restoration. No restoration guarantee is inferred from this scenario.

The old generic success text omitted endpoints. Results now name source, target and displaced sources/destinations. No-op results retain the target identity. Refusals include the requested connection. Preview remains explicitly transient and does not imply new nodes. Layout guidance asks callers to inspect and tidy only if needed. Compile/save remain explicit separate actions. Comments explain these distinctions.

## Limits

No successful execution-wire replacement, latent events, shared exec merge, asynchronous behavior, arbitrary node types, persistence or injected live rollback is certified. This is a deterministic native scenario, not evidence that every language model can select the correct target. User visual acceptance remains pending. Existing unrelated offline failures and the purchase comment-wire issue remain tracked separately.

Offline gate 182a672d192445af97975660b5fa913b: 22/26 pass. Existing validate-repo, check-language, client-retry and lookup-guards failures remain. Runner-verdicts passed on this run; the earlier missing-manifest failure is not reproduced.


## Orphan cleanup correction

Implemented ad7d71c1a5f90b254029881b8835468d497448b1; scope gate passed. Build and expanded native workflow pass: 47c9c30568a54267b542da4ad53bcbef. Final capture inspected: the obsolete 1+2 node is absent, and the two event flows are otherwise unchanged.

GraphConnect now examines displaced pure data sources after the planned/applied connection and reports exact IDs with no remaining output consumers. Every output pin is checked; a source with another consumer is not listed. The result explicitly says no nodes were deleted, asks the caller to inspect dependencies, and names PreviewTurboMCPGraphRemove/EditTurboMCPGraphRemove. Lack of consumers is not a blanket proof that an arbitrary pure function is safe to delete.

The workflow inspects the obsolete constant-addition node, verifies it has no links, previews removing that exact ID, checks preview non-mutation, then applies removal. It verifies exactly one deletion and a complete snapshot of all remaining nodes, positions, defaults and links in both graphs. Repeated removal refuses the missing ID without changes. Three independent runtime results still pass. Remove previews no longer incorrectly mention provisional new-node IDs.

This is explicit cleanup within the user's requested repair, not automatic deletion inside GraphConnect. Only displaced source nodes are reported; recursive dead-code elimination, arbitrary side effects, connected-node deletion and rollback injection are not certified. No whole-graph cleanup or layout change is performed.

Cleanup offline gate caac88cd0a564df2b5678133b43f04be: 22/26 passed, with the same four recorded structure/language/client-retry/lookup failures.
