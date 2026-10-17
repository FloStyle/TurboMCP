# Release guide

[Documentation](README.md) · [Project status](status.md)

## Two release artifacts

| Artifact | Contents |
| --- | --- |
| Public source repository | Every tracked file, with identical paths and bytes, in fresh Git history |
| Plugin installation package | Installable plugin and validated binaries for an exact engine distribution |

Tasks, specifications, tests and contributor tools belong in the public source tree. Unreal source, credentials, personal configuration and private recovery archives do not belong in either artifact.

## Preview the source tree

Stage the candidate changes, then run:

```powershell
python Tools/export-public.py --preview --output .local/source-preview
```

Choose a new output directory for each preview. The exporter reads staged Git blobs and refuses unstaged tracked edits, unresolved merges, symlinks, prohibited artifacts and sensitive-data patterns. It never silently omits a tracked file. Ignored files, untracked files and `.git` are excluded because they are not part of the tracked tree.

The manifest is written beside the export as `source-preview.manifest.json`. Verify it against the staged tree. Initialize a temporary Git repository in the export and run offline validation there. Review binary fixtures and third-party ownership separately: text scanning cannot establish their privacy or licensing status.

A preview is a review artifact. It does not approve or publish a release.

## Approve and publish

Follow the [release manager checklist](release-manager.md). Release requires a clean committed tree, complete acceptance evidence, validated plugin identity, readiness approval and the owner's explicit publication instruction.

Create a new Git repository from the approved complete export. Check that its root commit has no private ancestors and that its manifest matches the reviewed candidate. Never mirror the private repository or merge its history into the public one.

Keep implementation credentials separate from publication rights. A development push must not publish a release automatically.

## Build installation packages

Record the exact engine, platform and build identity, package hashes, and clean-install acceptance results. Follow the [installed-engine guide](installed-engine.md). Source export validation alone does not establish that a binary package works.
