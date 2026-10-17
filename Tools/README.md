# Development commands

Run commands from the repository root. Tools is deliberately kept as the existing command surface so scripts and suites retain stable names during the structural migration.

| Command | Purpose |
| --- | --- |
| `python Tools/validate-repo.py` | Layout, task integrity, public-policy and documentation checks |
| `python Tools/doctor.py` | Explicit engine path, pinned revision and project discovery |
| `python Tools/check-plugin.py` | Existing engine/include/reflection checks |
| `pwsh -File Tools/build.ps1 -Engine <root>` | Build the test editor and verify the plugin artifact |
| `pwsh -File Tools/start-editor.ps1` | Start the owned test editor |
| `pwsh -File Tools/run-suites.ps1 -Suites <name>` | Run selected integration suites |
| `python Tools/update-todo.py` | Regenerate TODO.md from repository task files |
| `python Tools/export-public.py --preview --output .local/public-preview` | Produce a reviewable allowlisted public tree |

Pure tests such as test-client-retry.py, test-result-contract.py, test-coverage-audit.py and test-runner-verdicts.py do not require the editor. Other tests may create or mutate fixture assets; follow the ownership helpers and serialize editor access. Do not infer a passing plugin from a passing static check.

`python Tools/test-repo-boundaries.py` verifies public export refusal controls. `python Tools/test-build-provenance.py` verifies distinct engine/source identity and staged changes.

CI template: Tools/ci/validate.yml. Activating it under .github/workflows requires a GitHub credential authorized for workflows; the current login lacks that scope.

The standard offline gate is `python Tools/validate.py`; test tiers are in Tests/catalog.json. Release evidence is checked with Tools/release-check.py. See [contributor rules](../docs/contributing.md) and [release manager](../docs/release-manager.md).
