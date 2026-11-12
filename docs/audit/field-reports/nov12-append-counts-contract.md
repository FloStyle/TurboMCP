# Append literal count clarity

Readiness: ready. Owner: integrator. Task: NOV09-07.

Current review: PinsWritten increments only when SetLiteral reports Changed. Equal parsed defaults return success without incrementing. This explains why accepted values can exceed the reported write count, without proving the precise historical six-field call.

Report accepted literal pin assignments, changed defaults and already-matching defaults separately. Count successful immediate and deferred assignments exactly once. Clarify aggregate struct literals count as one pin, not per field. Preserve all mutation, refusal and rollback behavior. Do not claim final graph read-back from these counters: they describe literal application before later graph operations. No new tool or signature.

Scope: TurboMCPGraphAppend.cpp only. Inspect both counter paths and result formatting; batch incremental compilation and focused Append validation with subsequent source work. Historical call unavailable; no invented reconstruction or numerical field-test claim.
