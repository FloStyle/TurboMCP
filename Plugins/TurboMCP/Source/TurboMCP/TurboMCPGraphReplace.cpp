// Copyright (c) 2026 FloStyle
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
#include "TurboMCPRollback.h"
#include "ScopedTransaction.h"
#include "Editor.h"

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
        int32 Matches = 0;
        UEdGraph* Graph = TurboMCPNodeId::FindGraphByName(InBlueprint, InGraphName.TrimStartAndEnd(), Matches);
        return Matches == 1 ? Graph : nullptr;
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
			//
			// COUNTED RATHER THAN ONLY SKIPPED, because it is still a node the text does not describe and the
			// report has to be able to account for it.
			++OutPlan.CommentKeysKept;
			continue;
		}
		if (Described.Contains(Key))
		{
			// COUNTED, BECAUSE WITHOUT IT THE PREVIEW HAS NO WAY TO NAME THIS BUCKET. Every node the loop
			// walks falls into exactly one of four: described, re-used, documentation, or removed, and the
			// report prints them separately rather than reporting their sum as "described by the text".
			++OutPlan.DescribedKeysKept;
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

	// A REFUSED TEXT WAS NEVER COMPARED WITH THE GRAPH, SO THIS PREVIEW PRINTS NO GRAPH ARITHMETIC AT ALL.
	// `Plan` returns before it reads the graph, so the node count and all four buckets are still zero - numbers
	// no measurement produced. Printing them would state "0 node(s) in the graph now" about a graph nobody had
	// looked at, and "the text describes every node that exists" about a text that was REFUSED. `Plan` states
	// the rule this obeys where it refuses: "A REFUSED TEXT HAS NO OPINION ABOUT THE OLD GRAPH".
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

	// THE FOUR BUCKETS, EACH MEASURED SEPARATELY, BECAUSE THEIR SUM IS NOT ONE OF THEM. `NodeCountBefore -
	// Removals.Num()` is NOT "described by the text": it also holds the re-used entry and result nodes and the
	// comments. A re-used entry is written into rather than described, and a comment is kept because it is
	// documentation, so printing that sum as one bucket lets a reader who adds it to the reuse line below
	// count those nodes twice.
	Out += FString::Printf(TEXT("\n%d node(s) in the graph now: %d described by the text, %d kept as entry/result/event, %d kept as documentation, %d would be added"),
		InPlan.NodeCountBefore, InPlan.DescribedKeysKept, InPlan.ReusedKeys.Num(),
		InPlan.CommentKeysKept, InPlan.Append.Nodes.Num());

	// THE PARTITION IS CHECKED RATHER THAN ASSUMED. Every node the loop walked fell into exactly one of those
	// four buckets, so a sum that does not come back is a node these figures have LOST - the one failure a
	// report of counts cannot show by itself.
	const int32 AccountedFor = InPlan.DescribedKeysKept + InPlan.ReusedKeys.Num()
		+ InPlan.CommentKeysKept + InPlan.Removals.Num();
	if (AccountedFor != InPlan.NodeCountBefore)
	{
		Out += FString::Printf(TEXT("\n    *** %d of the %d node(s) are in none of the counts above, so they do not add up to the graph."),
			InPlan.NodeCountBefore - AccountedFor, InPlan.NodeCountBefore);
	}

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
		// A ZERO THAT CARRIES THE DENOMINATOR IT WAS MEASURED AGAINST. "The text describes every node that
		// exists" would be a conclusion drawn from an absence, and it is FALSE for any graph holding a comment:
		// the comment exists, the text does not describe it, and it is kept because it is documentation. The
		// count that was taken is what belongs here.
		Out += FString::Printf(TEXT("\n    no removal: 0 of the %d node(s) are outside the text."),
			InPlan.NodeCountBefore);
	}

	Out += TEXT("\n; a preview changes NOTHING. Call the edit to write it.");
	return Out;
}

