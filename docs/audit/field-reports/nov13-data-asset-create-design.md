# NOV13-07 typed Data Asset creation

Design reviewed; implementation scope and claim still required.

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
