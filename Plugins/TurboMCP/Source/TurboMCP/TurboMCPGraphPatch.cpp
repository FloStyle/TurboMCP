// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "TurboMCPGraphPatch.h"
#include "TurboMCPGraphTextLex.h"

#include "TurboMCPRollback.h"
#include "TurboMCPAssetResolver.h"
#include "TurboMCPNodeId.h"

#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraph/EdGraphSchema.h"
#include "Engine/Blueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "ScopedTransaction.h"
#include "EdGraphSchema_K2.h"
#include "Editor.h"

namespace
{
    bool SetLiteral(UEdGraph* Graph, UEdGraphPin* Pin, const FString& Value, bool& Changed, FString& Error, bool bValidateOnly = false)
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
        if (bValidateOnly) { Changed = true; return true; }
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

	/** One graph is looked up the same way in all three writers. A local copy of APPEND's helper. */
	UEdGraph* FindGraphLocal(UBlueprint* InBlueprint, const FString& InGraphName)
	{
        int32 Matches = 0;
        UEdGraph* Graph = TurboMCPNodeId::FindGraphByName(InBlueprint, InGraphName.TrimStartAndEnd(), Matches);
        if (!Graph) return nullptr;
        return Matches == 1 ? Graph : nullptr;
    }

	/** The node in the graph whose long-form key is this, or null. */
	UEdGraphNode* NodeByKey(UEdGraph* InGraph, const FString& InKey)
	{
		if (InGraph == nullptr || InKey.IsEmpty())
		{
			return nullptr;
		}
		for (UEdGraphNode* Node : InGraph->Nodes)
		{
			if (Node != nullptr && TurboMCPNodeId::MakeForNode(Node, /*bLongForm=*/true) == InKey)
			{
				return Node;
			}
		}
		return nullptr;
	}

	/** The pin with this name, matched case-sensitively as the format prints it. */
	UEdGraphPin* PinByName(UEdGraphNode* InNode, const FString& InName)
	{
		if (InNode == nullptr)
		{
			return nullptr;
		}
		UEdGraphPin* Found = nullptr;
		for (UEdGraphPin* Candidate : InNode->Pins)
		{
			if (Candidate != nullptr && Candidate->Direction == EGPD_Input
				&& !Candidate->bOrphanedPin && Candidate->PinName.ToString().Equals(InName, ESearchCase::CaseSensitive))
			{
				if (Found != nullptr) { return nullptr; }
				Found = Candidate;
			}
		}
		return Found;
	}

	/** Append planning has already validated the shared graph-text escape layer. */
	FString Unquote(const FString& InValue)
	{
        FString Literal, Error;
        TurboMCPGraphTextLex::DecodeLiteral(InValue, Literal, Error);
        return Literal;
	}

}

