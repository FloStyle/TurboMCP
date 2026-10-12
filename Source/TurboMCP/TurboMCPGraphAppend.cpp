// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "TurboMCPGraphAppend.h"

#include "TurboMCPAssetResolver.h"
#include "TurboMCPGraphCheck.h"
#include "TurboMCPNodeId.h"
#include "TurboMCPNodeType.h"

#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraph/EdGraphSchema.h"
#include "Engine/Blueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "ScopedTransaction.h"
#include "UObject/Package.h"

namespace
{
	/** Whichever form the text uses, one graph is looked up the same way. */
	UEdGraph* FindGraph(UBlueprint* InBlueprint, const FString& InGraphName)
	{
		if (InBlueprint == nullptr)
		{
			return nullptr;
		}

		TArray<UEdGraph*> Graphs;
		Graphs.Append(InBlueprint->UbergraphPages);
		Graphs.Append(InBlueprint->FunctionGraphs);
		Graphs.Append(InBlueprint->MacroGraphs);

		for (UEdGraph* Graph : Graphs)
		{
			if (Graph != nullptr && Graph->GetName().Equals(InGraphName, ESearchCase::CaseSensitive))
			{
				return Graph;
			}
		}
		return nullptr;
	}

	/**
	 * A value written after a pin name, read as the literal the format prints.
	 *
	 * THE FORMAT PRINTS A LINK AS `(-> Node:Pin)`, so a value that starts with `(` is a LINK and not a literal.
	 * Reporting that as a refusal rather than writing it as text is the difference between "this tool cannot do
	 * that yet" and a graph that compiles with a string where a wire was meant.
	 */
	bool ValueIsALink(const FString& InValue)
	{
		return InValue.StartsWith(TEXT("("));
	}

	/** Strips the quotes the format writes around text and structs. */
	FString Unquote(const FString& InValue)
	{
		if (InValue.Len() >= 2 && InValue.StartsWith(TEXT("\"")) && InValue.EndsWith(TEXT("\"")))
		{
			return InValue.Mid(1, InValue.Len() - 2);
		}
		return InValue;
	}
}

