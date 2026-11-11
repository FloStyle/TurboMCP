# Project status

[Documentation](README.md)

TurboMCP is a development preview. The failed `v1.0.0` release and tag were withdrawn on 17 October 2026. No stable release is currently approved.

## Compatibility

| Target | Status |
| --- | --- |
| Windows, Unreal Engine 5.8.4 source | Development baseline; exact revision in [engine.lock.json](../engine.lock.json) |
| Installed / Launcher engine | Packaging and runtime validation pending |
| Unreal Engine 5.9 | Development API comparison underway; adapter and runtime validation pending |
| Other platforms | Outside the initial release scope |

## Release work remaining

The inventory accounts for 101 declared tools. Static test references do not establish runtime correctness. All 97 required acceptance cases still need release evidence; consult [acceptance data](../Tests/acceptance.json) and [release readiness](../release/status.json) for the maintained records.

The [roadmap](../ROADMAP.md) orders test infrastructure repairs, tool and safety audits, documentation verification, acceptance, and release automation. The first task is selected by `python Tools/next-task.py`.

GitHub CI is not active yet. Its template is in `Tools/ci/validate.yml`; installing the workflow requires an appropriately authorized GitHub credential. Local validation remains available through `python Tools/validate.py`.

Update this page when support or release status changes. Keep detailed evidence in the corresponding task and acceptance records.
