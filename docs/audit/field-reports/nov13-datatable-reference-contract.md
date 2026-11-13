# NOV09-16: DataTable reference evidence

Readiness: ready. Integration owner: integrator.

## Source reconciliation
At ed5adc65, reader discovery uses saved registry referencers then checks only UK2Node_GetDataTableRow in loaded Blueprints. DT-16 remains reproducible by source: a custom library call with the table literal is ignored. DT-18 does not establish a table-specific dependency through a shared struct; do not fabricate that relationship.

## Repair
Inspect supported literal input pin defaults on loaded Blueprint graphs: direct DefaultObject and exact exported object/package paths, including split pins. Preserve known GetDataTableRow classification as a row-read site. Other literal matches are references/candidates, not proof of read behavior. Report graph and node identity for evidence. Retain saved-registry provenance, distinguish unloaded/unsearched candidates, and explain that dynamic values and row-struct flow are not resolved. Include loaded unsaved Blueprint literal references rather than relying exclusively on saved registry edges. Do not load additional referencer assets, compile, save or edit them. Correct searched counts independently of matched graphs.

Keep bounded output and explicit omissions. Update tool help and comments in clear English. Reuse the DataTable synthetic suite with a literal reference node, a true row read, a different-table control and no dirty-state changes. Batch implementation before one focused build/test.

## Scope
TurboMCPDataTable.cpp, Public/Tools/TurboMCPToolsets.h, TurboMCPDataTableQualityTest.cpp. No other implementation files.
