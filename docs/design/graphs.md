# Graph identity, text and editing

## Source boundaries

[TurboMCPNodeId.cpp](../../Plugins/TurboMCP/Source/TurboMCP/TurboMCPNodeId.cpp) owns graph/node identity. TurboMCPGraphText.cpp and TurboMCPGraphTextLex.cpp handle representation. GraphAppend, GraphPatch, GraphReplace, GraphRepoint and GraphSwapCall implement distinct operations. GraphReachability, GraphDead, Layout and GraphTidy provide analysis/layout behavior. Keep identity and grammar rules shared across wrappers.

## Identity and representation

A write must name one graph and resolve one target. An empty graph name on a writer refuses with graph_name_required. An empty name on an asset-wide reader explicitly visits every supported graph, labels the graph sections and reports omissions. Never select the first graph as a convenience. Short keys require uniqueness checks; a prefix collision is an ambiguity, not a first-match choice.

A readable summary is not lossless editing input. Reports must retain every relevant node and both endpoints of each link, including multiple links on one pin, or declare incompleteness. Unsupported node families get explicit representation/refusal rather than disappearing. Canonical reader/writer grammar must define quoting, nested pure nodes, pin identity, defaults, type identifiers and unsupported forms before destructive round trips are accepted.

## Editing and layout

Create and validate nodes before wiring. Resolve type/wildcard constraints and check the resulting pin shape; compile and independently verify before reporting success. Replacement must retain the exact connections/properties specified by its contract, including exec/data links and dependent pure nodes. Use the common mutation lifecycle.

Layout uses actual geometry where required, or explicitly identifies estimates and refuses unsafe edits unless the contract allows an override. Cycle analysis follows graph edges; variable read/write relationships do not invent wires. Tidy must rerun its report and identify unresolved findings. Capture must show the nodes it claims to frame.

## Verification

Fixtures need multiple graphs, ambiguous short IDs, nested/shared pure nodes, multiple link sources, unsupported nodes, wildcard types, exec/data branches and unopened graph geometry. Test named and empty graph behavior independently. Existing test-graph-* suites are starting points; map exact cases before treating a suite name as acceptance evidence.
