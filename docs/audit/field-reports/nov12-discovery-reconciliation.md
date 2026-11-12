# Remaining reader/discovery reconciliation

Reviewed against private main 98f177d7. No implementation completed by this note.

## NOV09-10: confirmed open

GraphLoad declares empty FString defaults for Revision and EntryFilter. The known engine schema behavior exposes these as required; the field GL-33 call omitted both. BlueprintLoad has no C++ defaults for its five arguments, despite empty-value fallback behavior. ToolHelp also declares both parameters without defaults. Merely describing them as required does not meet the field request for optional initial calls.

Next bounded repair: provide nonempty discovery defaults with explicit normalization in GraphLoad and ToolHelp, and actual BlueprintLoad defaults. Preserve explicit empty legacy arguments. Revision normalization must never let a later page bypass stale-token checks. Avoid changing existing public argument names: for ToolSchema/ToolHelp spelling differences, inspect actual schema names and support compatibility or clearly give the correct cross-tool call. Add a focused omitted-arguments discovery scenario; one incremental build with the pending reporting fixes. Exact scope and base must be approved before edits.

## NOV09-11: remains open

BlueprintLoad help explicitly permits lower-detail fallback and notes GraphFilter does not filter all asset metadata. Current implementation records budget fallback rather than preserving requested detail. This does not meet consistent filtering/paging acceptance; no closure from improved warnings alone.

## NOV05-18: remains open

Existing member-variable removal uses preview/confirmation and preserves nodes. This does not establish local-variable removal or variable rename. Source search found no dedicated local removal/rename implementation. TypeParser ResolveBare handles built-in aliases then object loading, not a deterministic short-name registry resolver for user-defined structs. Do not close the combined capability task based only on member removal.
