// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0

#include "TurboMCPGraphSwapCall.h"

#include "TurboMCPAssetResolver.h"
#include "TurboMCPNodeId.h"
#include "TurboMCPNodeType.h"
#include "TurboMCPRollback.h"

#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraph/EdGraphSchema.h"
#include "Engine/Blueprint.h"
#include "K2Node_CallFunction.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "ScopedTransaction.h"
#include "Editor.h"
#include "Editor/TransBuffer.h"

namespace
{
	UEdGraph* FindGraph(UBlueprint* Blueprint, const FString& Name)
	{
        int32 Matches = 0;
        UEdGraph* Graph = TurboMCPNodeId::FindGraphByName(Blueprint, Name, Matches);
        return Matches == 1 ? Graph : nullptr;
	}

	UEdGraphNode* NodeById(UEdGraph* Graph, const FString& Id)
	{
		if (Graph == nullptr) return nullptr;
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (Node != nullptr && TurboMCPNodeId::MakeForNode(Node, true) == Id) return Node;
		}
		return nullptr;
	}

	UEdGraphPin* PinByName(UEdGraphNode* Node, const FString& Name, EEdGraphPinDirection Direction)
	{
		if (Node == nullptr) return nullptr;
		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin != nullptr && Pin->Direction == Direction && Pin->PinName.ToString() == Name) return Pin;
		}
		return nullptr;
	}

    // Keep all three default representations, including FText namespace/key identity.
    bool TransferDefaults(UEdGraphNode* Old, UEdGraphNode* Replacement, bool Apply, FString& Error, bool VerifyOnly = false)
    {
        const UEdGraphSchema* Schema = Replacement->GetSchema();
        if (!Schema) { Error = TEXT("replacement has no schema"); return false; }
        struct FDefault { FString Name; FString Value; UObject* Object; FText Text; };
        TArray<FDefault> Defaults;
        for (UEdGraphPin* Pin : Old->Pins)
        {
            if (!Pin || Pin->Direction != EGPD_Input || Pin->LinkedTo.Num() || Pin->PinType.PinCategory == TEXT("exec") || Pin->bDefaultValueIsIgnored) continue;
            UEdGraphPin* Dest = PinByName(Replacement, Pin->PinName.ToString(), EGPD_Input);
            if (Pin->bOrphanedPin || Pin->ParentPin || Pin->SubPins.Num() || !Dest || Dest->bOrphanedPin || Dest->ParentPin || Dest->SubPins.Num()
                || !(Dest->PinType == Pin->PinType) || Dest->bDefaultValueIsIgnored)
            { Error = FString::Printf(TEXT("cannot preserve default for pin '%s': missing, split, orphaned or incompatible pin"), *Pin->PinName.ToString()); return false; }
            const bool Equal = Dest->DefaultValue == Pin->DefaultValue && Dest->DefaultObject == Pin->DefaultObject && Dest->DefaultTextValue.EqualTo(Pin->DefaultTextValue);
            if (VerifyOnly && !Equal) { Error = TEXT("default changed during link migration"); return false; }
            if ((!Equal && Dest->bDefaultValueIsReadOnly) || !Schema->IsPinDefaultValid(Dest, Pin->DefaultValue, Pin->DefaultObject, Pin->DefaultTextValue).IsEmpty())
            { Error = FString::Printf(TEXT("replacement refuses default for pin '%s'"), *Pin->PinName.ToString()); return false; }
            Defaults.Add({Pin->PinName.ToString(), Pin->DefaultValue, Pin->DefaultObject, Pin->DefaultTextValue});
        }
        if (!Apply) return true;
        Replacement->Modify();
        for (const FDefault& Value : Defaults)
        {
            UEdGraphPin* Dest = PinByName(Replacement, Value.Name, EGPD_Input);
            if (!Dest) { Error = TEXT("default pin disappeared during migration"); return false; }
            Dest->DefaultValue = Value.Value;
            Dest->DefaultObject = Value.Object;
            Dest->DefaultTextValue = Value.Text;
            Replacement->PinDefaultValueChanged(Dest);
        }
        for (const FDefault& Value : Defaults)
        {
            const UEdGraphPin* Dest = PinByName(Replacement, Value.Name, EGPD_Input);
            if (!Dest || Dest->DefaultValue != Value.Value || Dest->DefaultObject != Value.Object || !Dest->DefaultTextValue.EqualTo(Value.Text))
            { Error = TEXT("default readback differs after node callback"); return false; }
        }
        return true;
    }

	bool HasLink(const UEdGraphPin* A, const UEdGraphPin* B)
	{
		return A != nullptr && B != nullptr && A->LinkedTo.Contains(B) && B->LinkedTo.Contains(A);
	}

	bool ParseKeepOld(const FString& Value, bool& OutKeepOld)
	{
		const FString Normal = Value.TrimStartAndEnd().ToLower();
		if (Normal.IsEmpty() || Normal == TEXT("true") || Normal == TEXT("yes"))
		{
			OutKeepOld = true;
			return true;
		}
		if (Normal == TEXT("false") || Normal == TEXT("no"))
		{
			OutKeepOld = false;
			return true;
		}
		return false;
	}
}

