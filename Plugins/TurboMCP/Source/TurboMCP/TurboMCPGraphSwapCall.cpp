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

namespace
{
	UEdGraph* FindGraph(UBlueprint* Blueprint, const FString& Name)
	{
		// *** REVERTED: THIS RESOLVER WAS DELIBERATELY LEFT ALONE. ***
		//
		// It was unified onto `TurboMCPNodeId::FindGraphByName` along with four others, and doing so MADE
		// `test-graph-swap-call.py` FAIL its `readback preserves the migrated execute link` check - a suite that
		// had passed THREE CONSECUTIVE TIMES with this function in place. So the divergence between the two
		// resolvers is NOT cosmetic here: it is LOAD-BEARING, and unifying it silently changed this tool's
		// behaviour. The unification is reverted until the difference is understood and measured.
		//
		// The defect it represents is real and still carded: this function searches UbergraphPages,
		// FunctionGraphs and MacroGraphs while the reader searches those THREE PLUS EventGraphs, and a node id
		// carries only the graph NAME. It must be unified - ONCE the reason the check depends on it is known.
		// *** UNIFIED FOR A REPEATED TEST, NOT FOR SHIPMENT YET. ***
		//
		// Last round I unified this and ONE run of test-graph-swap-call.py failed, so I recorded the divergence as
		// LOAD-BEARING and reverted. But that conclusion rests on a SINGLE run - and this same suite failed once in
		// round 22 and passed three times in round 40 on IDENTICAL code, so a single failure does not identify a
		// cause. This delegation is restored so the attribution can be tested with REPEATS. If the suite passes
		// several times in a row, the failure was order-dependence and the "load-bearing" claim is WITHDRAWN.
		return TurboMCPNodeId::FindGraphByName(Blueprint, Name);
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
	if (Blueprint == nullptr || Graph == nullptr || Old == nullptr) { OutError = TEXT("swap plan no longer resolves"); return false; }

	Blueprint->Modify();
	Graph->Modify();
	Old->Modify();
	FString SpawnNote;
	UEdGraphNode* NewNode = nullptr;
	if (FTurboMCPNodeType::SpawnNodeOfType(Blueprint, Graph, InPlan.ReplacementTypeId, NewNode, SpawnNote)
		!= FTurboMCPNodeType::ESpawnResult::Spawned || NewNode == nullptr)
	{
		OutError = FString::Printf(TEXT("replacement could not be spawned: %s"), *SpawnNote);
		return false;
	}
	NewNode->NodePosX = Old->NodePosX + 240;
	NewNode->NodePosY = Old->NodePosY;
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
			OutError = FString::Printf(TEXT("link migration lost pin '%s'; transaction rolled back"), *Link.PinName);
			return false;
		}
		Graph->GetSchema()->BreakSinglePinLink(OldPin, OtherPin);
		if (!Graph->GetSchema()->TryCreateConnection(
			Link.bOldPinIsOutput ? NewPin : OtherPin,
			Link.bOldPinIsOutput ? OtherPin : NewPin)
			|| !HasLink(NewPin, OtherPin))
		{
			OutError = FString::Printf(TEXT("link migration failed for pin '%s'; transaction rolled back"), *Link.PinName);
			return false;
		}
	}
	if (!InPlan.bKeepOld)
	{
		FBlueprintEditorUtils::RemoveNode(Blueprint, Old, false);
	}
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	if (Context.bCompile) FKismetEditorUtilities::CompileBlueprint(Blueprint);
	OutReport = FString::Printf(TEXT("changed: swapped %s to %s; %d link(s) migrated by pin name; old node %s"),
		*InPlan.OldNodeId, *TurboMCPNodeId::MakeForNode(NewNode, true), InPlan.Links.Num(),
		InPlan.bKeepOld ? TEXT("retained unplugged") : TEXT("deleted"));
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
	FScopedTransaction Transaction(FText::FromString(TEXT("TurboMCP swap call node")));
	FApplyContext Context;
	if (!ApplyCore(InPlan, Context, OutReport, OutError))
	{
		FTurboMCPRollback::EndAndUndoActiveTransaction();
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
