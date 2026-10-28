# Safe data-asset property edits

Approved integration contract for OCT27-14, X-113, X-116 and FT-130. Unreal remains stock. This document specifies required work; it is not implementation or test acceptance.

## Public workflow

Provide Read/Preview/EditTurboMCPDataAssetProperty. Read discovers editable reflected fields and prints the exact input format, current value, class, ownership and state revision. Preview validates a single explicit operation. Edit requires the reader/preview revision and rebuilds the plan against current state. Never send a whole live map to the stock JSON writer.

The target is an existing UDataAsset, including InputAction and project subclasses. Identify the asset by exact resolved path. A property path may traverse structs and existing instanced children owned by this asset, but must never traverse an external referenced asset. Refuse PIE, active transactions, read-only/transient fields, unsupported property kinds, ambiguous targets and stale revisions before mutation.

Required operations:

- set: set a supported scalar/value-struct field using the Unreal property text printed by the reader. Consume the entire input; refuse warnings, unresolved references, non-finite numbers and trailing text.
- map_upsert: add or replace one explicitly identified map entry. Parse the key and value separately into initialized temporary property storage. Object keys use exact resolved paths and preserve external identity; never treat keys as instanced children because the map value is instanced. Reject invalid/null keys and duplicate or ambiguous identities.
- map_remove: remove only the exact requested key. Preview displays the removed entry; absence is an explicit no-op.
- create_child: create an instanced object for a compatible reflected property, or an explicitly selected map value. Require a concrete compatible EditInlineNew class and a currently empty destination. Do not silently replace an existing child. Optional initial fields use the same temporary-validation rules. Existing owned children can be edited through their explicit property path.

Help must distinguish property text from stock JSON, include copyable examples and explain every parameter, revision requirement, supported operation, refusal, side effect and next step. Errors must identify the failed property/operation without claiming nothing changed after an unverified rollback.

## Memory and ownership

Use initialized temporary FProperty storage for value parsing and a scoped transient owner for proposed child construction. Keep temporary objects strongly referenced through planning. Never import into live property memory. Do not reuse the stock bypass-container writer. Avoid broad duplication of external assets or copying temporary child references into the final asset.

Capture the selected root property and all affected owned descendants before application, plus the package dirty flag. Revision must include affected descendant values and identities, not only the pointer in the root property. Preview constructs only temporary state, does not call Modify or edit notifications on the live asset, and leaves its values, child identities and dirty flag unchanged. Preview may run a selected class constructor on temporary state; document that explicitly.

At application, create any new child with its final correct owning outer, copy validated values, then install only the requested map entry/property under one owned transaction. Preserve unrelated entries and existing child identities. Mark the asset modified and send the appropriate property notification only after validation. Notifications can execute project code: compare actual resulting values/ownership with the plan afterward. On mismatch, restore the captured property/affected descendants and verify restoration. If callbacks change unrelated state, report the uncertainty/partial effect; do not certify arbitrary project callbacks as atomic.

Do not save, compile, run project gameplay or reload packages as implicit side effects. Keep undo and disk recovery separate (OCT27-15). Return the actual before/after values, changed property/key, created object path, final dirty state, verification result and separate save state, using the existing result formatter.

## Required evidence

Keep the intentionally failing stock reproduction separate from plugin regression acceptance. The stock evidence is run 88c27e3e88834583af0e4d8420911842, fixture f2b1ccfc1633f5e3188a986919fe968d9b093d98. It shows bypass-mode key corruption; non-bypass mode preserves the concrete-key fixture. Class-path instancing succeeds, so the exact historical false-success cause remains unknown.

Exercise public read/preview/edit on owned fixtures: preserve two existing map entries while adding a third; invalid second input refuses without changes; remove an exact entry; stable external key identity; new child correct class/outer/fields; edit existing child without replacing identity; no-op; stale revision; missing/incompatible/abstract class; invalid reference; dirty and clean baselines; full readback and undo. Add an InputAction key-settings case rather than relying only on a synthetic class. Capture output/help and verify clear English. Rejected input must preserve values, identities and dirty state. Test notification mismatch and verified rollback independently. No broad release certification follows from these fixtures.
