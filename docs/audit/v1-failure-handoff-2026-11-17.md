# V1 failures and remaining work — 2026-11-17

## Observed result

Candidate ec76157f52024fe9a18c48d5bdb9b9f6c351f427.
Supervised run 6e22e559836449dfb4baf47d33036ef4: build passed; 6 of 8 suites passed.
Offline validation: 28/28 passed (run 64ba6f64719d4a90a1c05de1b1aad557).
Local native logs: .local/executions/6e22e559836449dfb4baf47d33036ef4/02-suites/.

Passed: test-replace-plan, test-graph-edit-workflow, test-append-failure,
test-call-log-sink, test-data-asset-create, test-widget-roundtrip.

## Approved repair contract: v1-schema-callers

1. test-graph-append omits required wire-schema argument containingCommentId.
   Requests are rejected before the intended operation; 37 failed assertions
   include cascading failures. They are not evidence of 37 independent bugs.
2. test-datatable-rows omits required columns on ReadTurboMCPDataTable.
   FIELD stays None, dependent checks continue, then FIELD.lower() crashes.

Read the current declarations in Public/Tools/TurboMCPToolsets.h and the
production implementation before editing. Supply explicit current arguments
at the affected calls, including every required placement/read argument.
Use current documented defaults. Do not add generic client autofill or alter
production schemas. Retain preservation, refusal, mutation and cleanup checks.
Fail promptly with the actual prerequisite error when required reader output
is absent. Do not continue dependent tests with fabricated fields or values.
Preserve the fixture's no-save contract and scratch-row cleanup; no stock
engine asset save, checkout, broad fixture rewrite or new framework.
If a product failure remains after request repair, report its exact evidence
and stop for reviewed scope; do not weaken assertions or expand file scope.

Worker: source review, Python syntax and focused offline checks only. No
engine build/editor/MCP execution. Return base/candidate SHA, claim, exact
diff, checks and limitations. Retain claim. Integrator reviews and reruns
only these two failed suites through the supervisor. Keep old failures.

## Remaining V1 checklist

- [ ] Repair and review these two callers; rerun only the two failed suites (#15).
- [ ] Integrate the reviewed graph fix and acceptance/consumer work into main;
      close #27 only after integration, with its existing passing evidence.
- [ ] Build the owner-approved private 1.0.0 candidate (#16). Keep public
      preview labels until final acceptance passes.
- [ ] Test the same final ZIP in clean Blueprint-only and C++ consumers (#16).
      Verify loaded plugin identity and preserve the tested archive hashes.
- [ ] Verify the complete public export and finalize exact-candidate evidence,
      issue dispositions and release status (#14, #15).
- [ ] Publish stable v1.0.0 and its downloadable package; update release labels.

Unfinished extended features remain in #33; 5.9 remains in #19. They are not
silently counted as completed or added back to the agreed V1 scope.

## Website

Cleanup pushed to PR #31 at 0addd329: guide is TurboMCP-only, grey/yellow,
and navigation is corrected. PR validation passed and PR #31 merged as 4df99d7a.
Main validation and Pages deployment still need confirmation. The missing monitor screenshot is deferred; the existing request
example remains visible, with no broken image or simulated capture claim.
A real monitor capture with synthetic calls remains a follow-up.

## Takeover entry point

Canonical integration checkout: `C:/Dev/TurboMCP-Public`, branch
`codex/v1-graph-acceptance`. This branch contains reviewed local integration
work and is not yet pushed to main. Do not reset it to remote main. The
pre-handoff source/assignment commit is 7235054ee34bdb80040c321a45360304e92d0d4d.
The tested source candidate is ec76157f (full SHA above). Later changes here
are the repair assignment and this handoff, not a claimed new native pass.

Active worker: Sol High, `/root/sol_v1_schema_callers`.
Checkout: `C:/Users/flost/.codex/worktrees/v1-site-examples/validate-main`.
Branch: `codex/v1-schema-callers`; approved base:
7235054ee34bdb80040c321a45360304e92d0d4d.
Assignment: `v1-schema-callers`. Claim: 04f41e2c28ad48d4977b1d4169db332a.
Do not dispatch an overlapping worker or release its claim while it works.
If chat access is lost, inspect the branch and claim store before resuming.
Root released the completed macro assignment before this dispatch.

When Sol submits: fetch its candidate locally, inspect the two-file diff,
run Tools/check-worker-scope.py against the approved base and assignment,
and integrate only reviewed changes. Schedule the two failed suites through
Tools/execute-tests.py. Never run a second editor/build concurrently. No broad
suite repeat without new evidence. Keep the historical failures recorded.

Local implementation includes the graph repair and consumer harness, accepted
in source but not all publicly integrated. Consumer worker final e2f21c41
was integrated as 1547b88c; graph fixture final 6d1bdcf6 as 36cf3bef.
Scope/release decision is committed at ec76157f. Preserve those commits.

Owner explicitly approved a PRIVATE final-version 1.0.0 candidate for clean
consumer checks; publish only after it passes. Metadata/version changes and
final archive build have not been performed. An old preview ZIP is not valid
substitute evidence: its engine BuildId differs. Use the same final archive
for both consumer profiles and record its source identity and hashes.
Read docs/audit/v1-clean-consumer-contract-2026-11-17.md,
docs/release-1.0.0.md, Tests/acceptance.json, and docs/release-manager.md.
Do not claim stable readiness from passing source tests alone.

GitHub issue #15 has the current checkpoint comment:
https://github.com/FloStyle/TurboMCP/issues/15#issuecomment-6942676103
This local handoff and issue together are the takeover record. Website PR #31
is merged; check main CI/Pages before claiming the live site has changed.
TurboUX work must not be renewed. Figma testing is deferred. No new automatic
goal is active: the user cleared it. Continue only the authorized release work.

## Additional caller findings from Sol

Source review found two more stale DataTable test assumptions: the current
planner reports an unknown/ambiguous field with its internal-name guidance
before the older refusal branch; row reads include page/revision metadata
and default output includes values. The same scoped repair will request the
documented names-only output and delimit actual rows, while retaining exact
row identity/count and refusal assertions. These findings are not new
production fixes. Native confirmation remains pending.

## Workspace cleanup and engine access

A final Dev-folder cleanup has NOT been performed. Read-only inventory found
C:/Dev/TurboMCP alongside active C:/Dev/TurboMCP-Public, TurboMCP-Wiki-draft,
TurboUX-native-review-20261117, historical evidence folders, weekly-2026-10-19
and worktrees. They are review candidates, not approved deletions. Preserve
active worktrees, evidence and recovery material until ownership/reference
checks establish safe archival. The active repair worker still lives under
.codex/worktrees; do not move it during its claimed assignment.

Canonical workspace.local.json selects C:/UnrealEngine. engine.lock.json pins
5.8.4 at f45729c0b6f097038e72b52d366a62baeca3275b; UnrealEditor.exe exists at
C:/UnrealEngine/Engine/Binaries/Win64/UnrealEditor.exe. No UnrealEditor process
was observed during this check. The current repair worker has no local
workspace.local.json; do not assume its engine discovery is configured.
Its assignment is explicitly source/offline only. Native work is scheduled
by the integrator through the supervised runner with configured engine path
and shared ownership locks; filesystem availability is not execution ownership.
