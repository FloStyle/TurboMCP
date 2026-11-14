# Function rename and flag dependency review (NOV05-10)

The field report observed five of five widget-function renames and three of three purity changes refusing with "More than 256 dependent packages". The source path explains that refusal: `TurboMCPFunctionRenameTools.cpp` and `TurboMCPFunctionFlags.cpp` each start at the defining Blueprint package, recursively call `IAssetRegistry::GetReferencers` on every discovered package, and refuse when the visited set reaches 256. This count is transitive package reachability, not the number of call sites or even Blueprint callers. It can reject a function with no known caller. The exact field-project dependency graph is unavailable, so this is a source-confirmed refusal mechanism, not a reproduction of those eight calls.

Both operations currently load and classify every package reached before they inspect Blueprint call nodes. The cap also protects against unbounded work and incomplete caller updates. Removing or raising it alone would not prove safe rename or purity conversion. A direct-referencer-only change needs proof that it also finds calls through derived Blueprint classes and delegates; a package-only asset dependency does not identify a function call.

Repair contract to review before implementation:

1. Build a bounded, complete candidate set for the defining Blueprint, relevant derived classes and direct callable references, without recursively walking unrelated package consumers. Refuse an unclassifiable or incomplete candidate set before mutation. Never load a map implicitly.
2. Inspect candidate Blueprint graphs for exact function identity, calls, delegates and overrides before mutation. Keep the existing connected-execution and partial-postcondition guards. Preview and edit must use the same candidate rules, and edit must recheck current state.
3. Give a precise response: candidate packages inspected, actual callers found, skipped/unsupported evidence, and the reason for any refusal. Keep dynamic strings, native code and unavailable registry content outside the claimed proof.
4. In a disposable fixture, make more than 256 unrelated transitive package referencers but only a few direct callers. Verify preview succeeds, rename/flag changes update the intended calls, and unchanged assets remain unchanged. Also exercise derived callers, delegate/override refusal, map and unknown-asset refusal, and no mutation on an incomplete scan. Use one focused native run after the source and scope review.

This is an open P1 bug. No code or tool-help change is claimed here. The source review narrows the cause and acceptance criteria; it does not certify a repair or the field assets.