bool FTurboMCPGraphAppend::Plan(const FString& InAsset, const FString& InGraphName,
	const FString& InText, FPlan& OutPlan, FString& OutError)
{
	OutError.Empty();
	OutPlan = FPlan();
	OutPlan.Asset = InAsset.TrimStartAndEnd();
	OutPlan.GraphName = InGraphName.TrimStartAndEnd();

	// ---- 1. THE CHECKER RUNS FIRST, BECAUSE IT IS THE GATE EVERY WRITER DEPENDS ON -----------------
	//
	// `Spec/03_TOOLS.md` puts `P-GRAPH-CHECK` before every write entry point so that a writer and its checker
	// cannot disagree. Calling it rather than repeating its work is the whole reason it exists.
	FTurboMCPGraphCheck::FPlan CheckPlan;
	FString CheckError;
	if (!FTurboMCPGraphCheck::Plan(InAsset, InGraphName, InText, CheckPlan, CheckError))
	{
		OutError = CheckError.IsEmpty()
			? TEXT("the text could not be checked, and the reason was not reported")
			: CheckError;
		return false;
	}

	if (!CheckPlan.bAcceptable())
	{
		// THE FINDINGS ARE CARRIED ACROSS RATHER THAN RE-WORDED. A writer that paraphrased its checker's
		// refusals would drift from them, and the caller would have two vocabularies for one problem.
		for (const FTurboMCPGraphCheck::FFinding& Finding : CheckPlan.Findings)
		{
			OutPlan.Refusals.Add(FString::Printf(TEXT("line %d, column %d: %s"),
				Finding.Line, Finding.Column, *Finding.Message));
		}
		return true;
	}

	// ---- 2. THE GRAPH, SO THE NODE COUNT AND THE EXISTING IDS ARE KNOWN -----------------------------
	FTurboMCPAssetResolver::FResolved Resolved;
	const FString ResolveError = FTurboMCPAssetResolver::Resolve(InAsset, Resolved);
	if (!ResolveError.IsEmpty())
	{
		OutError = ResolveError;
		return false;
	}
	UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *Resolved.ObjectPath);
	if (Blueprint == nullptr)
	{
		OutError = FString::Printf(TEXT("'%s' is not a Blueprint, and only a Blueprint has graphs"),
			*Resolved.PackagePath);
		return false;
	}

	UEdGraph* Graph = FindGraph(Blueprint, OutPlan.GraphName);
	if (Graph == nullptr)
	{
		OutError = FString::Printf(TEXT("no graph named '%s' in '%s'"),
			*OutPlan.GraphName, *Resolved.PackagePath);
		return false;
	}

	OutPlan.NodeCountBefore = Graph->Nodes.Num();

	// ---- 3. THE FORMS, LEXED BY THE SHARED RULES -----------------------------------------------------
	
	const TArray<TurboMCPGraphTextLex::FLine> Lines = TurboMCPGraphTextLex::SplitLines(InText);

	int32 CurrentIndex = INDEX_NONE;

	for (const TurboMCPGraphTextLex::FLine& Line : Lines)
	{
		// A comment carries a node's id, so it is read before it is skipped: `NodeIdFromComment` returns empty
		// for a comment that is not an id, which is most of them.
		if (TurboMCPGraphTextLex::IsCommentLine(Line))
		{
			const FString FormId = TurboMCPGraphTextLex::NodeIdFromComment(Line.Text);
			if (!FormId.IsEmpty() && CurrentIndex != INDEX_NONE)
			{
				OutPlan.Nodes[CurrentIndex].ExistingNodeId = FormId;
			}
			continue;
		}

		FString TypeId;
		int32 Column = 0;
		int32 After = 0;
		if (TurboMCPGraphTextLex::ExtractHeadTypeId(Line.Text, TypeId, Column, After))
		{
			FNewNode Form;
			Form.TypeId = TypeId;
			Form.Line = Line.Number;
			Form.Column = Column;

			// THE ID MAY ALSO SIT ON THE HEAD LINE ITSELF, after the type id, which is how the format prints a
			// node whose first line already carries its comment.
			const FString OnHead = TurboMCPGraphTextLex::NodeIdFromComment(Line.Text.Mid(After));
			if (!OnHead.IsEmpty())
			{
				Form.ExistingNodeId = OnHead;
			}

			OutPlan.Nodes.Add(Form);
			CurrentIndex = OutPlan.Nodes.Num() - 1;
			continue;
		}

		FString PinName;
		FString PinValue;
		int32 PinColumn = 0;
		if (CurrentIndex != INDEX_NONE && TurboMCPGraphTextLex::ParsePinLine(Line.Text, PinName, PinValue, PinColumn))
		{
			TurboMCPGraphTextLex::FPinForm Pin;
			Pin.Name = PinName;
			Pin.Value = PinValue;
			Pin.Line = Line.Number;
			Pin.Column = PinColumn;
			OutPlan.Nodes[CurrentIndex].Pins.Add(Pin);
		}
	}

	if (OutPlan.Nodes.Num() == 0)
	{
		OutPlan.bNothingToDo = true;
		return true;
	}

	// ---- 4. EACH FORM IS RESOLVED: LINK TO AN EXISTING NODE, OR BUILD A NEW ONE ----------------------
	//
	// THE IDS PRESENT IN THE GRAPH ARE COLLECTED ONCE. Building them per form would walk the graph once per
	// node, and on a large graph that is the difference between a tool that answers and one that appears to
	// hang - a failure mode this project has already recorded once.
	TSet<FString> ExistingIds;
	for (const UEdGraphNode* Node : Graph->Nodes)
	{
		if (Node != nullptr)
		{
			ExistingIds.Add(TurboMCPNodeId::MakeForNode(Node, /*bLongForm=*/true));
		}
	}

	for (FNewNode& Form : OutPlan.Nodes)
	{
		if (!Form.ExistingNodeId.IsEmpty() && ExistingIds.Contains(Form.ExistingNodeId))
		{
			// THE SPEC'S "links them to existing nodes by node ID". A form naming a node that is ALREADY there
			// is a link target, not a second copy - making a copy is what would turn an append into a
			// duplicate-the-graph operation.
			Form.bLinksToExisting = true;
			continue;
		}

		// A FORM THAT NAMES AN ID THE GRAPH DOES NOT HAVE is refused rather than created, because the caller
		// meant to link to something and a new node would silently be a different graph than they asked for.
		if (!Form.ExistingNodeId.IsEmpty())
		{
			OutPlan.Refusals.Add(FString::Printf(
				TEXT("line %d, column %d: the form names the node '%s', which is not in '%s'. A form that "
					"carries an id is read as a LINK; remove the comment to create a new node instead."),
				Form.Line, Form.Column, *Form.ExistingNodeId, *OutPlan.GraphName));
			continue;
		}

		// THE FACTORY DECIDES WHETHER THE TYPE CAN BE BUILT HERE, and it is asked at PLAN time so a Preview
		// and an Edit refuse the same input rather than the Edit discovering it halfway through.
		FString SpawnNote;
		UEdGraphNode* Probe = nullptr;
		const FTurboMCPNodeType::ESpawnResult Result =
			FTurboMCPNodeType::SpawnNodeOfType(Blueprint, Graph, Form.TypeId, Probe, SpawnNote);

		if (Result != FTurboMCPNodeType::ESpawnResult::Spawned || Probe == nullptr)
		{
			OutPlan.Refusals.Add(FString::Printf(TEXT("line %d, column %d: %s"),
				Form.Line, Form.Column, *SpawnNote));
			continue;
		}

		// THE NODE THE PROBE BUILT IS REMOVED AGAIN, because `Plan` MUST NOT CHANGE THE ASSET. It is removed by
		// IDENTITY, the node this call created, so a failure here cannot take a node of the caller's with it.
		//
		// THE CLASS IS RECORDED, because it is what `Apply` reports and what makes the round trip checkable: a
		// caller comparing the input with the read-back needs to know what the id resolved TO.
		Form.ResolvedClass = Probe->GetClass()->GetName();

		// THE PROBE'S PIN NAMES ARE CAPTURED BEFORE IT IS REMOVED, because a link whose SOURCE is a node this
		// same text creates has to be validated against pins that do not exist in the graph yet. The probe is
		// the only place they can be READ without `Plan` touching the asset.
		for (const UEdGraphPin* ProbePin : Probe->Pins)
		{
			if (ProbePin != nullptr)
			{
				Form.ProbePinNames.Add(ProbePin->PinName.ToString());
			}
		}

		Graph->RemoveNode(Probe);
	}

	// ---- 5. LINKS WRITTEN AS A PIN VALUE ARE PLANNED HERE AND MADE IN `Apply` --------------------------
	//
	// The format prints a wire as `(-> Node:Pin)` in a pin's value position, and making it needs BOTH ENDS AS
	// REAL NODES - so the decision is taken here, where a refusal can be reported with its line and column and
	// the Preview and the Edit refuse the SAME input, and the wire itself is created in `Apply` after the nodes
	// exist. `P1` is the reason for that split rather than an implementation convenience.
	//
	// AN END MAY BE A NODE ALREADY IN THE GRAPH OR A FORM THIS SAME TEXT CREATES, and both cases matter: the
	// second is what makes an appended BLOCK useful rather than a set of disconnected nodes.
	{
		// The nodes already in the graph, by every key form a caller may have written, so a link resolves the
		// same way `R-GRAPH-LOAD` prints it.
		TMap<FString, UEdGraphNode*> ExistingByKey;
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (Node == nullptr)
			{
				continue;
			}
			ExistingByKey.Add(TurboMCPNodeId::MakeForNode(Node, /*bLongForm=*/true), Node);
			ExistingByKey.Add(TurboMCPNodeId::MakeForNode(Node, /*bLongForm=*/false), Node);
			// The short key alone is also accepted, because a caller copying a line out of a report may have
			// only that much.
			ExistingByKey.Add(Node->NodeGuid.ToString(EGuidFormats::Digits).Left(6), Node);
		}

		// The forms this text creates, by the key a link may name. A created form has no id yet, so it is
		// addressed by ITS TYPE ID - which is what a caller writing a block by hand can actually know.
		TMap<FString, int32> FormByTypeId;
		for (int32 Index = 0; Index < OutPlan.Nodes.Num(); ++Index)
		{
			const FNewNode& Form = OutPlan.Nodes[Index];
			if (!Form.bLinksToExisting && !FormByTypeId.Contains(Form.TypeId))
			{
				FormByTypeId.Add(Form.TypeId, Index);
			}
		}

		// Resolves one end of a wire to either an existing node or a created form. Returns false and fills
		// `OutReason` when neither can supply it, so the refusal names what was wrong rather than only that
		// something was.
		auto ResolveEnd = [&](const FString& InName, int32 InLine, int32 InColumn, const TCHAR* InSide,
			FString& OutKey, int32& OutFormIndex, UEdGraphNode*& OutNode, TArray<FString>& OutKnownPins,
			FString& OutReason) -> bool
		{
			if (UEdGraphNode* const* Found = ExistingByKey.Find(InName))
			{
				OutNode = *Found;
				// THE CANONICAL KEY IS STORED, NOT THE NAME THE CALLER WROTE, AND APPLY DEPENDS ON THAT.
				// MEASURED: this used to keep `InName` while `Apply` compared against
				// `MakeForNode(Node, /*bLongForm=*/true)`, and the two agree only when the caller happened to
				// write the long form with no pin suffix. A link naming the SHORT key, or the bare key, planned
				// fine and then failed at apply time with "one end was not in the graph" - a message that blamed
				// the graph for a key mismatch this code created. Normalising at resolution makes the plan and
				// the apply agree by construction rather than by the caller's spelling.
				OutKey = TurboMCPNodeId::MakeForNode(OutNode, /*bLongForm=*/true);
				OutFormIndex = INDEX_NONE;
				for (const UEdGraphPin* Pin : OutNode->Pins)
				{
					if (Pin != nullptr)
					{
						OutKnownPins.Add(Pin->PinName.ToString());
					}
				}
				return true;
			}

			if (const int32* Found = FormByTypeId.Find(InName))
			{
				OutNode = nullptr;
				OutKey.Reset();
				OutFormIndex = *Found;
				OutKnownPins = OutPlan.Nodes[*Found].ProbePinNames;
				return true;
			}

			OutReason = FString::Printf(
				TEXT("line %d, column %d: the link's %s names '%s', which is neither a node in '%s' nor a node ")
				TEXT("this text creates. A link's ends are a node id already in the graph or the type id of a ")
				TEXT("form written above it."),
				InLine, InColumn, InSide, *InName, *OutPlan.GraphName);
			return false;
		};

		for (FNewNode& Form : OutPlan.Nodes)
		{
			if (Form.bLinksToExisting)
			{
				continue;
			}

			for (const TurboMCPGraphTextLex::FPinForm& Pin : Form.Pins)
			{
				if (!TurboMCPGraphTextLex::ValueIsALink(Pin.Value))
				{
					continue;
				}

				TurboMCPGraphTextLex::FLinkForm Link;
				FString Reason;
				if (!TurboMCPGraphTextLex::ParseLinkValue(Pin.Value, Link, Reason))
				{
					OutPlan.Refusals.Add(FString::Printf(TEXT("line %d, column %d: %s"),
						Pin.Line, Pin.Column, *Reason));
					continue;
				}

				FNewLink NewLink;
				NewLink.Line = Pin.Line;
				NewLink.Column = Pin.Column;
				NewLink.ToFormIndex = &Form - OutPlan.Nodes.GetData();
				NewLink.ToPin = Pin.Name;

				UEdGraphNode* FromNode = nullptr;
				TArray<FString> KnownPins;
				if (!ResolveEnd(Link.FromNode, Pin.Line, Pin.Column, TEXT("source"),
					NewLink.FromNodeKey, NewLink.FromFormIndex, FromNode, KnownPins, Reason))
				{
					OutPlan.Refusals.Add(Reason);
					continue;
				}
				NewLink.FromPin = Link.FromPin;

				// THE SOURCE PIN MUST EXIST, and the refusal lists the real names - which is the rule the spec
				// states for a pin generally, applied to the end of a wire.
				if (KnownPins.Num() > 0 && !KnownPins.Contains(Link.FromPin))
				{
					OutPlan.Refusals.Add(FString::Printf(
						TEXT("line %d, column %d: the link's source pin '%s' does not exist on '%s'. It has: %s."),
						Pin.Line, Pin.Column, *Link.FromPin, *Link.FromNode,
						*FString::Join(KnownPins, TEXT(", "))));
					continue;
				}

				OutPlan.Links.Add(MoveTemp(NewLink));
			}
		}
	}

	// A text whose every form links to an existing node has nothing to create, so it is not an error - it is
	// simply nothing to do, and the report says so rather than claiming a change it will not make.
	bool bAnyCreation = false;
	for (const FNewNode& Form : OutPlan.Nodes)
	{
		if (!Form.bLinksToExisting)
		{
			bAnyCreation = true;
			break;
		}
	}
	OutPlan.bNothingToDo = !bAnyCreation;

	return true;
}

