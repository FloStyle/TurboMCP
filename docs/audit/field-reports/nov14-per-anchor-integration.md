# Append per-anchor integration review - 2026-11-14

Reviewed candidate: 96a26b439ae126c1ed39473e813c6d5ba4d030b8. Combined source reviewed against 6c8bfac4; latest scope gate against c89d17cf passed.

## Accepted implementation slice

Opt-in Placement=per-anchor groups connected new nodes, including data dependencies, and places independent components near fixed connections. Existing nodes remain fixed. Earlier component rectangles reserve space. Missing anchors, multiple execution sources and unavailable local placement refuse; there is no silent outside fallback. Existing local/outside modes remain available.

## Evidence and limits

- Supervised stock-engine build and focused TurboMCP.GraphAppend.Placement passed at the candidate. Execution e29fc29dce2748c2b6058f92e013518d; 16 fixture files restored.
- Four independent planner anchors, repeatability, fixed existing coordinates and conflicting-source refusal passed.
- Earlier full append execution 5d19da114b70490daa7c791ea388ec70 failed an outdated placement-mode expectation. It was corrected, then only the focused case was rerun. This is not a full-suite pass. Previously recorded append failures remain retained.
- Public per-anchor preview/apply, shared-producer and close-component collision scenarios still need focused evidence. The captured public fixture uses local mode, not per-anchor. No per-anchor visual acceptance is claimed.
- Multiple execution sources are conservatively refused, including potentially compatible sources.
- NOV09-03 remains open as partial implementation. NOV09-02 explicit movable-tail insertion is not implemented.

## Takeover

Finish per-anchor public workflow and geometry coverage in one batch, then implement explicitly scoped tail movement under the approved placement contract. Reuse valid evidence; run only focused checks for changed behavior. Continue remaining backlog by safety and impact, preserving unknown causes and partial capabilities. Keep DSH paused, stock Unreal, private main and dashboard current. Testing remains separate from implementation status.

## Public workflow follow-up

Candidate 8670c0c3 passed the focused placement workflow in execution 620a66e019264986918a22fabac21321. Added coverage proves public per-anchor preview is non-mutating and clean, apply creates exactly two connected consumers near their independent sources, and existing coordinates remain unchanged. A separate nearby-anchor planner case checks that reserved component rectangles do not overlap.

Execution 98a4dd9f2eb441ab97ecfc1d0989da07 failed because the new fixture ran before an older crowded-space/capture case and changed its graph. Moving the new case after the older case corrected test interference; both failures remain recorded. This focused pass does not supersede historical full-suite failures.

Still outstanding: positive shared-pure-producer geometry, preview/apply coordinate equality and visual evidence for per-anchor mode. Explicit insertion and stale-preview authorization remain separate unfinished work. NOV09-03 stays open.

## Shared producer and capture follow-up

Candidate 5a033bc6 passed focused execution 79e6ae0b40de45b6b3388970d929af4f. The shared pure producer remains one form left of both consumers; applied public consumer coordinates appear in the preview. Inspected the final LegacyGraph region capture ending A83DD5A9414548BE2EE1C1959D6B9693: two separate event/consumer pairs remain near their own sources with no visible overlap. The overview is zoomed out and is not fine typography or pin-alignment acceptance.

Remaining capability gap: connected components with multiple execution sources are conservatively refused regardless of compatibility. Do not equate this refusal with support for all compatible anchors. NOV09-03 remains partial on that specific gap; the previously missing focused workflow evidence is now available. Full append-suite acceptance remains separate.

## Multi-source completion

Candidate 9a792e33 passed supervised build and focused placement execution b93d32bdfcad4a7e95995866dc21e529. Multiple fixed execution sources now receive deterministic candidate trials, and every incoming execution attachment is checked. Nearby compatible sources pass; moving one source to a remote lane refuses and returns no partial positions. Existing nodes never move.

The implemented local policy requires forward execution connections and limits each source-to-consumer displacement to two footprint-plus-gap strides. This is a bounded local placement policy, not a general graph optimizer; infeasible local requests must use an explicit alternative. Shared producers, independent groups, nearby collision reservations, public preview/apply coordinates and final overview capture have focused evidence above. NOV09-03 implementation is complete under this policy. Historical full-suite failures remain open; NOV09-02 tail movement is not included.

## Retained-plan freshness safeguard

Reviewed candidate 45855967e82d6219e6a42ccaa794521cbb03cd3a. Supervised build and focused placement execution 9ea12abd2c3343df868b34e3fbe22b88 passed. Changed geometry and pin defaults invalidate a retained internal placement plan before mutation; the focused assertions preserve graph state and package dirtiness on refusal.

This is an internal FPlan safeguard, not a public preview confirmation token. The snapshot covers node identity, geometry, pin identity, defaults and connections; it is not a complete serialization of pin types, node properties or comment membership. Explicit movable-tail insertion and its stronger authorization/rollback contract remain open as NOV09-02. No full append-suite pass is claimed. Backlog remains 109/121 done, 12 open.

Takeover goal: finish NOV09-02 explicitly scoped tail insertion with preview/apply agreement, stale-plan refusal, fixed-neighbor preservation and atomic undo. Then resolve remaining actionable backlog by safety and impact. Keep unknown-cause investigations open, DSH paused and Unreal stock. Batch changes, run minimal focused checks, and integrate reviewed work into private main with tracking and dashboard updates. Implementation status and test evidence remain separate.

