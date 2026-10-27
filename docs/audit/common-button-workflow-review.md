# WF-UI-05: Common UI button workflow

Status: reader and preview scenario checked; the full editing and interaction workflow remains incomplete.
Implementation: bec049e. Regression wording update: 3b09a7e. Final build and both suites passed: f7ee45f24a69440fb5ee5dcc3c3654a6.

## Tested tools and behavior

ReadTurboMCPWidgetTree now exposes the assigned Common Button style class and inherited BP_OnHovered/BP_OnUnhovered event names. Context identifies compiled class defaults separately from a live instance. Detailed style values and runtime overrides are explicitly omitted. This reflection-based support adds no CommonUI dependency to TurboMCP; only TestProject enables the stock engine plugin.

LookTurboMCPWidgetPreview now constructs the widget before invoking the requested callback. The previous order produced identical normal and hover images because the callback ran before child-widget initialization. The test renders Purchase, calls BP_OnHovered and renders Ready to purchase, then renders Purchase through BP_OnUnhovered. Captures were inspected. Image differences/restoration, execution-denial refusal, invalid-size refusal, missing-callback partial result, and unchanged fixture tree/style/dirty flag are asserted.

ReadTurboMCPBlueprintLoad and ReadTurboMCPGraphLoad are reused for context. Fixture creation, style assignment and inherited-event wiring use Unreal APIs; they are not counted as tool-driven editing evidence. BindNewEvent was not used: this button overrides inherited events rather than binding a parent delegate. The two binding tools require another scenario and are not counted as reviewed here.

## Quality review and fixes

Actual output, public descriptions, parameter guidance, agent help, implementation and helper comments were reviewed. Preview explicitly reports its lifecycle and says a callback is not simulated pointer/focus input. Results retain partial failures, script diagnostics, image path, compile/save limits and external-effect limitations. The default callback result is now "not requested"; unknown-variable guidance says "Available editable examples". Existing regression assertions were updated for these two wording changes without removing behavior checks.

Widget helper comments incorrectly described designer bindings as generated runtime delegates. They now identify UWidgetBlueprint::Bindings and distinguish the standalone tree reader from the BlueprintLoad helper. The standalone reader retains its limited-export warning; added Common UI context is inspection metadata, not write-back data.

## Evidence and failures

Initial run ea5a544faf9748b5bd60cfbb79b718fa exposed missing style context and unchanged callback imagery. It also caught a missing variable GUID in the synthetic fixture; that setup error was corrected. Native run 7b7e2a150d0a4730b7152c0e3a13c58c then passed. Existing preview run 697ec291a840445987a418343c52125a failed only two stale wording assertions. Final combined run f7ee45f24a69440fb5ee5dcc3c3654a6 passed both test-common-button-workflow and test-widget-preview on the integrated source.

## Still required for the complete workflow

- Inspect actual style values and establish a targeted style-edit path.
- Discover and perform inherited event edits through tools rather than fixture setup.
- Test real pointer hover/unhover, focus, pressed and disabled states; explicit callback images prove none of these input routes.
- Verify the resulting graph visually after tool-driven edits and obtain user acceptance.
- Persistence, arbitrary user widget scripts and general side-effect rollback remain outside this unsaved scenario.

These are open workflow capabilities/evidence gaps, not completed features. No bug-free certification is claimed. Cross-workflow usage and quality criteria remain in workflow-tool-coverage.json; dashboard per-tool records are in purchase-workflow-review.json. Coverage is 30/125 unique tools in bounded scenarios. DSH remains paused.

Offline gate 91161d123b484a939583638659e187e6: 22/26 passed; existing structure, language, client-retry and lookup-guards failures remain separate validation debt.

## 2026-10-27 targeted shared-style tools

Candidate: 1a168124598dd718901238cf7b0382bf6b96ea7e. Stock-engine build and focused editor run e6c1c80f15394c469e7a07b506a34f3e passed.

Read/Preview/EditTurboMCPCommonButtonStyle provide explicit shared Blueprint style inspection and scalar/value-struct editing, including nested brush colors. The workflow passed padding and color readback, preview preservation, no-op, malformed/missing/reference refusals, compilation preservation, and live individual parameter help. Output identifies shared effects and explicitly excludes automatic compile, save and runtime refresh. Object references, containers and localized text are not writable through this path. No CommonUI module dependency or engine changes were added.

Full interaction/visual acceptance remains pending. These three tools are not yet added to the fully reviewed count: 63/155. Earlier totals above are historical. Next: finish this style workflow's visuals, then DataTables. DSH remains paused.

Offline integration checks: 26/26 passed, run 26729a9b3a8d4486bf36544df832eb48.

Next DataTable review gap: TurboMCP.DataTable.OwnedRows currently exercises internal Parse/Plan/Apply helpers, not public reader/preview/edit calls or their live help/output. Extend the disposable fixture for those boundaries rather than relying on the old engine-content mutation suite.

### Visual consumer check accepted

Native run b2b11644b5d44a6bb346facd68427350 passed. Inspected normal blue, hover green, pressed orange and disabled grey captures. The fixture assigns the tool-edited asset using CommonUI SetStyle after initialization. Earlier run e4655cf3ff47467181cd182c3729eb4a exposed the fixture using default grey styles; strict color assertions prevent false acceptance from image differences alone. Candidate 206ee56. The three shared-style tools are accepted for this bounded scenario: 66/155. This does not close inherited graph editing, end-to-end pointer routing, focus navigation or persistence coverage.
