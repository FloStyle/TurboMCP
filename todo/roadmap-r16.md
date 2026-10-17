# R16: Determine what prevents an installed-engine plugin

ID: R16
Status: todo
Priority: P1
Readiness: ready
Owner: unassigned
Design: docs/design/verification.md
Kind: investigation

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