bool FTurboMCPGraphPatch::Plan(const FString& InAsset, const FString& InGraphName,
	const FString& InText, FPlan& OutPlan, FString& OutError)
{
	OutError.Empty();
	OutPlan = FPlan();

	// ---- 1. THE APPEND PLAN RUNS FIRST, AND IT DECIDES MOST OF WHAT PATCH NEEDS -------------------------
	//
	// THIS IS THE SHARING THAT MATTERS. `APPEND` already calls `P-GRAPH-CHECK`, lexes the ONE dialect through
	// `TurboMCPGraphTextLex`, resolves type ids through the engine's own action database, decides which forms
	// name EXISTING nodes, plans the links, and words the refusals. PATCH needs all of it and adds only the
	// question of what DIFFERS - which is exactly the division REPLACE demonstrated.
	//
	// A SECOND PARSER WOULD BE A SECOND DIALECT, and this project has recorded that bug twice.
	if (!FTurboMCPGraphAppend::Plan(InAsset, InGraphName, InText, OutPlan.Append, OutError))
	{
		return false;
	}
	if (OutPlan.Append.Refusals.Num() > 0)
	{
		// A REFUSED TEXT HAS NO OPINION ABOUT THE GRAPH, so nothing is described as changing.
		return true;
	}

	// ---- 2. THE GRAPH, SO THE TEXT CAN BE MATCHED BY NODE ID --------------------------------------------
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
	UEdGraph* Graph = FindGraphLocal(Blueprint, OutPlan.Append.GraphName);
	if (Graph == nullptr)
	{
		OutError = FString::Printf(TEXT("no graph named '%s' in '%s'"),
			*OutPlan.Append.GraphName, *Resolved.PackagePath);
		return false;
	}
	OutPlan.NodeCountBefore = Graph->Nodes.Num();

	// ---- 3. EACH FORM MUST NAME AN EXISTING NODE, BECAUSE PATCH CHANGES AND DOES NOT ADD ----------------
	//
	// THE REFUSAL NAMES THE OTHER TOOL, which is the difference between a refusal a caller can act on and one
	// that only says no. The spec gives adding to `APPEND` and removing to `REPLACE`; a form with no id is a
	// request to ADD, so it is refused here and told where to go.
	for (const FTurboMCPGraphAppend::FNewNode& Form : OutPlan.Append.Nodes)
	{
		if (Form.ExistingNodeId.IsEmpty())
		{
			OutPlan.Refusals.Add(FString::Printf(
				TEXT("line %d, column %d: the form '%s' names no node, and PATCH CHANGES EXISTING NODES rather "
					"than adding them. Use P/E-GRAPH-APPEND to add a block, or P/E-GRAPH-REPLACE to rewrite a "
					"body."),
				Form.Line, Form.Column, *Form.TypeId));
			continue;
		}

		// THE PLAN ALREADY REFUSED A FORM NAMING AN ID THE GRAPH DOES NOT HAVE, so reaching here means the id
		// resolved. It is re-looked-up rather than trusted because the node pointer is needed below.
		UEdGraphNode* Node = NodeByKey(Graph, Form.ExistingNodeId);
		if (Node == nullptr)
		{
			OutPlan.Refusals.Add(FString::Printf(
				TEXT("line %d, column %d: the form names the node '%s', which is not in '%s' any more."),
				Form.Line, Form.Column, *Form.ExistingNodeId, *OutPlan.Append.GraphName));
			continue;
		}

		FUpdate Update;
		Update.Key = Form.ExistingNodeId;
		Update.ClassName = Node->GetClass()->GetName();
		Update.TypeId = Form.TypeId;
		Update.Line = Form.Line;

		// EACH LITERAL PIN IS CHECKED AGAINST THE PINS THE NODE REALLY HAS, at plan time, so a Preview and an
		// Edit refuse the same input. A LINK is refused because this apply path does not execute link plans;
		// writing `(-> A:B)` into a pin as text is the silent corruption `APPEND` already guards against.
		for (const TurboMCPGraphTextLex::FPinForm& Pin : Form.Pins)
		{
			if (TurboMCPGraphTextLex::ValueIsALink(Pin.Value))
			{
				OutPlan.Refusals.Add(FString::Printf(TEXT("line %d: Patch currently edits literal defaults only; requested link on '%s' would not be applied. No part of this patch will be written. Use an explicit connection operation and read back its endpoints/types."), Pin.Line, *Pin.Name));
				continue;
			}

			UEdGraphPin* Target = PinByName(Node, Pin.Name);
			if (Target == nullptr)
			{
				// THE REFUSAL LISTS THE REAL NAMES, which the spec requires for a pin that does not exist.
				TArray<FString> Real;
				for (const UEdGraphPin* Candidate : Node->Pins)
				{
					if (Candidate != nullptr)
					{
						Real.Add(Candidate->PinName.ToString());
					}
				}
				OutPlan.Refusals.Add(FString::Printf(
					TEXT("line %d, column %d: '%s' has no unique non-orphaned input pin named '%s'. It has: %s."),
					Pin.Line, Pin.Column, *Form.TypeId, *Pin.Name,
					Real.Num() > 0 ? *FString::Join(Real, TEXT(", ")) : TEXT("(no pins)")));
				continue;
			}

            bool bDifferent = false;
            FString ValidationError;
            if (!SetLiteral(Graph, Target, Unquote(Pin.Value), bDifferent, ValidationError, true))
            {
                OutPlan.Refusals.Add(FString::Printf(TEXT("line %d: pin '%s': %s"), Pin.Line, *Pin.Name, *ValidationError));
                continue;
            }
            Update.bWouldChange |= bDifferent;
			Update.Pins.Add(Pin);
		}

		OutPlan.Updates.Add(MoveTemp(Update));
	}

	// A FORM WITH NO PIN VALUES CHANGES NOTHING, and saying so is the honest answer rather than reporting a
	// change of zero. It is a REFUSAL because the caller asked for a patch and supplied no difference.
	for (int32 Index = OutPlan.Updates.Num() - 1; Index >= 0; --Index)
	{
		if (OutPlan.Updates[Index].Pins.Num() == 0)
		{
			OutPlan.Refusals.Add(FString::Printf(
				TEXT("line %d: the form names '%s' but writes no pin value, so there is nothing to change. A "
					"form with an id and no pins describes nothing different from the graph."),
				OutPlan.Updates[Index].Line, *OutPlan.Updates[Index].Key));
			OutPlan.Updates.RemoveAt(Index);
		}
	}

	if (OutPlan.Updates.Num() == 0 && OutPlan.Refusals.Num() == 0)
	{
		OutPlan.Refusals.Add(TEXT("the text describes no existing node, so there is nothing to patch. Use "
			"P/E-GRAPH-APPEND to add a block."));
	}

	return true;
}

