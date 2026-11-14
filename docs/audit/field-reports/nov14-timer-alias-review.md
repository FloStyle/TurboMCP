# NOV05-18 TimerHandle alias slice

The type parser now recognizes case-insensitive `TimerHandle` as `/Script/Engine.TimerHandle`. Unreal 5.8.4 declares FTimerHandle as BlueprintType. A native fixture compares the alias and full object-path pin types. The change does not add variable removal, variable rename or arbitrary short struct-name resolution.

Candidate 144319f81fc3931a3cde5ce3742e87e3ef87b921 against approved base d27db079c39bd3b86b4af49703a6237c220b2788 passed the exact two-file scope check and diff check. Supervised build f351f6ef1cf149fb94fe784cb09184e6 passed. The new native assertion was not run; runtime verification remains outstanding. The two historical Append-suite failures remain recorded, and the full Append suite was not run.

NOV05-18 stays in progress. Next: test the alias through a focused native invocation when the editor is scheduled, then separately design and implement local removal/rename and decide which other struct aliases the field workflow requires. Do not infer complete variable capability from this alias.

## Later runtime evidence

Supervised execution 810ccf6e47f9459a9d9b7dc106d218fc at f2f2e57e3fc76db8f9adb8171d6361cf51e0aea3 passed the complete selected test-graph-append wrapper, including InterfaceLocal (1 passed, 0 failed, 0 skipped). That native case checks TimerHandle against the full Engine struct path. The earlier untested statement above describes the original submission only. Historical suite failures remain recorded in nov13-loaded-node-search-review.md. NOV05-18 remains in progress: this evidence validates the alias slice, not all variable operations.

## Variable capability reconciliation

The original FT-144 asks for both member and local removal and rename. Current member removal is already implemented by Preview/EditTurboMCPVariableRemove. The native VariableRemove case passed in execution 810ccf6e47f9459a9d9b7dc106d218fc (1 passed, 0 failed, 0 skipped). Its assertions cover wrong/stale confirmation, a disconnected getter refusing removal without node deletion, successful unused-member removal, preservation of an unrelated connection and declaration, inherited-member refusal and confirmation help. This does not establish local removal or rename support.

Remaining implementation: local removal, local rename and member rename. TimerHandle is the only concrete missing short struct name identified by VA-11; do not invent a requirement to accept ambiguous arbitrary short names.

## Reviewed local-removal contract

Implement PreviewTurboMCPLocalVariableRemove(Asset, Function, Name) and EditTurboMCPLocalVariableRemove(Asset, Function, Name, Confirm). Resolve one exact owned function graph and exactly one function entry; refuse ambiguous, inherited, event, macro or malformed scopes. Resolve one valid local declaration GUID. Preview reports its type/default and all detected referencing node paths; a used local cannot be removed. Match local references by GUID and by name plus scope, including collapsed child graphs; never confuse a same-named member or another function's local with the selected declaration. Refuse unresolved same-name local scopes rather than guessing.

Use a freshness token covering the asset, scope, declarations and graph baseline. Edit repeats preflight and requires the token. Remove only the selected declaration, transactionally, preserving all other declarations and graph nodes, links, literals and positions. Do not call FBlueprintEditorUtils::RemoveLocalVariable: in stock 5.8.4 it also calls RemoveVariableNodes and asserts that there is exactly one entry. No implicit node deletion, save or checkout. Structural refresh has the existing member-removal compilation/undo limitations; report them accurately and verify rollback before claiming restoration.

Focused fixture: two functions and a member sharing a name; reject a used local and stale token without mutation; remove an unused local while preserving the other scopes and graph state; confirm missing/malformed scope refusal and help. Reuse the next scheduled native pass rather than rerunning unaffected suites.

Local rename needs a separate review: stock RenameLocalVariable calls ValidateBlueprintChildVariables and structural refresh, so it is not automatically a function-only change. Member rename needs dependency and binding discovery. Do not expose either helper as a narrow atomic edit without auditing those effects.

## Local removal implementation and acceptance

Candidate 9e8f9959072f8e8381de6623bf0c38f7147b19db implements Preview/EditTurboMCPLocalVariableRemove with fresh confirmation, exact owned function selection, local reference refusal and declaration-only mutation. The exact three-file scope passed against 7c0f8bfd89d6561baec3d3ba5dea9af4f3cd5134. Claim: 144f62e3ff8e4b4d92433de81881595f.

Stock 5.8.4 supervised build 59a4839afce942a3a941ffb64c71c8be passed. Supervised engine execution 2f52f068cbad408abad0e47d96debdc9 passed the selected test-graph-append wrapper, including the extended VariableRemove case (1 passed, 0 failed, 0 skipped). Assertions cover unused local removal; same-named member and other-function local preservation; stale token refusal; disconnected local getter refusal without deletion; missing/event scope refusal; and token help. This synthetic fixture does not certify arbitrary custom nodes, dynamic references or field-project behavior.

