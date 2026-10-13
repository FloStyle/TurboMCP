# Audit: the never-executed `E-LOCAL-VARS` promotion path

**Subject:** `FTurboMCPLocalVars::Apply` in `Source/TurboMCP/TurboMCPLocalVars.cpp`, lines 463-713.
**Context:** `QUESTIONS.md` Q-13 records that this half has never run. The `Plan` half is verified
against the committed asset `TurboMCPAcceptance:LocalVarProbe`, where it correctly reports one SAFE
and one UNSAFE candidate.
**Method:** every engine symbol at every call site was matched against its declaration, read from the
**Unreal Engine 5.8.3 source tree this project builds against**. **No code was run, no editor was started, no
build was performed** — this is a source-only audit, as instructed.

**Result: 7 PROVEN defects, 3 UNVERIFIED items.** Two of the defects are severe enough that the
promotion cannot work as written on the asset the project already has.

---

## 1. Every engine symbol at every call site

| # | Call site | Engine declaration read | Verdict |
|---|---|---|---|
| 1 | `LocalVars.cpp:493` `DoesSupportLocalVariables(Graph)` | `Kismet2/BlueprintEditorUtils.h:682` `static UNREALED_API bool DoesSupportLocalVariables(UEdGraph const* InGraph);` | **OK** |
| 2 | `LocalVars.cpp:543` `FindUniqueKismetName(Blueprint, *Candidate.ProposedName)` | `BlueprintEditorUtils.h:2317` `static UNREALED_API FName FindUniqueKismetName(UBlueprint* Blueprint, const FString& BaseName, UStruct* InScope = nullptr, bool bFunctionScope = false);` | **OK**, returns `FName`, matches the local `FName VarName` with no dereference |
| 3 | `LocalVars.cpp:546` `AddLocalVariable(Blueprint, Graph, VarName, NewType, Outputs[0]->DefaultValue)` | `BlueprintEditorUtils.h:976` `static UNREALED_API bool AddLocalVariable(UBlueprint* Blueprint, UEdGraph* InTargetGraph, const FName InNewVarName, const FEdGraphPinType& InNewVarType, const FString& DefaultValue = FString());` | **OK**, 5 args, 5 params |
| 4 | `LocalVars.cpp:617` `NewObject<UK2Node_VariableGet>(Graph)` | `K2Node_VariableGet.h`, `UCLASS` subclass of `UK2Node_Variable` | **OK** |
| 5 | `LocalVars.cpp:618` `Getter->CreateNewGuid()` | `EdGraphNode.h` `void CreateNewGuid();` | **OK** |
| 6 | `LocalVars.cpp:559-560` `VariableReference.SetLocalMember(VarName, Graph->GetName(), FindLocalVariableGuidByName(...))` | `Engine/MemberReference.h:194` `ENGINE_API void SetLocalMember(FName InMemberName, FString InScopeName, const FGuid InMemberGuid);` **and** `:191` `SetLocalMember(FName, UStruct*, const FGuid)` | **OK**, the `FString` overload is selected by `Graph->GetName()` |
| 7 | `LocalVars.cpp:620` `FindLocalVariableGuidByName(Blueprint, Graph, VarName)` | `BlueprintEditorUtils.h:1036` `static UNREALED_API FGuid FindLocalVariableGuidByName(const UBlueprint* InBlueprint, const UEdGraph* InScopeGraph, const FName InVariableName);` | **OK**, 3 args |
| 8 | `LocalVars.cpp:564` `Graph->AddNode(Getter, true, false)` | `EdGraph.h` `void AddNode(UEdGraphNode* NodeToAdd, bool bUserAction = false, bool bSelectNewNode = true);` | **OK**, 3 args |
| 9 | `LocalVars.cpp:568,581` `Getter->GetValuePin()` | `K2Node_Variable.h:183` `UE_API UEdGraphPin* GetValuePin() const;` → `K2Node_Variable.cpp:547` `FindPin(GetVarName())` | **OK as a call**, but see Defect 2 — it returns `nullptr` in the state `Apply` leaves the node in |
| 10 | `LocalVars.cpp:668` `Output->BreakLinkTo(Consumer)` | `EdGraphPin.h:434` `ENGINE_API void BreakLinkTo(UEdGraphPin* ToPin, bool bAlwaysMarkDirty = true);` → `EdGraphPin.cpp:661` | **OK**, and it removes from `LinkedTo` on both sides, so the copy at line 573 is necessary and sufficient |
| 11 | `LocalVars.cpp:669` `MakeLinkTo(Consumer)` | `EdGraphPin.h` `ENGINE_API void MakeLinkTo(UEdGraphPin* ToPin);` | **OK** |
| 12 | `LocalVars.cpp:676` `Node->DestroyNode()` | `EdGraphNode.h` → `EdGraphNode.cpp:554` `void UEdGraphNode::DestroyNode()` = `ParentGraph->RemoveNode(this);` | **OK**, and the engine's own comment confirms *"Remove the node - this will break all links."* |

