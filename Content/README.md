# Plugin content

**This folder is not the test kit.** It holds one thing: the engine-tool trap data that the plugin loads at
runtime.

| Path | What it is |
|---|---|
| `Traps/EngineToolTraps.json` | the traps of the engine's own MCP tools, as **data** rather than as code (`D-07`), read by `Source/TurboMCP/TurboMCPTraps.cpp` |

`TurboMCP.uplugin` declares **`CanContainContent: true`**, which is what allows a plugin to ship a `Content/`
folder at all. It is set because of the file above, not because a test asset lives here.

*** THE TEST ASSET KIT IS IN [`TestProject/Content/Tests/`](../TestProject/Content/Tests/), NOT HERE, AND THAT
IS DELIBERATE. ***

`Spec/06_ACCEPTANCE.md` section 1 asks for a test content folder and gives `Content/Tests/` inside the plugin
**as an example** — *"for example a `Content/Tests/` folder inside the plugin"*. The kit went into the test
project instead, because the suites address every asset by its **virtual path** — `/Game/Tests/TA01_HandLaid`,
`/Game/Tests/BP_NodeTest`, `/Game/Tests/TurboMCPMaterialFixture` — and a path under `/Game/` is mounted from
the project, so the assets have to be where the project can see them. Moving them into this folder would
change every one of those paths and buy nothing.

**What the specification required of the location is satisfied:** the kit is not inside `Source/TurboMCP/` and
it is not scattered through the project root, so a reader can find it, and `TestProject/` is the project the
suites run in.

## The kit, and how it must be made

**By hand, in the editor, not with the plugin's own tools.** `06_ACCEPTANCE.md` says this directly, and the
reason is circular otherwise: **a tool tested against assets a tool built proves nothing.** The one exception
is the asset `06_ACCEPTANCE.md` asks to be built with **the engine's** write tools rather than this plugin's.

**A generated fixture is NOT tracked.** `Tools/make-kit-assets.ps1`, `make-acceptance-asset.ps1`,
`make-material-asset.ps1` and the `make-*.py` scripts rebuild theirs from the committed C++, and the plugin's
suite runner restores what a suite touches. `TestProject/.gitignore` names the generated ones individually,
because the hand-made ones — `TA01_HandLaid`, `TA07_Card`, `TA11_KitEnum`, `TA13_KitPath` — sit in the same
folder and a wildcard would have taken them too.

## Baselines

`06_ACCEPTANCE.md` requires the node count, comment box count and total wire length of each asset to be
recorded in [`TestRecord.md`](../TestRecord.md) and checked before every test run. **A baseline that does not
match means the asset was changed and must be restored before testing, otherwise a passing test proves
nothing.**
