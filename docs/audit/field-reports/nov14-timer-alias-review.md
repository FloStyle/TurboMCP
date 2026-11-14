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
