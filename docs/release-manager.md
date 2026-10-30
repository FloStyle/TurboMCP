# Release manager

[Documentation](README.md) | [Public preparation pipeline](public-preparation.md) | [Public preparation checklist](../release/public-preparation.json)

The release manager is a local, fail-closed verification and export process. It never pushes or publishes. Run `python Tools/validate.py`, then build and execute mapped product acceptance against the candidate commit. Keep reports and binaries under ignored .local. Commit documentation, version, acceptance mapping and owner approval before collecting final evidence so the evidence identifies the exact final commit.

`python Tools/release-check.py --evidence .local/release/evidence.json` requires a clean tree, readiness ready, explicit approval, an executable mapping for every required acceptance case, exact source/engine identities, passing results, and intact log/artifact hashes. Blocked, missing, duplicate or stale results fail. `Tools/export-public.py --evidence ... --output ...` invokes this gate before a non-preview export; the existing plugin-tree approval also remains required.

Evidence schema 1 has source_commit, engine_commit, artifact {path, sha256}, and cases [{id, status: passed, exit_code: 0, command: [program, arguments], log, log_sha256}]. Paths are relative to the evidence file's directory. Evidence must be collected from actual runs; this validator checks integrity and completeness, not authenticity of manually authored assertions. Review the commands and logs. The tested artifact must be the installed binary used for runtime acceptance.

After acceptance, inspect the export and initialize a NEW Git repository in that export directory. Verify its root commit has no private parents, review files and identities, and push only on explicit owner authorization. Do not clone private history for publication. Binary packages are separate release assets, each with exact engine version/build identity and SHA256. Never copy the source-engine DLL into an unrelated Launcher installation and call it compatible.

The checked-in CI template remains inactive because the available GitHub credential lacks workflow permission. Enable Tools/ci/validate.yml as .github/workflows/validate.yml once an authorized workflow credential is available; then require its successful check for private main. Local validation already uses the same entry point. No successful remote CI run is claimed.

Before public publication, complete the [GitHub controls checklist](governance.md#github-controls-and-publication-checklist), test private vulnerability reporting and review binary assets under the [language policy](language.md). Versioned policies alone do not enable GitHub protections.