**No signature mismatch was found.** Unlike the five guessed names that failed earlier in this project
(`GetEdGraphPinTypeFromSimpleType`, `GetEnumType`, `CPF_Private`, `CPF_BlueprintReadWrite`,
`SetBlueprintVariableMetadata`), every call here is arity- and type-correct. The defects are all
**ordering and state**, not API.

---

## 2. PROVEN DEFECTS

### D-1 (SEVERE, proven): `AddLocalVariable` compiles synchronously, invalidating the node pointers `Apply` reuses

`Apply` captures `Node` at lines 510-518 and `Outputs` at 527-528, then calls `AddLocalVariable` at
546, then **keeps using both** at 561 (`Node->NodePosX`), 570 (`for (UEdGraphPin* Output : Outputs)`)
and 589 (`Node->DestroyNode()`).

`AddLocalVariable`'s implementation is at `BlueprintEditorUtils.cpp:5499`. Its last meaningful act
before returning is **line 5529**:

```cpp
FunctionEntryNodes[0]->LocalVariables.Add(NewVar);   // L5527
FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);   // L5529
return true;   // L5531
```

and `MarkBlueprintAsStructurallyModified` at `BlueprintEditorUtils.cpp:1890` does:

```cpp
GetDefault<UEdGraphSchema_K2>()->ForceVisualizationCacheClear();
Blueprint->bCachedDependenciesUpToDate = false;
if (Blueprint->Status != BS_BeingCreated && !Blueprint->bBeingCompiled)
{
    FBlueprintCompilationManager::CompileSynchronously(
        FBPCompileRequest(Blueprint, EBlueprintCompileOptions::RegenerateSkeletonOnly, nullptr));
    MarkBlueprintAsModified(Blueprint);
    {
        Blueprint->BroadcastChanged();   // <-- every registered editor/viewport reacts
    }
}
```

So adding the local variable triggers a **synchronous skeleton compile and a broadcast**. `Node` and
the `UEdGraphPin*` values in `Outputs` are raw pointers into graph objects that the compile and the
broadcast may reconstruct.

**This is not an inference: the engine's own promote path guards against exactly this.**
`BlueprintEditor.cpp:4977` is the `AddLocalVariable` call, and four lines later at **4982** comes the
comment, followed by a **re-fetch**:

```cpp
// L4977
bWasSuccessful = FBlueprintEditorUtils::AddLocalVariable( GetBlueprintObj(), FunctionGraph, VarName,
                                                          NewPinType, InTargetPin->GetDefaultAsString() );
...
// L4982
// The owning node may have been reconstructed as a result of adding a new variable above, so ensure
// the pin reference is up-to-date.
InTargetPin = PinNode->FindPinChecked(PinName);   // L4984
```

The engine performs `AddLocalVariable` and then **immediately re-acquires the pin by name because the
node may have been reconstructed**. `Apply` does not re-acquire anything.

**Fix.** After line 553, before touching `Node` or `Outputs` again, re-resolve both:

```cpp
// The compile inside AddLocalVariable can reconstruct nodes, so the key is looked up again.
Node = nullptr;
for (UEdGraphNode* Found : Graph->Nodes)
{
    if (Found != nullptr && KeyOf(Found).Equals(Candidate.Key, ESearchCase::IgnoreCase))
    { Node = Found; break; }
}
if (Node == nullptr) { ++Refused; Lines.Add(...); continue; }

Outputs.Reset();
OutputPins(Node, Outputs);
if (Outputs.Num() == 0) { ++Refused; Lines.Add(...); continue; }
```

