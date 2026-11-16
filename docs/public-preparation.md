# Public source preparation

[Release guide](release.md) · [Tracked checklist](../release/public-preparation.json) · [External agent workflow](external-agent-workflow.md)

## Separate preparation from development

Continue implementation in private main. Prepare candidates in a dedicated local folder outside the private checkout. Keep the private repository and its existing history private. A branch, worktree or mirror retains private ancestry. Do not copy the working directory or its Git metadata.

Recommended local layout (choose the root on your machine):

- TurboMCP: private development checkout.
- TurboMCP-public-prep/candidates/<source-sha>-<attempt>: immutable preview exports.
- TurboMCP-public-prep/evidence/<source-sha>-<attempt>: manifests, logs, scan results and source/tree identities, outside candidate source.
- TurboMCP-public: eventual independent public checkout, initialized from the approved export.

The preparation folder has no public remote. A preview is for local review. Repair source issues in private main and generate a new candidate. Keep prior reviewed candidates and their evidence intact. A failed write may leave partial output; treat it as a failed attempt, and retry with a new path.

## Export and verify a snapshot

1. Select a clean, committed private main revision. Record its full commit and tree IDs outside the candidate. Check ignored and untracked material for local dependencies separately; only tracked Git blobs are copied.
2. Run the exporter from that checkout, using a new output location:

   ```powershell
   python Tools/export-public.py --preview --output '<new-candidate-directory>'
   ```

   Preview freezes one staged Git tree and refuses unstaged tracked edits. For publication review, run it from the clean commit so the tree in the manifest matches that commit. A staged preview from a dirty checkout can have a different tree ID; it cannot stand in for the final committed candidate. Non-preview mode requires a clean committed source, recorded owner approval, the validated plugin tree and intact acceptance evidence. It copies the verified commit's tree.
3. Verify the complete path set, file sizes and SHA-256 hashes against both the sidecar manifest and the named source tree. The sidecar records the source commit at export start, the actual exported tree ID and Git file modes; keep it outside the source directory. For an older manifest without those fields, establish its source identity separately. Resolve a detected unsafe tracked path or file in the source or through an approved policy change; never filter it out of the export to get a pass.
4. Review tracked text, field reports and every binary fixture for private data and unwanted generated content. Check ownership and notices separately. For example, visual inspection is needed for images and editor inspection for serialized Unreal packages. The icon and trap-content provenance remain unresolved. The exporter's limited pattern checks do not establish privacy or permission to redistribute.
5. In a disposable copy of the verified export, initialize a fresh Git repository with the same object format as the source. Use git init, without cloning, using a private repository as a template or sharing an object store. Check the complete staged tree with git write-tree before committing. On Windows, apply any 100755 modes from the manifest to the index with git update-index --chmod=+x; a file copy does not set Git's executable bit there. Stop if staging omitted an ignored path or normalized any bytes: the staged tree ID must match source_tree. Review the public commit identity and message before creating the root.
6. Verify git rev-list --parents -n 1 HEAD prints only the new commit, git rev-list --all --count reports one, and git remote -v reports no remotes in the disposable validation repository. Verify there are no object alternates or private refs. Run offline checks and confirm no dependency on local caches, private reports or machine paths.
7. Build the distributable package for the exact supported engine and execute mapped acceptance in clean consumer projects against that artifact. Preserve failed attempts, limitations, source and engine identities, and artifact hashes. Complete release metadata and destination security checks. Any source change invalidates the candidate identity: export a new snapshot and refresh affected evidence.
8. Use the [release manager checklist](release-manager.md) for evidence and non-preview export. Only after explicit owner publication approval, initialize the independent public repository from the approved complete snapshot and recheck ancestry and manifest before pushing.

For later releases, use a clean checkout of the independent public repository and transfer the full reviewed source snapshot, including updates and tracked deletions, as content. Verify its tree and commit on top of public history. Keep the private commit to public commit mapping and audit evidence outside the public tree.

## Tracking rules

release/public-preparation.json tracks release problems and deliverables. Implementation state and verification are separate: an implemented task cannot satisfy a verification gate without evidence. Tool review coverage remains authoritative in docs/audit/workflow-tool-coverage.json; acceptance remains in Tests/acceptance.json. Do not duplicate tool completion counts here as an independently maintained metric.

No candidate has been approved by this checklist. Publication remains disabled in release/status.json. Local caches can accelerate development, but clean-install acceptance must not depend on them.
