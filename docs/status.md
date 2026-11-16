# Project status

[Documentation](README.md) | [Issues](github-issues.md) | [1.0.0 plan](release-1.0.0.md)

TurboMCP is a development preview. No stable release is currently approved.

| Target | Status |
| --- | --- |
| Windows, Unreal Engine 5.8.4 source | Development baseline; exact revision in [engine.lock.json](../engine.lock.json) |
| Installed / Launcher engine | Packaging and clean-consumer validation pending |
| Unreal Engine 5.9 | Separate preparation branch; adapter and runtime validation pending |
| Other platforms | Outside the first release scope |

Hosted offline CI is active. Daily source prereleases follow successful main pushes. These checks do not certify native behavior or installation packages.

The source-preview fixture audit collected 13 of 15 fixtures. Exact object resolution in unloaded multi-asset packages remains open. All 97 required release acceptance cases need exact-candidate evidence; see [acceptance data](../Tests/acceptance.json) and [release readiness](../release/status.json). Earlier evidence and version pins must be reconciled before stable approval.

The [GitHub issue map](github-issues.md) links current findings and release gates. Historical reports remain evidence; they are not new implementation assignments. The [roadmap](../ROADMAP.md) still defines execution order.
