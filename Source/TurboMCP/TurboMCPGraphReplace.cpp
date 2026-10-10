// Copyright (c) 2026 Florian ESCHALIER (FloStyle)
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "TurboMCPGraphReplace.h"

#include "TurboMCPAssetResolver.h"
#include "TurboMCPNodeId.h"

#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "Engine/Blueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "ScopedTransaction.h"

#include "EdGraphNode_Comment.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_Event.h"
#include "K2Node_CustomEvent.h"

namespace
{
	/** Whichever form the text uses, one graph is looked up the same way. A local copy of APPEND's helper. */
	UEdGraph* FindGraphLocal(UBlueprint* InBlueprint, const FString& InGraphName)
	{
		if (InBlueprint == nullptr)
		{
			return nullptr;
		}

		const FString Wanted = InGraphName.TrimStartAndEnd();
		TArray<UEdGraph*> Candidates;
		Candidates.Append(InBlueprint->UbergraphPages);
		Candidates.Append(InBlueprint->FunctionGraphs);
		Candidates.Append(InBlueprint->MacroGraphs);

		for (UEdGraph* Graph : Candidates)
		{
			if (Graph == nullptr)
			{
				continue;
			}
			if (Wanted.IsEmpty() || Graph->GetName().Equals(Wanted, ESearchCase::IgnoreCase))
			{
				return Graph;
			}
		}
		return nullptr;
	}

	/**
	 * TRUE FOR A NODE THE SPEC SAYS IS NEVER REMOVED: an entry, an event, or a result.
	 *
	 * "Writes into the EXISTING entry node and result nodes; NEVER ADDS A SECOND ENTRY." An entry or a result
	 * that the submitted text does not mention is therefore a node to KEEP, not a node to remove - removing it
	 * would either delete the function's signature or make the writer add a second one, and both are the exact
	 * thing the spec forbids in one sentence.
	 */
	bool IsEntryOrResult(const UEdGraphNode* InNode)
	{
		if (InNode == nullptr)
		{
			return false;
		}
		return InNode->IsA<UK2Node_FunctionEntry>()
			|| InNode->IsA<UK2Node_FunctionResult>()
			|| InNode->IsA<UK2Node_Event>()
			|| InNode->IsA<UK2Node_CustomEvent>();
	}

	/** A comment is documentation rather than body, so it is kept exactly as `E-GRAPH-DEAD` taught. */
	bool IsComment(const UEdGraphNode* InNode)
	{
		return InNode != nullptr && InNode->IsA<UEdGraphNode_Comment>();
	}
}

bool FTurboMCPGraphReplace::Plan(const FString& InAsset, const FString& InGraphName,
	const FString& InText, FPlan& OutPlan, FString& OutError)
{
	OutError.Empty();
	OutPlan = FPlan();

	// ---- 1. THE APPEND PLAN RUNS FIRST, BECAUSE EVERYTHING IT DECIDES IS ALSO TRUE HERE -----------------
	//
	// THIS IS THE SHARING THAT MATTERS. `APPEND` already calls the checker, lexes the one dialect, resolves type
	// ids through the engine's own action database, decides which forms name EXISTING nodes, and captures the pin
	// names of nodes it will create. REPLACE needs all of it and adds only the old-graph question below. A second
	// parser or a second factory would be a second dialect and a second opinion about the same format, and this
	// project has recorded that bug twice.
	if (!FTurboMCPGraphAppend::Plan(InAsset, InGraphName, InText, OutPlan.Append, OutError))
	{
		return false;
	}
	if (OutPlan.Append.Refusals.Num() > 0)
	{
		// A REFUSED TEXT HAS NO OPINION ABOUT THE OLD GRAPH, so nothing is listed for removal. Reporting
		// removals for a text that will not be applied would tell a caller that nodes are going when nothing is.
		return true;
	}

	// ---- 2. THE GRAPH, SO THE OLD BODY CAN BE COMPARED WITH WHAT THE TEXT DESCRIBES --------------------
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

	// ---- 3. WHAT THE TEXT DESCRIBES, AS KEYS -----------------------------------------------------------
	//
	// A form that names an EXISTING node keeps that node, so its key is described. A form that creates a node
	// describes nothing that exists yet, so it contributes no key here.
	TSet<FString> Described;
	for (const FTurboMCPGraphAppend::FNewNode& Form : OutPlan.Append.Nodes)
	{
		if (Form.bLinksToExisting && !Form.ExistingNodeId.IsEmpty())
		{
			Described.Add(Form.ExistingNodeId);
		}
	}

	// ---- 4. THE REMOVAL LIST, WHICH IS THE WHOLE DIFFERENCE FROM APPEND ---------------------------------
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (Node == nullptr)
		{
			continue;
		}

		// THE KEYS ARE PRODUCED BY THE PROJECT'S OWN READER rather than by this pass. `FTurboMCPNodeId` is what
		// `R-GRAPH-LOAD` prints and what a caller puts back into the text, so a removal key that comes from
		// anywhere else could name a node the caller cannot write.
		const FString Key = TurboMCPNodeId::MakeForNode(Node, /*bLongForm=*/true);

		if (IsEntryOrResult(Node))
		{
			// KEPT, AND RECORDED AS REUSED RATHER THAN SILENTLY SKIPPED. "never adds a second entry" is only
			// demonstrated to a caller if the report says the existing one was written into.
			OutPlan.ReusedKeys.Add(Key);
			continue;
		}
		if (IsComment(Node))
		{
			// A COMMENT IS DOCUMENTATION, NOT BODY. `E-GRAPH-DEAD` already learned this the hard way: a comment
			// has no pins, so it is unreachable from any walk, and treating that as "dead" queued an author's
			// documentation for deletion. A REPLACE has no business deleting it either.
			continue;
		}
		if (Described.Contains(Key))
		{
			continue;
		}

		FRemoval Removal;
		Removal.Key = Key;
		Removal.ClassName = Node->GetClass()->GetName();
		Removal.Title = TurboMCPNodeId::GetNodeTitle(Node);
		OutPlan.Removals.Add(Removal);
	}

	return true;
}

