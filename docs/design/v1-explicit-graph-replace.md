# V1 explicit graph replacement

Readiness: ready. Integration owner approved 2026-11-17.
Assignment: v1-explicit-graph-replace under R04; related debt owner R06 / ARC07.

## Confirmed defect and priority

At ead44fd2, TurboMCPGraphReplace.cpp adds eligible existing nodes omitted from
text to Removals. The fixture expects that behavior. ARC07 explicitly requires
a complete edit export, a fresh fingerprint and one deletion directive per
removed node. Fix this destructive contract mismatch before new widget tools.
This is a bounded implementation approval, not approval of all R06 findings.

## Required behavior

Implement ARC07's existing tmcp-edit-v1 and tmcp-delete-v1 envelopes exactly.
Add format=edit to the existing exact authored graph reader. Keep inspection
formats and Append/Patch behavior. Reuse the existing grammar and factories;
do not invent a second graph language or add new node families.

Use canonical asset ownership, authoritative graph GUID and full lowercase node
GUIDs. Deterministic SHA-256 covers semantic state, pin identities/types and both
link endpoints, distinct literal/object/localized-text defaults and signatures.
Include relevant layout/state fields if the operation depends on them. Never
reuse the paginated JSON report checksum as a semantic edit fingerprint.

Export every supported semantic node and edge, including shared pure nodes once.
For unsupported semantics, ambiguity, incomplete endpoints or truncation, return
complete=false with precise identities/reasons. Never infer completeness merely
from nonempty text. Restrict the accepted slice if fidelity is not demonstrated.

Before planning and again before live mutation, reject missing, incomplete,
foreign or stale headers, legacy snippets, ambiguous/foreign IDs, omitted
semantic nodes without explicit deletion, duplicate/conflicting deletions and
protected entry/result/event/comment deletion. No unsafe compatibility switch.
Preserve unrelated state, defaults, edges, comments and identities. Stage and
validate before mutation; retain existing transaction/rollback safeguards.
Do not introduce automatic compile/save. If an existing grammar limitation
prevents lossless support, fail closed and report it rather than silently drop
information. A broad disabled Replace is not completion of supported acceptance.

Migrate the three scoped native fixtures, Python callers, toolset help and user
reference together. Read-back assertions must independently inspect actual graph
state, not only serialize through the same new exporter. Root owns generated
catalogue/capability updates and acceptance status. No engine edits.

## Minimal checks and submission

Worker performs focused offline/source checks only. Return a clean committed
candidate, claim/base, changed files, actual checks and explicit native gaps.
Root reviews source before supervised build and test-replace-plan and
test-graph-edit-workflow. Run migrated vision path only as needed; retain known
append-suite failures and do not claim whole-suite success.

Required focused assertions: no-op round trip; unsupported export incomplete;
omitted node unchanged refusal; stale/foreign/duplicate/protected refusal;
explicit one-node deletion plus supported addition; exact surviving endpoints
and defaults; one undo; stale state changed between planning and application.
Refusals assert original tree/edges/dirty state/undo state. Do not replace these
with string-only assertions or weaken historical preservation tests.
