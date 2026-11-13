# NodeFind JSON review checkpoint
Implementation 89ae442b and f2bb935c integrated into main after exact scope review. Format=json provides structured hits, errors, paging and coverage. Text remains default; both modes and help explain Data Asset scope and reference-reader handoff. Safety guards unchanged.

Build 9088293ee59f4d1797194ffd7a4b26fb passed on stock 5.8.4. Focused run cbcd59ec634045b09b06cba2b632db31 FAILED: TurboMCP.GraphAppend.Inventory reported error: measurement render target unavailable, with no warnings. Existing position-measurement checks share this fixture. No JSON assertion failures were reported, but this is not a passing workflow. Isolate nonvisual acceptance next; retain this failure. No full append suite was run.

NOV13-08/09 remain in progress. Unloaded coverage and full-result file export remain unresolved.

## Focused acceptance completed
Candidate a2e71a9904c7f7ba1e60f5e4ffa342d602a643ea passed exact scope review. Run e87ecec50e014a729bb35e3b740984ce passed build and TurboMCP.Read.NodeSearchJson (0.091 seconds, no errors or warnings). The isolated fixture verifies all 200 node identities exactly once across one-hit pages, exact asset/graph fields, final null cursor, cumulative totals, text/count-only parity, empty results, JSON input errors, scope guidance and unchanged dirty state.

The earlier cbcd59ec634045b09b06cba2b632db31 failure remains valid: the supervisor launches with -nullrhi, while the original Inventory fixture calls visual graph-position measurement. The new fixture does not invoke that unrelated operation; the original visual fixture is unchanged and not certified. This is not a successful full append-suite rerun.

NOV13-08 and NOV13-09 are implemented with focused reader evidence. Project-scale performance, skipped-map coverage and full-result file export remain separate tasks.
