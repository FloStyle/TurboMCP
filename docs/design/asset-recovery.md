# Confirmed package recovery

Approved contract for OCT27-15. Extend the existing AssetRevert preview/edit pair to saved UDataAsset instances while preserving Blueprint support. Unreal stays stock. Undo remains a separate transaction operation.

Support one top-level Blueprint or data asset in its saved package. Refuse unsaved packages, unsupported assets, packages containing multiple top-level assets, PIE, active transactions, an open asset editor and detected external live-world references. Do not close other editors or promise raw native references can be discovered. Ordinary persistent references can be reinstanced by stock reload; report that effect.

Preview lists the exact asset/package, dirty state, disk file and the loss of all unsaved package changes. Bind its one-use, 60-second confirmation to the complete loaded package object state and disk content hash. Re-preview replaces earlier approval. Empty, wrong, expired or stale confirmation refuses without reload. Consume valid confirmation before attempting reload. Never save as part of recovery.

Reload through stock UPackageTools. Do not keep using pre-reload object pointers. Resolve the asset afresh and report observed class, dirty state and disk hash after reload. A failed/partial reload or unexpected verification result must not claim that nothing changed. Report unknown state and require inspection. Warn that global undo history may be reset and references/identities must be reread. PostLoad callbacks are not a guarantee of arbitrary project semantics.

Evidence: create and save an owned data-asset fixture; change it in memory; preview loss; refuse empty/wrong/stale confirmation; fresh approval restores the saved field without restarting or rewriting disk. Repeat token refuses. Verify dirty flag, disk hash, fresh identity and undo side-effect reporting. Exercise unsafe-world/open-editor/PIE prerequisites as feasible, and source-review unsupported reference limits. No workplace files, external engine changes or broad all-asset recovery guarantee.
