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
