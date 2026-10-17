# Migration validation

Observed on 2026-10-17 against the pinned Unreal 5.8.3 engine. This validates relocation and selected regressions, not all outstanding release acceptance.

- Repository layout, Python syntax, JSON, canonical links, task IDs and generated index: PASS.
- All PowerShell scripts parsed successfully.
- 244 plugin source/resource and test-project files preserved against the private backup (see preservation.json). The plugin descriptor was intentionally marked experimental/development; the project descriptor now declares AdditionalPluginDirectories.
- Engine/source preflight: PASS, 12 checks after building.
- Test editor build: PASS. Unreal compiled from Plugins/TurboMCP/Source in the migrated checkout; build receipt locates the plugin DLL in TestProject/Binaries/Win64.
- Native smoke test: TurboMCP.Result.LineContract, exactly 1 test selected and 1 Success, engine process exit 0.
- Public preview: 221 allowlisted files, no private task/document tree or Git history. Refusal controls cover token patterns, personal paths, untracked source and unexpected binaries.
- Unfinished normal release export refused and created no output directory.

## Focused regression scripts

All passed:

- `python Tools/test-client-retry.py`
- `python Tools/test-result-contract.py`
- `python Tools/test-coverage-audit.py`
- `python Tools/test-runner-verdicts.py`
- `python Tools/test-lookup-guards.py`
- `python Tools/test-widget-tree-nested.py`
- `python Tools/test-traps-plugin-identity.py`
- `python Tools/test-ownership.py`
- `python Tools/test-repo-boundaries.py`
- `python Tools/test-build-provenance.py`

## Artifact

- Engine commit: 396c9f059903aed5fec78ecd3d437a40c6415368
- DLL bytes: 3741184
- DLL SHA-256: CC3CEF84EB0FA7BF05ED5D468051C9AC2CBC4E0CC14E6DB3B516F057CB7F6F05
- Local build/native logs: ignored .local/migration-validation. Raw logs are not part of public exports.

The migration also corrected an observed pre-existing provenance defect: the helper recorded the plugin commit as the engine identity and stored prose in ArtifactPath. The focused provenance regression uses distinct temporary repositories and a staged-only change.

## Not certified

The full editor acceptance suite, fresh machine/Launcher compatibility, Unreal 5.9 compilation, public installation acceptance, and public publication are not certified. release/status.json remains not_ready. Imported task claims still require reconciliation. Tools/ci/validate.yml contains a CI template for structural/offline checks without engine access. It is not active: GitHub rejected workflow creation because the current OAuth credential lacks workflow scope. No remote CI run is claimed.
