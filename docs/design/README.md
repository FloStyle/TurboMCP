# Technical design index

Read [product requirements](../product.md) for scope and [architecture](../architecture.md) for normative ARC01-ARC16 contracts. The designs below explain implementation boundaries and verification responsibilities. They specify required behavior; they do not certify the current implementation. Task-specific designs must identify remaining decisions before a task becomes ready.

| Design | Responsibilities | Governing contracts |
| --- | --- | --- |
| [Protocol and results](protocol.md) | Discovery, typed outcomes, retries, media and logging | ARC02, ARC11, ARC14 |
| [Mutation lifecycle](mutations.md) | Plan/apply/verify/compile/rollback/save ownership | ARC03-ARC06, ARC08-ARC09 |
| [Graph operations](graphs.md) | Identity, representation, edits, reachability and layout | ARC06-ARC08, ARC16 |
| [Editor domains](editor-domains.md) | Materials, widgets, assets, capture and runtime observations | ARC04-ARC05, ARC10-ARC11 |
| [Build and verification](verification.md) | Test tiers, fixtures, packaging and compatibility | ARC12-ARC13 |

Source declarations live in Plugins/TurboMCP/Source/TurboMCP/Public/Tools/TurboMCPToolsets.h. The generated [tool reference](../user/tools.md) is derived from those declarations. Keep behavior documentation and implementation changes in the same commit. Do not create competing architecture files named final, latest or handover.

For a new feature or a semantic change, its task must state the problem, current evidence, accepted contract, design choice, alternatives rejected, exact scope, dependencies, test fixtures, acceptance and failure behavior. If an interface or engine assumption is unknown, resolve it with a bounded investigation first. Never fill a specification gap by claiming the current code must be correct.
