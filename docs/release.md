# Private development and public release

TurboMCP-Development is the development repository. The clean migration branch retains private history. TurboMCP_ is the temporary public destination; it is not a development branch in the private repository. The existing TurboMCP repository is retained until replacement is deliberately performed.

## Export boundary

release/export-policy.json is an explicit allowlist. The exporter copies plugin source/content/resources, the descriptor, selected public documentation and license files. It excludes tasks, internal architecture, test fixtures, engine source, local configuration, credentials, generated binaries and all .git data. Only tracked files are eligible. Each exported file is hashed.

`python Tools/export-public.py --preview --output .local/public-preview` creates a review-only tree even while development is unfinished. Output must not exist; exports never overwrite a checkout. A preview is not publication approval and is labeled accordingly.

Without --preview, export requires a clean working tree, release/status.json readiness = ready, public_export_approved = true, and validated_tree matching HEAD:Plugins/TurboMCP. Record the actual acceptance commands and results before changing the gate. These are workflow checks, not an authorization boundary against an agent with write credentials.

## Publication

Use a separate release checkout and a separate credential scoped to the public repository. Implementation agents should not receive a credential capable of publishing there. A missing remote, push hook, or disabled push URL helps prevent mistakes but can be bypassed by an agent that has broad GitHub credentials.

Initialize clean public history from an approved export. Never merge, cherry-pick, or mirror private development history into the public repository. Verify the staged file manifest, public links, secret scan, install/build behavior and source license notices. Publish only after the owner authorizes that release. Binaries belong in versioned release assets/CI artifacts, not the source Git tree.

Keep the old repository until code, tags, releases, issues and other metadata needed by the owner have been preserved. Renaming/replacing it is a separate final cutover; this migration does not delete or rename GitHub repositories or change their visibility.

The initial migrated plugin remains experimental and NOT_READY. The public export mechanism is ready for review; the product acceptance work remains in TODO.md.

See [release manager](release-manager.md) for evidence requirements and [installed engines](installed-engine.md) for packaging support.
