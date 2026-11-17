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

The source-preview fixture audit collected 13 of 15 fixtures. The unloaded exact-object resolver is now implemented and reviewed, with a passing supervised build and focused native metadata scenario on 6996d636. The original 13-of-15 fixture audit has not been rerun in full. All 97 required release acceptance cases need exact-candidate evidence; see [acceptance data](../Tests/acceptance.json) and [release readiness](../release/status.json). Earlier evidence and version pins must be reconciled before stable approval.

The [GitHub issue map](github-issues.md) links current findings and release gates. Historical reports remain evidence; they are not new implementation assignments. The [roadmap](../ROADMAP.md) still defines execution order.

The [interactive field guide](https://flostyle.github.io/TurboMCP/guide.html) now contains 33 technical chapters with reading and presentation views. Desktop and narrow-width browser checks passed. The supervised Win64 development-preview package passed BuildPlugin and archive verification on the pinned stock 5.8.4 source engine. Its module build ID matches that engine. Clean-consumer BP-only/C++ installation and Launcher compatibility remain unverified; GitHub #16 stays open.

The call-log disk sink now checks file-operation failures and bounds UTF-8
records. Its focused native regression passed on 30937018. Graph replacement
is under safety review; full release acceptance and clean-consumer installation
remain pending.

The [human handbook](https://github.com/FloStyle/TurboMCP/wiki) is published with installation, tool selection, Blueprint inspection, data workflows, edit verification and troubleshooting. The website revision retains the 196-tool catalogue and 33 guide chapters, with schema-checked request templates and recorded synthetic graph captures.