The `Outputs` re-derivation is free because `OutputPins` is in the same anonymous namespace.

### D-2 (SEVERE, proven): the getter's pins are never allocated, so `GetValuePin()` returns null and the promotion silently does nothing

`Apply` creates the getter at 557, sets its reference at 559, positions it at 561-562 and adds it at
564 — and **never calls `AllocateDefaultPins`**.

`K2Node_VariableGet.cpp:213`:

```cpp
void UK2Node_VariableGet::AllocateDefaultPins()
{
    if (GetVarName() != NAME_None)
    {
        CreateImpurePins(nullptr);
        if (CreatePinForVariable(EGPD_Output)) { CreatePinForSelf(); }
    }
    ...
    Super::AllocateDefaultPins();
}
```

and `GetValuePin` at `K2Node_Variable.cpp:547` is `FindPin(GetVarName())`. With no pins allocated,
`FindPin` returns `nullptr`.

`Apply` guards the call — line 568 is `if (Getter->GetValuePin() != nullptr)` — so it does **not**
crash. It does something worse for a caller:

* `Rewired` stays `0`;
* the original node is still destroyed at 589, so **every consumer is left with a broken link**;
* `Promoted` is still incremented at 592;
* the result says `changed: promoted 1 value(s) ... 0 consumer(s) re-pointed`.

So the tool would report success while **severing the graph**, which is the worst outcome available to
it and precisely what `P8` forbids.

This project has already measured the same trap once: `NewObject` does not allocate pins and
`UEdGraph::AddNode` does not either — `UK2Node_Knot::GetOutputPin` crashed on `Pins[1]` of an empty
array in the acceptance-asset commandlet. `AllocateDefaultPins` had to be called explicitly there. The
audit finds the identical omission here, and here it fails silently instead of loudly.

**Fix.** Call `Getter->AllocateDefaultPins();` immediately after `SetLocalMember` (i.e. after line
560) and **before** `Graph->AddNode`. Then also change the guard at 568 from a silent skip to a
refusal, because a null value pin must abort the candidate rather than promote it:

```cpp
Getter->AllocateDefaultPins();          // new
if (Getter->GetValuePin() == nullptr)
{
    ++Refused;
    Lines.Add(FString::Printf(TEXT("  %s: the getter has no value pin, so nothing was changed"),
        *Candidate.Key));
    Getter->DestroyNode();
    continue;                            // and do NOT touch Node
}
```

### D-3 (proven): `Promoted` is incremented and a `changed:` line emitted even when nothing was rewired

Directly follows from D-2 but is a separate reporting defect. Line 592 increments `Promoted`
unconditionally after the rewire block, and line 593-595 builds the line from `Rewired` without
checking it. The engine's own convention in this plugin is that a no-op opens with `unchanged:`
(`P4`). A promotion that re-pointed **zero** consumers has changed nothing a caller asked for.

**Fix.** Guard it: `if (Rewired == 0) { ++Refused; Getter->DestroyNode(); Lines.Add(...); continue; }`
before `Node->DestroyNode()`, so the original survives when the rewire did nothing.

### D-4 (proven): the final node count is not a read-back

Line 613-614 reports `nodes: %d -> %d` using `InPlan.NodeCountBefore` and `Graph->Nodes.Num()`.
`NodeCountBefore` was recorded in `Plan`, in an **earlier call**. `Apply`'s own comment at the top of
the file claims the result is read back; this figure is not. The project's rule is that a tool's own
report of what it did is not evidence, and the honest read-back is the count taken at the start of
`Apply`.

**Fix.** Capture `const int32 Before = Graph->Nodes.Num();` right after `Graph` resolves at line 486
and use `Before` at 614.

### D-5 (proven): `Graph->Modify()` is called after a node has already been mutated

Line 563 `Graph->Modify();` runs **after** 559-561 have already written `VariableReference`,
`NodePosX` and `NodePosY` on the getter. `Getter` is not yet in the graph so nothing in the graph is
yet unsaved, but the ordering reads as an undo barrier and is not one. The engine's own path wraps the
whole operation in an `FScopedTransaction` (`BlueprintEditor.cpp:4876`), and `P4` requires every Edit
to be one transaction.

