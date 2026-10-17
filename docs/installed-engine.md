# Installed-engine support

[Documentation](README.md)

Goal: install TurboMCP into a normal project's Plugins/TurboMCP folder, using an installed engine, without engine source patches. This is a compatibility target, not current certified support.

The present machine has source 5.8.3 and Launcher 5.6. The installed 5.6 tree has no ModelContextProtocol plugin descriptor, so it cannot validate this plugin's mandatory MCP dependency. Do not downgrade the source baseline or silently select that engine.

For an installed 5.8.3 candidate, inspect the shipped descriptors, public headers, import libraries and precompiled manifests for ModelContextProtocol, ToolsetRegistry and SlateInspectorToolset. All mandatory dependencies must exist and be loadable. Build the plugin against that exact installed distribution with RunUAT BuildPlugin, then install the produced package into a fresh project and test discovery, calls, media, undo/rollback, save, shutdown and reload. Test a Blueprint-only consumer separately from a C++ consumer. An engine version string alone does not establish ABI compatibility.

The completed [R16 investigation](audit/installed-engine-boundaries.md) documents production and fixture dependencies. Development-module extraction remains a proposal until packaging evidence establishes a need. Propose moving these dependencies (notably AnimGraph, NiagaraEditor and PythonScriptPlugin where used only by tests) into a development-only module only if the dependency evidence supports it. Verify actual production users before removing any dependency. Keep runtime/editor-only boundaries explicit; this plugin is an Editor module and is not intended to ship in a packaged game. Avoid private engine headers and engine patches. R14 must first establish a supported public identity path for 5.9; an adapter remains a proposal.

Record each supported matrix row independently: source/installed distribution, exact engine build, platform, compiler, plugin revision, package hash, clean-install result and runtime acceptance. Source 5.8.3 smoke, installed-engine acceptance and 5.9 acceptance are separate claims.

Epic documents source and binary plugin layouts and project-local plugin discovery in [Plugins in Unreal Engine](https://dev.epicgames.com/documentation/en-us/unreal-engine/plugins-in-unreal-engine). The local missing dependency and module coupling above are repository inspection findings, not a claim that all future Launcher distributions omit MCP.

## Investigation before packaging
R16 measured a successful 145-action source-engine target build with no engine-module compilation and documented the public integration boundaries. Source-engine use alone does not prove a full engine rebuild is required for each plugin change. R12 later proves packaging in an exact installed distribution and a fresh Blueprint-only consumer that does not compile game C++ or engine source. A package producer still builds matching binaries; consumers should not need that toolchain.
