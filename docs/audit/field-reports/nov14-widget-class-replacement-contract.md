# NOV05-16: placed widget class replacement

Readiness: design review; no implementation assignment yet.
Owner: integration owner. DSH paused; stock Unreal 5.8.4.

## Required outcome

Replace one named designer widget while retaining its name, parent slot, sibling
index, variable flag, compatible authored properties, and supported references.
A subtree rewrite or remove/add sequence does not satisfy this requirement.
Expose PreviewTurboMCPWidgetClassReplace and EditTurboMCPWidgetClassReplace.
Inputs: asset, widget name, replacement class path; edit additionally requires
the preview confirmation. No automatic save, checkout or full compilation.

## Preview and compatibility

Resolve both classes and the exact tree instance. Reject abstract, deprecated,
non-widget, circular, unavailable or ambiguous classes. Check parent BindWidget
requirements and Blueprint inheritance constraints. Report the old/new class,
parent and sibling index, exact slot type, children and named-slot contents.

Compare authored properties against defaults, using reflected type identity
and destination constraints, not matching names alone. Report each copied
property and each incompatible authored override. Never silently discard an
override. Object references must remain within their declared type constraints;
instanced subobjects require explicit ownership-preserving duplication.

Enumerate member getter/setter pins, bound events, designer bindings, navigation
targets, desired focus and animation bindings. Validate each against the new
class before mutation. Do not reconstruct graph nodes merely to suppress type
errors; preserve node/pin IDs and links or report the specific incompatibility.
A UButton and CommonButtonBase are not interchangeable runtime interfaces.

Check panel capacity before transferring children. Preserve child order and
all authored child-slot properties. A container change with incompatible slot
semantics requires an explicit mapping; unsupported mappings refuse with the
exact property or child involved. ListView entries are data-driven runtime
instances, not ordinary designer children. Never claim entry migration from
a successful designer-tree replacement.

## Apply and rollback

Bind confirmation to asset revision, old instance, class, tree topology, slot,
authored properties and reference inventory. Reject stale previews. Stage the
replacement off-tree, verify copied values, then use one transaction covering
tree, parent slot, replacement, old widget, children and any changed references.
Preserve the original slot object where supported; otherwise exact slot-value
and index equality is mandatory. Check every mutation result. On any failure
restore identity, structure, properties, references and prior dirty state; do
not clear dirty state unless restoration is proven. Retain the old object for
undo. Inspect structural-modification callbacks before adopting an engine
helper; full compile remains a separate explicit operation.

Return changed/unchanged, replacement identities, preserved items, changed
references, dirty-package effects and separate-compile guidance in plain English.
Do not report success if postconditions or rollback verification fail.

## Engine source findings

WidgetBlueprintOperationUtils.cpp:1072 ReplaceWidgetsWithTemplateClass creates
the replacement before all topology checks finish, imports properties as text,
and allows incompatible child-slot properties to be silently dropped. Its
child-transfer loop does not preflight destination capacity. It structurally
modifies the Blueprint and replaces variable references at the end. Directly
wrapping it would not meet the contract above. No engine edits are authorized.

## Focused acceptance

Use one synthetic widget fixture: compatible leaf replacement under a panel,
non-default parent slot and siblings, compatible member reference, plus one
unsupported event/property conversion. Verify preview immutability, apply,
exact preserved values and identities, separate compile, and undo before
compile. Add one multi-child to single-child refusal and a stale confirmation
refusal, both without mutation. Reuse the fixture for a CommonUI example if
its actual class API permits the requested conversion. Do not present a generic
leaf test as proof of CommonUI or ListView migration.

## Pending product distinction

The user has been asked whether ListView-to-DynamicEntryBox includes migrating
data population/selection logic or only designer replacement with an explicit
remaining-work report. Preserve the full requested feature as open until this
distinction and supported reference mappings are resolved.
