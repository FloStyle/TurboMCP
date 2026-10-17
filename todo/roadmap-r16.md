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
Task R16, claim `b419d20cd55b4f28959155084842327d` (owner `dsh-r16-01`). Approved base `af4d0efd97365df0f991f13ef369664b56aa927d`; branch `task/r16-installed-engine`; worktree `C:\ExampleDev\SampleRoot005\r16`. Original candidate `ff633b8e687bab7dffba45721553422339d316de` is preserved. The correction commit is the commit containing this evidence; its full SHA is returned in the submission message.

Source identity:
- Engine 5.8.3 at `396c9f059903aed5fec78ecd3d437a40c6415368` from `engine.lock.json` and the local source checkout.
- TurboMCP plugin tree at the base: `590661ec45c829264ea1743fccea994edb748092`.
- Forward target remains R13's recorded 5.9 development revision `5789fc07f00643fb37f85ede9894cd08f1cebcef`; it is not certified by this task.

Claim and shared state:
- `python Tools/next-task.py --claim dsh-r16-01` exited 0 and assigned R16.
- `python Tools/task_claim.py --inspect` exited 0. The store is `c:\ExampleDev\turbomcp\.git\turbomcp\task-claims.json`, the shared Git common directory, with active R16 claim `b419d20cd55b4f28959155084842327d` in this worktree. No new claim or base change was made after that check.

Findings:
- The plugin is one `Editor` module at `PostEngineInit`; descriptor `EnabledByDefault` is false, `CanContainContent` is true and `Installed` is false. Registration uses `UToolsetRegistry::RegisterToolsetClass` from the module loading-phase callback; no engine patch, private manifest API or custom transport is required.
- The Build.cs declares 35 module entries. The report keeps all 35 and separately traces used transitive interfaces. Production code uses editor/material/UMG/graph/MCP modules; commandlets and three development automation tests share the same module. `PythonScriptPlugin` is used only by `TurboMCPTestMapCommandlet.cpp`. `AnimGraph`, `Niagara`, `NiagaraEditor`, `ToolMenus`, `Kismet`, `KismetCompiler`, `JsonUtilities`, `ImageWrapper` and `InputCore` have no located direct include or symbol use and are marked as such; they were not removed.
- Module ownership corrections: `FAssetEditorToolkit` and `UAssetEditorSubsystem` are owned by `UnrealEd` (`Toolkits/AssetEditorToolkit.h`, `Subsystems/AssetEditorSubsystem.h`), not `EditorFramework` or `EditorSubsystem`. `EditorFramework` is used through `IToolkitHost::GetParentWidget` (`TurboMCPCaptureAsset.cpp:617-618`); `EditorSubsystem` is used through the `UEditorSubsystem` base of `UAssetEditorSubsystem` and subsystem lookup. Both remain declared.
- Transitive interfaces: `ISourceControlModule`, `ISourceControlProvider`, `ISourceControlState`, `FCheckOut` and `FUpdateStatus` come from `SourceControl`, which is a public dependency of `UnrealEd` (`UnrealEd.Build.cs:52-68`) and is not a direct TurboMCP.Build.cs entry. `FAnalyticsEventAttribute` comes from `Analytics`; `AnalyticsET.Build.cs` lists Analytics as a private dependency plus `PublicIncludePathModuleNames`, so the header is visible but it is not a public link dependency. `RenderCore.Build.cs` publicly depends on `RHI` and propagates that build closure; RHI symbols are exported by RHI, not RenderCore, and the plugin uses Engine `FRenderTarget::ReadPixels` plus `FApp::CanEverRender` rather than a direct RHI symbol.
- Engine interface evidence: the report records the owning public header, used symbol or interface, export mechanism and local link evidence for each located use. Registry/recorder, MCP analytics, SlateInspector screenshot, editor save, SourceControl, AnalyticsEventAttribute and render-target readback are traced. A source declaration is not link evidence; the integrator build proves this source-engine target linked, not installed availability or per-symbol portability.
- Header and declaration audit: every cited owning header was resolved against the pinned local tree and its declaration/export macro read. Corrections include `DeveloperSettings` at `Engine/Source/Runtime/DeveloperSettings/Public/Engine/DeveloperSettings.h` with `MinimalAPI` plus `DEVELOPERSETTINGS_API`; `SWidget` under `SlateCore` (`SLATECORE_API`) and `SButton` under `Slate` (`SLATE_API`, `SNew(SButton)` in `STurboMCPPanel.cpp:126`); `SGraphEditor` under `UnrealEd/Public/GraphEditor.h`; and `Engine/World.h` plus `Engine/Classes/Engine/TextureRenderTarget2D.h` for the Engine rows. All cited header paths exist. Local source checks are complete; installed-symbol and distribution proof are not.
- The integrator-owned build at original candidate `ff633b8` (run `7fe2db69671c429f82126beba6d529a6`) passed: exit 0, 145 actions, 298.43 seconds UBT, TurboMCP and TurboMCPTest compiled/linked, no engine-module compile actions, existing engine binaries reused. Doctor passed, 16 fixtures were restored, git status was clean, provenance `DirtyDiffPresent=false`, `NoOpCandidate=false`. Artifact `TestProject/Binaries/Win64/UnrealEditor-TurboMCP.dll`, 3,741,184 bytes, SHA-256 `14DEE2F2B7DE4278BA02CDB090511EA1CBB5760BD84CD7665A29ADD398C03AC0`, MSVC 14.50.35728, Windows SDK 10.0.22621.0. This does not certify packaging, an installed engine, a Blueprint-only consumer, 5.9 or the correction commit.
- A full engine rebuild is not required by the inspected build: no engine modules compiled, and no engine source edit is involved. `Tools/build.ps1` builds `TurboMCPTestEditor Win64 Development`; the plugin module uses `bUseUnity=false`. The historical build at `084b76c` (`91c49a6cddc544b8bc0ed9e0d6e3cd28`, 0 actions, dirty tree) remains historical only.
- Packaging requires `RunUAT BuildPlugin` against an exact engine distribution and produces a Win64 Editor package; the producer needs the matching engine and compiler, while a Blueprint-only consumer must not need game C++ or engine source. The consumer still needs matching binaries, all descriptor plugins, content and explicit enablement.
- The smallest supported integration remains the public registry path. The 5.9 `GetToolsetClass` removal is confirmed, but a registered-name identity replacement is not demonstrated authoritative. `GetToolsetName`, `ForEachToolset` and `Find` are public, but R14 must resolve the class-to-name path or leave identity unresolved; protected manifest access is forbidden. No adapter is implemented by R16.
- Release metadata defect: `DocsURL` and `SupportURL` point at the deleted public repository; no replacement URL was invented. Dependency metadata drift, fixture coupling and the unresolved 5.9 identity are recorded for bounded follow-ups.

