# NOV04-40: asset lifecycle design

Status: source review and proposed contract; not implemented or validated.

The report requests Blueprint function-library creation and asset deletion after a referencer check. These are separate capabilities. Neither is a fix claim for the unidentified NOV04-01 deletion crash.

## Function library

Use an explicit destination package and asset name. Preview must refuse an existing destination. Create a real function-library Blueprint with the stock function-library parent/type, rather than duplicating and emptying another asset. Verify the resulting Blueprint type, generated class and absence of copied graph content. Report the new asset path and dirty/save state. Creation must not silently overwrite or save unrelated assets. The exact factory route still needs source review and a focused fixture.

## Deletion

Require a separate preview, an exact asset identity and explicit confirmation tied to fresh preview evidence. Refuse stale evidence. Report irreversible disk effects and source-control expectations; do not promise editor undo restores a deleted file.

Check both registry dependencies and live references. Registry referencers alone do not establish that loaded objects, open editors or undo records are safe. Recheck immediately before mutation. Reject referenced or unsupported assets rather than replacing references or forcing deletion. Never invoke DeleteObjectsUnchecked or ForceDeleteObjects as a shortcut. Do not delete user assets during validation.

Stock source reviewed: UnrealEd/Public/ObjectTools.h documents DeleteObjectsUnchecked as bypassing use checks and not intended for direct calls; DeleteAssets exposes the checked deletion system. AssetRegistry/IAssetRegistry.h exposes dependency referencer queries, not a complete live-reference safety proof. The internal checked deletion route and its interaction with editor state still require review before implementation approval.

Required isolated evidence: unreferenced disposable asset; hard/soft/live referenced refusal; dirty/open asset handling; stale preview; source-control or read-only failure; exact post-deletion registry/file state; unaffected neighboring assets. Preserve failed evidence. A successful synthetic deletion cannot explain the original crash without its exact call and stack.

## Checked route findings

Stock ObjectTools.cpp constructs FAssetDeleteModel and ticks its scan before calling DoDelete in the noninteractive route. AssetDeleteModel.cpp CanDelete refuses when non-undo live references or on-disk references exist; DoDelete calls the unchecked primitive only after that check. Undo-only references do not block CanDelete, so undo-history effects must be examined and disclosed before exposing this route. A plugin must use the checked model/system rather than calling its internal unchecked primitive directly. No execution or safety certification follows from this source review.

UBlueprintFactory exposes ParentClass, BlueprintType and bSkipClassPicker. This provides a candidate stock creation route; the implementation still needs inspection for function-library restrictions and compiler behavior.

## Stock scripting deletion route

Source inspection on 2026-11-05: stock EditorToolset asset.py delete calls EditorAssetSubsystem.delete_asset (or delete_directory). EditorAssetSubsystem.cpp DeleteAsset, lines 795-814, loads the target and calls ObjectTools::ForceDeleteObjects with bShowConfirmation=false. DeleteLoadedAsset(s) also use force deletion. These scripting convenience methods are therefore not substitutes for the checked deletion model required above. This source fact does not identify the original field tool, reproduce its crash, or prove a crash cause; NOV04-01 still requires the original call and stack. No deletion was executed during this inspection.
