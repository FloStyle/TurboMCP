# Input event reachability

OCT27-03 approved repair: recognize stock K2 event-node interface implementations as graph execution entries, alongside existing event/function/tunnel entries. This covers Enhanced Input action nodes without treating an arbitrary node with an execution output as an entry. All execution outputs are traversed; pure producers remain backward dependencies of live consumers.

Use the existing shared reachability helper, so append preflight, compile diagnostics, graph readers, dead-node preview and layout agree. No engine edits, heuristic title matching or relaxed orphan checks.

Native evidence must use a real Enhanced Input node with an owned InputAction, connect a call from an execution output, and preserve an unreachable ordinary-call control. Exercise public append for an existing event and an event created in the same request; inspect compile reachability and fresh graph state. Refused append must leave graph nodes/links and dirty state unchanged. Record any missing fixture/discovery prerequisites rather than substituting a synthetic event implementation. No runtime input dispatch claim follows from static reachability.

Scoped source: shared reachability implementation/header, InputBlueprintNodes module dependency and a dedicated native regression. Catalog/wrapper registration and task evidence are integrator-owned. DSH remains paused.
