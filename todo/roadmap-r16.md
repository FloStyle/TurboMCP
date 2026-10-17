# R16: Establish installed-engine integration constraints

ID: R16
Status: todo
Priority: P1
Readiness: ready
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
Task R16, claim `b419d20cd55b4f28959155084842327d` (owner `dsh-r16-01`). Approved base `af4d0efd97365df0f991f13ef369664b56aa927d`; branch `task/r16-installed-engine`; worktree `C:\ExampleDev\SampleRoot005\r16`. The candidate is the commit containing this evidence; its full SHA is returned in the submission message.

Source identity:
- Engine 5.8.3 at `396c9f059903aed5fec78ecd3d437a40c6415368` from `engine.lock.json` and the local source checkout.
- TurboMCP plugin tree at the base: `590661ec45c829264ea1743fccea994edb748092`.
- Forward target remains R13's recorded 5.9 development revision `5789fc07f00643fb37f85ede9894cd08f1cebcef`; it is not certified by this task.

Claim and shared state:
- `python Tools/next-task.py --claim dsh-r16-01` exited 0 and assigned R16.
- `python Tools/task_claim.py --inspect` exited 0. The store is `c:\ExampleDev\turbomcp\.git\turbomcp\task-claims.json`, the shared Git common directory, with active R16 claim `b419d20cd55b4f28959155084842327d` in this worktree. No new claim or base change was made after that check.

Findings:
- The plugin is one `Editor` module, loading at `PostEngineInit`; descriptor `EnabledByDefault` is false, `CanContainContent` is true and `Installed` is false. Registration uses the public `UToolsetRegistry::RegisterToolsetClass` path from the module loading-phase callback; no engine patch, private manifest API or custom transport is required.
- The Build.cs declares 35 module dependencies. Production code uses the editor/material/UMG/graph/MCP modules; the commandlets and three development automation tests compile into the same module. `PythonScriptPlugin` is used only by `TurboMCPTestMapCommandlet.cpp`. `AnimGraph`, `Niagara` and `NiagaraEditor` are listed but no include or typed symbol caller was located; `ToolMenus` and `InputCore` likewise have no located direct caller. These are recorded as candidates, not removed.
- All inspected engine includes resolve to public engine or plugin headers; no `Private/` engine include was found. The plugin's public headers are under `Source/TurboMCP/Public`; private module headers are not exported. Binary exports are the module entry plus `TURBOMCP_API` on `UTurboMCPSettings`, `FTurboMCPMutationTransaction` and `LogTurboMCP`; the four toolset UCLASSes are internal.
- A full engine rebuild is not required by the plugin source. `Tools/build.ps1` builds `TurboMCPTestEditor Win64 Development`; the plugin compiles as a separate module with `bUseUnity=false`. Historical build evidence at commit `084b76c` (`91c49a6cddc544b8bc0ed9e0d6e3cd28`) reports `Target is up to date`, 0 actions and a reused plugin DLL, with `DirtyDiffPresent: true`; it is not current-base evidence.
- Packaging requires `RunUAT BuildPlugin` against an exact engine distribution and produces a Win64 Editor package. The producer needs the matching engine and compiler; a Blueprint-only consumer must not need game C++ or engine source. The consumer still needs matching binaries, all descriptor plugins, content and explicit plugin enablement.
- The smallest supported integration is the current public registry path plus a small compile-time adapter for the 5.9 `FToolset::GetToolsetClass` removal in `TurboMCPCallRecorder.cpp`. Optional development-module extraction is deferred until packaging evidence proves it necessary; no speculative dependency removal was made.
- Release metadata defect: `DocsURL` and `SupportURL` point at the deleted public repository; no replacement URL was invented. Dependency metadata drift, fixture coupling and the 5.9 recorder compile break are recorded in the boundary report for bounded follow-ups.

Validation:
- `python Tools/execute-tests.py --profile offline --execute` exited 0. Report: `.local/executions/4eeed91d6dfd4d148b76b59c20deedb5/report.json`; tree digest `addd6bcf5f9739a7cc7dcfe6e1225f52178c4e092a313bfa7c51e9f53d1d8ee4`; suite log `.local/executions/4eeed91d6dfd4d148b76b59c20deedb5/00.log`. All offline suites passed, including `validate-repo`, `check-language`, `test-task-review` and `test-worker-scope`.
- `python Tools/validate-repo.py` exited 0 before the task evidence was written.
- No build, editor, engine suite, packaging run, recovery command or installed-engine test was performed. Build evidence is explicitly pending; the integrator supplies it during review.

Changed files: `docs/audit/installed-engine-boundaries.md`, `docs/audit/installed-engine-boundaries.json`, and this Completion evidence only. No plugin code, engine source, dependency, pin, test, task status, TODO, review register or release control was changed.

Unresolved: installed 5.8.3 distribution is absent; the local 5.6 installation lacks `ModelContextProtocol` and `ToolsetRegistry`; no current-base build/provenance, packaged plugin, Blueprint-only clean install or runtime discovery/save/undo proof exists; `NoRedist` has no runtime enforcement in the inspected source; cross-harness engine/port locking and the 5.9 runtime remain unverified. Integration review is required before marking R16 done.

## Current review

**Decision:** plan. **Follow-up owner:** [R16](roadmap-r16.md).

Non-unity incremental plugin compilation is verified; that does not require a full engine rebuild. Production and fixture dependencies still need caller/export analysis.

### Next action and acceptance

Produce dependency rows with production/test callers, public header/export visibility and distribution evidence. Compare direct registry use and a small adapter before a module split. Record broken public URLs without inventing a replacement publication target. Workers perform source/offline work only; the integration owner supplies the baseline build evidence while cross-harness execution locking remains unverified.

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

## Dispatch and build ownership

Shared-claim reconciliation is complete. DSH and the integration host both read
the Git common-directory ledger with no active claim and SHA256
`2870b5c5aa0fd6ed77b31343e9003cbc1ba73bd787609edf7401dda93884f39e`.
This digest records the empty-ledger checkpoint; a new claim will change it.
Start from updated main in a fresh worktree and use the shared claim helper.
Do not use the old R13 checkout's helper or the retired per-profile ledger.

R16 workers may inspect source, write the three approved report/evidence files
and run the offline profile. They must not start builds, an editor, engine
suites or recovery commands. Cross-harness engine/port lock visibility remains
unverified because execution locks still use per-profile storage. The integration
owner will run the required baseline build through the supervisor and supply its
report during review. Workers may analyze existing evidence but must label its
revision and must not claim an unrun build passed. Submit the source report with
the build evidence explicitly pending if no matching report has been supplied.
This rule takes precedence over the general build command in Verification.
