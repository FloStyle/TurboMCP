# R10: Align human docs and help with verified behavior

ID: R10
Status: todo
Priority: P1
Readiness: ready
Owner: unassigned
Design: docs/design/editor-domains.md
Kind: documentation
Review: source-reviewed

## Problem
Document actual parameter requiredness, valid examples, side effects, supported scopes and refusal conditions. Remove historical narrative from active instructions only where its current contract has been retained. Keep a short human README and concise task entry point.

## Scope
ToolSchema, AgentHelp, Traps, generated user docs, README and public capability descriptions. No new tool surface.

## Dependencies
R18

## Acceptance
- Every advertised capability has accepted evidence or an explicit experimental limitation.
- Examples match live schemas and preserve native media responses.
- Help/trap data loads from the plugin identity, including renamed mount folders.

## Verification
- `python Tools/make-tools-doc.py`
- `python Tools/test-traps-plugin-identity.py`
- `python Tools/execute-tests.py --profile engine --suites test-agent-help,test-call-cost --execute`

## Stop conditions
Do not expand into another domain. If a prerequisite is absent, record blocked with the exact missing input. After two identical unexplained failures, stop repeating the command and diagnose. Do not run a full engine rebuild without evidence that an incremental build cannot answer the question. Do not change public schemas, weaken safety contracts or start deferred features.

## Deliverable
The bounded change plus regression evidence, updated affected specifications and a concise handoff.

## Completion evidence
Pending. Record source/engine/artifact identity, command, exit, log and remaining limitations. A reviewer must check acceptance before marking done.

## Related findings
- [04111899-8525-4699-a2b1-8d538e6f9fc4](04111899.md)
- [0b84b1d8-ac34-4ea6-b2a7-f196ad74e81f](0b84b1d8.md)
- [0e4658ad-625d-4f86-b81b-ee38cb298011](0e4658ad.md)
- [1db73082-b800-4503-b6df-2d1ad761baf7](1db73082.md)
- [48608da1-2d6d-42f5-8eac-095db728c5c2](48608da1.md)
- [701a6c3d-90d5-403d-84f1-5f2eacc2113d](701a6c3d.md)
- [745637f4-b07e-46fc-9251-c89e62fa2f04](745637f4.md)
- [8af56e37-5290-4b95-8057-8cc3a5df0ceb](8af56e37.md)
- [9263bd95-5a42-4723-aa29-cd43406efb35](9263bd95.md)
- [9d4624ff-4e90-4c5e-9f8f-1e621af2d537](9d4624ff.md)
- [9e5d5c9a-d446-40aa-a822-cb55d304476d](9e5d5c9a.md)
- [a312b008-744b-4fb0-a366-806c4bc03282](a312b008.md)
- [d03a170f-8ba2-4131-9258-c02644e22eb9](d03a170f.md)
- [e35faf67-33e1-47ac-9155-59c86e5b0f4c](e35faf67.md)

## Current review

**Decision:** plan. **Follow-up owner:** [R10](roadmap-r10.md).

Current task history contains obsolete names and unsupported universals. Help consumes trap data, so a second help source would introduce drift.

### Next action and acceptance

Use approved task decisions and live schemas. Fix bounded wording, valid examples, navigation and effect labels. Code defects found while documenting return to their repair owner; do not hide them with optimistic prose.

### Current source and verification

Reviewed at private main `6aee9dfd92a4c06a73e89c091973f982d715b7a2`. These source findings do not certify runtime behavior. Source links open the current file; line numbers refer to the reviewed revision.

- [Plugins/TurboMCP/Source/TurboMCP/TurboMCPAgentHelp.cpp](../Plugins/TurboMCP/Source/TurboMCP/TurboMCPAgentHelp.cpp)
- [Plugins/TurboMCP/Source/TurboMCP/TurboMCPTraps.cpp](../Plugins/TurboMCP/Source/TurboMCP/TurboMCPTraps.cpp)
- [Tools/make-tools-doc.py](../Tools/make-tools-doc.py)
- [README.md](../README.md)

Starting checks for the assigned follow-up:

- `python Tools/test-traps-plugin-identity.py`
- `python Tools/execute-tests.py --profile engine --suites test-agent-help --execute`

These suites are starting checks, not proof of the new cases. Use owned synthetic fixtures and retain local evidence. Reference records do not authorize implementation.

See the [review register](../docs/audit/task-review.md) for definitions, duplicate ownership and observed test results.
