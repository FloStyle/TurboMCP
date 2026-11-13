# Append count assertion reconciliation

Readiness: ready. Integration owner: integrator.

The runtime result contract now separates literal assignments accepted, defaults changed and defaults already matching. The wrapper still expects the withdrawn pin value(s) written phrase. Replace only that assertion with parsing of the full new triple; require accepted > 0, changed > 0 for this fresh unique-marker fixture, and accepted == changed + already matched. Keep independent graph growth, literal readback and all refusal assertions. Exact scope: Tools/test-graph-append.py.

Validate the parser against captured execution 4cb0955a1271404a8e75d7d1f86e1135 and negative zero/inconsistent/missing counts without another editor run. This does not retroactively change the failed suite verdict.