## Follow-up in progress

Source review found that the retained-plan snapshot omitted pin types: a type or qualifier could change without invalidating the plan. The follow-up serializes the reflected pin type and adds a qualifier-change refusal assertion. This follow-up is implemented but untested, and is not integrated into main yet; include it in the next focused placement build/check rather than claim the earlier evidence covers it.

## Explicit tail translation stage (in progress)

Added a non-mutating translation planner for NOV09-02. Canonical explicit IDs form an upper bound: new-rectangle collisions seed the required tail, and only obstructing downstream nodes from that scope are added. The subset receives one horizontal offset; unrelated allowed nodes stay fixed. Fixed obstructions and coordinate overflow refuse with no partial result. A focused synthetic case covers the necessary subset, relative positions, non-mutation and fixed-node refusal.

This stage is not exposed by the public append tools. Comment membership/growth, measured insertion positions, preview confirmation, transaction application and undo verification remain outstanding. Comments are excluded from ordinary collision handling because the enclosing insertion planner must validate their containment separately. Do not count this geometry stage as completed insertion capability.

Focused execution c52d652dc1bc4ed092453f9f192a1f16 at 6a21552d passed the stock-engine build and selected native placement regression. This includes the pin-type freshness refusal and explicit tail-translation geometry checks. It does not establish public tail insertion, comment growth, transaction rollback or full append-suite acceptance. NOV09-02 is in progress, owned by the integrator; the done count is unchanged.

## Comment containment stage

Candidate 4f48e0f0 passed stock-engine build and focused native execution 69fec17f2d294d6f87efc8fa256bd017. The internal comment planner uses the shared membership rule on a scratch copy. It preserves live geometry, allows only named rightward growth, preserves existing membership, and refuses blocked neighboring/ancestor growth. The focused case checks positive growth, non-mutation, unnamed-box membership refusal and a neighboring-box refusal. A source guard also rejects moving an output past a fixed data consumer; dedicated runtime coverage for that guard is still pending.

Public insertion is still unavailable. Remaining NOV09-02 work: connect measured new-node layout, scoped movements and comment growth to a retained preview authorization and one live transaction; validate rollback/undo; support explicitly authorized nested-comment movement; inspect the final visual case. Per-anchor help now explicitly describes the already implemented mode. No task is closed by this internal stage.


## Pending insertion integration - focused evidence update

Insertion remains uncommitted and NOV09-02 remains open. Execution
`987be1e0cc994acba00dab05fe74881a` built successfully and passed the focused
placement suite, including public preview-token issuance, apply, replay refusal,
stale geometry refusal and one-step undo in an unboxed execution chain.

The expanded boxed-chain run `3a6d574978b0416c9a527ac2d4650fc9` built but failed:
preview could not determine the moved comment member width. Scratch nodes lack
the live Slate size cache. The pending fix uses the same stored-width/320 fallback
as tail collision planning; this correction is not yet verified. The same run
reported no failures for the newly added empty nested-frame containment and
neighbor-conflict assertions. This does not certify the complete insertion path.

Remaining integration gates: rerun the boxed public workflow after the width fix,
inspect its capture, verify failed-apply restoration, and finish source/help review.
No full Append-suite pass or task completion is claimed.


Execution `224cb958c35f4c6098df94c8f9ff09e3` built and reached boxed apply;
its final capture was inspected (chain connected and separated inside the grown
box). Forced spawn failure restored the graph and dirty state. The suite still
failed its boxed one-step undo assertion. Existing synthetic comments were not
transactional, and insertion did not enroll them before Modify. Pending repair:
set RF_Transactional on every moved existing node and resized comment before
recording its transaction. This change requires the same focused verification;
the failed run is retained and NOV09-02 remains open.


## Integrated insertion - NOV09-02

Implementation `aef47a99` completes the bounded insertion contract. The public
append tools accept `insert`, an explicit movable-node JSON list, an optional
containing comment, and a ten-minute single-use preview token for Apply. Existing
local/outside/per-anchor modes retain fixed existing nodes. No compile or save is
performed. Preview reports old/new movement and comment width changes; stale
requests refuse before mutation. Nested frame relationships and existing comment
members are preserved; only the named outer frame can grow rightward.

Execution `3a9027ad15ac427fa377594056bf3921` passed the stock 5.8.4 build and
focused native placement test (one test, zero errors/warnings). It covers the
boxed chain, unrelated fixed event lane, token replay, stale geometry, one-step
undo including comment width, and forced creation failure after tail movement.
Restoration compares geometry, pin/default/link state and package dirtiness.
The final native insertion capture was inspected: connected separated nodes
inside the right-expanded frame. No additional full-suite rerun was performed.

Earlier failures above remain evidence: the missing scratch measurement and
nontransactional comment undo issues were corrected, not concealed by a rerun.
The original private field graph was not replayed. This is bounded workflow
acceptance, not whole-plugin or release certification. NOV09-02 is implemented;
field and broad regression coverage remain separate flags.

Offline aggregate `687d7c7e918c4f499d6f3097b57b6bde` exited 1: repository and language checks caught an audit-note encoding error. Both checks passed individually after the UTF-8 correction; all other aggregate checks passed. The original failed aggregate is retained, not reported as a clean aggregate pass.
