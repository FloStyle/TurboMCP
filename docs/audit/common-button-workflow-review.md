# WF-UI-05: Common UI button workflow

Status: planned; not executed or validated. DSH remains paused.

Use an existing Common Button with a Common UI style. Read the widget hierarchy, assigned style and relevant existing event behavior before editing. Put visual states in the style where appropriate; use events for additional label behavior. Discover inherited events versus parent delegates before choosing a binding mechanism.

New review candidates: ReadTurboMCPWidgetTree and LookTurboMCPWidgetPreview. PreviewTurboMCPBindNewEvent and EditTurboMCPBindNewEvent are conditional on the actual event mechanism. Reuse Blueprint/graph readers, discovery, Append, Connect, Tidy, Compile and Capture as needed. Planned tools do not count toward executed coverage.

Resolve these gaps first: style/property inspection and targeted editing; exact Common UI event discovery; interactive verification beyond a static preview. WidgetTree currently omits most properties and bindings. Do not invent a dedicated style tool or assume complete support.

For every tool, review behavior and refusals; output completeness and grammatical English; precise target identity and truthful changed/unchanged/partial results; descriptions, parameter examples and next-step guidance; implementation, shared helpers, error handling and explanatory comments. A smaller model should not need to guess identifiers, defaults, side effects or omitted information.

Acceptance requires actual before/after evidence: intended style/label behavior, unchanged unrelated widgets and bindings, correct event wiring, readable graph layout, no accidental orphan nodes, compilation and visual/interactive state checks. Record missing evidence as a flag. Do not describe limited passing evidence as bug-free certification. User visual acceptance remains a separate step.

Canonical tool usage and quality criteria: workflow-tool-coverage.json. Dashboard tool rows come from purchase-workflow-review.json; planned entries explicitly say not tested. Current reviewed count stays 28/125.
