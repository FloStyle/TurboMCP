# November 14 Tidy handoff classification slice

This is a bounded repair under NOV05-03. The accepted loop-cascade layout and its full seven-score workflow remain separate acceptance evidence.

The layout reader may call a loop Completed or branch False edge an intended R3/R6 handoff only when the control output, unambiguous reroute chain, peer lane, and edge geometry support that rule. A reroute alone does not excuse a diagonal edge. The current edge must be approximately horizontal or vertical in the reader's estimated pin model. A direct diagonal remains a defect candidate even when the target is below the peer lane. A segment crossing an unrelated comment box also remains a defect candidate.

Add one synthetic native fixture that compares an aligned routed handoff with the same knot moved sideways into a diagonal. Check reported classification for both, without saving, compiling a Blueprint, loading a map, or claiming pixel-exact geometry. Use one focused wrapper invocation and a stock 5.8.4 build. Keep prior cascade results and visual limitations in the task record; this slice cannot close NOV05-02/03 alone.

Approved R04 scope: TurboMCPLayoutReport.cpp, TurboMCPTidyFeedbackTest.cpp, Tools/test-layout.py. Integrator owns this contract, the assignment manifest, generated inventory and task status.
