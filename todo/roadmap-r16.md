# R16: Establish installed-engine integration constraints

ID: R16
Status: blocked
Priority: P1
Readiness: needs_reconciliation
Owner: unassigned
Design: docs/design/verification.md
Kind: investigation
Review: source-reviewed

## Problem
The owner currently relies on the 5.8.3 source build. Establish which requirements actually demand engine rebuilding, which require only a plugin build, and which would be missing in an installed distribution. Do not assume registration through Epic MCP requires engine patches.

## Scope
Plugin descriptor and Build.cs; TurboMCPModule.cpp registration; TurboMCPCallRecorder.cpp and analytics interception; SlateInspector coupling; fixture commandlets and test-only dependencies. Read public/private engine API boundaries. Record the deleted-public-repository DocsURL and SupportURL as release metadata defects; do not publish an invented replacement URL.

## Dependencies
R13

## Acceptance
- A dependency table identifies each dependency, production callers, test-only callers, header visibility, binary/export requirements and installed-distribution evidence.
- Identify and separate full engine rebuild, incremental plugin compile, packaging and consumer installation requirements. Measure the existing baseline build path; do not infer a full rebuild from source-engine usage.
- Compare direct supported registry registration, an isolated version adapter, optional development module extraction and any necessary fallback. Prefer the smallest supported integration; do not implement a custom transport as a shortcut.
- Specify the Blueprint-only consumer goal: copy a matching prebuilt plugin package, enable it and load it without compiling game C++ or Unreal source. The package producer may still need a compiler. Record exact build/ABI constraints.
- Missing installed 5.8.3 distribution is an explicit gap. This early source/dependency investigation can finish with that gap; R12 remains blocked from compatibility certification.
- Deliver bounded follow-up specifications for confirmed integration changes. No speculative dependency removal.

## Verification
- `python Tools/doctor.py`
- Inspect tracked module/descriptor and production include/caller references; record exact files and findings.
- `python Tools/execute-tests.py --profile build --execute` using workspace.local.json for the pinned baseline; retain the run report and build provenance.
- `python Tools/check-plugin.py`
- Record whether the build rebuilt engine modules, only the plugin, or reused artifacts; include actual build logs. No installed-engine claim from this build.

## Stop conditions
Do not patch engine source, change the pinned baseline, add a competing server, weaken safety contracts or change another task's scope. Missing prerequisites must be recorded. Do not rebuild the entire engine without evidence that an incremental build is insufficient. Stop after two identical unexplained failures and diagnose. Findings are not permission to implement unrelated fixes.

## Deliverable
A dependency and build-boundary report, a decision on the smallest integration strategy, and reviewable repair/package task specifications. Preserve the current working 5.8.3 installation.

## Completion evidence
Pending. Record exact source and engine identities, commands, exits, evidence locations, tested binary hashes where applicable, changed files and unresolved limits. Integration review is required before marking done.

## Current review

**Decision:** plan. **Follow-up owner:** [R16](roadmap-r16.md).

Non-unity incremental plugin compilation is verified; that does not require a full engine rebuild. Production and fixture dependencies still need caller/export analysis.

### Next action and acceptance

Produce dependency rows with production/test callers, public header/export visibility and distribution evidence. Compare direct registry use and a small adapter before a module split. Record broken public URLs without inventing a replacement publication target.

### Current source and verification

Reviewed at private main `6aee9dfd92a4c06a73e89c091973f982d715b7a2`. These source findings do not certify runtime behavior. Source links open the current file; line numbers refer to the reviewed revision.

- [Plugins/TurboMCP/Source/TurboMCP/TurboMCP.Build.cs](../Plugins/TurboMCP/Source/TurboMCP/TurboMCP.Build.cs)
- [Plugins/TurboMCP/Source/TurboMCP/TurboMCPModule.cpp](../Plugins/TurboMCP/Source/TurboMCP/TurboMCPModule.cpp)
- [Plugins/TurboMCP/Source/TurboMCP/TurboMCPCallRecorder.cpp](../Plugins/TurboMCP/Source/TurboMCP/TurboMCPCallRecorder.cpp)
- [Plugins/TurboMCP/TurboMCP.uplugin](../Plugins/TurboMCP/TurboMCP.uplugin)

Starting checks for the assigned follow-up:

- `python Tools/execute-tests.py --profile offline --execute`

These suites are starting checks, not proof of the new cases. Use owned synthetic fixtures and retain local evidence. Reference records do not authorize implementation.

See the [review register](../docs/audit/task-review.md) for definitions, duplicate ownership and observed test results.

## Dispatch prerequisite

R13 source comparison is accepted. Before dispatch, reconcile its reported
worker claim with the integration host: that host sees no active claim.
From the original DSH shell, return `python Tools/task_claim.py` and the resolved
claim-file path from `task_claim.location`. Do not create, release or overwrite
a claim merely to make the records agree. The integrator must verify the ledger
identity and safely resolve the exact old claim before restoring this task to
todo/ready. No R16 worker should start while this prerequisite is unresolved.
