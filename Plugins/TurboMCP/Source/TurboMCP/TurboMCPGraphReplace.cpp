// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "TurboMCPGraphReplace.h"

#include "TurboMCPAssetResolver.h"
#include "TurboMCPGraphReachability.h"
#include "TurboMCPGraphScratch.h"
#include "TurboMCPGraphText.h"
#include "TurboMCPGraphTextLex.h"
#include "TurboMCPNodeId.h"
#include "TurboMCPNodeType.h"

#include "EdGraph/EdGraphPin.h"
#include "EdGraph/EdGraphSchema.h"
#include "EdGraphNode_Comment.h"
#include "EdGraphSchema_K2.h"
#include "Editor.h"
#include "Editor/TransBuffer.h"
#include "K2Node_Event.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/Breakpoint.h"
#include "Kismet2/KismetDebugUtilities.h"
#include "Templates/UnrealTemplate.h"
#include "Misc/ScopeExit.h"
#include "Serialization/JsonReader.h"

namespace
{
	const FString EditPrefix = TEXT("; tmcp-edit-v1: ");
	const FString DeletePrefix = TEXT("; tmcp-delete-v1: ");

	FString ReplaceId(const FGuid& Id)
	{
		return Id.ToString(EGuidFormats::DigitsLower);
	}

	bool IsLowerHex(const FString& String, int32 Length)
	{
		return String.Len() == Length && TurboMCPNodeId::NormaliseKey(String).Equals(String, ESearchCase::CaseSensitive);
	}

	bool ParseReplaceId(const FString& String, FGuid& OutId)
	{
		return IsLowerHex(String, 32) && FGuid::ParseExact(String, EGuidFormats::Digits, OutId) && OutId.IsValid();
	}

	// ARC07 envelopes are flat and have an exact field set. Read the tokens rather
	// than a JSON object map, which would hide repeated (possibly conflicting) keys.
	bool ParseEditEnvelope(const FString& Json, TMap<FString, FString>& Out, bool bHeader)
	{
		const auto Reader = TJsonReaderFactory<>::Create(Json);
		EJsonNotation Token;
		if (!Reader->ReadNext(Token) || Token != EJsonNotation::ObjectStart) return false;
		bool bComplete = false;
		while (Reader->ReadNext(Token) && Token != EJsonNotation::ObjectEnd)
		{
			const FString Key = Reader->GetIdentifier();
			if (bHeader && Key.Equals(TEXT("complete"), ESearchCase::CaseSensitive))
			{
				if (bComplete || Token != EJsonNotation::Boolean || !Reader->GetValueAsBoolean()) return false;
				bComplete = true;
			}
			else
			{
				const bool bKnown = bHeader
					? Key.Equals(TEXT("asset"), ESearchCase::CaseSensitive) || Key.Equals(TEXT("graph"), ESearchCase::CaseSensitive)
						|| Key.Equals(TEXT("graphGuid"), ESearchCase::CaseSensitive) || Key.Equals(TEXT("baseFingerprint"), ESearchCase::CaseSensitive)
					: Key.Equals(TEXT("nodeId"), ESearchCase::CaseSensitive);
				if (!bKnown || Token != EJsonNotation::String || Out.Contains(Key)) return false;
				Out.Add(Key, Reader->GetValueAsString());
			}
		}
		return Token == EJsonNotation::ObjectEnd && !Reader->ReadNext(Token)
			&& Reader->GetErrorMessage().IsEmpty() && (bHeader ? bComplete && Out.Num() == 4 : Out.Num() == 1);
	}

	bool FindReplaceGraph(const FString& Asset, const FString& Name, UBlueprint*& Blueprint, UEdGraph*& Graph, FString& Error)
	{
		FTurboMCPAssetResolver::FResolved Resolved;
		Error = FTurboMCPAssetResolver::Resolve(Asset, Resolved);
		if (!Error.IsEmpty()) return false;
		Blueprint = LoadObject<UBlueprint>(nullptr, *Resolved.ObjectPath);
		if (!Blueprint)
		{
			Error = TEXT("replacement asset is missing or is not a Blueprint");
			return false;
		}
		int32 Matches = 0;
		Graph = TurboMCPNodeId::FindGraphByName(Blueprint, Name.TrimStartAndEnd(), Matches);
		if (!Graph || Matches != 1)
		{
			Error = TEXT("replacement needs one exact named Blueprint graph; graph is missing or ambiguous");
			return false;
		}
		return true;
	}

	UEdGraphPin* ExactReplacePin(UEdGraphNode* Node, const FString& Name, EEdGraphPinDirection Direction)
	{
		UEdGraphPin* Match = nullptr;
		if (Node)
			for (UEdGraphPin* Pin : Node->Pins)
				if (Pin && Pin->Direction == Direction && !Pin->bOrphanedPin && Pin->PinName.ToString().Equals(Name, ESearchCase::CaseSensitive))
				{
					if (Match) return nullptr;
					Match = Pin;
				}
		return Match;
	}

	struct FValidatedDefault
	{
		FString Literal;
		TObjectPtr<UObject> Object = nullptr;
		FText Text;
	};

	// Decode the single existing grammar literal by pin type. A localized FText
	// must retain its namespace/key, so do not use TrySetDefaultValue for it.
	bool ReplaceDefault(const UEdGraphSchema_K2& Schema, UEdGraphPin& Pin, const FString& Value,
		FValidatedDefault& Out, FString& Error)
	{
		if (Pin.bHidden || Pin.bDefaultValueIsIgnored || Pin.bDefaultValueIsReadOnly
			|| Pin.PinType.PinCategory == UEdGraphSchema_K2::PC_Exec || Pin.PinType.PinCategory == UEdGraphSchema_K2::PC_Wildcard)
		{
			Error = TEXT("the input does not support a default assignment");
			return false;
		}
		Schema.GetPinDefaultValuesFromString(Pin.PinType, Pin.GetOwningNode(), Value, Out.Literal, Out.Object, Out.Text, true);
		if ((Pin.PinType.PinCategory == UEdGraphSchema_K2::PC_Object || Pin.PinType.PinCategory == UEdGraphSchema_K2::PC_Class
			|| Pin.PinType.PinCategory == UEdGraphSchema_K2::PC_Interface)
			&& Value.StartsWith(TEXT("/")) && (!Out.Object || !Out.Object->GetPathName().Equals(Value, ESearchCase::CaseSensitive)))
		{
			Error = TEXT("the exact object default did not resolve");
			return false;
		}
		Error = Schema.IsPinDefaultValid(&Pin, Out.Literal, Out.Object, Out.Text);
		return Error.IsEmpty();
	}

