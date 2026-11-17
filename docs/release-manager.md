# Release manager

[Documentation](README.md) | [Public preparation pipeline](public-preparation.md) | [Public preparation checklist](../release/public-preparation.json)

The stable release manager is a local, fail-closed verification and export process. It never pushes or publishes. Run `python Tools/validate.py`, then build and execute mapped product acceptance against the candidate commit. Keep reports and binaries under ignored .local. Commit documentation, version, acceptance mapping and owner approval before collecting final evidence so the evidence identifies the exact final commit.

`python Tools/release-check.py --evidence .local/release/evidence.json` requires a clean tree, readiness ready, explicit approval, an executable mapping for every required acceptance case, exact source/engine identities, passing results, and intact log/artifact hashes. Blocked, missing, duplicate or stale results fail. `Tools/export-public.py --evidence ... --output ...` invokes this gate before a non-preview export; the existing plugin-tree approval also remains required.

Evidence schema 1 has source_commit, engine_commit, artifact {path, sha256}, and cases [{id, status: passed, exit_code: 0, command: [program, arguments], log, log_sha256}]. Paths are relative to the evidence file's directory. Evidence must be collected from actual runs; this validator checks integrity and completeness, not authenticity of manually authored assertions. Review the commands and logs. The tested release artifact must be the exact ZIP used in both installed-consumer phases. Source fixture checks identify their development DLL separately; they are not evidence that those fixtures ran against the package DLL. See the owner-approved scope in release-1.0.0.md.

After acceptance, verify the complete export and review its source and artifact identities. Follow [public source preparation](public-preparation.md) for any initial hosting cutover. Advance stable and publish the version tag only under explicit release authorization. Each binary package records its exact engine version, build identity and SHA-256. Source-engine DLLs cannot establish Launcher compatibility.

The checked-in GitHub workflow and local validation run the offline gate. Verify it on the active integration host. Hosted offline results do not satisfy native installation or release acceptance.

Before public publication, complete the [GitHub controls checklist](governance.md#github-controls-and-publication-checklist), test private vulnerability reporting and review binary assets under the [language policy](language.md). Versioned policies alone do not enable GitHub protections.

## Owner-authorized daily source publication (2026-11-16)

The owner authorizes daily source prereleases after successful main-push
validation in the cleaned FloStyle/TurboMCP repository. This supersedes earlier
manual-only wording for that development channel. The exact guards, rolling
daily tag rule and source-only limits are in [the release process](release.md).
Stable version tags, installation packages and stable acceptance remain
manual and unchanged. This automation does not change repository visibility.