FString FTurboMCPGraphPatch::RenderPreview(const FPlan& InPlan)
{
	FString Out = FString::Printf(TEXT("preview: patch %s in %s"),
		*InPlan.Append.GraphName, *InPlan.Append.Asset);

	if (InPlan.Append.Refusals.Num() > 0)
	{
		Out += TEXT("\ngraph node count not collected: planning refused before patch inspection");
	}
	else
	{
		Out += FString::Printf(TEXT("\n%d node(s) in the graph now, %d named by the text"),
			InPlan.NodeCountBefore, InPlan.Updates.Num());
	}

	// THE COUNT THAT MATTERS FOR `T-WRITE-5`, STATED IN THE PREVIEW SO A CALLER CAN SEE IT BEFORE WRITING:
	// a patch that would touch more nodes than the caller intended is visible here rather than after the fact.
    int32 ChangedNodes = 0;
    if (InPlan.AllRefusals().IsEmpty())
        for (const FUpdate& Update : InPlan.Updates) ChangedNodes += Update.bWouldChange ? 1 : 0;
    Out += FString::Printf(TEXT("\n%d node(s) would change"), ChangedNodes);

	for (const FUpdate& Update : InPlan.Updates)
	{
		if (!Update.bWouldChange || !InPlan.AllRefusals().IsEmpty()) continue;
		Out += FString::Printf(TEXT("\n    would CHANGE  %s  %s  %d submitted pin value(s)"),
			*Update.Key, *Update.ClassName, Update.Pins.Num());
	}
	if (ChangedNodes == 0)
	{
		Out += TEXT("\n    no node would change.");
	}

	// *** BOTH REFUSAL LISTS ARE PRINTED, AND DROPPING THE INHERITED ONE WAS A REAL DEFECT. ***
	//
	// MEASURED, and it is the failure this comment exists to prevent: a text writing `NoSuchPin "x"` is refused
	// by `APPEND`'s plan - IT DOES VALIDATE THE PINS OF A FORM THAT NAMES AN EXISTING NODE - and `Plan` returns
	// EARLY, before this class has looked at the graph at all. PRINTING ONLY `InPlan.Refusals` HERE SHOWS THE
	// CALLER A PREVIEW WITH NO REFUSAL AND NO COUNT, because that list is EMPTY at this point:
	// `0 node(s) in the graph now, 0 named by the text`. A refusal that is never printed is a preview that says
	// nothing is wrong, which is strictly worse than no preview.
	//
	// SO THE REFUSALS ARE THE UNION OF BOTH LISTS. They use the SAME WORDING because both come from
	// `TurboMCPGraphTextLex` and `APPEND`'s pin checks, so a duplicate is dropped rather than shown twice - a
	// repeated refusal reads as two separate faults.
	TArray<FString> AllRefusals = InPlan.AllRefusals();

	if (AllRefusals.Num() > 0)
	{
		Out += FString::Printf(TEXT("\n\nunchanged: %d refusal(s). Nothing would be written."),
			AllRefusals.Num());
		for (const FString& Refusal : AllRefusals)
		{
			Out += FString::Printf(TEXT("\n    refused %s"), *Refusal);
		}
		return Out;
	}

	Out += TEXT("\n; a preview changes NOTHING. Call the edit to write it.");
	return Out;
}

