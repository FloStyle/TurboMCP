# Editor experience requirements

[Documentation](../README.md)

Improve the existing TurboMCP Slate panel, registration and resources before introducing another UI framework. Current entry points are STurboMCPPanel.cpp, TurboMCPModule.cpp, TurboMCPPanelToolRunner.cpp and Resources/Icon128.png.

## Information hierarchy

Show connection and registration status first, including an explicit unknown state. Separate tool discovery, recent calls and settings. Show engine/plugin identity where actually known. Use text labels with icons and color; never communicate state by color alone. Keep detailed diagnostics expandable and avoid presenting debug internals as the primary workflow.

## Interaction and safety

Read-only status inspection must not call mutation tools, compile or start play mode. Distinguish inspection, preview, execution-capable preview and mutation in the tool browser. Show required inputs and actionable validation near controls. Never display success based only on transport success. Separate in-memory change, compile, rollback and save outcomes. Copy/export actions must warn about project data and remain user initiated; do not auto-export logs or feedback.

Support keyboard navigation, visible focus, readable scaling, narrow docked panels and high-DPI displays. Provide descriptive tooltips and accessible labels. Keep lists bounded and avoid expensive polling or editor focus stealing. Design the icon for small sizes with clear silhouette; retain a reproducible source asset and license record.

## Design acceptance

R18 delivers annotated layout states for connected, unavailable, empty, busy, refused, failed and partial outcomes. Include narrow/wide and high-DPI layouts, keyboard order, state-to-control mapping and a before/after usability checklist. Owner review approves the design before a separate implementation task changes Slate code or assets.

Implementation acceptance must use the pinned 5.8.3 editor and verify panel open/close/reopen, status changes, keyboard operation, resizing, scaling, tool search, result display and absence of unintended writes. Existing panel regressions must remain covered. A mockup or screenshot alone is not functional acceptance.

## Approved first pass: 18 October 2026

The owner approved the compact web prototype as the direction for native implementation. This approves the interaction design, not simulated backend capabilities. Keep the native Slate panel; deliver bounded slices with source review and pinned-engine tests. Blueprint/Widget Blueprint targeted read/write reliability remains the Monday release priority.

- Default to a compact Monitor view. Separate tool browsing/help/testing from history. Use a shared selected-item inspector rather than several permanently open panels.
- Use neutral selection and Read/Preview/Edit badges. Reserve green for successful outcomes, red for failures, orange for issues/refusals and blue for observed active work; always include text. An unmatched start means completion not observed, not proven active work.
- Provide Start, Stop and Refresh status, endpoint display and client-appropriate copy configuration. Start must not restart an already-started server. Stop does not promise cancellation or rollback. Distinguish configured address/internal state from verified reachability.
- Record calls by default for new settings; preserve a persisted OFF choice. Keep bounded history, search, outcome filters, follow-latest and request/result inspection. Replace Sample with Fill example arguments and explain that examples are schema scaffolding, not validated runnable requests.
- Keep measured zero distinct from unknown character counts; label token estimates and unknown totals honestly. Do not fabricate agent turns or session attribution where telemetry lacks them.
- Add Changed assets review navigation only for observed edits: asset, graph/location, operation and known save state. Distinguish session edits from depot differences. Open/locate/diff actions need verified native support; unavailable actions remain disabled with a reason. Do not infer successful changes from a tool name or transport success.
- Treat human graph tidying as a separate preview/apply workflow; do not expose a simulated action as an implemented tool.

First implementation slice should repair monitoring freshness at the log cap and clear/refill boundary, unknown totals and ambiguous in-flight states before adding presentation. Then add compact Monitor/Tools navigation and inspector, followed by bounded controls/menu/icon changes. Changed-asset attribution is a separate slice if new telemetry is required. The mockup is not a license to invent telemetry or expand editing scope.

Implementation readiness still requires exact file ownership, source-backed contracts, negative controls and regression coverage for each slice. Validate narrow docked and wide floating panels, keyboard/focus, scaling, empty/unavailable/pending/refused/error states, and no unintended execution. R18 as a whole is not marked complete by design approval.
