# Project status

[Documentation](README.md) | [Issues](github-issues.md) | [1.0.0 plan](release-1.0.0.md)

TurboMCP 1.0.0 is stable for Windows on the pinned Unreal Engine 5.8.4 source build. Launcher builds and Unreal 5.9 are not certified yet.

| Target | Status |
| --- | --- |
| Windows, Unreal Engine 5.8.4 source | Stable 1.0.0; exact revision in [engine.lock.json](../engine.lock.json) |
| Installed / Launcher engine | Not certified; no packaging or clean-consumer validation |
| Unreal Engine 5.9 | Separate preparation branch; adapter and runtime validation pending |
| Other platforms | Outside the first release scope |

Hosted offline CI is active. Daily source prereleases follow successful main pushes. These checks do not certify native behavior or installation packages.

The source-preview fixture audit collected 13 of 15 fixtures. The unloaded exact-object resolver is now implemented and reviewed, with a passing supervised build and focused native metadata scenario on 6996d636. The original 13-of-15 fixture audit has not been rerun in full. The ten V1-* release gates passed on `ef1d1e1f` and the [release readiness](../release/status.json) record carries the owner approval. The 97 original acceptance specifications remain historical evidence; see [acceptance data](../Tests/acceptance.json).

The [GitHub issue map](github-issues.md) links current findings and release gates. Historical reports remain evidence; they are not new implementation assignments. The [roadmap](../ROADMAP.md) still defines execution order.

The [interactive field guide](https://flostyle.github.io/TurboMCP/guide.html) now contains 33 technical chapters with reading and presentation views. Desktop and narrow-width browser checks passed. The 1.0.0 package passed BuildPlugin and archive verification on the pinned stock 5.8.4 source engine, and both clean consumers (Blueprint-only and C++) passed on that archive in run `b8dd28b1`. Launcher builds remain unverified.

The call-log disk sink now checks file-operation failures and bounds UTF-8
records. Its focused native regression passed on 30937018. Graph replacement
is under safety review; full release acceptance and clean-consumer installation
remain pending.

The [human handbook](https://github.com/FloStyle/TurboMCP/wiki) is published with installation, tool selection, Blueprint inspection, data workflows, edit verification and troubleshooting. The website revision retains the 196-tool catalogue and 33 guide chapters, with schema-checked request templates and recorded synthetic graph captures.