	FString RemovalDebuggerIssue(const UBlueprint& Blueprint, const UEdGraphNode& Node)
	{
		// FindBreakpointForNode can clean and save settings. This lookup is read-only.
		if (FKismetDebugUtilities::FindBreakpointByPredicate(&Blueprint, [&](const FBlueprintBreakpoint& Breakpoint)
			{ return Breakpoint.GetLocation() == &Node; }))
			return TEXT("node has a Blueprint breakpoint");
		for (const UEdGraphPin* Pin : Node.Pins)
			if (FKismetDebugUtilities::DoesPinHaveWatches(&Blueprint, Pin))
				return TEXT("node has a Blueprint pin watch: ") + Pin->PinName.ToString();
		return FString();
	}

	// K2's convenience wrapper starts a global editor transaction and marks the
	// owning Blueprint modified. The base API makes the same pin and node
	// notifications. Replace owns the live transaction and Blueprint notification.
	void BreakReplaceLink(const UEdGraphSchema_K2& Schema, UEdGraphPin* First, UEdGraphPin* Second)
	{
		Schema.UEdGraphSchema::BreakSinglePinLink(First, Second);
	}

	bool RemoveReplaceNode(const UEdGraphSchema_K2& Schema, UEdGraph& Graph, UEdGraphNode& Node)
	{
		// BlueprintEditorUtils::RemoveNode delegates to the transacting K2
		// BreakPinLinks and also edits debugger settings outside graph undo.
		// Break incident links with the base API, then use stock node destruction.
		TArray<TPair<FEdGraphPinReference, FEdGraphPinReference>> Incident;
		for (UEdGraphPin* Pin : Node.Pins)
		{
			if (!Pin) return false;
			for (UEdGraphPin* Other : Pin->LinkedTo)
			{
				if (!Other || Other->IsPendingKill()) return false;
				if (Other->GetOwningNodeUnchecked() != &Node || Pin->Direction == EGPD_Output)
					Incident.Emplace(FEdGraphPinReference(Pin), FEdGraphPinReference(Other));
			}
		}

		for (const auto& Edge : Incident)
		{
			UEdGraphPin* First = Edge.Key.Get();
			UEdGraphPin* Second = Edge.Value.Get();
			if (!First || !Second || !First->LinkedTo.Contains(Second) || !Second->LinkedTo.Contains(First))
				return false;
			BreakReplaceLink(Schema, First, Second);
		}
		for (const UEdGraphPin* Pin : Node.Pins)
			if (!Pin || !Pin->LinkedTo.IsEmpty()) return false;

		Node.Modify();
		Node.DestroyNode();
		return !Graph.Nodes.Contains(&Node);
	}

	bool RetainedIdentityOnly(const UEdGraphNode& Node)
	{
		return Node.IsA<UK2Node_FunctionEntry>() || Node.IsA<UK2Node_FunctionResult>() || Node.IsA<UK2Node_Event>();
	}

	bool ValidLabel(const FString& Text)
	{
		if (Text.IsEmpty() || (Text.Len() == 32 && !TurboMCPNodeId::NormaliseKey(Text).IsEmpty())) return false;
		for (const TCHAR Ch : Text)
			if (!((Ch >= TEXT('a') && Ch <= TEXT('z')) || (Ch >= TEXT('A') && Ch <= TEXT('Z'))
				|| (Ch >= TEXT('0') && Ch <= TEXT('9')) || Ch == TEXT('_'))) return false;
		return true;
	}
}

