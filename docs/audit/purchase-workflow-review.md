# Purchase workflow tool review

The repaired scenario passes its expanded behavioral assertions. The final capture has been inspected, but **user visual acceptance and full per-tool certification remain open**. The per-tool record is [purchase-workflow-review.json](purchase-workflow-review.json). No tool is marked Ready from a passing inventory alone.

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
- Historical candidate 9e90b5a differed from its native run only by a comment encoding repair; current evidence follows below.
- Assignment workflow-purchase-clarity passed the full-SHA scope check from af770775dd7f825216f9f83d86c4c944fb6b8cf2 to 9e90b5a88f5bcb51c3d3fa742aca4ab3b2bf5b44.
- Earlier fixture assignment scope check did not pass because integrator-owned catalog/runner registration was committed after its approved base. That registration was independently inspected; it must not be represented as a worker scope pass.

## Workflow repairs verified on 2026-10-25

9e1cc0a repairs final-scale SVG rendering, the scratch function title, redundant input getters, reader spacing and long comment titles. dd8f58f reports exact verified pin defaults and accurately describes CommentList membership. Capture, compile and comment implementation notes were corrected. Both assignments passed their full-SHA scope gates.

Run c8735a8f289248269dad165ce361fd5d passed build and the expanded purchase scenario. It checks clean inspection, missing targets/invalid parameters, preview non-mutation, exact changed/unchanged pin results, runtime values, annotation placement/bubble state, preserved unrelated content and written image artifacts. Actual PNG inspection confirmed visible SVG icons and no title/node or title/execution-wire interference. Numerical findings alone did not establish this.

Run 3e4c2fe30e9e41b89b63350a15631d28 passed connected-target graph round-trip. Its purchase failure was a test expectation using error: for PinDefault's documented unchanged: refused result; the corrected assertion checks that specific prefix and target name, and subsequent runs passed.

The SVG fault was isolated to vectors requested at the final zoom: FWidgetRenderer batches them without updating the shared vector atlas. A normal renderer resource update with an empty window buffer, followed by repaint, resolves the capture defect. No live asset editor or custom engine modification is required. This does not establish the cause of the separate live CP-14 contrast issue.

## Remaining limits

- User visual acceptance is pending. All 18 participating tools have bounded scenario evidence; they are not globally certified.
- GraphLoad literal escaping/unsupported forms, wider types, full failure restoration, Widget editing, append/batch and save/reload remain separate cases.
- Tidy preserves existing comment anchors. Creating a box in a poor original location before tidy can leave long wires; this scenario tidies first, then annotates and inspects the image.
- Run 61f4696c2bbf4848ac6171de134dfb9e failed the older tidy-scope and comment-box suites because their required fixture nodes were absent. Do not count them as passes.
- Offline run effc7e24c58847d990fb5940b8c72f85 passed 22/26. Remaining failures: ready task 02674939.md lacks Problem; docs/user/tools.md has invalid UTF-8; client retry expectations mismatch; lookup guards report existing findings.
- Inventory of 125 tools is not completion of 125 behavioral and wording reviews.