bool FTurboMCPGraphPatch::Apply(const FPlan& InPlan, FString& OutReport, FString& OutError)
{
	OutReport.Empty();
	OutError.Empty();
    if (!IsInGameThread() || !GEditor || GEditor->IsTransactionActive() || GEditor->IsPlaySessionInProgress())
    {
        OutError = TEXT("unchanged: graph write requires an idle editor outside an active transaction");
        return false;
    }


	// BOTH REFUSAL LISTS DECIDE, because `APPEND` refuses things this class never sees - it validates the pins of
	// a form naming an existing node, and when it refuses, `Plan` returns early with `Refusals` still empty.
	// MEASURED: a text writing `NoSuchPin "x"` reached here with zero refusals of this class's own.
	if (InPlan.AllRefusals().Num() > 0)
	{
		// THE PLAN ALREADY DECIDED. Re-deciding here is how a Preview and an Edit come to disagree, which is
		// the one thing the "put every check in Plan" rule exists to prevent.
		OutReport = RenderPreview(InPlan);
		return true;
	}

	FTurboMCPAssetResolver::FResolved Resolved;
	const FString ResolveError = FTurboMCPAssetResolver::Resolve(InPlan.Append.Asset, Resolved);
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
	UEdGraph* Graph = FindGraphLocal(Blueprint, InPlan.Append.GraphName);
	if (Graph == nullptr)
	{
		OutError = FString::Printf(TEXT("no graph named '%s' in '%s'"),
			*InPlan.Append.GraphName, *Resolved.PackagePath);
		return false;
	}

	// ---- 1. EVERY NODE'S ID AND ITS DESCRIBED PIN VALUES, BEFORE THE WRITE ------------------------------
	//
	// TAKEN BEFORE ANYTHING IS TOUCHED, because `T-WRITE-5` is a claim about what CHANGED and the only way to
	// measure a change is to have the state it is a change FROM. A tool that reports its own change set proves
	// nothing; the sets below are what the report is computed from.
	TSet<FString> KeysBefore;
	for (const UEdGraphNode* Node : Graph->Nodes)
	{
		if (Node != nullptr)
		{
			KeysBefore.Add(TurboMCPNodeId::MakeForNode(Node, /*bLongForm=*/true));
		}
	}

    // Validate every requested default before opening a transaction. A repeated
    // patch must not dirty a clean package or add an empty undo entry.
    bool bNeedsWrite = false;
    for (const FUpdate& Update : InPlan.Updates)
    {
        UEdGraphNode* Node = NodeByKey(Graph, Update.Key);
        if (!Node)
        {
            OutError = TEXT("patch target is missing; no write attempted");
            return false;
        }
        for (const TurboMCPGraphTextLex::FPinForm& Pin : Update.Pins)
        {
            bool bDifferent = false;
            FString ValidationError;
            if (!SetLiteral(Graph, PinByName(Node, Pin.Name), Unquote(Pin.Value), bDifferent, ValidationError, true))
            {
                OutError = FString::Printf(TEXT("line %d: pin '%s': %s; no write attempted"), Pin.Line, *Pin.Name, *ValidationError);
                return false;
            }
            bNeedsWrite |= bDifferent;
        }
    }
    if (!bNeedsWrite)
    {
        OutReport = FString::Printf(TEXT("unchanged: all requested defaults already match in %s on %s; no transaction created; compile=not_run; save=not_requested"),
            *InPlan.Append.GraphName, *InPlan.Append.Asset);
        return true;
    }

	// ---- 2. ONE TRANSACTION ---------------------------------------------------------------------------
	FScopedTransaction Transaction(NSLOCTEXT("TurboMCP", "GraphPatch", "TurboMCP patch a graph"));
	Blueprint->Modify();
	Graph->Modify();

	TArray<FString> ActuallyChanged;
	TArray<FString> Missing;
	int32 PinsWritten = 0;

	for (const FUpdate& Update : InPlan.Updates)
	{
		// RE-DISCOVERED FROM THE LIVE GRAPH rather than trusted from the plan, because a plan is a decision
		// about an earlier state and this is the state that is being written.
		UEdGraphNode* Node = NodeByKey(Graph, Update.Key);
		if (Node == nullptr)
		{
			Missing.Add(Update.Key);
			continue;
		}

		bool bNodeChanged = false;
		for (const TurboMCPGraphTextLex::FPinForm& Pin : Update.Pins)
		{
			UEdGraphPin* Target = PinByName(Node, Pin.Name);
			if (Target == nullptr)
			{
				// THE PLAN CHECKED THIS AND IT HELD THEN. Reaching here means the graph moved underneath, so it
				// ROLLS BACK rather than writing a partial patch.
				if (!FTurboMCPRollback::EndAndUndoActiveTransaction())
                {
                    OutError = TEXT("partial: graph write failed and rollback failed; inspect the graph before retrying; save=not_requested");
                    return false;
                }
				OutError = FString::Printf(
					TEXT("line %d: the plan accepted pin '%s' on '%s', but the pin was gone at apply time. ")
					TEXT("NOTHING was written."),
					Pin.Line, *Pin.Name, *Update.Key);
				return false;
			}

			const FString Literal = Unquote(Pin.Value);

			// *** ONLY A PIN WHOSE VALUE DIFFERS IS WRITTEN, AND THAT IS THE WHOLE CAPABILITY. ***
			//
			// "applies only the differences" is not satisfied by setting every described pin to the value it
			// already holds: that would mark nodes modified, dirty the asset and - for a caller watching the
			// change - report a change that is not one. The comparison is what makes `T-WRITE-5`'s "changes
			// exactly two nodes" true rather than merely plausible.
			//
			// *** PROVEN BY BREAKING IT. *** MEASURED, with this gate forced to `false` and the binary rebuilt:
			// `RESULT: FAIL (2) -> A SECOND IDENTICAL PATCH SAYS unchanged, which is what makes it a DIFF, the
			// second patch changed ZERO nodes`, and EVERY OTHER CHECK STILL PASSED. That is the right shape for a
			// break test of this property: removing the diff does not corrupt the graph, it makes the tool do
			// work it should not, so only the checks that measure the SIZE of the change can see it. Restored,
			// the suite is `RESULT: PASS (31 check(s) run)`.
            bool Changed = false; FString PinError;
            if (!SetLiteral(Graph, Target, Literal, Changed, PinError))
            {
                const bool Undone = FTurboMCPRollback::EndAndUndoActiveTransaction();
                OutError = FString::Printf(TEXT("%s: invalid pin default '%s': %s"), Undone ? TEXT("unchanged; rollback=complete") : TEXT("partial; rollback=failed"), *Pin.Name, *PinError);
                return false;
            }
            if (Changed) { ++PinsWritten; bNodeChanged = true; }
		}

		if (bNodeChanged)
		{
			ActuallyChanged.Add(Update.Key);
		}
	}

	// Link requests are refused during planning; this path applies literal defaults only.
	if (Missing.Num() > 0)
	{
		if (!FTurboMCPRollback::EndAndUndoActiveTransaction())
                {
                    OutError = TEXT("partial: graph write failed and rollback failed; inspect the graph before retrying; save=not_requested");
                    return false;
                }
		OutError = FString::Printf(
			TEXT("the plan named %d node(s) that are no longer in the graph (%s). NOTHING was written."),
			Missing.Num(), *FString::Join(Missing, TEXT(", ")));
		return false;
	}

	// ---- 3. MARK DIRTY WITHOUT COMPILING, THEN READ BACK AND MEASURE THE CHANGE ---------------------------------------------
	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	// Compile is explicit and must run after the edit transaction ends.

	// THE READ BACK IS THE CHECK THAT MATTERS MOST, because `T-WRITE-5` is a claim about WHICH nodes changed and
	// a tool's own summary is not evidence for it. The ids are re-derived from the live graph.
	TSet<FString> KeysAfter;
	for (const UEdGraphNode* Node : Graph->Nodes)
	{
		if (Node != nullptr)
		{
			KeysAfter.Add(TurboMCPNodeId::MakeForNode(Node, /*bLongForm=*/true));
		}
	}

	// ADDED AND REMOVED ARE BOTH EMPTY FOR A CORRECT PATCH, and they are computed rather than asserted so that a
	// patch which quietly created or destroyed a node FAILS THE COMPARE instead of passing on a summary.
	TArray<FString> Added;
	for (const FString& Key : KeysAfter)
	{
		if (!KeysBefore.Contains(Key))
		{
			Added.Add(Key);
		}
	}
	TArray<FString> Removed;
	for (const FString& Key : KeysBefore)
	{
		if (!KeysAfter.Contains(Key))
		{
			Removed.Add(Key);
		}
	}

	// ---- 4. THE REPORT, FIRST LINE `changed` OR `unchanged` --------------------------------------------
	const bool bChanged = ActuallyChanged.Num() > 0;
	FString Out = FString::Printf(TEXT("%s: patched %s in %s : %d node(s) changed, %d pin value(s) written"),
		bChanged ? TEXT("changed") : TEXT("unchanged"),
		*InPlan.Append.GraphName, *InPlan.Append.Asset, ActuallyChanged.Num(), PinsWritten);

	// THE NODES THAT CHANGED, BY KEY. This is what `T-WRITE-5` is measured against, and a list rather than a
	// count because a count cannot show that the RIGHT two nodes moved.
	//
	// THE HEADER AGREES WITH THE FIRST LINE, WHICH IS NOT COSMETIC. A report whose first line says `unchanged`
	// and whose next line says `changed, by key:` TELLS A READER TWO OPPOSITE THINGS IN TWO LINES, and this
	// project has recorded that exact shape before - a valid change to an entity while a field's comment claimed
	// it held the old one. The verb and the list are derived from the SAME value so they cannot disagree.
	if (ActuallyChanged.Num() > 0)
	{
		Out += TEXT("\nchanged, by key:");
		for (const FString& Key : ActuallyChanged)
		{
			Out += FString::Printf(TEXT("\n    CHANGED  %s"), *Key);
		}
	}
	else
	{
		Out += TEXT("\nno node changed, so there is nothing to list: every pin the text describes already held the value it gives");
	}

	// THE COUNT THAT MATTERS FOR THE ACCEPTANCE ROW, STATED AS A NUMBER A SUITE CAN PARSE.
	Out += FString::Printf(TEXT("\n%d node(s) named by the text, %d of them changed, %d left alone"),
		InPlan.Updates.Num(), ActuallyChanged.Num(), InPlan.NodeCountBefore - ActuallyChanged.Num());

	// A CORRECT PATCH ADDS NOTHING AND REMOVES NOTHING. Reporting both computed sets rather than asserting
	// they are empty is what makes this a MEASUREMENT: a patch that created or destroyed a node fails here.
	if (Added.Num() > 0 || Removed.Num() > 0)
	{
		Out += FString::Printf(TEXT("\nFAILED COMPARE: the node set CHANGED. %d added, %d removed, and a patch "
			"may do NEITHER."), Added.Num(), Removed.Num());
		if (Added.Num() > 0)
		{
			Out += FString::Printf(TEXT("\n    ADDED    %s"), *FString::Join(Added, TEXT(", ")));
		}
		if (Removed.Num() > 0)
		{
			Out += FString::Printf(TEXT("\n    REMOVED  %s"), *FString::Join(Removed, TEXT(", ")));
		}
		Out += TEXT("\n; this is a FAILURE, not a success with a note. Read the graph back and compare.");
	}
	else
	{
		Out += TEXT("\nthe read back matches: every other node is still there, and no node was added.");
	}

	Out += TEXT("\ncompile=not_run; save=not_requested; run EditTurboMCPCompile explicitly after editing");
    if (Added.Num() > 0 || Removed.Num() > 0)
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