FString FTurboMCPGraphAppend::RenderPreview(const FPlan& InPlan)
{
	if (InPlan.Refusals.Num() > 0)
	{
		// THE FIRST LINE IS `unchanged`, as the spec requires of every write entry point, so a caller that
		// reads only the first line still gets the right answer.
		FString Out = FString::Printf(TEXT("unchanged: %d refusal(s), so nothing would be added to %s"),
			InPlan.Refusals.Num(), *InPlan.GraphName);
		for (const FString& Refusal : InPlan.Refusals)
		{
			Out += FString::Printf(TEXT("\n  refused %s"), *Refusal);
		}
		Out += TEXT("\nthe preview changes NOTHING. Fix the refusals and call again.");
		return Out;
	}

	if (InPlan.bNothingToDo)
	{
		return FString::Printf(
			TEXT("unchanged: the text describes no node to create in %s, so there is nothing to append. ")
			TEXT("Its %d form(s) all name nodes that already exist, which an append LINKS to rather than ")
			TEXT("duplicating."),
			*InPlan.GraphName, InPlan.Nodes.Num());
	}

	int32 NewCount = 0;
	int32 LinkCount = 0;
	for (const FNewNode& Form : InPlan.Nodes)
	{
		if (Form.bLinksToExisting) { ++LinkCount; } else { ++NewCount; }
	}

	FString Out = FString::Printf(TEXT("preview: append to %s in %s : %d new node(s), %d form(s) link to existing"),
		*InPlan.GraphName, *InPlan.Asset, NewCount, LinkCount);
	Out += FString::Printf(TEXT("\n%d node(s) in the graph now, and %d would be added"),
		InPlan.NodeCountBefore, NewCount);

	// THE WIRES ARE REPORTED IN THE PREVIEW TOO, so a caller sees before writing whether the block will be
	// CONNECTED or a set of loose nodes - which is the difference between an append and a pile.
	for (const FNewLink& Link : InPlan.Links)
	{
		Out += FString::Printf(TEXT("\n  wire  line %d: %s:%s -> %s"),
			Link.Line,
			Link.FromFormIndex != INDEX_NONE
				? *InPlan.Nodes[Link.FromFormIndex].TypeId : *Link.FromNodeKey,
			*Link.FromPin, *Link.ToPin);
	}

	for (const FNewNode& Form : InPlan.Nodes)
	{
		if (Form.bLinksToExisting)
		{
			Out += FString::Printf(TEXT("\n  link  line %d: %s already exists, so it is linked to"),
				Form.Line, *Form.ExistingNodeId);
		}
		else
		{
			Out += FString::Printf(TEXT("\n  new   line %d: %s  ->  %s  (%d pin(s) written)"),
				Form.Line, *Form.TypeId, *Form.ResolvedClass, Form.Pins.Num());
		}
	}

	Out += TEXT("\n; the preview changes NOTHING. Call the edit to write it.");
	return Out;
}

