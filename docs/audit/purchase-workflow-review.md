# Purchase workflow tool review

The repeatable scenario exists and passes its behavioral assertions. It is **not yet visually approved or a complete review of every participating tool**. The per-tool record is [purchase-workflow-review.json](purchase-workflow-review.json). No tool is marked Ready from a passing inventory alone.

## Scenario

CalculatePurchase accepts UnitPrice, Quantity, DiscountPercent and AvailableFunds. It returns FinalPrice and CanAfford. A deliberate wrong multiplier converts 10 percent to 100 percent. The workflow corrects that literal, names repeated arithmetic Subtotal and PurchaseTotal, tidies the function, adds a comment, and checks the result. An unrelated function must remain unchanged. The fixture is uniquely named, never saved and disposed after the run.

Independent expectations: 50 x 2 with a 10 percent discount costs 90 and is affordable with 100; 50 x 4 costs 180 and is not affordable. These checks run before and after cleanup, so tidy/local promotion cannot pass merely by compiling.

## Instructions for a tool-using model

1. Read the live schema. Use BlueprintLoad to select the relevant function, then GraphLoad for its exact content. Supply revision and entryFilter as empty strings for an unfiltered first read. Never invent node IDs, type IDs or pin names.
2. State the observed problem and the smallest change. In this fixture, only the percent conversion literal is wrong. NodeFind locates candidates; NodeContext explains static execution context, not every data dependency.
3. Set the exact unconnected input default with PinDefault. Read back and compile separately. A successful compile does not prove the calculation; compare independent expected values.
4. Preview LocalVars. Only reviewed deterministic scalar arithmetic with stable input ancestry is supported. Supply Names as a JSON object mapping the exact output IDs printed by the preview to meaningful names. Use the same mapping in the edit. Do not turn arbitrary pure calls into entry-time locals.
5. Preview tidy with an explicit scope. This fixture authorizes its whole function; normal fixes should preserve unrelated layout. Edit recomputes against current state. It may duplicate getters. It does not save or compile.
6. Add short, useful comments. Inspect the actual capture after comment creation; zero numeric overlaps does not guarantee readable comment titles or bubbles.
7. Read the result, verify behavior and inspect the capture. Report remaining problems, dirty/save state and any partial failure. Do not save merely because an edit returned success.

Partial pages and filtered reads are inspection fragments. Keep revision and all other read arguments consistent while paging. Do not pass fragments or blindly concatenated page envelopes to a writer. A clean GraphCheck result is limited to its checks, not a promise that a writer will accept the text.

## Evidence and fixes

- 50c22ec fixed missing source-output pin names in graph reads.
- 9e90b5a fixed graph closing delimiters swallowed by semicolon comments, added caller-selected local names, preserved partial-input rejection, and corrected tidy help/output wording.
- Run 3a32f7e906f94bf6829af00fa95ee3a1: build, purchase workflow and connected-target reader round-trip passed.
- Run 1a36a0820e1f4f51a37bf636806c60ee: local ancestry and promotion suites passed; the broader run failed on the reader defect subsequently corrected.
- Run 1b5649bf2c4c44f68bef8ed29077061d: purchase and 125-tool text inventory passed. The remaining round-trip failure was an obsolete assertion expecting a stronger success claim; the corrected test retains zero-finding and explicit limitation assertions.
- The final candidate differs from the last native test only by a comment encoding repair.
- Assignment workflow-purchase-clarity passed the full-SHA scope check from af770775dd7f825216f9f83d86c4c944fb6b8cf2 to 9e90b5a88f5bcb51c3d3fa742aca4ab3b2bf5b44.
- Earlier fixture assignment scope check did not pass because integrator-owned catalog/runner registration was committed after its approved base. That registration was independently inspected; it must not be represented as a worker scope pass.

## Still required

- Fix comment title/bubble interference seen in the final capture. Numeric node overlap checks missed it.
- Finish per-tool negative cases, schema examples, code/comment review and actual read/vision side effects. Transcript presence alone is not behavioral acceptance.
- Review GraphLoad literal escaping and unsupported-form fidelity before broad write-back claims.
- Correct historical compile/save comments that imply transaction rollback behavior the implementation does not provide.
- Keep automatic local names as a documented fallback; meaningful caller names are now supported.
- Separate broader widget, append/batch, save/reload and rendering workflows from this unsaved arithmetic scenario.

The offline gate is not green: stale TODO, invalid UTF-8 in docs/user/tools.md, client retry contract failure and heuristic lookup-guard findings require reconciliation. They are not hidden by the focused native passes. The first sandboxed offline run additionally hit temporary-directory permission errors; use the elevated follow-up report for the actual gate result.
