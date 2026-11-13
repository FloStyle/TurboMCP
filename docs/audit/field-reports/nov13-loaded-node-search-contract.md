# Loaded Blueprint search coverage

Readiness: ready. Integration owner: integrator. Partial repair of NOV13-03; do not close the parent coverage issue.

Current review: NodeFind candidates come exclusively from the registry and the dependency guard runs before any loaded-object lookup. WidgetInventory already uses FastGetAsset(false). GraphInventory's unsaved fixture is not registered and therefore cannot be searched. Preserve this existing test expectation; extend evidence to folder scope and check dirty state.

Union already loaded, valid asset Blueprints with registry candidates for each selected scope. Respect folder recursion, project content mounts and exact asset identity. Deduplicate and sort before computing the cursor fingerprint. Do not include objects needing load or PostLoad; do not execute compilation or registration to make them searchable. Use already loaded instances directly. Keep the existing dependency preflight and load flags for every unloaded candidate. Avoid retaining raw pointers across calls. Preserve all current search matching, paging and output contracts.

Scope: TurboMCPNodeResolve.cpp and TurboMCPGraphInventoryTest.cpp. One build and focused GraphAppend.Inventory automation evidence after implementation. No full append regression needed. Unloaded map/redirector coverage remains open: a successful single-asset load is not evidence of no map dependency loading. Deferred-load flags are not a safe substitute for fully resolved graph reads.