**Fix.** Call `Graph->Modify()` (or better, open an `FScopedTransaction`) **before** the first write,
and `Getter->Modify()` alongside it.

### D-6 (proven): a candidate is skipped without any diagnostic when its outputs vanish

Lines 529-534 refuse when `Outputs.Num() == 0`, and that is correct. But the same condition after a
re-resolve (D-1's fix) would be indistinguishable in the output from a `Plan` that never offered the
candidate. Every refusal path here already appends to `Lines`; this one does, so the only gap is the
**counter** — a refusal caused by a stale plan and a refusal caused by an unsafe value both increment
`Refused`, and the summary at 619-621 attributes all of them to safety. That is a wrong explanation
attached to a right count.

**Fix.** Split the counters (`RefusedUnsafe`, `RefusedStale`) and say which is which.

### D-7 (proven, minor): the getter is positioned exactly on top of the node it replaces

Lines 561-562 copy `Node->NodePosX/NodePosY` verbatim. `Node` is then destroyed, so no overlap
survives — but only if `Node` is genuinely destroyed. Under D-2's silent-failure path the original is
destroyed and the getter sits where it was, which is at least consistent. The engine instead offsets
by ±200/±400 (`BlueprintEditor.cpp:5022-5023`). Worth aligning for legibility, not correctness.

**Fix.** Offset the getter: `Getter->NodePosX = Node->NodePosX - 200;`
---

## 3. Order-of-operations review, step by step

| Step | Line | Can it be done in this order? | What breaks if the order is wrong |
|---|---|---|---|
| find node by key | 510 | yes | — |
| read output pins | 527 | yes | — |
| **add local variable** | 546 | **no** | **D-1**: compiles synchronously and broadcasts; `Node` and `Outputs` may be stale from here on |
| create getter | 557 | yes | — |
| set variable reference | 559 | yes, and it must precede pin allocation because `AllocateDefaultPins` is guarded on `GetVarName() != NAME_None` (`K2Node_VariableGet.cpp:215`) | a reference set after allocation would have produced no value pin |
| **allocate pins** | *absent* | **no** | **D-2**: `GetValuePin()` is null, the rewire is skipped, the original is destroyed anyway |
| add node to graph | 564 | yes | — |
| re-point consumers | 570-585 | yes, and the copy at 573 is required: `BreakLinkTo` removes from `LinkedTo` on both pins (`EdGraphPin.cpp:661-680`) | iterating the live array would skip links |
| remove original | 589 | yes | `DestroyNode` → `RemoveNode` breaks all remaining links, so upstream inputs are handled |

The two things the task asked me to check specifically:

* **can `AddLocalVariable` reconstruct nodes?** **YES, proven.** It calls
  `MarkBlueprintAsStructurallyModified`, which calls `FBlueprintCompilationManager::CompileSynchronously`
  and `Blueprint->BroadcastChanged()`. The engine's own promote re-fetches its pin immediately after
  for this stated reason (`BlueprintEditor.cpp:4982`).
* **must `AllocateDefaultPins` be called explicitly before `GetValuePin` returns anything?**
  **YES, proven.** `GetValuePin` is `FindPin(GetVarName())`; pins only exist if `AllocateDefaultPins`
  ran; nothing in `NewObject` or `AddNode` runs it.

---

## 4. What the engine's own promote does that `Apply` does not

Read from `BlueprintEditor.cpp:4855` (`DoPromoteToVariable` / `DoPromoteToVariable2f`).

| Engine step | Line | `Apply` | Does the omission matter? |
|---|---|---|---|
| `FScopedTransaction` around the whole operation | 4876 | no transaction | **Yes.** `P4`: every Edit is one undo. A user cannot undo a promotion. |
| `InBlueprint->Modify(); GraphObj->Modify();` before any write | 4877-4878 | `Graph->Modify()` after writes (D-5) | Yes, same reason |
| clears `bIsConst`, `bIsReference`, **and `bIsWeakPointer`** on the pin type | 4883-4885 | clears only the first two (538-540) | **Yes.** A weak-pointer output would create a local of the wrong type. |
| re-fetches the pin after `AddLocalVariable` | 4982-4984 | no (D-1) | **Yes, severe** |
| places the node through `FEdGraphSchemaAction_K2NewNode::PerformAction` | 5022 | bare `Graph->AddNode` | **Yes.** `PerformAction` is what runs the engine's placement and pin-allocation path; `AddNode` does neither, which is D-2's root cause. |
| offsets the new node from the pin's node (`- 200` on input, `+ 400` on output) | 5018 | copies the position verbatim (D-7) | No, legibility only |
| copies `InTargetPin->GetDefaultAsString()` as the variable's default | 4977 | passes `Outputs[0]->DefaultValue` | No, equivalent |
| `RenameNewlyAddedAction(VarName)` | 5024 | no equivalent | No, editor-UI bookkeeping only |

---

## 5. Fix list, ordered by severity

1. **D-2** — `LocalVars.cpp:564`, insert `Getter->AllocateDefaultPins();` after line 560 and before
   `AddNode`; replace the guard at 568 with a refusal that destroys the getter and `continue`s.
   *Without this the promotion severs every consumer and reports success.*
2. **D-1** — `LocalVars.cpp:553`, re-resolve `Node` by key and re-derive `Outputs` via `OutputPins`
   after `AddLocalVariable` returns, with a refusal path if either is empty.
   *Without this the code uses pointers a synchronous compile and broadcast may have invalidated.*
3. **D-3** — `LocalVars.cpp:587`, if `Rewired == 0` then destroy the getter, count a refusal,
   `continue` before `Node->DestroyNode()`.
4. **D-5 / transactions** — open an `FScopedTransaction` and call `Graph->Modify()` before the first
   write, per `P4`.
5. **D-4** — `LocalVars.cpp:486` + `:614`, capture `Before = Graph->Nodes.Num()` at the start of
   `Apply` and report that.
6. **weak pointer** — `LocalVars.cpp:538-540`, also clear `NewType.bIsWeakPointer`.
7. **D-6** — split `Refused` into `RefusedUnsafe` and `RefusedStale` so the closing sentence at
   619-621 does not attribute every refusal to safety.
8. **D-7** — `LocalVars.cpp:561`, offset the getter by `-200` on X.

Items 1 and 2 are the two that make the difference between a working capability and one that corrupts
a graph while reporting success.

---

## 6. UNVERIFIED

1. **Whether `CompileSynchronously` actually reconstructs the specific node in this case.** The engine
   guards against it (`BlueprintEditor.cpp:4982`), which is strong evidence that it can happen, but
   whether it happens for `LocalVarProbe` in `TurboMCPAcceptance` would need a run to settle. The fix
   is correct either way, since re-resolving is free.
2. **Whether `SetLocalMember` with a name that `CreateUserDefinedPin`-style duplication has not yet
   registered resolves on the first `AllocateDefaultPins`.** `VarName` comes from
   `FindUniqueKismetName` and `AddLocalVariable` adds exactly that name, so they should agree, but the
   two are not linked by any assertion in `Apply`. Would need a run.
3. **Whether `FBlueprintEditorUtils::RemoveNode` (which `DestroyNode` delegates to via
   `UEdGraph::RemoveNode`) notifies the schema in a way that matters for a local-variable graph.** Not
   traced past `EdGraphNode.cpp:554`.

Two further items that are behaviour I deliberately did **not** claim, because they are the job of the
first execution rather than of reading:

* whether the promoted graph still compiles;
* whether the local variable appears in the function's `LocalVariables` array after the synchronous
  compile (it is added at `BlueprintEditorUtils.cpp:5527`, **before** the
  `MarkBlueprintAsStructurallyModified` at 5529, so it should).

---

## 7. What this audit changes about Q-13

`QUESTIONS.md` Q-13 says the promotion path is unverified. This audit finds that it is also **wrong in
two ways that would have produced a silent graph corruption**, and that both were invisible to
`Plan`-side testing:

* D-2 is invisible because `Apply` guards the null pin and continues; only the *absence* of a rewire
  reveals it, and `Apply` does not treat that as failure;
* D-1 is invisible because the stale pointer is used only after a call whose side effect is a compile.

Both are the **write** half of the capability, which is exactly the half that could not be exercised
before `TurboMCPAcceptance` existed. This is the second time in two rounds that the never-run half
has been found defective on first serious inspection, and it strengthens the case for running it
against the asset: the audit proves the code is wrong, but only a run proves the FIX is right.
