# WF-UI-06: Parent widget delegate binding

Status: focused pair passed the bounded native scenario; user visual acceptance pending.
Candidate: 88e57fd. Build/native evidence: 3a38a896a17b48b1aecb7db7b399af75. Scope gate passed.

## Focus

PreviewTurboMCPBindNewEvent and EditTurboMCPBindNewEvent were the only new full tool reviews. WidgetTree, GraphLoad, GraphConnect, Compile and Capture supported the scenario without another full audit.

A parent Widget Blueprint contains a Common Button. Its existing Bind Event node targets the exposed OnButtonBaseClicked delegate. The tools create HandlePurchase with the delegate's Button parameter and connect its delegate output to that node. A separate supported connection step attaches a small handler body that records the sender. Executing registration and broadcasting the native multicast delegate delivers the exact live button instance to the parent.

Fixture APIs create the parent/child assets, initial Bind node and body setter; none of that counts as tool-driven creation evidence. This tests the delegate registration route, not real pointer clicks or inherited button event overrides.

## Behavior checks

- Preview preserves graph snapshot and dirty flag.
- Missing node, missing pin and invalid name are refused by both tools without mutation.
- Edit creates exactly one typed handler, retaining every original node position, default and unrelated link.
- The new Button parameter has the Common Button object type and the delegate input points to the handler.
- Actual live handler ID appears in the edit result; preview IDs are explicitly provisional.
- Repeated event names are refused by preview/edit, including before another compile.
- A second-name replacement preview identifies the previous handler and preserves it and its wiring.
- Widget tree is unchanged. Compile succeeds. Native broadcast reaches the compiled handler with the exact sender.
- Final graph capture inspected: no visible node overlap and no leftover signature-probe node.

## Quality fixes

The old result said an input source was replaced even when the input had no source. Results now distinguish none from exact displaced node/pin IDs and name the target, signature and handler parameters. Existing source nodes are retained; the caller is told to inspect their remaining uses before cleanup.

Help and output explain that the tool creates an empty handler for an existing delegate input. It does not create the Bind node, provide the handler body or execute registration. The target Bind node must execute at runtime. Compile/save and runtime verification are separate steps. These distinctions are intended to prevent small models from reporting a completed interaction merely because a handler node exists.

Reviewed the binding implementation, signature probe, exact pin resolver, schema connection helper, scratch simulation, transaction wrapper, duplicate-name checks, descriptions and recorded refusal/success output. No core binding failure was reproduced in this fixture. Initial build failures were fixture API mistakes, corrected before native execution; the stock engine was not changed.

## Limits

This scalar object-parameter signature does not certify all return, reference, container or inherited signature cases. Live replacement of an existing source, injected rollback and persistence were not tested. A successful native delegate broadcast does not prove real mouse/focus behavior. Common UI style editing remains outside this focused pass. Coverage is 32/125 unique tools with bounded workflow evidence, not 125 globally certified tools.

Offline gate 0ca6720f20674e56a15e6e302ce40e50: 22/26 passed; existing structure, language, client-retry and lookup-guard failures remain. No clean repository-wide gate is claimed.

## Binding layout correction - 2026-10-25

Implemented 694a70d: the workflow now calls Preview/Edit GraphTidy on its five affected nodes after wiring the body. No fixture coordinates were manually corrected and no production Tidy change was required. The getter sits near the Bind target; HandlePurchase and Set execution pins align, as do their data pins. Preview preserves the graph; edit preserves node count, links and defaults. Native build/workflow passed 9fa27a0144b542d7a2a69409989a210e, including compile and sender dispatch. Updated capture inspected; human acceptance remains pending.

Initial assertions incorrectly measured getter distance from the Slate pin widget origin, including its label width. The final check measures visible node-edge clearance (at most 160 units), plus pin-row alignment (at most 2 units). Offline gate d20bd814286945678b180f291d0a1ee5 remains 22/26 with the same four baseline failures. Coverage remains 32/125; Tidy is reused support.