bool FTurboMCPGraphSwapCall::Plan(const FString& InAsset, const FString& InGraphName,
	const FString& InNodeId, const FString& InReplacementTypeId, const FString& InKeepOld,
	FPlan& OutPlan, FString& OutError)
{
	OutPlan = FPlan();
	OutError.Empty();
	OutPlan.Asset = InAsset.TrimStartAndEnd();
	OutPlan.GraphName = InGraphName.TrimStartAndEnd();
	OutPlan.OldNodeId = InNodeId.TrimStartAndEnd();
	OutPlan.ReplacementTypeId = InReplacementTypeId.TrimStartAndEnd();
	if (!ParseKeepOld(InKeepOld, OutPlan.bKeepOld))
	{
		OutError = TEXT("keepOld must be true or false; true is the safe default");
		return false;
	}

	FTurboMCPAssetResolver::FResolved Resolved;
	const FString ResolveError = FTurboMCPAssetResolver::Resolve(OutPlan.Asset, Resolved);
	if (!ResolveError.IsEmpty()) { OutError = ResolveError; return false; }
	UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *Resolved.ObjectPath);
	UEdGraph* Graph = FindGraph(Blueprint, OutPlan.GraphName);
	if (Blueprint == nullptr || Graph == nullptr)
	{
		OutError = FString::Printf(TEXT("no graph named '%s' in '%s'"), *OutPlan.GraphName, *Resolved.PackagePath);
		return false;
	}
	OutPlan.NodeCountBefore = Graph->Nodes.Num();
	UEdGraphNode* Old = NodeById(Graph, OutPlan.OldNodeId);
	if (Old == nullptr)
	{
		OutError = FString::Printf(TEXT("node '%s' is not in graph '%s'"), *OutPlan.OldNodeId, *OutPlan.GraphName);
		return false;
	}
	UK2Node_CallFunction* OldCall = Cast<UK2Node_CallFunction>(Old);
	if (OldCall == nullptr)
	{
		OutPlan.Refusals.Add(FString::Printf(TEXT("node '%s' is %s, not a UK2Node_CallFunction"),
			*OutPlan.OldNodeId, *Old->GetClass()->GetName()));
		return true;
	}
	OutPlan.OldClass = Old->GetClass()->GetName();
	OutPlan.OldTitle = TurboMCPNodeId::GetNodeTitle(Old);

	FString ProbeNote;
	UEdGraphNode* Probe = FTurboMCPNodeType::BuildProbeNode(Blueprint, Graph, OutPlan.ReplacementTypeId, ProbeNote);
	if (Probe == nullptr)
	{
		OutPlan.Refusals.Add(FString::Printf(TEXT("replacement type '%s' is not usable here: %s"),
			*OutPlan.ReplacementTypeId, *ProbeNote));
		return true;
	}
	if (Cast<UK2Node_CallFunction>(Probe) == nullptr)
	{
		OutPlan.Refusals.Add(FString::Printf(TEXT("replacement type '%s' resolves to %s, not a call node"),
			*OutPlan.ReplacementTypeId, *Probe->GetClass()->GetName()));
		return true;
	}
	OutPlan.NewClass = Probe->GetClass()->GetName();
    FString DefaultError;
    if (!TransferDefaults(Old, Probe, false, DefaultError)) OutPlan.Refusals.Add(DefaultError);

	for (UEdGraphPin* OldPin : Old->Pins)
	{
		if (OldPin == nullptr || OldPin->LinkedTo.Num() == 0) continue;
		UEdGraphPin* NewPin = PinByName(Probe, OldPin->PinName.ToString(), OldPin->Direction);
		if (NewPin == nullptr)
		{
			OutPlan.UnmatchedPins.Add(OldPin->PinName.ToString());
			continue;
		}
		for (UEdGraphPin* Other : OldPin->LinkedTo)
		{
			if (Other == nullptr || Other->GetOwningNode() == nullptr) continue;
			FLink Link;
			Link.PinName = OldPin->PinName.ToString();
			Link.OtherNodeId = TurboMCPNodeId::MakeForNode(Other->GetOwningNode(), true);
			Link.OtherPinName = Other->PinName.ToString();
			Link.bOldPinIsOutput = OldPin->Direction == EGPD_Output;
			OutPlan.Links.Add(MoveTemp(Link));
		}
	}
	for (const FString& Pin : OutPlan.UnmatchedPins)
	{
		OutPlan.Refusals.Add(FString::Printf(TEXT("linked pin '%s' has no same-named pin on replacement; its link was not changed"), *Pin));
	}
	return true;
}

