# NOV09-13: Blueprint reference and soft-default repair

## Current source review

`FTurboMCPBlueprintRead::RenderRelations` scans graphs in the requested Blueprint only. Its unreferenced-function heading omits that boundary, so an external caller can be mistaken for an unused function. A filtered read already refuses to make the claim.

`RenderVariables` and the JSON writer use `FBPVariableDescription::DefaultValue` as if an empty declaration proved a type default. For soft object and soft class properties, the generated class default object may contain the effective saved path. The field report does not establish whether its specific asset stored that path, so the repair must report source and availability rather than infer it.

## Approved repair contract

- Label the static scan as limited to the current Blueprint. Do not claim project-wide nonuse.
- For soft object/class member variables, inspect the already existing generated class and default object without creating or compiling either. Report an effective path when present, an explicit unset value when present and empty, or unavailable when the generated property/default object cannot be inspected. Preserve the declaration string separately when it differs.
- Keep text and JSON descriptions consistent. Preserve the existing JSON `default` field and add status/source fields for callers that need to distinguish cases.
- Prove the behavior using one disposable synthetic Blueprint with a known soft path in its class default object, an unset soft path, and an unavailable class case. Check that read-only inspection does not dirty the package. Run only this focused native test and the offline gate.

Scope: `TurboMCPBlueprintRead.cpp`, `TurboMCPBlueprintJson.cpp`, `Public/TurboMCPBlueprintRead.h`, and one new focused test file. No asset save, new tool, engine change, or project-wide reference walk.
