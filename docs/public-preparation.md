# Public source preparation

[Release guide](release.md) | [Tracked checklist](../release/public-preparation.json)

## Separate preparation from development

Continue implementation in private main. Prepare public candidates in a dedicated sibling folder, not a Git worktree or branch of the private repository. A private branch retains private ancestry. Do not copy the working directory or its .git folder.

Recommended local layout (machine-specific root chosen by the integrator):

- TurboMCP: private development checkout.
- TurboMCP-public-prep/candidates/<source-sha>-<attempt>: immutable preview exports.
- TurboMCP-public-prep/evidence/<source-sha>-<attempt>: manifests, logs, scan results and source/tree identities, outside candidate source.
- TurboMCP-public: eventual independent public checkout, initialized only from the approved export.

The preparation folder is local and has no public remote. Creating it or a preview is not publication approval. Do not keep a second implementation branch there: repair issues in private main and regenerate a new candidate. Never overwrite a reviewed candidate.

## Repeatable pipeline

1. Select a clean committed private main revision. Record its full commit and tree IDs and the exporter revision outside the candidate. Check ignored and untracked material separately for local dependencies; do not copy it.
2. Run Tools/export-public.py --preview --output <new-candidate-directory>. The existing exporter copies every tracked file unchanged and refuses detected unsafe content. Never silently filter tracked files to make the scan pass: fix the source or explicitly revise the export policy first.
3. Verify every candidate path and hash against the sidecar manifest and source tree. Record failures without exposing matching secret text. Review scanner coverage and manual findings; a passing regex scan is not a safety certificate.
4. Review all text and binary assets for private data, ownership, licensing and unwanted generated content. Audit tracked reports as well as plugin source. Existing icon and trap-content provenance blockers remain open.
5. Initialize an independent temporary Git repository for candidate validation, with no remote. Verify a single root with no parents or private Git objects. Run offline checks and confirm no dependency on local caches, private reports or machine paths.
6. Build the distributable package for the exact supported stock engine. Execute mapped acceptance in clean consumer projects against that artifact. Preserve failed attempts, limitations, source identities and artifact hashes.
7. Complete documentation and GitHub controls. Any source change invalidates the candidate identity: create another export and refresh affected evidence. Use the release manager for final evidence and non-preview export; preparation does not bypass approval gates.
8. After explicit owner publication approval, initialize the public repository from the approved complete export and verify ancestry and manifest again before pushing. Future releases transfer reviewed content snapshots into public history; never merge private Git ancestry. Preserve a private-source to public-commit mapping outside the exported source.

## Tracking rules

release/public-preparation.json tracks release problems and deliverables. Implementation state and verification are separate. Done but untested work is done, but cannot satisfy a release verification gate without evidence. Tool review coverage remains authoritative in docs/audit/workflow-tool-coverage.json; acceptance remains in Tests/acceptance.json. Do not duplicate tool completion counts here as an independently maintained metric.

Current preparation is documented, not certified. No public candidate has been approved by this checklist. Publication remains disabled in release/status.json. Local execution caches can accelerate development, but clean-install acceptance must not rely on them.
