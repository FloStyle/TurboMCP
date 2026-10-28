# Per-user tool controls: OCT27-17

Keep the master preference off by default. Store settings only in EditorPerProjectUserSettings. Changes apply after editor restart, matching master enablement; status must distinguish active policy from pending preferences.

Offer domain disable presets and exact tool-name overrides (inherit, enabled, disabled). Explicit overrides take precedence over the domain preset; master OFF always wins. Centralize domain classification and expose the assigned domain in diagnostics. Domains cover Blueprint graphs, Widgets/CommonUI, data/assets, play/input, vision, diagnostics, and other tools. Unknown names must produce a configuration diagnostic rather than silently matching a different tool.

Treat corresponding Preview/Edit tools as a safety pair: disabling preview also disables edit even when edit is explicitly enabled. Disabling edit may leave preview available. Keep ReadTurboMCPStatus discoverable while the master is enabled so users can diagnose and recover through Editor Preferences. Do not let a model modify these personal settings through a new tool.

Snapshot policy at startup. Filter disabled tools from registry discovery and reject direct routed calls before execution. Keep stock engine safety filters independent. Guard the shared call wrapper as well as registry filtering; do not rely on discovery hiding alone. Report active enabled/disabled counts, reasons and restart-required changes using clear English. Direct C++ internal helper calls are not a security boundary; MCP and panel dispatch must honor the policy.

Verify individual disable, domain disable, explicit override precedence, preview/edit dependency, status recovery, no execution of a disabled call, and persisted preferences across a real editor restart. Preserve and restore the test user's prior configuration. Inspect the Editor Preferences UI if presentation changes. No engine edits, no global/shared default configuration changes.
