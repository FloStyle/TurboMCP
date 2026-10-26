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
#include "K2Node_PromotableOperator.h"
#include "BlueprintTypePromotion.h"
#include "EdGraphSchema_K2.h"
#include "UObject/UnrealType.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "ScopedTransaction.h"
#include "Editor.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
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
        UEdGraphNode* Match = nullptr;
		for (UEdGraphNode* Node : Graph->Nodes)
		{
            if (Node && (TurboMCPNodeId::MakeForNode(Node, true) == Id
                || TurboMCPNodeId::MakeForNode(Node, false) == Id))
            {
                if (Match && Match != Node) return nullptr;
                Match = Node;
            }
		}
		return Match;
	}

	UEdGraphPin* PinByName(UEdGraphNode* Node, const FString& Name, EEdGraphPinDirection Direction)
	{
		if (Node == nullptr) return nullptr;
        UEdGraphPin* Match = nullptr;
		for (UEdGraphPin* Pin : Node->Pins)
		{
            if (Pin != nullptr && Pin->Direction == Direction && Pin->PinName.ToString() == Name)
            {
                if (Match != nullptr) return nullptr;
                Match = Pin;
            }
		}
		return Match;
	}

    bool IsImplicitStaticReceiver(const UEdGraphNode* Node, const UEdGraphPin* Pin)
    {
        const UK2Node_CallFunction* Call = Cast<UK2Node_CallFunction>(Node);
        const UFunction* Function = Call ? Call->GetTargetFunction() : nullptr;
        return Pin && Pin->PinName == UEdGraphSchema_K2::PN_Self && Pin->bHidden
            && Pin->LinkedTo.IsEmpty() && Function && Function->HasAnyFunctionFlags(FUNC_Static);
    }

    // Explicit receiver changes are permitted only for instance functions owned by self.
    bool ConfigureSelf(UBlueprint* Blueprint, UEdGraphNode* Node, FString& Error)
    {
        UK2Node_CallFunction* Call = Cast<UK2Node_CallFunction>(Node);
        UFunction* Function = Call ? Call->GetTargetFunction() : nullptr;
        UClass* SelfClass = Blueprint ? Blueprint->SkeletonGeneratedClass : nullptr;
        if (!Function || Function->HasAnyFunctionFlags(FUNC_Static) || !SelfClass
            || !(SelfClass->IsChildOf(Function->GetOwnerClass())
                || (Blueprint->GeneratedClass && Blueprint->GeneratedClass->IsChildOf(Function->GetOwnerClass()))))
        { Error = TEXT("target_self requires an instance function available on this Blueprint; static or unrelated functions are refused"); return false; }
        UEdGraphPin* Receiver = PinByName(Node, UEdGraphSchema_K2::PN_Self.ToString(), EGPD_Input);
        if (!Receiver || Receiver->bOrphanedPin || Receiver->ParentPin || !Receiver->SubPins.IsEmpty() || !Receiver->LinkedTo.IsEmpty())
        { Error = TEXT("target_self requires an unlinked, unsplit receiver pin on the replacement"); return false; }
        Receiver->DefaultValue.Empty(); Receiver->DefaultObject = nullptr; Receiver->DefaultTextValue = FText::GetEmpty();
        Call->FunctionReference.SetSelfMember(Function->GetFName());
        return true;
    }

    // Discovery exposes promotable operators as wildcards. Resolve a concrete
    // engine overload from the old call's mapped pins before testing defaults.
    // Only the probe/new node is changed; no editor conversion transaction is used.
    bool SpecializeOperator(UEdGraphNode* Old, UEdGraphNode* Replacement,
        const TMap<FString, FString>& PinMap, FString& Error)
    {
        UK2Node_PromotableOperator* Op = Cast<UK2Node_PromotableOperator>(Replacement);
        if (!Op) return true;
        TArray<UEdGraphPin*> Hints;
        for (UEdGraphPin* P : Old->Pins)
        {
            if (!P || IsImplicitStaticReceiver(Old, P) || P->PinType.PinCategory == TEXT("exec")) continue;
            const FString Name = P->PinName.ToString();
            UEdGraphPin* Dest = PinByName(Op, PinMap.Contains(Name) ? PinMap[Name] : Name, P->Direction);
            if (!Dest || P->bOrphanedPin || P->ParentPin || !P->SubPins.IsEmpty()) continue;
            if (Dest->PinType.PinCategory == TEXT("wildcard")) Dest->PinType = P->PinType;
            Hints.Add(Dest);
        }
        UFunction* Function = FTypePromotion::FindBestMatchingFunc(Op->GetOperationName(), Hints);
        if (!Function) { Error = TEXT("no compatible concrete operator overload for the mapped pins"); return false; }
        const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
        for (UEdGraphPin* P : Hints)
        {
            const FProperty* Property = FindFProperty<FProperty>(Function, P->PinName);
            FEdGraphPinType Concrete;
            if (!Property || !Schema->ConvertPropertyToPinType(Property, Concrete) || !(Concrete == P->PinType))
            { Error = TEXT("operator replacement requires an implicit conversion; use matching concrete pin types"); return false; }
            Schema->SetPinAutogeneratedDefaultValueBasedOnType(P);
        }
        Op->SetFromFunction(Function);
        return true;
    }

    // Keep all three default representations, including FText namespace/key identity.
    bool TransferDefaults(UEdGraphNode* Old, UEdGraphNode* Replacement, bool Apply, FString& Error, const TMap<FString, FString>& PinMap, bool VerifyOnly = false, bool TargetSelf = false)
    {
        const UEdGraphSchema* Schema = Replacement->GetSchema();
        if (!Schema) { Error = TEXT("replacement has no schema"); return false; }
        struct FDefault { FString Name; FString Value; UObject* Object; FText Text; };
        TArray<FDefault> Defaults;
        for (UEdGraphPin* Pin : Old->Pins)
        {
            if (!Pin || (TargetSelf && Pin->PinName == UEdGraphSchema_K2::PN_Self) || IsImplicitStaticReceiver(Old, Pin) || Pin->Direction != EGPD_Input || Pin->LinkedTo.Num() || Pin->PinType.PinCategory == TEXT("exec") || Pin->bDefaultValueIsIgnored) continue;
            UEdGraphPin* Dest = PinByName(Replacement, PinMap.Contains(Pin->PinName.ToString()) ? PinMap[Pin->PinName.ToString()] : Pin->PinName.ToString(), EGPD_Input);
            if (Pin->bOrphanedPin || Pin->ParentPin || Pin->SubPins.Num() || !Dest || Dest->bOrphanedPin || Dest->ParentPin || Dest->SubPins.Num()
                || !(Dest->PinType == Pin->PinType) || Dest->bDefaultValueIsIgnored)
            { Error = FString::Printf(TEXT("cannot preserve default for pin '%s': missing, split, orphaned or incompatible pin"), *Pin->PinName.ToString()); return false; }
            const bool Equal = Dest->DefaultValue == Pin->DefaultValue && Dest->DefaultObject == Pin->DefaultObject && Dest->DefaultTextValue.EqualTo(Pin->DefaultTextValue);
            if (VerifyOnly && !Equal) { Error = TEXT("default changed during link migration"); return false; }
            if ((!Equal && Dest->bDefaultValueIsReadOnly) || !Schema->IsPinDefaultValid(Dest, Pin->DefaultValue, Pin->DefaultObject, Pin->DefaultTextValue).IsEmpty())
            { Error = FString::Printf(TEXT("replacement refuses default for pin '%s'"), *Pin->PinName.ToString()); return false; }
            Defaults.Add({Dest->PinName.ToString(), Pin->DefaultValue, Pin->DefaultObject, Pin->DefaultTextValue});
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
	FPlan& OutPlan, FString& OutError, const FString& Options)
{
	OutPlan = FPlan();
	OutError.Empty();
	OutPlan.Asset = InAsset.TrimStartAndEnd();
	OutPlan.GraphName = InGraphName.TrimStartAndEnd();
	OutPlan.OldNodeId = InNodeId.TrimStartAndEnd();
	OutPlan.ReplacementTypeId = InReplacementTypeId.TrimStartAndEnd();
	if (!ParseKeepOld(InKeepOld, OutPlan.bKeepOld))
	{
		OutError = TEXT("KeepOld must be true/yes or false/no; empty means true and retains the old node disconnected");
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
		OutError = FString::Printf(TEXT("node '%s' is missing or ambiguous in graph '%s'; use an exact reader node ID"), *OutPlan.OldNodeId, *OutPlan.GraphName);
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
	TSharedPtr<FJsonObject> Config;
	if (!Options.TrimStartAndEnd().IsEmpty())
	{
		if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Options), Config) || !Config)
		{ OutError = TEXT("Options must be a JSON object"); return false; }
		for (const auto& Field : Config->Values)
			if (FString(*Field.Key) != TEXT("pin_map") && FString(*Field.Key) != TEXT("new_inputs") && FString(*Field.Key) != TEXT("target_self"))
			{ OutError = TEXT("unknown Options field; expected pin_map, new_inputs or target_self"); return false; }
        if (Config->HasField(TEXT("target_self")) && !Config->TryGetBoolField(TEXT("target_self"), OutPlan.bTargetSelf))
        { OutError = TEXT("target_self must be a JSON boolean"); return false; }
		const TSharedPtr<FJsonObject>* Map = nullptr;
		if (Config->HasField(TEXT("pin_map")))
		{
			if (!Config->TryGetObjectField(TEXT("pin_map"), Map) || !Map || !Map->IsValid())
			{ OutError = TEXT("pin_map must be an object"); return false; }
			TSet<FString> Targets;
			for (const auto& Pair : (*Map)->Values)
			{
				FString Name;
				if (!Pair.Value || !Pair.Value->TryGetString(Name) || Name.IsEmpty() || Targets.Contains(Name))
				{ OutError = TEXT("pin_map requires unique nonempty destination pin names"); return false; }
				const FString OldName(*Pair.Key);
				UEdGraphPin* OldPin = PinByName(Old, OldName, EGPD_Input);
				if (!OldPin) OldPin = PinByName(Old, OldName, EGPD_Output);
				UEdGraphPin* Dest = OldPin ? PinByName(Probe, Name, OldPin->Direction) : nullptr;
				if (!Dest || OldPin->bOrphanedPin || Dest->bOrphanedPin || (!(OldPin->PinType == Dest->PinType) && !(Cast<UK2Node_PromotableOperator>(Probe) && Dest->PinType.PinCategory == TEXT("wildcard"))))
				{ OutError = TEXT("pin_map references missing or incompatible pins"); return false; }
				Targets.Add(Name); OutPlan.PinMap.Add(OldName, Name);
			}
		}
	}

    if (OutPlan.bTargetSelf)
    {
        if (OutPlan.PinMap.Contains(TEXT("self")) || OutPlan.PinMap.FindKey(TEXT("self")))
        { OutError = TEXT("target_self cannot be combined with a self pin mapping"); return false; }
        if (!ConfigureSelf(Blueprint, Probe, OutError)) return false;
    }
    if (!SpecializeOperator(Old, Probe, OutPlan.PinMap, OutError)) return false;
	TSet<FString> Destinations;
	for (UEdGraphPin* Pin : Old->Pins)
	{
		if (!Pin) continue;
		const FString Name = Pin->PinName.ToString();
		const FString Target = OutPlan.PinMap.Contains(Name) ? OutPlan.PinMap[Name] : Name;
		const FString Key = FString::FromInt(int32(Pin->Direction)) + TEXT(":") + Target;
		if (Destinations.Contains(Key)) { OutError = TEXT("pin_map merges two source pins; unchanged"); return false; }
		Destinations.Add(Key);
	}
    FString DefaultError;
    if (!TransferDefaults(Old, Probe, false, DefaultError, OutPlan.PinMap, false, OutPlan.bTargetSelf)) OutPlan.Refusals.Add(DefaultError);

	for (UEdGraphPin* OldPin : Old->Pins)
	{
		if (OldPin == nullptr || (OutPlan.bTargetSelf && OldPin->PinName == UEdGraphSchema_K2::PN_Self) || OldPin->LinkedTo.Num() == 0) continue;
		const FString DestName = OutPlan.PinMap.Contains(OldPin->PinName.ToString()) ? OutPlan.PinMap[OldPin->PinName.ToString()] : OldPin->PinName.ToString();
		UEdGraphPin* NewPin = PinByName(Probe, DestName, OldPin->Direction);
		if (NewPin == nullptr || OldPin->bOrphanedPin || NewPin->bOrphanedPin || !(NewPin->PinType == OldPin->PinType))
		{
			OutPlan.UnmatchedPins.Add(OldPin->PinName.ToString());
			continue;
		}
		for (UEdGraphPin* Other : OldPin->LinkedTo)
		{
			if (Other == nullptr || Other->GetOwningNode() == nullptr) continue;
			FLink Link;
			Link.PinName = OldPin->PinName.ToString();
			Link.NewPinName = DestName;
			Link.OtherNodeId = TurboMCPNodeId::MakeForNode(Other->GetOwningNode(), true);
			Link.OtherPinName = Other->PinName.ToString();
			Link.bOldPinIsOutput = OldPin->Direction == EGPD_Output;
			OutPlan.Links.Add(MoveTemp(Link));
		}
	}
	if (Config && Config->HasField(TEXT("new_inputs")))
	{
		const TSharedPtr<FJsonObject>* Inputs = nullptr;
		if (!Config->TryGetObjectField(TEXT("new_inputs"), Inputs) || !Inputs || !Inputs->IsValid())
		{ OutError = TEXT("new_inputs must be an object"); return false; }
		for (const auto& Pair : (*Inputs)->Values)
		{
			const TSharedPtr<FJsonObject>* Spec = nullptr;
			FString NodeId, PinName; const FString DestName(*Pair.Key);
			if (!Pair.Value || !Pair.Value->TryGetObject(Spec) || !Spec || !Spec->IsValid() || (*Spec)->Values.Num() != 2
				|| !(*Spec)->TryGetStringField(TEXT("node"), NodeId) || !(*Spec)->TryGetStringField(TEXT("pin"), PinName))
			{ OutError = TEXT("new_inputs entries require node and pin strings"); return false; }
			UEdGraphNode* Source = NodeById(Graph, NodeId);
			UEdGraphPin* From = PinByName(Source, PinName, EGPD_Output);
			UEdGraphPin* To = PinByName(Probe, DestName, EGPD_Input);
			if ((OutPlan.bTargetSelf && DestName == TEXT("self")) || !From || !To || Source == Old || From->bOrphanedPin || To->bOrphanedPin
				|| From->PinType.PinCategory == TEXT("exec") || !(From->PinType == To->PinType)
				|| OutPlan.Links.ContainsByPredicate([&](const FLink& L) { return !L.bOldPinIsOutput && L.NewPinName == DestName; }))
			{ OutError = TEXT("new input link is missing, incompatible, or already supplied by the old call"); return false; }
			FLink L; L.bNewInput = true; L.NewPinName = DestName; L.OtherNodeId = NodeId; L.OtherPinName = PinName;
			OutPlan.Links.Add(MoveTemp(L));
		}
	}
	for (const FString& Pin : OutPlan.UnmatchedPins)
	{
		OutPlan.Refusals.Add(FString::Printf(TEXT("linked pin '%s' has no compatible mapped pin on replacement; its link was not changed"), *Pin));
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
    if (!SpecializeOperator(Old, NewNode, InPlan.PinMap, OutError)) return false;
    if (InPlan.bTargetSelf && !ConfigureSelf(Blueprint, NewNode, OutError)) return false;
    if (!TransferDefaults(Old, NewNode, true, OutError, InPlan.PinMap, false, InPlan.bTargetSelf)) return false;
	for (const FLink& Link : InPlan.Links)
	{
		UEdGraphPin* OldPin = PinByName(Old, Link.PinName, Link.bOldPinIsOutput ? EGPD_Output : EGPD_Input);
		UEdGraphNode* OtherNode = NodeById(Graph, Link.OtherNodeId);
		UEdGraphPin* OtherPin = PinByName(OtherNode, Link.OtherPinName,
			Link.bOldPinIsOutput ? EGPD_Input : EGPD_Output);
		UEdGraphPin* NewPin = PinByName(NewNode, Link.NewPinName,
			Link.bOldPinIsOutput ? EGPD_Output : EGPD_Input);
		if ((!Link.bNewInput && OldPin == nullptr) || OtherPin == nullptr || NewPin == nullptr)
		{
			OutError = FString::Printf(TEXT("link migration lost pin '%s'; caller must roll back"), *Link.PinName);
			return false;
		}
		if (!Link.bNewInput) Graph->GetSchema()->BreakSinglePinLink(OldPin, OtherPin);
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
    if (!TransferDefaults(Old, NewNode, false, OutError, InPlan.PinMap, true, InPlan.bTargetSelf)) return false;
    if (InPlan.bTargetSelf)
    {
        if (UEdGraphPin* Receiver = PinByName(Old, TEXT("self"), EGPD_Input))
            Graph->GetSchema()->BreakPinLinks(*Receiver, true);
        if (!ConfigureSelf(Blueprint, NewNode, OutError)) return false;
    }
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
        UEdGraphPin* NewPin = PinByName(NewNode, Link.NewPinName, Link.bOldPinIsOutput ? EGPD_Output : EGPD_Input);
        UEdGraphPin* OtherPin = PinByName(NodeById(Graph, Link.OtherNodeId), Link.OtherPinName, Link.bOldPinIsOutput ? EGPD_Input : EGPD_Output);
        if (!HasLink(NewPin, OtherPin))
        { OutError = TEXT("swap final link readback failed; caller must roll back"); return false; }
    }
	OutReport = FString::Printf(TEXT("changed: swapped %s to %s; %d link(s) migrated or added using explicit pin names; old node %s"),
		*InPlan.OldNodeId, *TurboMCPNodeId::MakeForNode(NewNode, true), InPlan.Links.Num(),
		InPlan.bKeepOld ? TEXT("retained unplugged") : TEXT("deleted"));
    if (InPlan.bTargetSelf) OutReport += TEXT("\nreceiver=self; old receiver links/default discarded explicitly; receiver source nodes retained");
    OutReport += TEXT("\nnon-receiver defaults=preserved; compile=not_run; save=not_requested; run EditTurboMCPCompile explicitly after editing");
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
        // A successful undo call does not verify properties, callbacks or dirty state.
        OutError = (Undone ? TEXT("partial: rollback=applied; restoration=unverified; ") : TEXT("partial: rollback=not_confirmed; restoration=unverified; ")) + OutError
            + TEXT("\nInspect the asset before retrying or saving; compile=not_run; save=not_requested.");
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
		Out += FString::Printf(TEXT("\n- %s%s => replacement.%s; peer %s:%s"), Link.bNewInput ? TEXT("new input ") : TEXT("migrate "), *Link.PinName, *Link.NewPinName, *Link.OtherNodeId, *Link.OtherPinName);
	}
    if (InPlan.bTargetSelf) Out += TEXT("\nreceiver: use self; discard old receiver links/default only; receiver source nodes are retained");
    FTurboMCPAssetResolver::FResolved R;
    if (FTurboMCPAssetResolver::Resolve(InPlan.Asset,R).IsEmpty())
        if (UBlueprint* BP=LoadObject<UBlueprint>(nullptr,*R.ObjectPath))
            if (UEdGraph* G=FindGraph(BP,InPlan.GraphName))
                if (UEdGraphNode* N=NodeById(G,InPlan.OldNodeId))
                    for (const UEdGraphPin* P : N->Pins)
                        if (P && !(InPlan.bTargetSelf && P->PinName == UEdGraphSchema_K2::PN_Self) && !IsImplicitStaticReceiver(N, P) && P->Direction==EGPD_Input && P->LinkedTo.IsEmpty() && P->PinType.PinCategory!=TEXT("exec") && !P->bDefaultValueIsIgnored)
                        {
                            const FString Name=P->PinName.ToString();
                            Out+=TEXT("\n- default ")+Name+TEXT(" => ")+(InPlan.PinMap.Contains(Name)?InPlan.PinMap[Name]:Name);
                            Out+=TEXT(": ")+P->DefaultValue+TEXT(" ")+P->DefaultTextValue.ToString();
                            if(P->DefaultObject) Out+=TEXT(" ")+P->DefaultObject->GetPathName();
                        }

	Out += TEXT("\nPreview only; edit replans from current state. No compile or save. KeepOld=true deliberately leaves a disconnected old node; false removes it after transfer checks. Inspect layout after editing.");
	return Out;
}
