# Collapse to function: OCT27-18

Extend the existing Preview/EditTurboMCPGraphCollapse pair. Keep exact asset, graph and node selection, proposed member name and function member kind. Do not create duplicate tools. Existing copy behavior must remain intact.

Preview must not create a graph, change flags, reconstruct nodes or modify links. Stock FBlueprintEditor::CanCollapseSelectionToFunction creates a temporary function on the owner, so it is not a read-only preview API. Derive the supported boundary plan from the current graph instead. Edit repeats validation before mutation.

Support the reported small string-processing chain with external input sources and output consumers, and an eligible single-entry/single-continuation execution selection. Describe the proposed function signature, exact moved nodes, replacement call and boundary reconnections. Pure selections must remain usable by their existing data consumers without introducing an unexecuted call. Reject latent nodes, event/entry/result nodes, unsupported local-variable captures, ambiguous IDs, mixed execution entries, unsupported boundary types and conflicting names before mutation. Do not infer semantic safety solely from successful compilation.

Preserve internal values/links, external consumer/source relationships, unrelated nodes and graph layout. Move selected comments with their contents; do not move unrelated contents of a partially selected comment silently. Place the replacement within the old selection region without overlaps, and keep the new function readable. Preserve intended function evaluation semantics and report unsupported cases explicitly. Use stock editor APIs where suitable; no engine edits.

Verify a three-node string chain through actual public tools, with input/output boundaries and independent runtime results before/after. Include connected execution flow, refusal state, comments, position preservation, compile, live help and inspected screenshots. Any partial mutation or failed restoration must be reported honestly. No automatic save. Reused tools do not increase unique coverage. Integrator owns suite registration, evidence and dashboard integration.
