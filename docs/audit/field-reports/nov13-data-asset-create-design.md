# NOV13-07 typed Data Asset creation

Readiness: ready. Integrator-approved assignment: nov13-data-asset-create. Claim the full commit containing this scope before implementation.

## Behavior
Add PreviewTurboMCPDataAssetCreate and EditTurboMCPDataAssetCreate. Inputs: exact class path, new package path, optional initial-fields JSON, explicit Save=false and AddToSourceControl=false. Creation means a fresh class-default instance, never duplication or silent clearing of class defaults. The primary field need is seven empty typed instances followed by existing map edits.

Preview validates destination, concrete UDataAsset subclass, editable initial properties and persistence prerequisites without creating packages, invoking constructors, saving or contacting source control for writes. Native exact class paths and already loaded Blueprint-generated subclasses must be supported; unresolved/unsafe class loading must be explained, never silently substituted. Refuse abstract/deprecated/superseded classes and invalid or occupied destinations in memory, registry or disk.

Reuse TurboMCPDataAssetValues for validated scalar/value-struct initial values and exact loaded reference handling. Reject unsupported containers/delegates/localized text explicitly; existing property tools perform subsequent supported map/array edits. Validate every initial field before constructing anything. Revalidate destination and class before applying. Preserve stock engine; no project code sandbox claim.

Create under a fresh package, apply fields, verify actual class and readback, notify Asset Registry and mark dirty. Return exact object path, current reader-compatible revision and explicit memory/save/source-control outcomes. Never claim no change after allocation or callback failure; identify recoverable partial state and prohibit blind retry. Do not compile unrelated Blueprints.

Save is opt-in. AddToSourceControl requires Save and a successful verified file write; never auto-checkout or submit. Provider absent/failure must produce an honest partial result while preserving the created asset and disk evidence. A refused preflight must create nothing. No overwrite or destructive rollback of existing assets.

## Implementation preparation
Inspect asset factory/NewObject lifecycle, transaction support for creation, existing explicit save implementation and source-control MarkForAdd APIs on stock 5.8.4. Resolve undo semantics before declaring ready. Expose reusable reader revision calculation rather than duplicating its hash logic. Tool schema, parameter help and English results are part of the implementation.

Proposed scope: new TurboMCPDataAssetCreate.cpp and focused fixture/test wrapper; existing DataAssetTools.cpp/private shared revision header, Toolsets.h, AgentHelp.cpp. Integrator registers the catalog and refreshes inventory separately after reviewed scope. Do not create the feature under this design-only document.

## Minimal acceptance
One build and one synthetic workflow: concrete native data asset creation, optional scalar initialization/readback/revision, occupied-path refusal, invalid class/property/value refusal without package creation, class defaults preserved, no implicit disk save; one explicit save/add path with source-control unavailable clearly reported. Blueprint-derived class behavior must be checked if claimed supported. Record persistence and undo limits separately. Do not call implementation complete while source-control add or revision return is omitted.

## Stock 5.8.4 lifecycle findings (November 13)
- `UObjectGlobals.cpp`, StaticConstructObject_Internal, lines 4856-4868: with an active transaction and RF_Transactional, new non-UField objects are recorded in their garbage state before being restored live. Undo therefore has a native object-creation record. RF_Transactional alone, outside an active transaction, is insufficient.
- `AssetTools.cpp`, CreateAsset, lines 1779-1803: stock creation uses Public, Standalone and Transactional flags, then notifies Asset Registry and marks the package dirty. This is lifecycle evidence, not proof that registry and dirty-state restoration meet our undo contract.
- Implementation must begin the scoped transaction before allocation, and the focused workflow must check undo/redo object validity, registry visibility and package dirty state. Do not claim complete undo from object validity alone. Saving remains outside the memory transaction and cannot be undone on disk.
- The existing FunctionLibraryCreate helper has no scoped transaction; do not copy its lifecycle as evidence of creation undo support.
- Reader revision currently comes from DataAssetTools.cpp FSnapshot: sorted owned objects, class/path and exported non-transient properties hashed as UTF-8 MD5. Extract/reuse this calculation so creation returns the same revision the reader subsequently reports.
- The existing explicit save implementation checks both engine return and dirty state. New creation additionally needs file evidence; a successful allocation or AssetCreated notification is not evidence of persistence.

Remaining preparation: establish registry undo behavior and exact source-control add prerequisites, then approve an exact implementation/test scope. These findings do not close NOV13-07 or authorize a claim without that scope.

## Implementation decision
Use stock scoped transactional creation with RF_Transactional. The first focused workflow must prove undo/redo and registry visibility before accepting the feature. If registry restoration fails, repair it within the assigned creation implementation rather than dropping the undo check. This is a verification requirement, not a reason to defer implementation. Source-control add requires an enabled, available provider after successful save, followed by state readback; unavailable or failed add is an explicit partial result. Add never implies checkout or submission. The wrapper and native test are explicitly included in this assignment; catalog registration remains integrator-owned.