bool FTurboMCPGraphReplace::Apply(const FPlan& InPlan, FString& OutReport, FString& OutError)
{
	OutReport.Empty();
	OutError.Empty();
    if (!IsInGameThread() || !GEditor || GEditor->IsTransactionActive() || GEditor->IsPlaySessionInProgress())
    {
        OutError = TEXT("unchanged: graph write requires an idle editor outside an active transaction");
        return false;
    }


	if (InPlan.Append.Refusals.Num() > 0)
	{
		// THE PLAN ALREADY DECIDED. Nothing is written and nothing is removed.
		OutReport = RenderPreview(InPlan);
		return true;
	}

	// ---- 1. THE ADD HALF, THROUGH APPEND'S OWN APPLY ---------------------------------------------------
	//
	// IT IS CALLED RATHER THAN REIMPLEMENTED, so the creation, the linking, the layout and the
	// read-back comparison are the SAME CODE the append tool uses. Two implementations of one write path is how
	// the two tools come to disagree about the format, which is the failure this project has recorded twice.
    // Validate destructive identities before the append phase can change anything.
    FTurboMCPAssetResolver::FResolved PreflightResolved;
    const FString PreflightError = FTurboMCPAssetResolver::Resolve(InPlan.Append.Asset, PreflightResolved);
    UBlueprint* PreflightBlueprint = PreflightError.IsEmpty() ? LoadObject<UBlueprint>(nullptr, *PreflightResolved.ObjectPath) : nullptr;
    UEdGraph* PreflightGraph = FindGraphLocal(PreflightBlueprint, InPlan.Append.GraphName);
    if (!PreflightGraph || PreflightGraph->Nodes.Num() != InPlan.NodeCountBefore)
    { OutError = TEXT("unchanged: replacement graph missing, ambiguous or changed since planning"); return false; }
    for (const FRemoval& Removal : InPlan.Removals)
    {
        int32 Matches = 0;
        for (UEdGraphNode* Node : PreflightGraph->Nodes)
            if (Node && TurboMCPNodeId::MakeForNode(Node, true) == Removal.Key && !IsEntryOrResult(Node) && !IsComment(Node)) ++Matches;
        if (Matches != 1)
        { OutError = TEXT("unchanged: replacement removal target is missing, ambiguous or protected"); return false; }
    }
    FScopedTransaction Transaction(NSLOCTEXT("TurboMCP", "GraphReplace", "TurboMCP replace a graph body"));
    UBlueprint* Blueprint = PreflightBlueprint;
    UEdGraph* Graph = PreflightGraph;
    Blueprint->Modify();
    Graph->Modify();
	FString AppendReport;
	FString AppendError;
	if (!FTurboMCPGraphAppend::Apply(InPlan.Append, AppendReport, AppendError, true))
	{
        // Append may already have undone the shared transaction. Never undo earlier history.
        if (GEditor->IsTransactionActive() && !FTurboMCPRollback::EndAndUndoActiveTransaction())
            OutError = TEXT("partial: replacement append failed and rollback failed; inspect graph before retrying. ") + AppendError;
        else
            OutError = AppendError;
        return false;
	}

	// ---- 2. THE REMOVE HALF, WHICH IS THE PART ONLY REPLACE DOES ---------------------------------------
	if (InPlan.Removals.Num() == 0)
	{
		// NOTHING TO REMOVE, SO THE APPEND REPORT STANDS. Saying what was removed when nothing was is the kind
		// of sentence this project has learned to distrust, so the count is printed with the denominator it was
		// measured against rather than with a claim about the text: "the text describes every node that exists"
		// is FALSE for any graph holding a comment, which is kept because it is documentation and not because
		// the text describes it. A caller of the only destructive writer in the plugin should not have to infer
		// that the zero is a measurement.
		OutReport = AppendReport;
		OutReport += FString::Printf(TEXT("\nremoved 0 node(s): 0 of the %d node(s) were outside the text."),
			InPlan.NodeCountBefore);
		return true;
	}

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

	// ---- 3. MARK DIRTY WITHOUT COMPILING, THEN READ BACK AND COMPARE ---------------------------------------------------------
	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	// Compile is explicit and must run after the edit transaction ends.

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
	const int32 FailedToRemove = Wanted.Num() - Survived.Num();

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

	Out += TEXT("\ncompile=not_run; save=not_requested; run EditTurboMCPCompile explicitly after editing");
    if (FailedToRemove > 0 || NotRemoved.Num() > 0)
    {
        const bool Undone = FTurboMCPRollback::EndAndUndoActiveTransaction();
        OutError = Undone ? TEXT("unchanged: replacement verification failed; append and removal rollback=complete")
                          : TEXT("partial: replacement verification failed; rollback=failed; inspect graph before retrying");
        OutReport = TEXT("save=not_requested; inspect the graph before retrying replacement");
        return false;
    }
    Out += TEXT("\nundo=one_transaction_for_append_and_removal");
	OutReport = Out;
	return true;
}