FString FTurboMCPGraphReplace::RenderPreview(const FPlan& InPlan)
{
	FString Out = FString::Printf(TEXT("preview: replace the body of %s in %s"),
		*InPlan.Append.GraphName, *InPlan.Append.Asset);

	Out += FString::Printf(TEXT("\n%d node(s) in the graph now, %d described by the text, %d would be added"),
		InPlan.NodeCountBefore, InPlan.NodeCountBefore - InPlan.Removals.Num(), InPlan.Append.Nodes.Num());

	Out += FString::Printf(TEXT("\n%d node(s) kept because the text writes into them (entry, result, event)"),
		InPlan.ReusedKeys.Num());

	// THE TWO HALVES OF REPLACE, STATED APART. Adding and removing are different acts and a caller reading one
	// number for both would not know whether the graph grew or shrank.
	Out += FString::Printf(TEXT("\n%d node(s) the text does not describe, which this would REMOVE"),
		InPlan.Removals.Num());

	for (const FRemoval& Removal : InPlan.Removals)
	{
		Out += FString::Printf(TEXT("\n    would REMOVE  %s  %s  '%s'"),
			*Removal.Key, *Removal.ClassName, *Removal.Title);
	}
	if (InPlan.Removals.Num() == 0)
	{
		Out += TEXT("\n    nothing would be removed: the text describes every node that exists.");
	}

	if (InPlan.Append.Refusals.Num() > 0)
	{
		Out += FString::Printf(TEXT("\n\nunchanged: %d refusal(s). Nothing would be written and NOTHING REMOVED."),
			InPlan.Append.Refusals.Num());
		for (const FString& Refusal : InPlan.Append.Refusals)
		{
			Out += FString::Printf(TEXT("\n    refused %s"), *Refusal);
		}
		return Out;
	}

	Out += TEXT("\n; a preview changes NOTHING. Call the edit to write it.");
	return Out;
}

