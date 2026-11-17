# 1.0.0 release plan

Status: candidate preparation. No stable package is published yet.

[Milestone](https://github.com/FloStyle/TurboMCP/milestone/1) | [Releases](https://github.com/FloStyle/TurboMCP/releases) | [Issue map](github-issues.md)

## Approved scope: 17 November 2026

The owner chose: release the current documented tool set after critical fixes and focused acceptance. Track unfinished features for later. Support is limited to Windows and the stock Unreal 5.8.4 source build in engine.lock.json. No Launcher or 5.9 claim.

Keep the existing tool contracts and safety guards. Compilation and saving remain explicit. GraphPatch does not gain linked-pin edits. DataTable operations do not gain ordered mixed set/remove on one row. Production compile-baseline coordination is separate work. These features and the extended acceptance backlog remain open in [#33](https://github.com/FloStyle/TurboMCP/issues/33).

The original 97 acceptance specifications remain in Tests/acceptance.json with their previous statuses and evidence. They are not 97 completed tests. The ten V1-* records are the release gates for this approved scope. A failed focused check must be repaired or explicitly resolved; it cannot be removed to obtain a pass.

## Required work

- [x] Owner agreed scope (#14); source and issue reconciliation is recorded below.
- [x] Exact unloaded-object resolution (#13), repository controls (#17), fresh-checkout claims (#18), and call-log repair (#28) integrated.
- [x] Explicit graph deletion repair (#27) integrated into public main after review and focused native acceptance (c9a84897; two-suite rerun 4ff208c2b7ef46eca4ed3aa88e28bb73).
- [x] Frozen-candidate focused acceptance rehearsal (#15) on the pre-approval candidate: eight fixtures in one supervised request, all passed.
- [x] Versioned package rehearsal on the pre-approval candidate: BuildPlugin produced a 1.0.0 ZIP and a manifest with installed_consumer_acceptance: pending.
- [ ] Both clean consumers (Blueprint-only and C++) on the final ZIP (#16).
- [ ] Publish stable and v1.0.0 with the tested ZIP, manifest and SHA-256; update website, wiki and README to V1.

These two rehearsals are not release evidence. They ran before the owner-approval commit, and release-check.py requires the evidence source commit to equal the approved tree. The final run IDs and the final ZIP SHA-256 belong in .local/release/evidence.json and the release notes, not in this tree. Collect each gate again on the approved commit, once.

## Minimal acceptance

Run the existing offline gate once. Use one supervised native request for the eight fixtures named by the V1-* mappings: replacement, existing graph-edit workflow, append, append failure, call-log sink, typed Data Asset creation, DataTable rows, and widget roundtrip. Reuse that request for all matching assertions. Do not run the old 27-suite extended proposal for this release. Do not count one passing fixture as a whole-suite pass. Preserve the two known append failures until the current append driver establishes its actual result.

Then build the final versioned ZIP once and exercise it in fresh Blueprint-only and C++ projects. Both phases use the same archive and editor BuildId. The C++ consumer may build its own module and use Unreal's stock metadata writer for its own receipts. It must not rebuild or rewrite the packaged plugin. The original ZIP and manifest remain immutable; attach acceptance evidence separately.

Development-editor fixture evidence proves source behavior on the frozen commit. Clean-consumer evidence separately proves the packaged DLL identity, load, discovery and small read/edit/refusal workflow. Do not describe the development editor's DLL as the installed package DLL. The release artifact in the evidence record is the exact ZIP used by both consumers.

Review the complete tracked source export and release dispositions. The existing release-check gate still requires every required V1 record to pass with exact source/engine identity and hashed logs/artifact. The owner's release instruction authorizes publication once these gates pass. A daily prerelease or an old preview ZIP does not satisfy them.

## Open issue dispositions

| Issues | V1 disposition |
| --- | --- |
| #2, #4, #5 | Stock-engine reports remain open. No addon fix claimed. After a StopPIE error, inspect actual play state before editing. |
| #3, #10, #11 | Exact field/client evidence is missing. Preserve the investigations and current typed/default guards. |
| #6, #7 | Partial layout work remains open; the supported append path gets one focused check. No general layout guarantee. |
| #8 | Current local-player input has scoped behavior. General process-wide console isolation is not promised. |
| #9, #12 | Placed-widget class replacement and broader unloaded scan coverage are later capabilities. |
| #19 | Unreal 5.9 remains a separate compatibility branch and acceptance task. |
| #27 | Blocking addon safety fix: explicit deletion, complete exports and freshness checks. |
| #33 | Deferred features and extended acceptance; not completed work. |

These dispositions use the [source reconciliation](audit/v1-remaining-reconciliation-2026-11-17.json). Unknown causes remain unknown. A new reproduced defect in a supported V1 operation still blocks release if it breaks its safety or core workflow.

## Recorded evidence so far

Graph candidate 36cf3bef passed supervised run b1a2376442d34477862c1035c27794a8: Replace.LinkedRetention 1/1, 6.164 seconds, zero warnings; all 16 fixture files restored. Earlier failures are retained. The macro-discovery crash is guarded; the last two failures were fixture setup that skipped stock event registration and node placement. Production freshness checks were not weakened.

The older preview package from 9c97d01a passed BuildPlugin, but its BuildId differs from the current editor. It is not the V1 package. Clean consumer implementation has source review and focused offline checks; native final-package acceptance remains pending.

The 1.0.0 candidate passed the offline gate only after docs/audit/capabilities.json was regenerated. The version bump changed the plugin tree hash, so validate-repo refused the stale inventory (aabe6dcd). Regenerating that file changed no source, declaration or test.

A first supervised attempt on 0034d78c (run ad5194c4736643ba8cbedd72970d714b) is retained as blocked. The editor started, but TurboMCP loaded disabled because the fresh worktree had no bEnableTurboMCP=True in its user settings. The server registered no toolsets, so all eight suites were blocked. A worktree that runs native suites must carry that setting. The clean-consumer contract already writes it into generated projects.

See [release manager](release-manager.md) for the evidence format and final gate.