bool FTurboMCPGraphSwapCall::ApplyCore(const FPlan& InPlan, const FApplyContext& Context, FString& OutReport, FString& OutError)
{
	OutReport.Empty();
	OutError.Empty();
    UTransBuffer* Buffer = GEditor ? Cast<UTransBuffer>(GEditor->Trans) : nullptr;
    if (!IsInGameThread() || !GEditor || !Buffer || Buffer->ActiveCount != 1 || GEditor->IsPlaySessionInProgress() || Context.bCompile || Context.bSave)
    { OutError = TEXT("unchanged: swap requires one caller-owned transaction; compile/save must be separate"); return false; }
	if (!InPlan.bAcceptable())
	{
		OutReport = RenderPreview(InPlan);
		return true;
	}
	FTurboMCPAssetResolver::FResolved Resolved;
	const FString ResolveError = FTurboMCPAssetResolver::Resolve(InPlan.Asset, Resolved);
	if (!ResolveError.IsEmpty()) { OutError = ResolveError; return false; }
	UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *Resolved.ObjectPath);
	UEdGraph* Graph = FindGraph(Blueprint, InPlan.GraphName);
	UEdGraphNode* Old = NodeById(Graph, InPlan.OldNodeId);
	if (Blueprint == nullptr || Graph == nullptr || Old == nullptr || !Graph->GetSchema() || Graph->Nodes.Num() != InPlan.NodeCountBefore) { OutError = TEXT("swap plan no longer resolves"); return false; }

	Blueprint->Modify();
	Graph->Modify();
	Old->Modify();
    for (UEdGraphNode* Node : Graph->Nodes) if (Node) Node->Modify();
    const int64 NewX = int64(Old->NodePosX) + (InPlan.bKeepOld ? 240 : 0);
    if (NewX > MAX_int32 || NewX < MIN_int32)
    { OutError = TEXT("replacement position exceeds supported coordinates"); return false; }
	FString SpawnNote;
	UEdGraphNode* NewNode = nullptr;
	if (FTurboMCPNodeType::SpawnNodeOfType(Blueprint, Graph, InPlan.ReplacementTypeId, NewNode, SpawnNote)
		!= FTurboMCPNodeType::ESpawnResult::Spawned || NewNode == nullptr)
	{
		OutError = FString::Printf(TEXT("replacement could not be spawned: %s"), *SpawnNote);
		return false;
	}
	NewNode->NodePosX = int32(NewX);
	NewNode->NodePosY = Old->NodePosY;
    if (!TransferDefaults(Old, NewNode, true, OutError)) return false;
	for (const FLink& Link : InPlan.Links)
	{
		UEdGraphPin* OldPin = PinByName(Old, Link.PinName, Link.bOldPinIsOutput ? EGPD_Output : EGPD_Input);
		UEdGraphNode* OtherNode = NodeById(Graph, Link.OtherNodeId);
		UEdGraphPin* OtherPin = PinByName(OtherNode, Link.OtherPinName,
			Link.bOldPinIsOutput ? EGPD_Input : EGPD_Output);
		UEdGraphPin* NewPin = PinByName(NewNode, Link.PinName,
			Link.bOldPinIsOutput ? EGPD_Output : EGPD_Input);
		if (OldPin == nullptr || OtherPin == nullptr || NewPin == nullptr)
		{
			OutError = FString::Printf(TEXT("link migration lost pin '%s'; caller must roll back"), *Link.PinName);
			return false;
		}
		Graph->GetSchema()->BreakSinglePinLink(OldPin, OtherPin);
		if (!Graph->GetSchema()->TryCreateConnection(
			Link.bOldPinIsOutput ? NewPin : OtherPin,
			Link.bOldPinIsOutput ? OtherPin : NewPin)
			|| !HasLink(NewPin, OtherPin))
		{
			OutError = FString::Printf(TEXT("link migration failed for pin '%s'; caller must roll back"), *Link.PinName);
			return false;
		}
	}
    // Revalidate defaults after link callbacks, before deleting the source.
    if (!TransferDefaults(Old, NewNode, false, OutError, true)) return false;
	if (!InPlan.bKeepOld)
	{
		FBlueprintEditorUtils::RemoveNode(Blueprint, Old, true);
	}
	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
    const int32 Expected = InPlan.NodeCountBefore + (InPlan.bKeepOld ? 1 : 0);
    if (Graph->Nodes.Num() != Expected || !Graph->Nodes.Contains(NewNode) || (!InPlan.bKeepOld && Graph->Nodes.Contains(Old)))
    { OutError = TEXT("swap node-set readback failed; caller must roll back"); return false; }
    for (const FLink& Link : InPlan.Links)
    {
        UEdGraphPin* NewPin = PinByName(NewNode, Link.PinName, Link.bOldPinIsOutput ? EGPD_Output : EGPD_Input);
        UEdGraphPin* OtherPin = PinByName(NodeById(Graph, Link.OtherNodeId), Link.OtherPinName, Link.bOldPinIsOutput ? EGPD_Input : EGPD_Output);
        if (!HasLink(NewPin, OtherPin))
        { OutError = TEXT("swap final link readback failed; caller must roll back"); return false; }
    }
	OutReport = FString::Printf(TEXT("changed: swapped %s to %s; %d link(s) migrated by pin name; old node %s"),
		*InPlan.OldNodeId, *TurboMCPNodeId::MakeForNode(NewNode, true), InPlan.Links.Num(),
		InPlan.bKeepOld ? TEXT("retained unplugged") : TEXT("deleted"));
    OutReport += TEXT("\ndefaults=preserved; compile=not_run; save=not_requested; run EditTurboMCPCompile explicitly after editing");
	return true;
}

