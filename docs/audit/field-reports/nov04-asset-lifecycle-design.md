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
