# NOV09-14/15: bounded DataTable values and usable columns

Readiness: ready. Integration owner: integrator.

## Confirmed current behavior
At fd6e12a3, TurboMCPDataTable.cpp stops at MaxRowNames=60 with no continuation. Columns matches only reflected names; empty Columns returns no values. Localized text uses lossless ExportText only. These match DT-14/15 and are not duplicate completed fixes.

## Required repair
- Keep sorted row ordering; add bounded offset/limit and row-name substring filtering with total/matched/shown and next offset. Include revision identity; later pages must refuse stale revision/query so changed tables cannot silently skip rows. Validate arguments before expensive work. Never imply omitted rows were inspected.
- Keep existing first three positional arguments compatible. Empty Columns selects all values for all/schema; provide explicit names-only selection. Resolve exact reflected names first, then unique authored/display aliases. Refuse ambiguous names with candidates; do not strip arbitrary suffixes and guess.
- Add explicit lossless versus display text mode, retaining lossless as default. Display-only output must be labeled inspection-only and cannot be mistaken for a write-back literal. Escape line breaks. No writes, compiles or saves.
- Bound response rows and report oversized row/value omissions honestly. Page metadata must not claim values complete when truncated. Preserve schema/reader coverage wording; NOV09-16 reader detection remains separate.
- Update help and existing synthetic DataTable quality test: >60 rows traversed once without gaps/duplicates, filtering, stale continuation refusal, invalid range, names-only, empty/all columns, alias resolution and ambiguous refusal where fixture permits, display text versus lossless. Keep initial failures and coverage limits.

## Exact scope
TurboMCPDataTable.cpp, Public/Tools/TurboMCPToolsets.h and TurboMCPDataTableQualityTest.cpp. Reuse the existing supervised DataTable quality suite. One incremental compile and focused scenario after batching changes; no full regression.
