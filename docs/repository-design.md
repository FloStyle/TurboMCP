# Repository design

[Documentation](README.md)

Maintain one complete source tree for private development and public contribution. A public snapshot must preserve every tracked file's path and Git blob bytes, including AGENTS.md, tasks, specifications, tests and build tools. No public-only README, documentation fork or hidden development subtree is maintained.

The root README describes each directory. Production plugin files belong under Plugins/TurboMCP; binaries are release assets. Tests and technical specifications evolve in the same commit as behavior. Unreal stays in its own licensed repository. Local configuration, logs and recovery evidence stay outside tracked source.

The current editor module contains production code and development commandlets. Extracting test-only dependencies requires a separately tested module design; directory movement alone does not establish an installable boundary. Use Public for externally consumed interfaces and private implementation for internal code. Keep the plugin module identity stable until a migration is explicitly designed.

Current technical contracts live in architecture.md. Designs explain how those contracts should be implemented and name unresolved decisions. Tasks reference those contracts and provide narrow acceptance criteria. Historical quotations cannot override current contracts. A requirement document is not evidence that a feature is implemented or tested.

The publication tool refuses unsafe tracked files rather than silently omitting them. Its verification manifest lives outside the exported tree. A normal Git clone retains private history; use a fresh Git root for the first public publication. Private history is retained solely in the private repository and must never be pushed to the public remote.
