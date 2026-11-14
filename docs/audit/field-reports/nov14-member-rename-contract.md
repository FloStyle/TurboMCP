# NOV05-18 — Member variable rename repair contract

Readiness: ready
Owner: integration owner (direct implementation)
Current review: 2026-11-14, stock Unreal 5.8.4

## Problem

Local rename, declaration removal and TimerHandle resolution already have focused native evidence. Blueprint member rename is still missing. This contract does not close NOV05-18.

The stock FBlueprintEditorUtils::RenameMemberVariable implementation in BlueprintEditorUtils.cpp changes RepNotify associations after a modal prompt, validates child variables by potentially renaming them, transfers defaults from the CDO, and calls MarkBlueprintAsStructurallyModified. Calling it directly would hide material side effects from an MCP caller.

## Required behavior

Provide explicit PreviewTurboMCPVariableRename and EditTurboMCPVariableRename tools with Asset, Name, NewName and an apply confirmation token. Resolve an exact owned declaration. Preserve its GUID, type, flags, metadata, replication association, and effective default value. Update supported getter/setter references by identity, including dependent Blueprint references and inherited self references; never change a same-named local or unrelated member. Preserve graph topology, pin defaults and positions. Report all affected assets and reference counts before mutation.

Discover dependencies without loading maps. Include resident unsaved Blueprints and validate object readiness. Fail before mutation when discovery is incomplete or a reference kind cannot be updated safely. Explicitly report the blocking asset/reference and reason. Unsupported bindings, class-default pins, delegate declarations or dynamic/native references must not silently disappear from the coverage report. Refusals are limitations, not implemented rename coverage.

Reject conflicting child declarations instead of renaming children automatically. Preserve OnRep association rather than invoking the stock dialog or detaching it. Check name validity and case behavior explicitly. Confirmation must cover declaration/default state, affected references and dependency coverage; re-evaluate on apply. A stale token changes nothing.

No automatic save, checkout, map load, PIE, or engine edits. State structural refresh/compilation effects accurately. Do not claim undo safety across reinstancing. Follow the current architecture contracts and existing rollback helpers; verify state before reporting success, and report an unverified rollback honestly.

## Focused evidence

Use a synthetic owner Blueprint plus child and external caller: non-default CDO value, member getter/setter references, same-named local and unrelated member, connected pins, and child collision. Cover successful preview/apply, stale token, collision and unsupported reference refusal. Verify declaration GUID/default/metadata/replication association and unrelated graph state. Compile once for the batch, then run the affected native fixture only. Include save/read-back only if needed to prove default persistence; no whole-suite repetition.

## Scope and tracking

Implementation scope: existing TurboMCPVariableRemove.cpp, Public/Tools/TurboMCPToolsets.h and TurboMCPVariableRemoveTest.cpp. Shared catalog, evidence and plan updates remain integrator-owned. Keep the issue in progress until supported behavior has implementation evidence; track runtime validation separately.
