# TurboMCP development

Private development repository for the TurboMCP Unreal editor plugin. Current release readiness: **not ready**. The migration preserves unfinished implementation; it does not certify a release.

Start with [AGENTS.md](AGENTS.md), [TODO.md](TODO.md), and [workspace setup](docs/workspace.md). Product contracts are in [architecture](docs/architecture.md), with [acceptance criteria](docs/acceptance.md). The public-facing documentation lives separately in [docs/public](docs/public/README.md).

| Path | Purpose |
| --- | --- |
| Plugins/TurboMCP/ | Installable plugin: descriptor, C++ source, runtime content, resources |
| TestProject/ | Unreal integration project and versioned fixtures |
| Tools/ | Build, editor, test, validation, and export entry points |
| Tests/acceptance.json | Required release acceptance cases; not evidence of passing |
| docs/ | Current development contracts and workflow |
| docs/public/ | Documentation eligible for a public export |
| todo/ + TODO.md | Repository-owned tasks and generated index |
| release/ | Release readiness and export policy |
| engine.lock.json | Exact tested baseline and separate unsupported forward target |

```powershell
python Tools/validate-repo.py
python Tools/doctor.py
pwsh -File Tools/build.ps1 -Engine C:/UnrealEngine
python Tools/test-result-contract.py
```

Keep the existing engine checkout. TestProject discovers `../Plugins` through AdditionalPluginDirectories; setup does not install a junction inside the engine. See [migration provenance](docs/migration.md) for the preserved history and recovery path. See [5.9 planning](docs/engine-compatibility.md) before upgrading.

Prepare a review-only public tree with `python Tools/export-public.py --preview --output .local/public-preview`. This does not push, initialize public Git history, or assert release readiness. See [release workflow](docs/release.md).
