# NOV13-04: bounded bulk result export

Readiness: design reviewed; implementation assignment pending.

## Source findings

The November 12 S2 report requests all pages in one compact JSON file. Reference pages already serialize a single JSON object. NodeFind now has JSON mode. WidgetInventory returns a structured result whose data contains nextCursor, matches and pageIssues. Concatenation in the field remains unconfirmed; do not label it a fixed serializer defect.

## Required implementation

Provide a plugin tool for bulk export, restricted to RefsBatch, NodeFind and WidgetInventory. Do not expose arbitrary tool dispatch. Accept the selected reader and its query arguments as JSON, with explicit page/time/output-size limits. Validate keys and types against each supported reader contract; force NodeFind JSON. Preserve all reader safety guards and use the same cursor with unchanged query parameters. Do not retry unknown outcomes.

Write one compact UTF-8 JSON document to a unique file under Saved/TurboMCP/Exports. The caller cannot supply an arbitrary filesystem destination. Never overwrite an existing export. Return the artifact path, reader, collected page count, byte count, continuation and completion status without echoing the entire data set. Clearly describe this as writing a report file, never saving project assets. Widget scanning may load packages and execute PostLoad; retain that warning and its effects.

The document contains the query, collected pages, aggregate export metadata and reader coverage evidence. Keeping page envelopes avoids losing reference kinds, root chains, omissions, dirty transitions or tool-specific metadata. A single document containing page objects satisfies full-result export without unsafe schema flattening.

Distinguish traversalFinished from complete. Exhausting a cursor does not erase skipped assets, omitted evidence, traversal caps or errors from earlier pages. Empty successful results are valid. Detect repeated cursors and malformed pages. A cap, timeout or reader error produces an explicitly partial document and continuation when available. Cancelled/failed writes must never appear as completed artifacts; use a temporary file and finalize only after serialization succeeds. No map load, checkout, PIE or engine changes are added by the exporter.

## Focused acceptance

Use synthetic reader responses to check multi-page assembly, valid single-document parsing, early-page incompleteness, empty results, repeated cursors, reader refusal and file-write failure. One supervised live fixture should prove the reflected tool, saved JSON and a small paged query. Do not rerun unrelated suites. Record file writes separately from asset effects. Test cancellation only if the execution API exposes cancellation; otherwise document time/page caps and interruption behavior without claiming cancellation support.

## Scope to approve before coding

New bulk export implementation and focused test; public tool declaration; agent help. Integrator owns tool inventory, test registration, status and evidence. Existing readers should not need behavioral changes. Review FTurboMCPResult envelopes and native parameter parsing before finalizing exact file scope.