Validation:
- Correction run after the documentation corrections: `python Tools/execute-tests.py --profile offline --execute` exited 0. Report `.local/executions/208b7f332f174c47b5bcfdd4059ab172/report.json`; tree digest `1e3596b7e2937fe568de80d00f63d4262f6af3ab8ad653677c8c8d9fc30cc79a`; suite log `.local/executions/208b7f332f174c47b5bcfdd4059ab172/00.log`. All offline suites passed.
- Candidate-bound offline evidence: `.local/executions/44e70a8eff0c4dfbbcdeccd70c51bf57/report.json` at original candidate `ff633b8` passed with tree digest `3040f00ea58bc2250844bad394cbb4cdf2b1e867d8bb67540c7673282a390451`.
- `python Tools/validate-repo.py` exited 0 after the report corrections.
- No build, editor, engine suite, packaging run, recovery command or installed-engine test was performed by this worker. The integrator build is cited above as an external observation.

Changed files: `docs/audit/installed-engine-boundaries.md`, `docs/audit/installed-engine-boundaries.json`, and this Completion evidence only. No plugin code, engine source, dependency, pin, test, task status, TODO, review register or release control was changed.

Unresolved: installed 5.8.3 distribution is absent; the local 5.6 installation lacks `ModelContextProtocol` and `ToolsetRegistry`; no packaged plugin, Blueprint-only clean install or runtime discovery/save/undo proof exists; the corrected documentation commit was not rebuilt; per-symbol installed export availability is unverified; `NoRedist` has no runtime enforcement in the inspected source; cross-harness engine/port locking and the 5.9 runtime remain unverified. Integration review is required before marking R16 done.

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
