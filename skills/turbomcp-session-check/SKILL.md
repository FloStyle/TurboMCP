---
name: turbomcp-session-check
description: Check a TurboMCP field-test session for tool discovery, version context and observation gaps before normal use. Use for installation verification or connection troubleshooting; do not modify the plugin or execute project code.
---
# Read-only session check

Use the connected editor and the user's authorized installation context. Discover current schemas before invoking tools. Do not guess endpoints, argument names, version identity or tool availability. Read public plugin status and registration information when available. Record engine version, plugin version and client version as Unknown if they cannot be obtained without reading proprietary files or broad machine inventories.

Check that the intended project and editor are selected, without copying their names or paths into feedback. Tool registration alone does not prove that a particular binary or engine is supported. Compare the installed version with the published support documentation supplied by the user; do not download or install updates as part of this check.

Do not use tools that compile, save, play, render project code, mutate assets or execute custom scripts merely to test connectivity. The Read prefix does not guarantee a side-effect-free tool. If a missing permission or unavailable tool prevents inspection, report the limitation without bypassing it or killing another editor.

Return a short local status: connected or blocked, discovery result, known version context, unknowns and the next diagnostic step. Do not produce a health or compatibility certificate. For a work-session report, use turbomcp-session-report if installed, or ask the user to provide it. This skill never submits issues or grants contribution rights.
