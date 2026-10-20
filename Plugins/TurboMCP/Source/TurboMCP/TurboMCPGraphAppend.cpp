// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "TurboMCPGraphAppend.h"

#include "TurboMCPRollback.h"
#include "TurboMCPAssetResolver.h"
#include "TurboMCPGraphCheck.h"
#include "TurboMCPNodeId.h"
#include "TurboMCPNodeType.h"

#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraph/EdGraphSchema.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "ScopedTransaction.h"
#include "Editor.h"
#include "UObject/Package.h"

namespace
{
    bool SetLiteral(UEdGraph* Graph, UEdGraphPin* Pin, const FString& Value, bool& Changed, FString& Error)
    {
        Changed = false;
        const UEdGraphSchema_K2* Schema = Graph ? Cast<UEdGraphSchema_K2>(Graph->GetSchema()) : nullptr;
        if (!Schema || !Pin || Pin->Direction != EGPD_Input || Pin->bOrphanedPin
            || Pin->bDefaultValueIsReadOnly || Pin->bDefaultValueIsIgnored || Pin->LinkedTo.Num()
            || Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
        { Error = TEXT("pin cannot accept a literal default"); return false; }
        FString Parsed; TObjectPtr<UObject> Object = nullptr; FText Text;
        Schema->GetPinDefaultValuesFromString(Pin->PinType, Pin->GetOwningNode(), Value, Parsed, Object, Text, false);
        Error = Schema->IsPinDefaultValid(Pin, Parsed, Object, Text);
        if (!Error.IsEmpty()) return false;
        if (Pin->DefaultValue == Parsed && Pin->DefaultObject == Object && Pin->DefaultTextValue.EqualTo(Text)) return true;
        const FGuid NodeGuid = Pin->GetOwningNode()->NodeGuid;
        const FName PinName = Pin->PinName;
        Pin->GetOwningNode()->Modify();
        Schema->TrySetDefaultValue(*Pin, Value, true);
        for (UEdGraphNode* Node : Graph->Nodes)
            if (Node && Node->NodeGuid == NodeGuid)
                for (UEdGraphPin* Current : Node->Pins)
                    if (Current && Current->Direction == EGPD_Input && Current->PinName == PinName
                        && Current->DefaultValue == Parsed && Current->DefaultObject == Object
                        && Current->DefaultTextValue.EqualTo(Text))
                    { Changed = true; return true; }
        Error = TEXT("pin default read-back failed after schema callback");
        return false;
    }

	/** Whichever form the text uses, one graph is looked up the same way. */
	UEdGraph* FindGraph(UBlueprint* InBlueprint, const FString& InGraphName)
	{
		// *** ONE RESOLUTION POLICY WITH EVERY OTHER TOOL AND THE READER. ***
		// This was one of EIGHT graph resolvers. It searched UbergraphPages, FunctionGraphs and MacroGraphs;
		// the reader's TurboMCPNodeId::FindGraphByName searches those THREE PLUS EventGraphs, and nothing
		// compared them. A node id is `...:GraphName:<hash>` - the graph NAME ONLY - so a name matching
		// two graphs in different arrays would make this tool EDIT ONE AND REPORT ON ANOTHER, and no
		// caller could tell. That is the ARC06 family: identity that cannot be verified across calls.
		// It now delegates, so this tool and the reader resolve a name identically.
				int32 Matches = 0;
        UEdGraph* Graph = TurboMCPNodeId::FindGraphByName(InBlueprint, InGraphName, Matches);
        return Matches == 1 ? Graph : nullptr;
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

	/** A connection counts only when both pin records agree. */
	bool HasLink(const UEdGraphPin* InA, const UEdGraphPin* InB)
	{
		return InA != nullptr && InB != nullptr
			&& InA->LinkedTo.Contains(InB) && InB->LinkedTo.Contains(InA);
	}

	/** The execution output that can carry a displaced continuation after an insertion. */
	UEdGraphPin* FindExecContinuation(UEdGraphNode* InNode)
	{
		if (InNode == nullptr)
		{
			return nullptr;
		}
		for (UEdGraphPin* Pin : InNode->Pins)
		{
			if (Pin != nullptr && Pin->Direction == EGPD_Output
				&& Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
			{
				return Pin;
			}
		}
		return nullptr;
	}

	/**
	 * A requested placement must name every created form exactly once. A missing, duplicated, link-only or
	 * out-of-range entry refuses before the caller opens a transaction.
	 */
	bool ValidatePlannedPositions(const FTurboMCPGraphAppend::FPlan& InPlan, FString& OutError)
	{
		int32 Expected = 0;
		for (const FTurboMCPGraphAppend::FNewNode& Form : InPlan.Nodes)
		{
			if (!Form.bLinksToExisting)
			{
				++Expected;
			}
		}

		TSet<int32> Seen;
		for (const FTurboMCPGraphAppend::FPlannedPosition& Position : InPlan.PlannedPositions)
		{
			if (!InPlan.Nodes.IsValidIndex(Position.FormIndex)
				|| InPlan.Nodes[Position.FormIndex].bLinksToExisting)
			{
				OutError = FString::Printf(
					TEXT("placement: planned position %d does not address a node-creating form"),
					Position.FormIndex);
				return false;
			}
			if (Seen.Contains(Position.FormIndex))
			{
				OutError = FString::Printf(
					TEXT("placement: form %d has more than one planned position"), Position.FormIndex);
				return false;
			}
			Seen.Add(Position.FormIndex);
		}

		if (Seen.Num() != Expected)
		{
			OutError = FString::Printf(
				TEXT("placement: %d node(s) would be created but %d position(s) were planned"),
				Expected, Seen.Num());
			return false;
		}
		return true;
	}
}

bool FTurboMCPGraphAppend::Plan(const FString& InAsset, const FString& InGraphName,
	const FString& InText, FPlan& OutPlan, FString& OutError, bool bRequestPlacement)
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
			if (CurrentIndex != INDEX_NONE)
			{
				const FString FormId = TurboMCPGraphTextLex::NodeIdFromComment(Line.Text);
				if (!FormId.IsEmpty())
				{
					OutPlan.Nodes[CurrentIndex].ExistingNodeId = FormId;
				}
				const FString LocalLabel = TurboMCPGraphTextLex::LocalLabelFromComment(Line.Text);
				if (!LocalLabel.IsEmpty())
				{
					OutPlan.Nodes[CurrentIndex].LocalLabel = LocalLabel;
				}
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
			const FString HeadLabel = TurboMCPGraphTextLex::LocalLabelFromComment(Line.Text.Mid(After));
			if (!HeadLabel.IsEmpty())
			{
				Form.LocalLabel = HeadLabel;
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
			// Continuations such as `:then` and `:else` are reader containers, not input pins. Keep the append
			// parser aligned with GraphCheck so exact reader text cannot later try to write a `:then` literal.
			if (PinName.StartsWith(TEXT(":")))
			{
				continue;
			}
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
		// THE PROBE IS BUILT IN A THROWAWAY TRANSIENT GRAPH. The real graph is used only to filter the action
		// list. `UBlueprintNodeSpawner::SpawnNode` calls `ParentGraph->Modify()`; using the real graph would
		// mark the asset dirty even though the probe is removed again. A transient graph makes that
		// `MarkPackageDirty` a no-op while preserving the same action filtering, class and pin resolution.
		FString SpawnNote;
		UEdGraphNode* Probe = FTurboMCPNodeType::BuildProbeNode(Blueprint, Graph, Form.TypeId,
			SpawnNote);
		if (Probe == nullptr)
		{
			OutPlan.Refusals.Add(FString::Printf(TEXT("line %d, column %d: %s"),
				Form.Line, Form.Column, *SpawnNote));
			continue;
		}
		UEdGraph* ProbeGraph = Cast<UEdGraph>(Probe->GetOuter());
		if (ProbeGraph == nullptr || ProbeGraph == Graph
			|| Probe->GetGraph() != ProbeGraph
			|| !ProbeGraph->HasAnyFlags(RF_Transient)
			|| ProbeGraph->GetOuter() != Blueprint)
		{
			// The spawner returned something we cannot prove is owned by the throwaway graph. Do not mark it
			// garbage: it may be a real-graph node or a shared template/cache object, and this path must not
			// mutate or retire either. Refuse the form instead.
			OutPlan.Refusals.Add(FString::Printf(
				TEXT("line %d, column %d: the probe for '%s' did not report a node owned by the transient ")
				TEXT("scratch graph, so this type is refused rather than mutated or retired."),
				Form.Line, Form.Column, *Form.TypeId));
			continue;
		}

		// THE CLASS IS RECORDED, because it is what `Apply` reports and what makes the round trip checkable: a
		// caller comparing the input with the read-back needs to know what the id resolved TO.
		Form.ResolvedClass = Probe->GetClass()->GetName();

		// THE PROBE'S PIN NAMES ARE CAPTURED BEFORE IT IS RETIRED, because a link whose SOURCE is a node this
		// same text creates has to be validated against pins that do not exist in the graph yet.
		if (Probe->Pins.Num() == 0)
		{
			Probe->AllocateDefaultPins();
		}
		for (const UEdGraphPin* ProbePin : Probe->Pins)
		{
			if (ProbePin != nullptr)
			{
				Form.ProbePinNames.Add(ProbePin->PinName.ToString());
			}
		}

		// The probe was never added to the real graph, so retire it with the throwaway graph it owns.
		Probe->MarkAsGarbage();
		if (ProbeGraph != nullptr && ProbeGraph != Graph)
		{
			ProbeGraph->MarkAsGarbage();
		}
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

		// Qualified keys are unique because `MakeForNode` grows its prefix, but the six-character alias is
		// not: two GUIDs can share it. Keep the alias in its own map and record a shared one instead of
		// letting `TMap::Add` keep the last node. Only a reference to a shared alias refuses.
		TMap<FString, UEdGraphNode*> ExistingByBareKey;
		TSet<FString> AmbiguousBareKeys;
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (Node == nullptr)
			{
				continue;
			}
			ExistingByKey.Add(TurboMCPNodeId::MakeForNode(Node, /*bLongForm=*/true), Node);
			ExistingByKey.Add(TurboMCPNodeId::MakeForNode(Node, /*bLongForm=*/false), Node);

			// `MakeKey` is the project's own lower-case spelling, and `NormaliseKey` lower-cases the caller's
			// token below, so the alias match does not depend on container case rules.
			const FString BareKey = TurboMCPNodeId::MakeKey(Node->NodeGuid);
			if (UEdGraphNode* const* Existing = ExistingByBareKey.Find(BareKey))
			{
				if (*Existing != Node)
				{
					AmbiguousBareKeys.Add(BareKey);
				}
			}
			else
			{
				ExistingByBareKey.Add(BareKey, Node);
			}
		}

		// The forms this text creates, by the key a link may name. A created form has no id yet, so it is
		// addressed by ITS TYPE ID - which is what a caller writing a block by hand can actually know.
		TMap<FString, int32> FormByTypeId;
		TMap<FString, int32> FormByLocalLabel;
		for (int32 Index = 0; Index < OutPlan.Nodes.Num(); ++Index)
		{
			const FNewNode& Form = OutPlan.Nodes[Index];
			if (Form.bLinksToExisting)
			{
				continue;
			}
			if (!FormByTypeId.Contains(Form.TypeId))
			{
				FormByTypeId.Add(Form.TypeId, Index);
			}
			if (!Form.LocalLabel.IsEmpty())
			{
				if (FormByLocalLabel.Contains(Form.LocalLabel))
				{
					OutPlan.Refusals.Add(FString::Printf(
						TEXT("line %d, column %d: local label '%s' is already used by another new node in this text."),
						Form.Line, Form.Column, *Form.LocalLabel));
				}
				else
				{
					FormByLocalLabel.Add(Form.LocalLabel, Index);
				}
			}
		}

		// Resolves one end of a wire to either an existing node or a created form. Returns false and fills
		// `OutReason` when neither can supply it, so the refusal names what was wrong rather than only that
		// something was.
		auto ResolveEnd = [&](const FString& InName, int32 InLine, int32 InColumn, const TCHAR* InSide,
			FString& OutKey, int32& OutFormIndex, UEdGraphNode*& OutNode, TArray<FString>& OutKnownPins,
			FString& OutReason) -> bool
		{
			// An existing node is accepted by whichever spelling resolved it. The canonical long key is
			// stored, not the caller's spelling, because `Apply` compares against it.
			auto AcceptExisting = [&](UEdGraphNode* InNode) -> bool
			{
				OutNode = InNode;
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
			};

			if (UEdGraphNode* const* Found = ExistingByKey.Find(InName))
			{
				return AcceptExisting(*Found);
			}

			// Only a token that normalises to exactly six hex characters is an alias. `NormaliseKey` also
			// accepts longer strings, and those are labels or type ids in this dialect.
			const FString LookupKey = TurboMCPNodeId::NormaliseKey(InName);
			if (LookupKey.Len() == TurboMCPNodeId::StandardKeyDigits)
			{
				if (AmbiguousBareKeys.Contains(LookupKey))
				{
					// The token cannot select one of the sharing nodes. Refuse and name the qualified ids.
					TArray<FString> Candidates;
					for (const UEdGraphNode* Node : Graph->Nodes)
					{
						if (Node != nullptr
							&& TurboMCPNodeId::MakeKey(Node->NodeGuid).Equals(LookupKey, ESearchCase::IgnoreCase))
						{
							Candidates.Add(TurboMCPNodeId::MakeForNode(Node));
						}
					}
					OutReason = FString::Printf(
						TEXT("line %d, column %d: the link's %s names the short node id '%s', which fits %d ")
						TEXT("nodes in '%s', so it is ambiguous. Use a qualified id: %s."),
						InLine, InColumn, InSide, *InName, Candidates.Num(), *OutPlan.GraphName,
						*FString::Join(Candidates, TEXT(", ")));
					return false;
				}

				if (UEdGraphNode* const* Found = ExistingByBareKey.Find(LookupKey))
				{
					return AcceptExisting(*Found);
				}
			}

			if (const int32* Found = FormByLocalLabel.Find(InName))
			{
				OutNode = nullptr;
				OutKey.Reset();
				OutFormIndex = *Found;
				OutKnownPins = OutPlan.Nodes[*Found].ProbePinNames;
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
				NewLink.ToPin = Pin.Name;

				UEdGraphNode* ToNode = nullptr;
				TArray<FString> DestinationPins;
				if (Form.bLinksToExisting)
				{
					FString DestinationReason;
					if (!ResolveEnd(Form.ExistingNodeId, Pin.Line, Pin.Column, TEXT("destination"),
						NewLink.ToNodeKey, NewLink.ToFormIndex, ToNode, DestinationPins, DestinationReason))
					{
						OutPlan.Refusals.Add(DestinationReason);
						continue;
					}
				}
				else
				{
					NewLink.ToFormIndex = &Form - OutPlan.Nodes.GetData();
					DestinationPins = Form.ProbePinNames;
				}
				if (DestinationPins.Num() > 0 && !DestinationPins.Contains(Pin.Name))
				{
					OutPlan.Refusals.Add(FString::Printf(
						TEXT("line %d, column %d: the link's destination pin '%s' does not exist. It has: %s."),
						Pin.Line, Pin.Column, *Pin.Name, *FString::Join(DestinationPins, TEXT(", "))));
					continue;
				}

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

	// Placement is opt-in so the shared callers keep their previous behavior. It runs only on the actual
	// graph, after the probes are gone, and refuses here rather than at apply time.
	if (bRequestPlacement)
	{
		OutPlan.bPlacementRequested = true;
		if (OutPlan.Refusals.Num() == 0 && !OutPlan.bNothingToDo)
		{
			FString PlacementRefusal;
			if (!PlanPlacement(Graph, OutPlan.Nodes, OutPlan.PlannedPositions,
				OutPlan.bPlacementUsedEstimatedSize, PlacementRefusal))
			{
				OutPlan.Refusals.Add(PlacementRefusal);
			}
		}
	}

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

	if (InPlan.bPlacementRequested && InPlan.PlannedPositions.Num() > 0)
	{
		Out += TEXT("\nplacement:");
		for (const FPlannedPosition& Position : InPlan.PlannedPositions)
		{
			if (!InPlan.Nodes.IsValidIndex(Position.FormIndex))
			{
				continue;
			}
			const FNewNode& Form = InPlan.Nodes[Position.FormIndex];
			Out += FString::Printf(TEXT("\n  line %d: %s at (%d,%d)"),
				Form.Line, *Form.TypeId, Position.X, Position.Y);
		}
		Out += TEXT("\n; existing bounds keep each stored positive dimension and estimate only a missing ");
		Out += TEXT("one; new nodes use an estimated 320x160 footprint; not a measured Slate footprint.");
	}

	Out += TEXT("\n; the preview changes NOTHING. Call the edit to write it.");
	return Out;
}

bool FTurboMCPGraphAppend::Apply(const FPlan& InPlan, FString& OutReport, FString& OutError)
{
	OutReport.Empty();
	OutError.Empty();
    if (!IsInGameThread() || !GEditor || GEditor->IsTransactionActive() || GEditor->IsPlaySessionInProgress())
    {
        OutError = TEXT("unchanged: graph write requires an idle editor outside an active transaction");
        return false;
    }


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

	// A REQUESTED PLACEMENT IS CHECKED BEFORE THE TRANSACTION. A plan with missing, duplicated or invalid
	// positions must never fall back to the origin or create anything.
	if (InPlan.bPlacementRequested && !ValidatePlannedPositions(InPlan, OutError))
	{
		OutError = FString::Printf(TEXT("%s NOTHING was written."), *OutError);
		return false;
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
	TArray<const FPlannedPosition*> PositionByForm;
	if (InPlan.bPlacementRequested)
	{
		PositionByForm.SetNumZeroed(InPlan.Nodes.Num());
		for (const FPlannedPosition& Position : InPlan.PlannedPositions)
		{
			if (PositionByForm.IsValidIndex(Position.FormIndex))
			{
				PositionByForm[Position.FormIndex] = &Position;
			}
		}
	}

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
			if (!FTurboMCPRollback::EndAndUndoActiveTransaction())
                {
                    OutError = TEXT("partial: graph write failed and rollback failed; inspect the graph before retrying; save=not_requested");
                    return false;
                }
			OutError = FString::Printf(
				TEXT("line %d: the plan accepted '%s' but the graph refused it at apply time: %s. ")
				TEXT("NOTHING was written."),
				Form.Line, *Form.TypeId, *SpawnNote);
			return false;
		}

		if (InPlan.bPlacementRequested)
		{
			const FPlannedPosition* Position = PositionByForm.IsValidIndex(FormIndex)
				? PositionByForm[FormIndex] : nullptr;
			if (Position == nullptr)
			{
				if (!FTurboMCPRollback::EndAndUndoActiveTransaction())
                {
                    OutError = TEXT("partial: graph write failed and rollback failed; inspect the graph before retrying; save=not_requested");
                    return false;
                }
				OutError = FString::Printf(
					TEXT("placement: form %d has no planned position at apply time. NOTHING was written."),
					FormIndex);
				return false;
			}
			Made->NodePosX = Position->X;
			Made->NodePosY = Position->Y;
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
				if (!FTurboMCPRollback::EndAndUndoActiveTransaction())
                {
                    OutError = TEXT("partial: graph write failed and rollback failed; inspect the graph before retrying; save=not_requested");
                    return false;
                }
				OutError = FString::Printf(
					TEXT("line %d: '%s' has no pin named '%s' at apply time. NOTHING was written."),
					Pin.Line, *Form.TypeId, *Pin.Name);
				return false;
			}

			// THE PIN'S OWN TYPE DECIDES HOW THE LITERAL IS READ. Writing "42" into a string pin as a number
			// would fail silently, which is why the schema's default-value path is used rather than a raw
			// string assignment.
			const FString Literal = Unquote(Pin.Value);
            bool Changed = false; FString PinError;
            if (!SetLiteral(Graph, Target, Literal, Changed, PinError))
            {
                const bool Undone = FTurboMCPRollback::EndAndUndoActiveTransaction();
                OutError = FString::Printf(TEXT("%s: invalid pin default '%s': %s"), Undone ? TEXT("unchanged; rollback=complete") : TEXT("partial; rollback=failed"), *Pin.Name, *PinError);
                return false;
            }
            if (Changed) ++PinsWritten;
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

		UEdGraphNode* ToNode = nullptr;
		if (Link.ToFormIndex != INDEX_NONE)
		{
			ToNode = CreatedByForm.IsValidIndex(Link.ToFormIndex) ? CreatedByForm[Link.ToFormIndex] : nullptr;
		}
		else
		{
			for (UEdGraphNode* Node : Graph->Nodes)
			{
				if (Node != nullptr && TurboMCPNodeId::MakeForNode(Node, /*bLongForm=*/true) == Link.ToNodeKey)
				{
					ToNode = Node;
					break;
				}
			}
		}

		if (FromNode == nullptr || ToNode == nullptr)
		{
			if (!FTurboMCPRollback::EndAndUndoActiveTransaction())
                {
                    OutError = TEXT("partial: graph write failed and rollback failed; inspect the graph before retrying; save=not_requested");
                    return false;
                }
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
			if (!FTurboMCPRollback::EndAndUndoActiveTransaction())
                {
                    OutError = TEXT("partial: graph write failed and rollback failed; inspect the graph before retrying; save=not_requested");
                    return false;
                }
			OutError = FString::Printf(
				TEXT("line %d: the plan accepted a link '%s' -> '%s', but a pin was missing at apply time ")
				TEXT("(%s). NOTHING was written."),
				Link.Line, *Link.FromPin, *Link.ToPin,
				FromPin == nullptr ? TEXT("source") : TEXT("target"));
			return false;
		}

		// THE ENGINE'S OWN CONNECTION PATH decides whether the two pins may join. An exec output can already
		// feed a node: K2 then REPLACES that wire when it accepts this one. That is an INSERTION, not merely
		// an append, so the displaced continuation must be carried through the new node or restored on failure.
		const UEdGraphSchema* Schema = Graph->GetSchema();
		const TArray<UEdGraphPin*> OriginalTargets = FromPin->LinkedTo;
		const bool bSchemaMadeLink = Schema != nullptr && Schema->TryCreateConnection(FromPin, ToPin);
		if (!bSchemaMadeLink || !HasLink(FromPin, ToPin))
		{
			// A schema is allowed to reject a replacement after it has changed its pin state. Restore the exact
			// pre-link chain before ending the transaction, then verify the restoration rather than trusting it.
			bool bRestored = Schema != nullptr;
			if (Schema != nullptr && HasLink(FromPin, ToPin))
			{
				Schema->BreakSinglePinLink(FromPin, ToPin);
			}
			if (Schema != nullptr)
			{
				for (UEdGraphPin* OriginalTarget : OriginalTargets)
				{
					if (OriginalTarget != nullptr && !HasLink(FromPin, OriginalTarget))
					{
						bRestored = Schema->TryCreateConnection(FromPin, OriginalTarget)
							&& HasLink(FromPin, OriginalTarget) && bRestored;
					}
				}
			}
			if (!FTurboMCPRollback::EndAndUndoActiveTransaction())
                {
                    OutError = TEXT("partial: graph write failed and rollback failed; inspect the graph before retrying; save=not_requested");
                    return false;
                }
			OutError = FString::Printf(
				TEXT("line %d: the engine refused the link '%s' -> '%s', or did not retain it. ")
				TEXT("The original chain was %s before rollback; NOTHING was written."),
				Link.Line, *Link.FromPin, *Link.ToPin, bRestored ? TEXT("restored") : TEXT("NOT fully restored"));
			return false;
		}

		TArray<UEdGraphPin*> ReplacedTargets;
		for (UEdGraphPin* OriginalTarget : OriginalTargets)
		{
			if (OriginalTarget != nullptr && !HasLink(FromPin, OriginalTarget))
			{
				ReplacedTargets.Add(OriginalTarget);
			}
		}

		// A tail has no displaced target and is the ordinary append case. A non-empty replacement is safe only
		// for exec wires: value pins have no unambiguous continuation output, so refusing is safer than cutting
		// an existing data path.
		TArray<TPair<UEdGraphPin*, UEdGraphPin*>> Continuations;
		auto RestoreOriginalChain = [&]() -> bool
		{
			bool bRestored = true;
			if (HasLink(FromPin, ToPin))
			{
				Schema->BreakSinglePinLink(FromPin, ToPin);
			}
			for (const TPair<UEdGraphPin*, UEdGraphPin*>& Continuation : Continuations)
			{
				if (HasLink(Continuation.Key, Continuation.Value))
				{
					Schema->BreakSinglePinLink(Continuation.Key, Continuation.Value);
				}
			}
			for (UEdGraphPin* OriginalTarget : ReplacedTargets)
			{
				bRestored = OriginalTarget != nullptr
					&& Schema->TryCreateConnection(FromPin, OriginalTarget)
					&& HasLink(FromPin, OriginalTarget)
					&& bRestored;
			}
			return bRestored;
		};

		if (ReplacedTargets.Num() > 0)
		{
			UEdGraphPin* Continuation = FromPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec
				&& ToPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec
				? FindExecContinuation(ToNode) : nullptr;
			bool bContinued = Continuation != nullptr;
			for (UEdGraphPin* OriginalTarget : ReplacedTargets)
			{
				if (OriginalTarget == nullptr || !Schema->TryCreateConnection(Continuation, OriginalTarget)
					|| !HasLink(Continuation, OriginalTarget))
				{
					bContinued = false;
					break;
				}
				Continuations.Add(TPair<UEdGraphPin*, UEdGraphPin*>(Continuation, OriginalTarget));
			}
			if (!bContinued)
			{
				const bool bRestored = RestoreOriginalChain();
				if (!FTurboMCPRollback::EndAndUndoActiveTransaction())
                {
                    OutError = TEXT("partial: graph write failed and rollback failed; inspect the graph before retrying; save=not_requested");
                    return false;
                }
				OutError = FString::Printf(
					TEXT("line %d: the new link replaced an existing chain but its continuation could not be made. ")
					TEXT("The original chain was %s before rollback; NOTHING was written."),
					Link.Line, bRestored ? TEXT("restored") : TEXT("NOT fully restored"));
				return false;
			}
		}
		++LinksMade;
	}

	// ---- COMPILE, THEN READ BACK AND COMPARE, which the spec demands rather than a claim of success --------
	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

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

	// Placement evidence is appended only when the caller asked for it, so the legacy report is unchanged.
	if (InPlan.bPlacementRequested)
	{
		Out += TEXT("\nplacement:");
		for (int32 FormIndex = 0; FormIndex < InPlan.Nodes.Num(); ++FormIndex)
		{
			if (InPlan.Nodes[FormIndex].bLinksToExisting
				|| !CreatedByForm.IsValidIndex(FormIndex) || CreatedByForm[FormIndex] == nullptr)
			{
				continue;
			}
			const FPlannedPosition* Position = PositionByForm.IsValidIndex(FormIndex)
				? PositionByForm[FormIndex] : nullptr;
			if (Position == nullptr)
			{
				continue;
			}
			Out += FString::Printf(TEXT("\n  node %s at (%d,%d)"),
				*TurboMCPNodeId::MakeForNode(CreatedByForm[FormIndex]), Position->X, Position->Y);
		}
		Out += TEXT("\n; existing bounds keep each stored positive dimension and estimate only a missing ");
		Out += TEXT("one; created nodes use an estimated 320x160 footprint; this is not a measured Slate ");
		Out += TEXT("footprint.");
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
	Out += TEXT("\ncompile=not_run; save=not_requested; run EditTurboMCPCompile explicitly after editing");
    if (Unmatched.Num() > 0)
    {
        const bool Undone = FTurboMCPRollback::EndAndUndoActiveTransaction();
        OutError = Undone ? TEXT("unchanged: node-set verification failed; rollback=complete")
                          : TEXT("partial: node-set verification failed; rollback=failed");
        OutReport = TEXT("save=not_requested; attempted read-back is not final state");
        return false;
    }
	OutReport = Out;
	return true;
}