bool FTurboMCPGraphSwapCall::Apply(const FPlan& InPlan, FString& OutReport, FString& OutError)
{
	if (!InPlan.bAcceptable())
	{
		OutReport = RenderPreview(InPlan);
		OutError.Empty();
		return true;
	}
    if (!IsInGameThread() || !GEditor || GEditor->IsTransactionActive() || GEditor->IsPlaySessionInProgress())
    { OutReport.Empty(); OutError = TEXT("unchanged: swap requires an idle editor outside an active transaction"); return false; }
    TUniquePtr<FScopedTransaction> Transaction = MakeUnique<FScopedTransaction>(FText::FromString(TEXT("TurboMCP swap call node")));
	FApplyContext Context;
	if (!ApplyCore(InPlan, Context, OutReport, OutError))
	{
        UTransBuffer* Buffer = Cast<UTransBuffer>(GEditor->Trans);
        const FGuid OwnedId = Buffer && Buffer->ActiveCount == 1 && !Buffer->UndoBuffer.IsEmpty() ? Buffer->UndoBuffer.Last()->GetId() : FGuid();
        Transaction.Reset();
        const int32 Index = Buffer ? Buffer->GetQueueLength() - Buffer->GetUndoCount() - 1 : -1;
        const FTransaction* Record = Index >= 0 ? Buffer->GetTransaction(Index) : nullptr;
        const bool Undone = OwnedId.IsValid() && Record && Record->GetId() == OwnedId && GEditor->UndoTransaction(false);
        OutError = (Undone ? TEXT("unchanged: rollback=complete; ") : TEXT("partial: rollback=not_confirmed; ")) + OutError;
        OutReport.Empty();
		return false;
	}
	return true;
}
FString FTurboMCPGraphSwapCall::RenderPreview(const FPlan& InPlan)
{
	FString Out = TEXT("unchanged: call swap preview");
	if (InPlan.Refusals.Num() > 0)
	{
		Out += TEXT("\nrefused:");
		for (const FString& Refusal : InPlan.Refusals) Out += FString::Printf(TEXT("\n- %s"), *Refusal);
		return Out;
	}
	Out += FString::Printf(TEXT("\nold: %s (%s, %s)\nreplacement: %s (%s)\nlinks to migrate: %d\nkeepOld: %s"),
		*InPlan.OldNodeId, *InPlan.OldClass, *InPlan.OldTitle, *InPlan.ReplacementTypeId, *InPlan.NewClass,
		InPlan.Links.Num(), InPlan.bKeepOld ? TEXT("true") : TEXT("false"));
	for (const FLink& Link : InPlan.Links)
	{
		Out += FString::Printf(TEXT("\n- %s -> %s:%s"), *Link.PinName, *Link.OtherNodeId, *Link.OtherPinName);
	}
	return Out;
}
