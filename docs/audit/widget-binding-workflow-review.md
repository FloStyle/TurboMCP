# Widget property binding review

WF-UI-10-WIDGET-BINDING, 2026-10-25. Focus: EditTurboMCPWidgetBindAdd.

## Implemented and verified

Candidate 4f28cec. Native build and workflow c24c254d03aa4196a02263d7e83e1df6 passed without warnings. Scope gate passed. A disposable label binds to GetPurchaseText, compiles and displays Purchase ready. Replacement with GetSoldText leaves one binding and displays Sold out. Property/Delegate aliases and widget-name case normalize to an unchanged request, preserving a clean package. Missing widget/function/property, incompatible return type, impure getter and multicast OnClicked are refused without replacing the valid binding. Designer text, widget identity and slot geometry remain intact.

The old implementation stored invalid binding metadata. The new preflight mirrors Unreal's single-cast delegate signature and pure/const requirements before modifying metadata. Public help and actual output now distinguish designer property binding from graph event binding, report replacement, explain refusal and the next action, and explicitly state that compile, save and a dedicated undo transaction were not performed. Reviewed resolver, reflection compatibility, replacement/no-op logic, comments and English prose. No engine changes.

## Evidence and limits

Offline 7050368c75c24a07aa062eaf75436486: 22/26, the existing structure, language, client-retry and lookup-guard failures. This gate preceded only the final test capture-order adjustment; the final native build/workflow passed afterwards.

Initial fixture failures were documented rather than accepted: a temporary Slate root released resources before text read-back; a newly added button needed a fixture GUID. Retaining the Slate root through a rendered frame fixed the text assertion. Capture now runs before the fixture's separate renderer. The initial visual review incorrectly reported a missing button. See the correction below.

Only text property binding is covered. Other delegate families, inherited bindings, function renaming, undo restoration and save/reload remain untested. FunctionAdd is fixture setup, not an additional completed tool review. Coverage is 38/125 tools reviewed in bounded scenarios, not full certification.

## Preview follow-up correction

WF-UI-10-PREVIEW-REPEAT is withdrawn, not a production fix. Direct pixel inspection of all four archived captures from 61c7799f9bb447b08ca34c1a74e6e896 and c24c254d03aa4196a02263d7e83e1df6 found the button present: 1600/1600 pixels in its inner region (x410..489, y45..64) exceed brightness 30. The previous interpretation of the displayed images was wrong.

Native run 38bdf1a0d7e441ca9066455e39122b23 builds and passes four consecutive captures: two before replacement, two after, with the fixture renderer between stages. Each capture is decoded with Unreal's PNG decoder and asserts at least 1000 visible button pixels; all measured 1600. Template size remains 100x30 and visibility remains Visible. No production or engine changes. An initial wrapper attempt failed because Pillow was unavailable; the final regression uses Unreal's decoder without a new Python dependency.

Regression candidate bfb240e passed scope review. Offline gate 6f1a109f57fe405dad3d03c759632d1b: 22/26, the same four baseline failures.
