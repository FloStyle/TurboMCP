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
and navigation is corrected. CI, merge and Pages deployment remain separate
steps. The missing monitor screenshot is deferred; the existing request
example remains visible, with no broken image or simulated capture claim.
A real monitor capture with synthetic calls remains a follow-up.
