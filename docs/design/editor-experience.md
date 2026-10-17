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
