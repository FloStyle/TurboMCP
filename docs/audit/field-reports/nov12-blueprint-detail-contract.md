# NOV09-11: preserve Blueprint read detail and filter scope

Readiness: ready. Integration owner: integrator.

## Confirmed source findings
Reviewed after 5952f23c. BlueprintRead::Read recursively downgrades bodies/full to signatures and then manifest. It can return a manifest exceeding the requested budget. GraphFilter selects bodies, but RenderManifest, RenderVariables, RenderSignatures and RenderRelations still use global data. The JSON renderer filters graph bodies but emits global variables/components/signatures. JSON already refuses an oversized document with its required character count.

Field BL-15 explicitly accepts either paging at requested detail or a clear refusal giving the required size. Implement the latter consistently now; do not quietly return a lower level. BL-16 requires all graph-dependent sections to respect GraphFilter. Optional argument discovery is separately implemented by NOV09-10.

## Required repair
- Preserve requested detail. Render the requested result once; on budget overflow return a clear refusal with required character count, supplied budget, and narrowing/GraphLoad guidance. Never recurse or downgrade. Include notes in size accounting. Keep per-asset sibling success and refusal counts accurate.
- Use the selected graph set for names, signatures/events, relation scans, callers and variable usage. Keep asset identity/parent/interfaces as explicitly asset-level context.
- Filter member-variable and component details to references in selected graphs; label the limited scope. Do not infer asset-wide unused/dead status from filtered scans. Preserve unfiltered behavior.
- Apply equivalent scope to text and JSON. Keep unknown/empty-match refusals and existing public names.
- Review help and header comments; remove historical fallback claims and excessive historical implementation commentary.
- Do not change BlueprintOverview/lint semantics accidentally: inspect shared helper callers before changing helper defaults.

## Bounded implementation and verification
Approve exact schema-2 file scope before implementation: BlueprintLoad.cpp, BlueprintRead.cpp and its public header, BlueprintJson.cpp, public toolset header, plus the selected existing Blueprint reader suite after inspection. Any additional targets require integrator scope amendment.
One incremental compile and one focused suite after the batch: small budget refuses at requested detail with exact needed size; larger retry retains signatures/bodies; filtered text/JSON omit unrelated graph metadata; no-match refuses; unfiltered context remains available. Retain initial failures. No full regression.