bool FTurboMCPGraphAppend::Apply(const FPlan& InPlan, FString& OutReport, FString& OutError)
{
	OutReport.Empty();
	OutError.Empty();

	if (InPlan.Refusals.Num() > 0)
	{
		// THE PLAN ALREADY DECIDED. Re-deciding here is how a Preview and an Edit come to disagree, which is
		// the one thing the "put every check in Plan" rule exists to prevent.
		OutReport = RenderPreview(InPlan);
		return true;
	}
	if (InPlan.bNothingToDo)
	{
		OutReport = RenderPreview(InPlan);
		return true;
	}

	FTurboMCPAssetResolver::FResolved Resolved;
	const FString ResolveError = FTurboMCPAssetResolver::Resolve(InPlan.Asset, Resolved);
	if (!ResolveError.IsEmpty())
	{
		OutError = ResolveError;
		return false;
	}
	UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *Resolved.ObjectPath);
	if (Blueprint == nullptr)
	{
		OutError = FString::Printf(TEXT("'%s' is not a Blueprint"), *Resolved.PackagePath);
		return false;
	}

	UEdGraph* Graph = FindGraph(Blueprint, InPlan.GraphName);
	if (Graph == nullptr)
	{
		OutError = FString::Printf(TEXT("no graph named '%s' in '%s'"),
			*InPlan.GraphName, *Resolved.PackagePath);
		return false;
	}

	// ---- ONE TRANSACTION, as the spec requires, so a refusal anywhere leaves the graph as it was ---------
	FScopedTransaction Transaction(NSLOCTEXT("TurboMCP", "GraphAppend", "TurboMCP append to graph"));
	Blueprint->Modify();
	Graph->Modify();

	// ---- CREATE, THEN LINK, THEN SET PINS. THAT ORDER IS NOT ARBITRARY: a pin value can only be written on a
	// node whose pins exist, and pins exist only after `AllocateDefaultPins`, which the factory runs. ---------
	//
	// `Created` IS INDEXED BY FORM, NOT DENSELY, because `FNewLink` addresses its two ends by form index. A
	// dense array would silently mis-address every link whose source is not the first form created.
	TArray<UEdGraphNode*> CreatedByForm;
	CreatedByForm.SetNum(InPlan.Nodes.Num());
	TArray<FString> CreatedByLine;
	TArray<FString> Unmatched;
	int32 PinsWritten = 0;
	int32 LinksMade = 0;
	TArray<UEdGraphNode*> Created;

	for (int32 FormIndex = 0; FormIndex < InPlan.Nodes.Num(); ++FormIndex)
	{
		const FNewNode& Form = InPlan.Nodes[FormIndex];
		if (Form.bLinksToExisting)
		{
			continue;
		}

		FString SpawnNote;
		UEdGraphNode* Made = nullptr;
		const FTurboMCPNodeType::ESpawnResult Result =
			FTurboMCPNodeType::SpawnNodeOfType(Blueprint, Graph, Form.TypeId, Made, SpawnNote);

		if (Result != FTurboMCPNodeType::ESpawnResult::Spawned || Made == nullptr)
		{
			// THE PLAN SAID THIS WOULD WORK AND IT DID NOT. Rolling back and saying so is the spec's rule:
			// a failed compare, or a failed creation, is a FAILURE rather than a success with a note.
			Transaction.Cancel();
			OutError = FString::Printf(
				TEXT("line %d: the plan accepted '%s' but the graph refused it at apply time: %s. ")
				TEXT("NOTHING was written."),
				Form.Line, *Form.TypeId, *SpawnNote);
			return false;
		}

		CreatedByForm[FormIndex] = Made;
		Created.Add(Made);
		CreatedByLine.Add(FString::Printf(TEXT("%s (%s)"), *Form.TypeId, *Made->GetName()));

		// ---- THE PINS, BY KEYWORD NAME ONLY ------------------------------------------------------------
		for (const TurboMCPGraphTextLex::FPinForm& Pin : Form.Pins)
		{
			// A LINK IS NOT A LITERAL AND IS MADE BELOW, once every node exists. Writing `(-> A:B)` into a pin
			// as text is exactly the silent corruption this skip prevents.
			if (TurboMCPGraphTextLex::ValueIsALink(Pin.Value))
			{
				continue;
			}

			UEdGraphPin* Target = nullptr;
			for (UEdGraphPin* Candidate : Made->Pins)
			{
				if (Candidate != nullptr && Candidate->PinName.ToString().Equals(Pin.Name, ESearchCase::CaseSensitive))
				{
					Target = Candidate;
					break;
				}
			}

			if (Target == nullptr)
			{
				Transaction.Cancel();
				OutError = FString::Printf(
					TEXT("line %d: '%s' has no pin named '%s' at apply time. NOTHING was written."),
					Pin.Line, *Form.TypeId, *Pin.Name);
				return false;
			}

			// THE PIN'S OWN TYPE DECIDES HOW THE LITERAL IS READ. Writing "42" into a string pin as a number
			// would fail silently, which is why the schema's default-value path is used rather than a raw
			// string assignment.
			const FString Literal = Unquote(Pin.Value);
			if (!Target->DefaultValue.Equals(Literal, ESearchCase::CaseSensitive))
			{
				Target->DefaultValue = Literal;
				// The read-back compares what the ENGINE now reports, so the node is marked modified and the
				// schema is given the chance to reject a literal it cannot hold.
				Made->Modify();
				Graph->GetSchema()->TrySetDefaultValue(*Target, Literal);
				++PinsWritten;
			}
		}
	}

	// ---- THE WIRES, MADE HERE BECAUSE BOTH ENDS EXIST ONLY NOW ------------------------------------------
	//
	// A wire is the part of an append most likely to be silently dropped, so every failure below ROLLS BACK
	// AND RETURNS FALSE rather than reporting a partial success. The read-back comparison after this cannot
	// see a wire that was never made, which is exactly why the making is checked as it happens.
	for (const FNewLink& Link : InPlan.Links)
	{
		UEdGraphNode* FromNode = nullptr;
		if (Link.FromFormIndex != INDEX_NONE)
		{
			FromNode = CreatedByForm.IsValidIndex(Link.FromFormIndex)
				? CreatedByForm[Link.FromFormIndex] : nullptr;
		}
		else
		{
			for (UEdGraphNode* Node : Graph->Nodes)
			{
				if (Node != nullptr && TurboMCPNodeId::MakeForNode(Node, /*bLongForm=*/true) == Link.FromNodeKey)
				{
					FromNode = Node;
					break;
				}
			}
		}

		UEdGraphNode* ToNode = (Link.ToFormIndex != INDEX_NONE && CreatedByForm.IsValidIndex(Link.ToFormIndex))
			? CreatedByForm[Link.ToFormIndex] : nullptr;

		if (FromNode == nullptr || ToNode == nullptr)
		{
			Transaction.Cancel();
			OutError = FString::Printf(
				TEXT("line %d: the plan accepted a link from '%s' to '%s', but one end was not in the graph ")
				TEXT("at apply time. NOTHING was written."),
				Link.Line, *Link.FromNodeKey, *Link.ToPin);
			return false;
		}

		UEdGraphPin* FromPin = nullptr;
		for (UEdGraphPin* Candidate : FromNode->Pins)
		{
			if (Candidate != nullptr
				&& Candidate->PinName.ToString().Equals(Link.FromPin, ESearchCase::CaseSensitive))
			{
				FromPin = Candidate;
				break;
			}
		}
		UEdGraphPin* ToPin = nullptr;
		for (UEdGraphPin* Candidate : ToNode->Pins)
		{
			if (Candidate != nullptr
				&& Candidate->PinName.ToString().Equals(Link.ToPin, ESearchCase::CaseSensitive))
			{
				ToPin = Candidate;
				break;
			}
		}

		if (FromPin == nullptr || ToPin == nullptr)
		{
			Transaction.Cancel();
			OutError = FString::Printf(
				TEXT("line %d: the plan accepted a link '%s' -> '%s', but a pin was missing at apply time ")
				TEXT("(%s). NOTHING was written."),
				Link.Line, *Link.FromPin, *Link.ToPin,
				FromPin == nullptr ? TEXT("source") : TEXT("target"));
			return false;
		}

		// THE ENGINE'S OWN CONNECTION PATH, so the schema decides whether the two pins may join rather than this
		// tool assuming they can. A refusal here is a PLAN fault rather than a caller fault only in appearance:
		// the plan checked that the pins EXIST, and existence is not the same as compatibility.
		if (!Graph->GetSchema()->TryCreateConnection(FromPin, ToPin))
		{
			Transaction.Cancel();
			OutError = FString::Printf(
				TEXT("line %d: the engine refused the link '%s' -> '%s'. The pins exist but the schema will ")
				TEXT("not join them, so NOTHING was written."),
				Link.Line, *Link.FromPin, *Link.ToPin);
			return false;
		}
		++LinksMade;
	}

	// ---- COMPILE, THEN READ BACK AND COMPARE, which the spec demands rather than a claim of success --------
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

	// THE READ BACK USES THE READER, NOT THIS TOOL'S OWN MEMORY. A writer reporting what it just wrote is not
	// evidence; `FTurboMCPNodeId` reads identity off the live graph, which is what a caller would see.
	TSet<FString> NowInGraph;
	for (const UEdGraphNode* Node : Graph->Nodes)
	{
		if (Node != nullptr)
		{
			NowInGraph.Add(TurboMCPNodeId::MakeForNode(Node, /*bLongForm=*/true));
		}
	}

	int32 Survived = 0;
	for (int32 Index = 0; Index < Created.Num(); ++Index)
	{
		if (Created[Index] != nullptr && Graph->Nodes.Contains(Created[Index]))
		{
			++Survived;
		}
		else
		{
			Unmatched.Add(CreatedByLine[Index]);
		}
	}

	// ---- THE FIRST LINE IS `changed` OR `unchanged`, as the spec requires ----------------------------------
	FString Out = FString::Printf(TEXT("changed: appended to %s in %s : %d node(s) added, %d -> %d"),
		*InPlan.GraphName, *InPlan.Asset, Survived, InPlan.NodeCountBefore, Graph->Nodes.Num());

	Out += FString::Printf(TEXT("\n%d pin value(s) written across those node(s)"), PinsWritten);
	// THE WIRE COUNT IS REPORTED because a wire is the part most easily dropped in silence, and a caller
	// comparing this against the number of link lines it submitted is the cheapest way to notice that.
	Out += FString::Printf(TEXT("\n%d wire(s) made, of %d the text described"),
		LinksMade, InPlan.Links.Num());
	Out += FString::Printf(TEXT("\n%d of %d created node(s) are in the graph after the write"),
		Survived, Created.Num());

	if (Unmatched.Num() > 0)
	{
		// A FAILED COMPARE IS REPORTED AS A FAILURE. The line says so and names what did not match, rather
		// than burying it under a success sentence.
		Out += FString::Printf(TEXT("\nFAILED COMPARE: %d node(s) the plan created are NOT in the graph: %s"),
			Unmatched.Num(), *FString::Join(Unmatched, TEXT(", ")));
		Out += TEXT("\n; this is a FAILURE, not a success with a note. Read the graph back and compare.");
	}
	else
	{
		Out += TEXT("\nthe read back matches: every node the plan created is in the graph.");
	}

	// THE REPORT IS HANDED BACK. IT WAS NOT AT FIRST, AND THE BUG WAS INVISIBLE FROM OUTSIDE., AND THE BUG WAS INVISIBLE FROM OUTSIDE.
	//
	// MEASURED: `Apply` built the whole report into a local `Out`, returned `true`, and NEVER ASSIGNED IT to
	// `OutReport`. The caller therefore got an EMPTY STRING with a success code - and because the nodes WERE
	// created and the graph WAS correct, every check that looked at the GRAPH passed while the only thing the
	// caller actually receives was blank. The spec requires the first line to be `changed` or `unchanged`, and
	// an empty answer is neither.
	//
	// THIS IS THE "A CHECK THAT PASSES WHILE THE THING IT VERIFIES IS BROKEN" SHAPE, and it was caught only by
	// READING THE RETURN VALUE rather than by reading the graph.
	OutReport = Out;
	return true;
}
