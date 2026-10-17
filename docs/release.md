# Source publication and plugin releases

The development repository is private until publication is explicitly authorized. The public source tree is a complete 1:1 copy of a reviewed tracked tree. Tasks, specifications, tests, tools and contributor instructions are part of that public source repository. A plugin install package is a different artifact containing the installable plugin and validated binaries for an exact engine distribution.

## Preview

Stage the candidate changes, then run `python Tools/export-public.py --preview --output .local/source-preview`. The exporter reads staged Git blobs, preserves paths and bytes, and refuses unstaged tracked edits, symlinks, unresolved merges, prohibited tracked artifacts and sensitive-data patterns. Ignored/untracked local files and .git are not source. No tracked file is silently omitted. Review metadata is written beside the directory as source-preview.manifest.json, so it cannot change the copied tree.

A preview does not certify release readiness or authorize publication. Verify the manifest against the staged tree and run offline validation from the exported tree after initializing a temporary Git repository. Inspect binary fixtures and third-party ownership separately; text pattern checks are not a complete privacy or licensing audit.

## Release

A release requires a clean committed tree, readiness and owner approval, validated plugin identity and complete acceptance evidence. See [release manager](release-manager.md). Initialize a new Git repository in the approved complete export. Verify its root commit has no private ancestors and its source manifest matches the reviewed candidate. Push only after the owner authorizes publication. Never mirror or merge private history.

Do not put development credentials, private recovery archives or personal machine configuration into tracked files. If something cannot be public, keep it outside this source tree rather than adding a silent export exception. Implementation credentials should not grant publication rights.

Binary releases require separate exact engine/platform/build identity, package hashes and clean-install acceptance. See [installed-engine support](installed-engine.md). Current development remains experimental; the export mechanism does not certify v1.
