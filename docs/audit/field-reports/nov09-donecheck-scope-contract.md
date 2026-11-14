# NOV09-06: scoped DoneCheck capture

Current review: CP-17 reports a 421-node graph where the text findings were returned but full-graph capture exceeded native zoom. Current `LookTurboMCPLayoutDoneCheck` always calls `LookTurboMCPCapture` in `graph` mode, while `LookTurboMCPLayoutReport` already accepts node, comment-box and region scopes. The field project is unavailable; the reported capture failure is credible from this source path.

## Repair

Add an optional `Scope` argument to DoneCheck, using the existing layout-report syntax. Empty scope keeps full-graph behavior. A node scope captures that node, a region scope captures that graph-space rectangle, and a comment-box scope captures its resolved rectangle as a region. Resolve the same scope for report and capture; reject an invalid or ambiguous scope before capture. Keep existing report paging defaults. Do not silently fall back to a full graph, automatically approve layout, edit the graph, compile or save. State the capture mode/target and retain any capture error beside the successful text report.

## Scope and verification

The exact schema-2 assignment `nov09-donecheck-scope` owns `TurboMCPLayout.cpp`, `TurboMCPReportScope.h`, the toolset declaration/help, and `Tools/test-layout-output.py`. Use the disposable Blueprint fixture for a focused node/region scope and invalid-scope refusal check, plus one supervised 5.8.4 build. Do not rerun the entire graph suite. A large 421-node field screenshot remains unverified and the native zoom limit is a stated limitation.
