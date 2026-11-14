# Exhaustive read boundary (NOV10-14)

Decision: TurboMCP does not expose a general-purpose "read-only script" mode. The stock `ProgrammaticToolset.execute_tool_script` route remains filtered while TurboMCP is enabled. A flag or text filter cannot establish that arbitrary script code will avoid writes, map loads, native load callbacks or other effects. This decision does not claim to intercept direct Python or other editor execution paths.

The November 10 field report needed bulk reference, widget and node inspection without hundreds of client round trips. The current supported routes are purpose-built and bounded:

- `ReadTurboMCPRefsBatch` traverses saved Asset Registry edges with kinds, page cursors and explicit registry coverage limits. Config, dynamic and unsaved references are outside that evidence.
- `ReadTurboMCPWidgetInventory` pages registry-selected Widget Blueprints and reports matched rules and inspected instances. Loading a candidate can invoke native load hooks; skipped assets are not negative matches.
- `ReadTurboMCPNodeFind` supports folder/project scope, OR matching and count-only pages. Its scanned/skipped/finished fields must be checked before treating an empty result as exhaustive. NOV13-03 remains open for unloaded map and redirector-dependent coverage; its map-load guard stays in place.
- Paged `ReadTurboMCPDataAssetProperty` reads batches of named properties or collection keys, within its asset and property limits. Blueprint defaults require an already loaded generated class.

These readers answer the reported bulk queries within their stated scopes; they do not make every project query exhaustive. When a needed query is still missing, record its exact assets, output shape, coverage boundary and side effects as a separate bounded reader task. Do not widen script execution merely because a caller labels code "read-only." No engine change, new tool or runtime test is claimed by this design decision.

Source checked on private main `c1ab6fa0`: `TurboMCPModule.cpp` installs the exact stock-script filter; `TurboMCPToolsets.cpp` discloses the filter and its direct-Python limit; `Public/Tools/TurboMCPToolsets.h` declares the four bounded reader contracts. The NOV10-04/06/07/08 integration records provide their focused evidence. NOV13-03 retains the unresolved coverage case.