Offline run e840b7c9f76446b383d77fe750400871 passed all checks except validate-repo's stale capability inventory. Integration regenerated the inventory for the two new tools and reran validate-repo separately. No unrelated native suites were repeated.

NOV05-18 remains in progress: local and member rename remain unimplemented. The historical local-removal contract above is now implemented, with this evidence defining its verified scope.

## Rename source review and approved next contract

Stock 5.8.4 source review: BlueprintEditorUtils.cpp RenameLocalVariable calls RenameVariableReferencesInGraph over every node in the function, then ValidateBlueprintChildVariables and structural refresh. K2Node_Variable.cpp HandleVariableRenamed checks owner class and name, but does not require local scope before renaming a self-context member. Consequently, invoking the broad helper is not proof that a same-named member will remain untouched. This is a source-derived risk, not a reproduced field failure. Child validation can also rename colliding child members/components/timelines/functions.

Approved next slice: Preview/EditTurboMCPLocalVariableRename(Asset, Function, Name, NewName[, Confirm]). Exact owned function and valid local GUID; validate NewName against parameters/locals and reject ambiguity. Collect only local references matching declaration GUID and function scope, including nested graphs. Same-named members and locals in other functions remain untouched. Report paths of affected nodes before mutation and bind confirmation to declaration/reference/graph state. Update declaration name/friendly name and precisely selected variable references/pins while retaining GUIDs, defaults, links and positions. Do not invoke broad child-name validation. Reject unsupported reference node kinds explicitly rather than silently skipping them. Postconditions must compare reference scope/name/GUID as well as graph topology; existing GraphState alone does not capture member references. Failure must verify restoration before claiming rollback. No save or checkout; disclose structural refresh limitations.

Focused regression: a function containing both a local getter/setter and a same-named member getter, another function with a same-named local, a loaded child with NewName already declared, connected local pins and nondefault literals. Verify only the selected local/references rename and other identities/defaults/links remain unchanged. Verify collision and stale-token refusals, fresh preview, and help. This is the next implementation slice; no passing runtime claim yet.

Member rename remains separate: stock helper can show a RepNotify dialog after mutation, clears its association after confirmation, refreshes defaults from the CDO, validates child collisions, compiles and handles delegate signature graphs. Its preview must enumerate dependent assets and binding/reference kinds; it cannot safely be a blind wrapper or a loaded-assets-only completeness claim.

## Local rename implementation and acceptance

Candidate 3b25d16e9ea83e96c29bdcbaaf109c56569a5216 passed the exact three-file scope against approved base a188c16df2ad151d5a2c3faf485b228d3fafb70a (claim 39f60c4ba35a4955aabb1c28dcd2923e). Preview/EditTurboMCPLocalVariableRename updates the selected local declaration and native getter/setter references, preserving identity and topology. Unsupported reference kinds refuse explicitly; the broad engine rename helper is not used.

Supervised execution 75e93064b9df49b7810134a11a975041 passed the build and selected test-graph-append wrapper. VariableRemove reported 1 passed, 0 failed, 0 skipped. Its added assertions cover stale confirmation, successful rename, same-named member/other-function local and loaded-child preservation, pin identity, links, literals, invalid empty name and help. The planned explicit name-collision case was not added; no collision-fixture or visual acceptance claim is made. Static lookup audit passed 126/126 including its negative control.

NOV05-18 stays in progress for member rename. These results cover a synthetic fixture, not arbitrary custom nodes or field-project acceptance. Earlier pending statements above describe prior revisions.

## Reference inspection load-safety repair

Source review of InspectReferences found that it checks referencer asset classes for World but loads each Blueprint with GetAsset without first walking its hard dependencies. A Blueprint may therefore load a map dependency indirectly before the later map check. This is a source-confirmed gap, not a reproduced crash. Repair it before reusing discovery for member rename.

Approved bounded contract: before loading any unloaded dependent Blueprint, traverse its package hard dependencies without loading objects. Refuse map package flags, redirectors, missing dependency metadata and a bounded traversal overflow. Reuse already resident valid Blueprint objects without loading; do not force-load them just for inspection. Check missing referencer-package registry entries rather than treating them as proof of no references. Preserve current reference and mutation guards. Report the package and refusal reason. This does not sandbox arbitrary PostLoad hooks or add unloaded-search capability. Add focused missing-metadata and known-map rejection assertions plus retain the existing loaded synthetic removal fixture. No speculative field-crash closure.
