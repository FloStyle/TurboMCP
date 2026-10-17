# Product requirements

[Documentation](README.md)

## Purpose and users

TurboMCP extends Unreal's MCP server so a human-supervised agent can inspect and edit an Unreal Editor project with explicit targets, bounded output and verifiable results. Contributors must be able to build, test and understand the plugin from this source repository without private documents or a particular agent harness.

## v1 boundary

The first certified target is Windows with the source-engine revision pinned in engine.lock.json. Existing capabilities cover graph inspection/editing, layout, materials, widgets, asset/type operations, capture, runtime observations and diagnostics. A declaration is not a supported capability until its acceptance evidence exists. Installed engines and Unreal 5.9 are separate matrix targets; neither inherits certification from the baseline.

Required behavior:

1. Reads identify the asset and graph scope, report incomplete or unavailable data, and never turn missing evidence into a measured zero.
2. A mutation resolves an unambiguous target, validates a plan before changes, detects stale state, verifies the result independently and reports its real outcome.
3. Compilation, memory rollback, undo, source-control checkout and disk persistence are separate operations and separately reported outcomes.
4. A tool that can execute project code says so. Preview does not imply sandboxed execution.
5. Results are bounded and diagnosable. Truncation or unsupported cases are explicit; compact output cannot discard information required for a destructive edit.
6. Tests restore owned fixtures, reject blocked/missing cases as success, and identify the tested source, engine and binary.
7. Source publication preserves the complete contributor tree with fresh history; installation packages are independently tested artifacts.

## Out of scope until separately designed

General multi-asset atomic editing, broad new authoring families, remote unauthenticated service deployment, autonomous publication and packaged-game runtime support are not v1 promises. The detailed release scope and refusal semantics are in [technical contracts](architecture.md). Backlog proposals remain visible without becoming implemented features.

## Completion definition

Every advertised capability has supported inputs, effects, refusal conditions, limitations and linked passing acceptance cases. Required release cases in Tests/acceptance.json run against the candidate. The copied source tree validates independently; a clean consumer project loads the exact packaged artifact. User documentation reflects those verified capabilities. The release manager remains closed while any required evidence is missing.
