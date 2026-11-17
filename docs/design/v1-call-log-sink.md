# V1 call-log disk sink

Readiness: ready. Integration owner approved 2026-11-17.
Assignment: v1-call-log-sink under R04. Contract: ARC14; cases L01/L02.

## Confirmed behavior

At ead44fd2 the production AppendToFile ignores directory creation, deletion,
move and append results. It writes a successful rotation marker even after a
failed move. It checks size before an unbounded append, and disk work occurs
outside the log mutex. This can lose evidence or exceed the advertised bound.

## Repair

Keep logging opt-in and local. Serialize size inspection, rotation and append
as one sink operation. Check every file operation and retain bounded diagnostic
state for failed directory/size/rotation/write operations. Report diagnostics
through existing log reads; do not change the underlying tool's factual result,
change/refusal/undo metadata, or re-execute it. Avoid recursive logging and lock
reentrancy when surfacing a sink error. Preserve in-memory admission semantics.

Bound each encoded UTF-8 JSONL record and the active/previous file sizes. Keep
valid JSON and UTF-8; explicitly record omitted fields/bytes, retaining factual
measured duration/result-size metadata. Count newline and rotation marker bytes.
Never claim a write/flush/rotation succeeded unless it did. If rotation fails,
refuse further disk growth rather than exceed the cap or destroy the current
log. Preserve existing file bytes on a failed move; report any loss of previous
backup caused by a partially completed rotation. Do not clear failure state
merely because an unrelated call succeeded. No stronger durability claim than
actual checked file API semantics.

Use existing file and synchronization APIs. A scoped injectable sink/failure
seam in the approved files is allowed for deterministic tests; do not replace
the global platform file, change global settings or redirect production logs.
No telemetry, new service, credential reader or schema expansion. Update affected
comments/help text to describe real behavior in concise English. Root owns shared
catalogue registration and acceptance records. Keep scope disjoint from GraphReplace.

## Minimal verification

Worker performs focused offline checks and submits a clean candidate; root
schedules one supervised native fixture after source review, together with
other accepted changes where practical. Use an owned temporary directory and
isolated test instance, never user logs. Exercise denied append, directory
failure, backup-delete/move refusal, simultaneous threshold crossings and one
oversized Unicode record. Inspect actual bytes/JSON, active+backup caps,
rotation markers, omission counters and unchanged input operation metadata.
Also prove ordinary small records retain the established JSON fields and memory
records are admitted once. Failed writes remain visible without manufacturing a
failed operation or successful rotation. Preserve unrelated logging tests.