bool FTurboMCPGraphReplace::Plan(const FString& InAsset, const FString& InGraphName,
	const FString& InText, FPlan& OutPlan, FString& OutError)
{
	using namespace TurboMCPGraphTextLex;
	OutPlan = FPlan();
	OutPlan.Asset = InAsset;
	OutPlan.Graph = InGraphName;
	OutError.Reset();
	auto Refuse = [&](const FString& Reason)
	{
		OutPlan.Refusals.Add(Reason);
		return true;
	};
	if (!IsInGameThread() || !GEditor || GEditor->IsTransactionActive() || GIsTransacting
		|| GEditor->IsPlaySessionInProgress() || GCompilingBlueprint)
		return Refuse(TEXT("replacement requires an idle editor outside PIE, compilation and transactions"));
	if (InText.Len() > 24000)
		return Refuse(TEXT("replacement text exceeds the 24000 character edit document limit"));

	const TArray<FLine> Lines = SplitLines(InText);
	TMap<FString, FString> Header;
	if (Lines.IsEmpty() || !Lines[0].Text.StartsWith(EditPrefix, ESearchCase::CaseSensitive)
		|| !ParseEditEnvelope(Lines[0].Text.Mid(EditPrefix.Len()), Header, true))
		return Refuse(TEXT("replacement requires a valid complete tmcp-edit-v1 header on its first line; legacy, partial and malformed documents are refused"));
	OutPlan.BaseFingerprint = Header.FindChecked(TEXT("baseFingerprint"));
	if (!ParseReplaceId(Header.FindChecked(TEXT("graphGuid")), OutPlan.GraphGuid)
		|| !IsLowerHex(OutPlan.BaseFingerprint, 64) || Header.FindChecked(TEXT("graph")).IsEmpty())
		return Refuse(TEXT("invalid edit graph GUID, graph description or SHA-256 fingerprint"));

	UBlueprint* Blueprint = nullptr;
	UEdGraph* Graph = nullptr;
	if (!FindReplaceGraph(InAsset, InGraphName, Blueprint, Graph, OutError)) return false;
	OutPlan.Asset = Blueprint->GetPathName();
	OutPlan.Graph = Graph->GetName();
	if (!Header.FindChecked(TEXT("asset")).Equals(OutPlan.Asset, ESearchCase::CaseSensitive)
		|| OutPlan.GraphGuid != Graph->GraphGuid)
		return Refuse(TEXT("foreign edit header: canonical asset or graph GUID does not match the selected graph"));
	FString CurrentBody;
	TArray<FString> Issues;
	const FString CurrentFingerprint = FTurboMCPGraphText::EditSnapshot(*Blueprint, *Graph, CurrentBody, Issues);
	if (!Issues.IsEmpty()) return Refuse(TEXT("current graph has no complete edit export: ") + FString::Join(Issues, TEXT("; ")));
	if (!OutPlan.BaseFingerprint.Equals(CurrentFingerprint, ESearchCase::CaseSensitive))
		return Refuse(TEXT("stale edit fingerprint; read a fresh complete edit export"));

	TMap<FGuid, UEdGraphNode*> Nodes;
	for (UEdGraphNode* Node : Graph->Nodes) Nodes.Add(Node->NodeGuid, Node);

	struct FPendingLink { int32 Form; int32 Input; FString From; FString Output; };
	TArray<FPendingLink> PendingLinks;
	TMap<FGuid, int32> ExistingForms;
	TMap<FString, int32> Labels;
	TSet<FGuid> Deleted;
	int32 Current = INDEX_NONE;
	for (int32 LineIndex = 1; LineIndex < Lines.Num(); ++LineIndex)
	{
		const FLine& Line = Lines[LineIndex];
		const FString Text = Line.Text.TrimStartAndEnd();
		if (Text.IsEmpty()) continue;
		auto Invalid = [&](const FString& Reason)
		{
			return Refuse(FString::Printf(TEXT("line %d: %s"), Line.Number, *Reason));
		};
		if (Text.StartsWith(DeletePrefix, ESearchCase::CaseSensitive))
		{
			TMap<FString, FString> Directive;
			FGuid Id;
			if (Current != INDEX_NONE || !ParseEditEnvelope(Text.Mid(DeletePrefix.Len()), Directive, false)
				|| !ParseReplaceId(Directive.FindChecked(TEXT("nodeId")), Id))
				return Invalid(TEXT("invalid tmcp-delete-v1 directive"));
			if (Deleted.Contains(Id)) return Invalid(TEXT("duplicate deletion directive for ") + ReplaceId(Id));
			UEdGraphNode* const* Node = Nodes.Find(Id);
			if (!Node) return Invalid(TEXT("foreign or missing deletion target ") + ReplaceId(Id));
			if (FTurboMCPGraphText::IsEditRemovalProtected(**Node))
				return Invalid(TEXT("protected deletion target ") + ReplaceId(Id) + TEXT(" (entries, results, events, comments or engine-protected nodes)"));
			const FString DebuggerIssue = RemovalDebuggerIssue(*Blueprint, **Node);
			if (!DebuggerIssue.IsEmpty())
				return Invalid(TEXT("cannot delete ") + ReplaceId(Id) + TEXT(": ") + DebuggerIssue);
			Deleted.Add(Id);
			OutPlan.Removals.Add({Id, (*Node)->GetClass()->GetName(), TurboMCPNodeId::GetNodeTitle(*Node)});
			continue;
		}
		// A strict flat subset of the shared multiline grammar: a type head,
		// existing ID on the head or a local label, input pin lines, then ')'.
		FString Type;
		int32 Column = 0, After = 0;
		if (ExtractHeadTypeId(Text, Type, Column, After))
		{
			if (Current != INDEX_NONE) return Invalid(TEXT("nested forms are unsupported; use separate forms and explicit pin links"));
			FString Suffix = Text.Mid(After).TrimStartAndEnd();
			const bool bClosed = Suffix.RemoveFromStart(TEXT(")"));
			Suffix.TrimStartAndEndInline();
			FForm Form;
			Form.Type = Type;
			const int32 Index = OutPlan.Forms.Num();
			if (!Suffix.IsEmpty())
			{
				if (!Suffix.RemoveFromStart(TEXT(";"))) return Invalid(TEXT("unexpected text after node head"));
				Suffix.TrimStartAndEndInline();
				if (Suffix.StartsWith(TEXT("label "), ESearchCase::CaseSensitive))
				{
					Form.Label = LocalLabelFromComment(Suffix);
					if (!ValidLabel(Form.Label) || Labels.Contains(Form.Label))
						return Invalid(TEXT("invalid or duplicate new-node label"));
					Labels.Add(Form.Label, Index);
				}
				else
				{
					if (!ParseReplaceId(Suffix, Form.Id))
						return Invalid(TEXT("existing nodes require an exact 32-digit lowercase GUID"));
					UEdGraphNode* const* Node = Nodes.Find(Form.Id);
					if (!Node) return Invalid(TEXT("foreign or missing existing node ") + Suffix);
					if (ExistingForms.Contains(Form.Id)) return Invalid(TEXT("duplicate node form for ") + Suffix);
					if (!FTurboMCPGraphText::EditType(**Node).Equals(Form.Type, ESearchCase::CaseSensitive))
						return Invalid(TEXT("existing node type differs from the export for ") + Suffix);
					ExistingForms.Add(Form.Id, Index);
				}
			}
			OutPlan.Forms.Add(MoveTemp(Form));
			Current = bClosed ? INDEX_NONE : Index;
			continue;
		}
		if (Text == TEXT(")") && Current != INDEX_NONE)
		{
			Current = INDEX_NONE;
			continue;
		}
		FString PinName, Value;
		if (Current == INDEX_NONE || Text.StartsWith(TEXT(";")) || !ParsePinLine(Text, PinName, Value, Column)
			|| Value.IsEmpty())
			return Invalid(TEXT("expected a node form, explicit input value, closing delimiter or deletion directive"));
		FForm& Form = OutPlan.Forms[Current];
		int32 InputIndex = Form.Inputs.IndexOfByPredicate([&](const FInput& Input)
		{
			return Input.Name.Equals(PinName, ESearchCase::CaseSensitive);
		});
		if (InputIndex == INDEX_NONE) InputIndex = Form.Inputs.Add({PinName, {}, {}});
		FInput& Input = Form.Inputs[InputIndex];
		if (ValueIsALink(Value))
		{
			FLinkForm Link;
			FString Error;
			if (Input.Default.IsSet() || !ParseLinkValue(Value, Link, Error))
				return Invalid(TEXT("invalid or conflicting link on pin ") + PinName + TEXT(" ") + Error);
			// ParsePinLine removes the link's closing ')'. Require one actual
			// delimiter outside quotes and no ignored trailing text or comments.
			const FString Prefix = TEXT(" (-> ") + Link.FromNode + TEXT(":");
			const FString QuotedOutput = Prefix + QuoteLiteral(Link.FromPin) + TEXT(")");
			const FString PlainOutput = Prefix + Link.FromPin + TEXT(")");
			if (!Text.Equals(QuoteLiteral(PinName) + QuotedOutput, ESearchCase::CaseSensitive)
				&& !Text.Equals(PinName + QuotedOutput, ESearchCase::CaseSensitive)
				&& !Text.Equals(QuoteLiteral(PinName) + PlainOutput, ESearchCase::CaseSensitive)
				&& !Text.Equals(PinName + PlainOutput, ESearchCase::CaseSensitive))
				return Invalid(TEXT("link must use an explicit (-> identity:output) expression on its own pin line"));
			if (PendingLinks.ContainsByPredicate([&](const FPendingLink& Prior)
				{ return Prior.Form == Current && Prior.Input == InputIndex && Prior.From == Link.FromNode && Prior.Output == Link.FromPin; }))
				return Invalid(TEXT("duplicate pin link for ") + PinName);
			PendingLinks.Add({Current, InputIndex, Link.FromNode, Link.FromPin});
		}
		else
		{
			FString Literal, Error;
			if (Input.Default.IsSet() || PendingLinks.ContainsByPredicate([&](const FPendingLink& Link)
				{ return Link.Form == Current && Link.Input == InputIndex; }) || !DecodeLiteral(Value, Literal, Error))
				return Invalid(TEXT("invalid, duplicate or conflicting default for ") + PinName + TEXT(" ") + Error);
			// Do not let the shared reader's trailing-closer/comment tolerance
			// turn an incomplete or branch heading line into a literal write.
			if (!Text.Equals(QuoteLiteral(PinName) + TEXT(" ") + Value, ESearchCase::CaseSensitive)
				&& !Text.Equals(PinName + TEXT(" ") + Value, ESearchCase::CaseSensitive))
				return Invalid(TEXT("default must use a single pin/value line"));
			Input.Default = MoveTemp(Literal);
		}
	}
	if (Current != INDEX_NONE) return Refuse(TEXT("unterminated node form in edit document"));

	for (const auto& Pair : ExistingForms)
		if (Deleted.Contains(Pair.Key)) return Refuse(TEXT("conflicting retention and deletion for ") + ReplaceId(Pair.Key));
	for (const auto& Pair : Nodes)
		if (!ExistingForms.Contains(Pair.Key) && !Deleted.Contains(Pair.Key)
			&& !Pair.Value->IsA<UEdGraphNode_Comment>())
			return Refuse(TEXT("omitted semantic node without an explicit deletion directive: ") + ReplaceId(Pair.Key));

	for (const FPendingLink& Link : PendingLinks)
	{
		FGuid Id;
		const int32* Index = ParseReplaceId(Link.From, Id) ? ExistingForms.Find(Id) : Labels.Find(Link.From);
		if (!Index) return Refuse(TEXT("foreign, omitted, deleted or ambiguous link source ") + Link.From
			+ TEXT("; use a retained full GUID or the unique label of a new node"));
		const FForm& Source = OutPlan.Forms[*Index];
		if (Source.Id.IsValid() && !ExactReplacePin(Nodes.FindChecked(Source.Id), Link.Output, EGPD_Output))
			return Refuse(TEXT("missing or ambiguous source pin ") + Link.From + TEXT(":") + Link.Output);
		OutPlan.Forms[Link.Form].Inputs[Link.Input].Links.Add({*Index, Link.Output});
	}

	TArray<FTurboMCPGraphAppend::FNewNode> PlacementForms;
	TArray<FTurboMCPGraphAppend::FNewLink> PlacementLinks;
	for (const FForm& Form : OutPlan.Forms)
	{
		auto& Placement = PlacementForms.AddDefaulted_GetRef();
		Placement.bLinksToExisting = Form.Id.IsValid();
		Placement.TypeId = Form.Type;
		if (!Form.Id.IsValid()) continue;
		UEdGraphNode* Node = Nodes.FindChecked(Form.Id);
		for (const UEdGraphPin* Pin : Node->Pins)
			if (FTurboMCPGraphText::IsEditInput(*Pin) && !Form.Inputs.ContainsByPredicate([&](const FInput& Input)
				{ return Input.Name.Equals(Pin->PinName.ToString(), ESearchCase::CaseSensitive); }))
				return Refuse(TEXT("omitted input ") + ReplaceId(Form.Id) + TEXT(":") + Pin->PinName.ToString()
					+ TEXT("; preserve its wire or submit an explicit replacement value/link"));
		for (const FInput& Input : Form.Inputs)
		{
			UEdGraphPin* Pin = ExactReplacePin(Node, Input.Name, EGPD_Input);
			if (!Pin) return Refuse(TEXT("unknown or ambiguous input ") + ReplaceId(Form.Id) + TEXT(":") + Input.Name);
			if (Input.Default.IsSet() && (Pin->bHidden || Pin->bDefaultValueIsIgnored || Pin->bDefaultValueIsReadOnly
				|| Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec || Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Wildcard))
				return Refuse(TEXT("unsupported default assignment on ") + ReplaceId(Form.Id) + TEXT(":") + Input.Name);
			bool bDiffers = Input.Default.IsSet()
				? !Pin->LinkedTo.IsEmpty() || !FTurboMCPGraphText::EditLiteral(*Pin).Equals(Input.Default.GetValue(), ESearchCase::CaseSensitive)
				: Pin->LinkedTo.Num() != Input.Links.Num();
			if (!bDiffers && !Input.Default.IsSet())
				for (const FLink& Link : Input.Links)
				{
					const FGuid& SourceId = OutPlan.Forms[Link.From].Id;
					bDiffers |= !SourceId.IsValid() || !Pin->LinkedTo.Contains(ExactReplacePin(Nodes.FindRef(SourceId), Link.Output, EGPD_Output));
				}
			if (bDiffers) OutPlan.Updates.Add(ReplaceId(Form.Id) + TEXT(":") + Input.Name);
		}
	}

	for (int32 To = 0; To < OutPlan.Forms.Num(); ++To)
		for (const FInput& Input : OutPlan.Forms[To].Inputs)
			for (const FLink& Link : Input.Links)
			{
				auto& PlacementLink = PlacementLinks.AddDefaulted_GetRef();
				PlacementLink.FromPin = Link.Output;
				PlacementLink.ToPin = Input.Name;
				const FGuid& FromId = OutPlan.Forms[Link.From].Id;
				const FGuid& ToId = OutPlan.Forms[To].Id;
				if (FromId.IsValid()) PlacementLink.FromNodeKey = TurboMCPNodeId::MakeForNode(Nodes.FindChecked(FromId), true);
				else PlacementLink.FromFormIndex = Link.From;
				if (ToId.IsValid()) PlacementLink.ToNodeKey = TurboMCPNodeId::MakeForNode(Nodes.FindChecked(ToId), true);
				else PlacementLink.ToFormIndex = To;
			}
	if (PlacementForms.ContainsByPredicate([](const FTurboMCPGraphAppend::FNewNode& Form) { return !Form.bLinksToExisting; }))
	{
		bool bEstimated = false;
		FString PlacementError;
		if (!FTurboMCPGraphAppend::PlanPlacement(Graph, PlacementForms, OutPlan.Positions, bEstimated, PlacementError, &PlacementLinks))
			return Refuse(TEXT("replacement placement refused: ") + PlacementError);
	}

	// Simulate the combined deletion/default/link/spawn plan, including execution
	// reachability, on a disposable duplicate before opening a live transaction.
	if (OutPlan.HasChanges())
	{
		TGuardValue<ITransaction*> SuppressScratchUndo(GUndo, nullptr);
		TurboMCPGraphScratch::FCopy Scratch;
		FString ScratchError;
		if (!Scratch.Init(Blueprint, Graph) || !Execute(OutPlan, *Scratch.Owner.Get(), *Scratch.Graph, Graph, ScratchError))
			return Refuse(TEXT("replacement preflight refused: ") + (ScratchError.IsEmpty() ? TEXT("scratch graph could not be created") : ScratchError));
	}
	OutPlan.bValidated = true;
	return true;
}

