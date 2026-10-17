# R17: Expose verified tool preferences in discovery and help

ID: R17
Status: todo
Priority: P1
Readiness: ready
Owner: unassigned
Design: docs/design/tool-selection.md
Kind: implementation

## Problem
Agents sometimes choose an Epic tool when TurboMCP offers a useful alternative. Make verified, narrowly scoped preferences visible in the live descriptions and schemas that clients actually discover. Do not blanket-prefer TurboMCP or claim untested supersession.

## Scope
docs/audit/tool-routing.json, ToolSchema/AgentHelp/Traps, tool metadata and generated reference output. Preserve function names, signatures, native tools and result contracts. No runtime behavior refactor or new operation.

## Dependencies
R09

## Acceptance
- All 101 tools retain a routing row. Every preferred alternative identifies exact native tool, supported engine/scope, measured benefit and accepted comparison evidence. Unknown rows remain unverified.
- Actual discovered help/schema displays preference and reason early, with effects and refusal conditions. Generated documentation uses the same record; no competing hand-maintained list.
- 5.8.3 preference does not silently apply to 5.9. Unsupported version/scope yields no preference claim.
- Fixed prompts cover overlapping workflows, native-only workflows, unavailable preferred tools, refusals and unknown outcomes. No refusal bypass or mutation replay occurs.
- Deterministic metadata checks pass. Client-choice trials record exact Claude/Sonnet or other client/model identity and observed selection counts; lack of client access is an explicit limitation.
- If no row has accepted comparison evidence, stop before publishing preference labels and request the missing bounded parity probe.

## Verification
- `python Tools/audit-inventory.py --check`
- `python Tools/test-traps-plugin-identity.py`
- `python Tools/make-tools-doc.py`
- `pwsh -File Tools/run-suites.ps1 -Suites test-agent-help,test-call-cost`
- Add a focused regression for missing/stale routing evidence and discovery wording; register it in Tests/catalog.json. Record the real new command after creating it.
- Use the owned baseline editor for live discovery and prompt trials. Never use a proprietary project fixture.
- `python Tools/validate.py`

## Stop conditions
Do not patch engine source, change the pinned baseline, add a competing server, weaken safety contracts or change another task's scope. Missing prerequisites must be recorded. Do not rebuild the entire engine without evidence that an incremental build is insufficient. Stop after two identical unexplained failures and diagnose. Findings are not permission to implement unrelated fixes.

## Deliverable
Evidence-backed routing data, live discovery/help changes, generated documentation and a reproducible client-selection result table. No superiority claim beyond tested scope.

## Completion evidence
Pending. Record exact source and engine identities, commands, exits, evidence locations, tested binary hashes where applicable, changed files and unresolved limits. Integration review is required before marking done.