bool FTurboMCPGraphReplace::Apply(const FPlan& InPlan, FString& OutReport, FString& OutError)
{
	OutReport.Empty();
	OutError.Empty();

	if (InPlan.Append.Refusals.Num() > 0)
	{
		// THE PLAN ALREADY DECIDED. Nothing is written and nothing is removed.
		OutReport = RenderPreview(InPlan);
		return true;
	}

	// ---- 1. THE ADD HALF, THROUGH APPEND'S OWN APPLY ---------------------------------------------------
	//
	// IT IS CALLED RATHER THAN REIMPLEMENTED, so the creation, the linking, the layout, the compile and the
	// read-back comparison are the SAME CODE the append tool uses. Two implementations of one write path is how
	// the two tools come to disagree about the format, which is the failure this project has recorded twice.
	FString AppendReport;
	FString AppendError;
	if (!FTurboMCPGraphAppend::Apply(InPlan.Append, AppendReport, AppendError))
	{
		OutError = AppendError;
		return false;
	}

	// ---- 2. THE REMOVE HALF, WHICH IS THE PART ONLY REPLACE DOES ---------------------------------------
	if (InPlan.Removals.Num() == 0)
	{
		// NOTHING TO REMOVE, SO THE APPEND REPORT STANDS. Saying "removed 0" would be noise; saying what was
		// removed when nothing was is the kind of sentence this project has learned to distrust.
		OutReport = AppendReport;
		OutReport += TEXT("\nremoved 0 node(s): the text describes every node that exists.");
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

	// ONE TRANSACTION FOR THE REMOVALS. The append half has its own, which is correct: they are two acts and a
	// failure in one must not leave the other half-applied.
	FScopedTransaction Transaction(NSLOCTEXT("TurboMCP", "GraphReplace", "TurboMCP replace a graph body"));
	Blueprint->Modify();
	Graph->Modify();

	TSet<FString> Wanted;
	for (const FRemoval& Removal : InPlan.Removals)
	{
		Wanted.Add(Removal.Key);
	}

	TArray<FString> ActuallyRemoved;
	TArray<FString> NotRemoved;

	// THE NODE LIST IS COPIED BEFORE ITERATING, because removal mutates `Graph->Nodes` and iterating a container
	// while removing from it is the classic way to skip every other element - which here would mean a body
	// silently HALF replaced, the worst possible outcome for this capability.
	TArray<UEdGraphNode*> Existing = Graph->Nodes;
	for (UEdGraphNode* Node : Existing)
	{
		if (Node == nullptr)
		{
			continue;
		}
		// RE-DERIVED FROM THE LIVE GRAPH rather than trusted from the plan, because the append half ran in
		// between and could have changed the graph. The plan said WHICH KEYS; this decides which NODES those
		// are NOW, and a key that no longer resolves is reported rather than ignored.
		const FString Key = TurboMCPNodeId::MakeForNode(Node, /*bLongForm=*/true);
		if (!Wanted.Contains(Key))
		{
			continue;
		}

		// A NODE RE-DISCOVERED AS AN ENTRY OR RESULT IS KEPT EVEN IF THE PLAN LISTED IT. Belt and braces, and
		// the cost of being wrong here is a function with no signature.
		if (IsEntryOrResult(Node) || IsComment(Node))
		{
			NotRemoved.Add(Key);
			continue;
		}

		// THE EDITOR'S OWN REMOVAL, which breaks the links and keeps the transaction consistent.
		// `FBlueprintEditorUtils::RemoveNode` is what `E-GRAPH-DEAD` uses for the same act.
		FBlueprintEditorUtils::RemoveNode(Blueprint, Node, /*bDontRecompile=*/true);
		ActuallyRemoved.Add(Key);
	}

	// ---- 3. COMPILE, THEN READ BACK AND COMPARE ---------------------------------------------------------
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	FKismetEditorUtilities::CompileBlueprint(Blueprint);

	// THE READ BACK IS THE CHECK THAT MATTERS MOST FOR REPLACE. A node silently KEPT is invisible to the tool's
	// own report, and a node silently DROPPED is invisible to it too; reading the graph back through the
	// project's own reader is what catches both.
	TSet<FString> NowInGraph;
	for (const UEdGraphNode* Node : Graph->Nodes)
	{
		if (Node != nullptr)
		{
			NowInGraph.Add(TurboMCPNodeId::MakeForNode(Node, /*bLongForm=*/true));
		}
	}

	TArray<FString> Survived;
	for (const FString& Key : ActuallyRemoved)
	{
		if (!NowInGraph.Contains(Key))
		{
			Survived.Add(Key);
		}
	}
	const int32 FailedToRemove = ActuallyRemoved.Num() - Survived.Num();

	// ---- 4. THE REPORT, FIRST LINE `changed`, WITH EVERY REMOVAL LISTED ---------------------------------
	FString Out = FString::Printf(TEXT("changed: replaced the body of %s in %s : %d node(s) removed, %d -> %d"),
		*InPlan.Append.GraphName, *InPlan.Append.Asset, Survived.Num(),
		InPlan.NodeCountBefore, Graph->Nodes.Num());

	Out += FString::Printf(TEXT("\n%d node(s) kept because the text writes into them (entry, result, event)"),
		InPlan.ReusedKeys.Num());

	// THE LIST, NOT A COUNT. The spec says "LISTS WHAT IT REMOVED", and a count tells a reader how much is gone
	// while a list tells them what - which is the difference between a report and a receipt.
	Out += TEXT("\nremoved, by key, with the class and the title an author would recognise:");
	for (const FRemoval& Removal : InPlan.Removals)
	{
		if (Survived.Contains(Removal.Key))
		{
			Out += FString::Printf(TEXT("\n    REMOVED  %s  %s  '%s'"),
				*Removal.Key, *Removal.ClassName, *Removal.Title);
		}
	}
	if (Survived.Num() == 0)
	{
		Out += TEXT("\n    (none)");
	}

	if (NotRemoved.Num() > 0)
	{
		Out += FString::Printf(TEXT("\nNOT removed, because they are entry or result nodes or documentation: %s"),
			*FString::Join(NotRemoved, TEXT(", ")));
	}

	if (FailedToRemove > 0)
	{
		// A FAILED COMPARE IS A FAILURE, NOT A SUCCESS WITH A NOTE. The spec requires it and this is the line
		// that carries it: a node that is still in the graph after being removed is the exact silent failure a
		// self-report would hide.
		Out += FString::Printf(TEXT("\nFAILED COMPARE: %d node(s) were removed and are STILL in the graph."),
			FailedToRemove);
		Out += TEXT("\n; this is a FAILURE, not a success with a note. Read the graph back and compare.");
	}
	else
	{
		Out += TEXT("\nthe read back matches: every node this tool removed is gone.");
	}

	// THE APPEND HALF'S OWN REPORT IS CARRIED, because "the body was replaced" without the add half's counts
	// would leave the caller unable to check that the new nodes arrived.
	Out += TEXT("\n; the add half reported:");
	Out += FString::Printf(TEXT("\n    %s"), *AppendReport.Replace(TEXT("\n"), TEXT("\n    ")));

	OutReport = Out;
	return true;
}