bool FTurboMCPGraphReplace::Execute(const FPlan& Plan, UBlueprint& Blueprint, UEdGraph& Graph,
	UEdGraph* DiscoveryGraph, FString& OutError, bool bFailAfterRemoval)
{
	const auto* Schema = CastChecked<UEdGraphSchema_K2>(Graph.GetSchema());
	auto Refuse = [&](const FString& Reason) { OutError = Reason; return false; };
	struct FPinBefore { FString Shape; FString Default; };
	struct FNodeBefore { FString State; TMap<FGuid, FPinBefore> Pins; };
	TMap<const UEdGraphNode*, FNodeBefore> Retained;
	TSet<FGuid> DeleteIds;
	for (const FRemoval& Removal : Plan.Removals) DeleteIds.Add(Removal.Id);
	for (UEdGraphNode* Node : Graph.Nodes)
	{
		if (DeleteIds.Contains(Node->NodeGuid)) continue;
		auto& Before = Retained.Add(Node);
		Before.State = FTurboMCPGraphText::EditNodeState(*Node);
		for (const UEdGraphPin* Pin : Node->Pins)
			Before.Pins.Add(Pin->PinId, {FTurboMCPGraphText::EditPinShape(*Pin), FTurboMCPGraphText::EditDefaultState(*Pin)});
	}

	if (!DiscoveryGraph)
	{
		Blueprint.Modify();
		Graph.Modify();
	}
	for (const FRemoval& Removal : Plan.Removals)
	{
		UEdGraphNode* Node = TurboMCPGraphScratch::ByGuid(&Graph, Removal.Id);
		if (!Node || FTurboMCPGraphText::IsEditRemovalProtected(*Node))
			return Refuse(TEXT("removal target missing or protected: ") + ReplaceId(Removal.Id));
		if (!RemoveReplaceNode(*Schema, Graph, *Node))
			return Refuse(TEXT("could not remove node and its exact wires: ") + ReplaceId(Removal.Id));
	}

	TArray<UEdGraphNode*> Instances;
	Instances.SetNumZeroed(Plan.Forms.Num());
	for (int32 Index = 0; Index < Plan.Forms.Num(); ++Index)
	{
		const FForm& Form = Plan.Forms[Index];
		UEdGraphNode*& Node = Instances[Index];
		if (Form.Id.IsValid())
		{
			Node = TurboMCPGraphScratch::ByGuid(&Graph, Form.Id);
			if (!Node || !Retained.Contains(Node)) return Refuse(TEXT("retained node identity changed: ") + ReplaceId(Form.Id));
			continue;
		}
		const int32 CountBefore = Graph.Nodes.Num();
		FString FactoryError;
		// Resolve creation in the same authored Blueprint context on scratch and live.
		UBlueprint* FactoryOwner = DiscoveryGraph ? CastChecked<UBlueprint>(DiscoveryGraph->GetOuter()) : &Blueprint;
		if (FTurboMCPNodeType::SpawnNodeOfType(FactoryOwner, &Graph, Form.Type, Node, FactoryError, DiscoveryGraph)
			!= FTurboMCPNodeType::ESpawnResult::Spawned || !Node)
			return Refuse(TEXT("cannot create ") + Form.Type + TEXT(": ") + FactoryError);
		if (Node->GetOuter() != &Graph || !Graph.Nodes.Contains(Node) || Graph.Nodes.Num() != CountBefore + 1
			|| FTurboMCPGraphText::EditType(*Node).IsEmpty() || RetainedIdentityOnly(*Node))
			return Refuse(TEXT("unsupported creation family or engine-created companions: ") + Form.Type);
		const auto* Position = Plan.Positions.FindByPredicate([&](const FTurboMCPGraphAppend::FPlannedPosition& Item)
			{ return Item.FormIndex == Index; });
		if (!Position) return Refuse(TEXT("new node has no planned position: ") + Form.Type);
		Node->NodePosX = Position->X;
		Node->NodePosY = Position->Y;
	}

	auto ResolveOutputs = [&](const FInput& Input, TArray<UEdGraphPin*>& Outputs)
	{
		for (const FLink& Link : Input.Links)
		{
			UEdGraphPin* Pin = ExactReplacePin(Instances[Link.From], Link.Output, EGPD_Output);
			if (!Pin || Pin->bNotConnectable || Outputs.Contains(Pin)) return false;
			Outputs.Add(Pin);
		}
		return true;
	};

	// First remove only displaced, explicitly submitted incoming links.
	for (int32 Index = 0; Index < Plan.Forms.Num(); ++Index)
		for (const FInput& Input : Plan.Forms[Index].Inputs)
		{
			UEdGraphNode* Node = Instances[Index];
			UEdGraphPin* Pin = ExactReplacePin(Node, Input.Name, EGPD_Input);
			TArray<UEdGraphPin*> Outputs;
			if (!Pin || !ResolveOutputs(Input, Outputs)
				|| (Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec && Outputs.Num() > 1))
				return Refuse(TEXT("unsupported input or ambiguous links on ") + Input.Name);
			const TArray<UEdGraphPin*> Original = Pin->LinkedTo;
			for (UEdGraphPin* Prior : Original)
				if (!Outputs.Contains(Prior))
				{
					Pin = ExactReplacePin(Node, Input.Name, EGPD_Input);
					if (!Pin || !Pin->LinkedTo.Contains(Prior)) return Refuse(TEXT("input was reconstructed while breaking ") + Input.Name);
					Node->Modify();
					Prior->GetOwningNode()->Modify();
					BreakReplaceLink(*Schema, Prior, Pin);
				}
		}

	for (int32 Index = 0; Index < Plan.Forms.Num(); ++Index)
		for (const FInput& Input : Plan.Forms[Index].Inputs)
		{
			UEdGraphNode* Node = Instances[Index];
			for (const FLink& Link : Input.Links)
			{
				UEdGraphPin* Pin = ExactReplacePin(Node, Input.Name, EGPD_Input);
				UEdGraphPin* Output = ExactReplacePin(Instances[Link.From], Link.Output, EGPD_Output);
				if (!Pin || !Output) return Refuse(TEXT("input or source was reconstructed while writing ") + Input.Name);
				if (Pin->LinkedTo.Contains(Output)) continue;
				if (Pin->bNotConnectable || Output->bNotConnectable)
					return Refuse(TEXT("non-connectable pin in requested link: ") + Input.Name);
				if (Schema->CanCreateConnection(Output, Pin).Response != CONNECT_RESPONSE_MAKE)
					return Refuse(TEXT("schema requires a conversion, an unrelated link break or refuses the link: ") + Link.Output + TEXT(" -> ") + Input.Name);
				Node->Modify();
				Output->GetOwningNode()->Modify();
				// K2 validated the exact connection above. Its TryCreateConnection
				// wrapper additionally marks the Blueprint modified, which touches
				// generated-class state borrowed by the scratch copy.
				if (!Schema->UEdGraphSchema::TryCreateConnection(Output, Pin))
					return Refuse(TEXT("schema could not connect input ") + Input.Name);
			}
		}

	TMap<const UEdGraphPin*, FString> WrittenDefaults;
	for (int32 Index = 0; Index < Plan.Forms.Num(); ++Index)
		for (const FInput& Input : Plan.Forms[Index].Inputs)
		{
			if (!Input.Default.IsSet()) continue;
			UEdGraphNode* Node = Instances[Index];
			UEdGraphPin* Pin = ExactReplacePin(Node, Input.Name, EGPD_Input);
			if (!Pin) return Refuse(TEXT("default target disappeared: ") + Input.Name);
			const FString& Literal = Input.Default.GetValue();
			if (FTurboMCPGraphText::EditLiteral(*Pin).Equals(Literal, ESearchCase::CaseSensitive)) continue;
			FValidatedDefault Value;
			if (!ReplaceDefault(*Schema, *Pin, Literal, Value, OutError))
			{
				OutError = TEXT("invalid default for ") + Input.Name + TEXT(": ") + OutError;
				return false;
			}
			Node->Modify();
			if (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Text)
				Schema->TrySetDefaultText(*Pin, Value.Text, false);
			else if (Value.Object)
				Schema->TrySetDefaultObject(*Pin, Value.Object, false);
			else
				Schema->TrySetDefaultValue(*Pin, Literal, false);
			UEdGraphPin* After = ExactReplacePin(Node, Input.Name, EGPD_Input);
			if (!After || !FTurboMCPGraphText::EditLiteral(*After).Equals(Literal, ESearchCase::CaseSensitive))
				return Refuse(TEXT("schema normalized, rejected or lost the exact requested default for ") + Input.Name);
			WrittenDefaults.Add(After, FTurboMCPGraphText::EditDefaultState(*After));
		}

	if (bFailAfterRemoval) return Refuse(TEXT("injected post-removal regression failure"));
	if (!DiscoveryGraph) FBlueprintEditorUtils::MarkBlueprintAsModified(&Blueprint);
	if (Graph.Nodes.Num() != Retained.Num() + Plan.Positions.Num())
		return Refuse(TEXT("unexpected node count after staged replacement"));

	TSet<FGuid> Ids;
	TSet<const UEdGraphNode*> Reachable;
	if (!Plan.Positions.IsEmpty()) TurboMCPGraphReachability::MarkReachable(&Graph, Reachable);
	for (const UEdGraphNode* Node : Graph.Nodes)
	{
		if (!Node || Node->GetOuter() != &Graph || !Node->NodeGuid.IsValid() || Ids.Contains(Node->NodeGuid)
			|| DeleteIds.Contains(Node->NodeGuid))
			return Refuse(TEXT("unexpected node ownership or identity after staged replacement"));
		Ids.Add(Node->NodeGuid);
		const bool bExisting = Retained.Contains(Node);
		const int32 FormIndex = Instances.IndexOfByPredicate([Node](const UEdGraphNode* Instance) { return Instance == Node; });
		if (!bExisting && FormIndex == INDEX_NONE)
			return Refuse(TEXT("unplanned node appeared after staged replacement"));
		const FNodeBefore* Before = Retained.Find(Node);
		if (Before && (!Before->State.Equals(FTurboMCPGraphText::EditNodeState(*Node), ESearchCase::CaseSensitive) || Before->Pins.Num() != Node->Pins.Num()))
			return Refuse(TEXT("retained node changed outside the requested inputs: ") + ReplaceId(Node->NodeGuid));
		const FForm* Form = FormIndex != INDEX_NONE ? &Plan.Forms[FormIndex] : nullptr;
		for (const UEdGraphPin* Pin : Node->Pins)
		{
			if (!Pin || Pin->GetOwningNodeUnchecked() != Node || Pin->bOrphanedPin || Pin->ParentPin || !Pin->SubPins.IsEmpty())
				return Refuse(TEXT("pin identity or support changed during replacement"));
			if (Before)
			{
				const auto* Prior = Before->Pins.Find(Pin->PinId);
				const FString* Written = WrittenDefaults.Find(Pin);
				if (!Prior || !Prior->Shape.Equals(FTurboMCPGraphText::EditPinShape(*Pin), ESearchCase::CaseSensitive)
					|| !(Written ? *Written : Prior->Default).Equals(FTurboMCPGraphText::EditDefaultState(*Pin), ESearchCase::CaseSensitive))
					return Refuse(TEXT("retained pin identity, type or default changed outside the plan: ") + Pin->PinName.ToString());
			}
			if (Pin->Direction != EGPD_Input)
			{
				if (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec && Pin->LinkedTo.Num() > 1)
					return Refuse(TEXT("replacement produced an unsupported execution output fan-out"));
				for (const UEdGraphPin* Sink : Pin->LinkedTo)
				{
					const UEdGraphNode* SinkNode = Sink ? Sink->GetOwningNodeUnchecked() : nullptr;
					if (!SinkNode || !Graph.Nodes.Contains(SinkNode) || !SinkNode->Pins.Contains(Sink)
						|| Sink->Direction != EGPD_Input || !Sink->LinkedTo.Contains(Pin))
						return Refuse(TEXT("replacement produced a foreign or incomplete output endpoint"));
				}
				continue;
			}
			const FInput* Request = Form ? Form->Inputs.FindByPredicate([&](const FInput& Input)
				{ return Input.Name.Equals(Pin->PinName.ToString(), ESearchCase::CaseSensitive); }) : nullptr;
			TArray<UEdGraphPin*> Outputs;
			if ((Request && !ResolveOutputs(*Request, Outputs)) || Pin->LinkedTo.Num() != Outputs.Num())
				return Refuse(TEXT("input link set differs from the requested body: ") + Pin->PinName.ToString());
			for (const UEdGraphPin* Output : Outputs)
				if (!Pin->LinkedTo.Contains(Output) || !Output->LinkedTo.Contains(Pin))
					return Refuse(TEXT("an exact requested wire did not survive: ") + Pin->PinName.ToString());
			if (!bExisting && !Reachable.Contains(Node) && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
				return Refuse(TEXT("new execution node has no path from a recognized entry: ") + Form->Type);
		}
	}
	for (const auto& Pair : Retained)
		if (!Graph.Nodes.Contains(Pair.Key)) return Refuse(TEXT("replacement lost a retained node"));
	return true;
}

FString FTurboMCPGraphReplace::RenderPreview(const FPlan& Plan)
{
	if (!Plan.bAcceptable())
		return TEXT("unchanged: replacement refused; nothing would be written or removed\n  ")
			+ FString::Join(Plan.Refusals, TEXT("\n  "))
			+ TEXT("\nRead Format=edit and keep its complete header and every retained node form. Use tmcp-delete-v1 for each deletion.");
	FString Out = FString::Printf(TEXT("preview: replace %s in %s; %d addition(s), %d input update(s), %d explicit removal(s)"),
		*Plan.Graph, *Plan.Asset, Plan.Positions.Num(), Plan.Updates.Num(), Plan.Removals.Num());
	for (const auto& Addition : Plan.Positions)
		Out += TEXT("\n  would ADD ") + Plan.Forms[Addition.FormIndex].Type;
	for (const FString& Update : Plan.Updates)
		Out += TEXT("\n  would UPDATE ") + Update;
	for (const FRemoval& Removal : Plan.Removals)
		Out += FString::Printf(TEXT("\n  would REMOVE %s %s '%s'"), *ReplaceId(Removal.Id), *Removal.ClassName, *Removal.Title);
	if (!Plan.HasChanges()) Out += TEXT("\n  no changes requested");
	return Out + TEXT("\nNo replacement was applied; compile=not_run; save=not_requested");
}

bool FTurboMCPGraphReplace::Apply(const FPlan& Plan, FString& OutReport, FString& OutError)
{
	return ApplyInternal(Plan, OutReport, OutError, false);
}

bool FTurboMCPGraphReplace::ApplyInternal(const FPlan& Plan, FString& OutReport, FString& OutError, bool bFailAfterRemoval)
{
	OutReport.Reset();
	OutError.Reset();
	if (!Plan.bAcceptable() || !IsInGameThread() || !GEditor || !GEditor->CanTransact() || GIsTransacting
		|| GEditor->IsTransactionActive() || GEditor->IsPlaySessionInProgress() || GCompilingBlueprint)
	{
		OutError = TEXT("unchanged: replacement requires a validated plan and an idle editor with undo");
		return false;
	}
	UBlueprint* Blueprint = nullptr;
	UEdGraph* Graph = nullptr;
	if (!FindReplaceGraph(Plan.Asset, Plan.Graph, Blueprint, Graph, OutError)) return false;
	FString Body;
	TArray<FString> Issues;
	if (!Blueprint->GetPathName().Equals(Plan.Asset, ESearchCase::CaseSensitive) || Plan.GraphGuid != Graph->GraphGuid
		|| FTurboMCPGraphText::EditSnapshot(*Blueprint, *Graph, Body, Issues) != Plan.BaseFingerprint || !Issues.IsEmpty())
	{
		OutError = TEXT("unchanged: replacement graph is stale, foreign or no longer supports a complete edit export");
		return false;
	}
	if (!Plan.HasChanges())
	{
		OutReport = TEXT("unchanged: edit export already matches; no transaction created; compile=not_run; save=not_requested");
		return true;
	}
	// Debugger settings can change without altering the graph fingerprint.
	for (const FRemoval& Removal : Plan.Removals)
	{
		const UEdGraphNode* Node = TurboMCPGraphScratch::ByGuid(Graph, Removal.Id);
		if (!Node || FTurboMCPGraphText::IsEditRemovalProtected(*Node))
		{
			OutError = TEXT("unchanged: removal target missing or protected: ") + ReplaceId(Removal.Id);
			return false;
		}
		const FString DebuggerIssue = RemovalDebuggerIssue(*Blueprint, *Node);
		if (!DebuggerIssue.IsEmpty())
		{
			OutError = TEXT("unchanged: cannot delete ") + ReplaceId(Removal.Id) + TEXT(": ") + DebuggerIssue;
			return false;
		}
	}

	UTransBuffer* Buffer = Cast<UTransBuffer>(GEditor->Trans);
	if (!Buffer) { OutError = TEXT("unchanged: editor undo is unavailable"); return false; }
	GEditor->BeginTransaction(NSLOCTEXT("TurboMCP", "GraphReplace", "TurboMCP replace a graph body"));
	const FGuid OwnedId = Buffer->ActiveCount == 1 && !Buffer->UndoBuffer.IsEmpty()
		? Buffer->UndoBuffer.Last()->GetId() : FGuid();
	auto OwnsActiveTransaction = [&]()
	{
		return OwnedId.IsValid() && Buffer->ActiveCount == 1 && !Buffer->UndoBuffer.IsEmpty()
			&& Buffer->UndoBuffer.Last()->GetId() == OwnedId;
	};
	ON_SCOPE_EXIT { if (OwnsActiveTransaction()) GEditor->EndTransaction(); };
	if (!OwnedId.IsValid()) { OutError = TEXT("unchanged: could not acquire an owned replacement transaction"); return false; }
	FString Failure;
	if (!Execute(Plan, *Blueprint, *Graph, nullptr, Failure, bFailAfterRemoval))
	{
		bool bUndone = false;
		if (OwnsActiveTransaction())
		{
			GEditor->EndTransaction();
			const int32 Index = Buffer->GetQueueLength() - Buffer->GetUndoCount() - 1;
			const FTransaction* Record = Index >= 0 ? Buffer->GetTransaction(Index) : nullptr;
			bUndone = Record && Record->GetId() == OwnedId && GEditor->UndoTransaction(false);
		}
		OutError = TEXT("partial: ") + Failure
			+ (bUndone ? TEXT("; owned undo applied; restoration is unverified") : TEXT("; owned undo failed or unavailable; restoration is unverified"))
			+ TEXT("; inspect the graph before retrying; compile=not_run; save=not_requested");
		return false;
	}
	OutReport = FString::Printf(TEXT("changed: replaced %s in %s; %d node(s) added, %d input(s) updated, %d node(s) removed"),
		*Plan.Graph, *Plan.Asset, Plan.Positions.Num(), Plan.Updates.Num(), Plan.Removals.Num());
	for (const FRemoval& Removal : Plan.Removals)
		OutReport += FString::Printf(TEXT("\n  REMOVED %s %s '%s'"), *ReplaceId(Removal.Id), *Removal.ClassName, *Removal.Title);
	OutReport += TEXT("\nRetained nodes, pin defaults and exact wires verified; undo=one_transaction; compile=not_run; save=not_requested");
	return true;
}
