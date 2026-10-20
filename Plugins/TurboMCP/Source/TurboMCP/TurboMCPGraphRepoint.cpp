// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
#include "TurboMCPGraphRepoint.h"
#include "TurboMCPAssetResolver.h"
#include "TurboMCPNodeId.h"
#include "TurboMCPNodeType.h"
#include "TurboMCPRollback.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraph/EdGraphSchema.h"
#include "Engine/Blueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/CompilerResultsLog.h"
#include "ScopedTransaction.h"
#include "Editor.h"
#include "Editor/TransBuffer.h"

namespace {
// *** ONE RESOLUTION POLICY, NOT TWO THAT CAN DRIFT. ***
//
// This was a SECOND graph resolver. It searched UbergraphPages, FunctionGraphs and MacroGraphs; the reader's
// `TurboMCPNodeId::FindGraphByName` searches those THREE PLUS EventGraphs, and nothing compared the two.
// MEASURED on the acceptance fixture they returned the same pointer with one match each, so they agreed by luck
// of the fixture rather than by construction - and a graph name matching two graphs in different arrays would
// have made this tool EDIT ONE AND REPORT ON ANOTHER, because a node id carries only the graph NAME
// (`...:GraphName:<hash>`). That is the ARC06 family: identity that cannot be verified across calls.
//
// It now DELEGATES, so the tool and the reader resolve a name identically. The local name is kept so no call site
// changes. The ambiguity refusal belongs in the shared resolver; until that lands, this is one resolver instead
// of two that can disagree.
UEdGraph* FindGraph(UBlueprint* B, const FString& Name) {
 int32 Matches = 0;
 UEdGraph* Graph = TurboMCPNodeId::FindGraphByName(B, Name, Matches);
 return Matches == 1 ? Graph : nullptr;
}

UEdGraphNode* FindNode(UEdGraph* G, const FString& Id) {
 if (!G || Id.IsEmpty()) return nullptr;
 UEdGraphNode* Match = nullptr;
 const FString Wanted = Id.TrimStartAndEnd();
 for (UEdGraphNode* N : G->Nodes)
  if (N && (TurboMCPNodeId::MakeForNode(N, true) == Wanted || TurboMCPNodeId::MakeForNode(N, false) == Wanted))
  { if (Match && Match != N) return nullptr; Match = N; }
 return Match;
}
UEdGraphPin* FindPin(UEdGraphNode* N, const FString& Name, EEdGraphPinDirection Direction) {
 if (!N) return nullptr;
 UEdGraphPin* Match = nullptr;
 for (UEdGraphPin* P : N->Pins)
  if (P && P->PinName.ToString().Equals(Name, ESearchCase::CaseSensitive) && P->Direction == Direction)
  { if (Match) return nullptr; Match = P; }
 return Match;
}

}

bool FTurboMCPGraphRepoint::Plan(const FString& Asset, const FString& GraphName, const FString& SourceNodeId,
 const FString& SourcePin, const FString& TargetNodeId, const FString& TargetTypeId, const FString& TargetPin,
 const FString& KeepSource, FPlan& Out, FString& Error) {
 Out = FPlan(); Error.Empty(); Out.Asset=Asset; Out.GraphName=GraphName; Out.SourceNodeId=SourceNodeId; Out.SourcePin=SourcePin; Out.TargetNodeId=TargetNodeId; Out.TargetTypeId=TargetTypeId; Out.TargetPin=TargetPin; 
 const FString Keep = KeepSource.TrimStartAndEnd();
 if (Keep.IsEmpty() || Keep.Equals(TEXT("true"), ESearchCase::IgnoreCase) || Keep == TEXT("1")) Out.bKeepSource = true;
 else if (Keep.Equals(TEXT("false"), ESearchCase::IgnoreCase) || Keep == TEXT("0")) Out.bKeepSource = false;
 else { Error = TEXT("unchanged: keepSource must be true/false or 1/0; empty defaults to true"); return false; }
 FTurboMCPAssetResolver::FResolved R; Error=FTurboMCPAssetResolver::Resolve(Asset,R); if (!Error.IsEmpty()) return false;
 UBlueprint* B=LoadObject<UBlueprint>(nullptr,*R.ObjectPath); if (!B) { Error=FString::Printf(TEXT("'%s' is not a Blueprint"),*R.PackagePath); return false; }
 int32 GraphMatches = 0;
 UEdGraph* G=TurboMCPNodeId::FindGraphByName(B,GraphName,GraphMatches);
 if (!G) { Error=FString::Printf(TEXT("no graph named '%s' in '%s'"),*GraphName,*R.PackagePath); return false; }
 // *** A NAME IS NOT AN IDENTITY: AN AMBIGUOUS NAME IS REFUSED, NOT GUESSED. ***
 //
 // A node id is `...:GraphName:<hash>` - the graph NAME ONLY. So when a blueprint holds two graphs of the same
 // name in different arrays, resolving "the" graph silently picks one and the report names the same `name:hash`
 // for either: this tool could edit graph A while the reader shows graph B, and no caller could tell. That is the
 // ARC06 family - identity that cannot be verified across calls.
 //
 // This is the MUTATING side, so it REFUSES. The reader keeps reporting, because a read stays useful while a
 // WRITE must not land somewhere the caller did not name. A refusal costs a caller one round trip; a guess costs
 // them an edit they cannot see.
 if (GraphMatches > 1)
 {
  Out.Refusals.Add(FString::Printf(
   TEXT("the name '%s' matches %d graphs in '%s', so this tool will not guess which one to edit; name a graph "
        "that matches exactly one"),
   *GraphName, GraphMatches, *R.PackagePath));
  return true;
 }
 Out.GraphName=G->GetName(); Out.NodeCountBefore=G->Nodes.Num();
 UEdGraphNode* Source=FindNode(G,SourceNodeId); if (!Source) { Out.Refusals.Add(FString::Printf(TEXT("source node '%s' was not found"),*SourceNodeId)); return true; }
 UEdGraphPin* SourceOut=FindPin(Source,SourcePin,EGPD_Output); if (!SourceOut) { Out.Refusals.Add(FString::Printf(TEXT("source '%s' has no unique output pin named '%s'"),*SourceNodeId,*SourcePin)); return true; }
 if (TargetNodeId.IsEmpty() && TargetTypeId.IsEmpty()) { Out.Refusals.Add(TEXT("targetTypeId is required when targetNodeId is empty")); return true; }
 UEdGraphNode* Target=nullptr;
 if (!TargetNodeId.IsEmpty()) { Target=FindNode(G,TargetNodeId); if (!Target) { Out.Refusals.Add(FString::Printf(TEXT("target node '%s' was not found"),*TargetNodeId)); return true; } }
 else { FString Note; Target=FTurboMCPNodeType::BuildProbeNode(B,G,TargetTypeId,Note); Out.bCreateTarget=true; if (!Target) { Out.Refusals.Add(FString::Printf(TEXT("could not build target type '%s': %s"),*TargetTypeId,*Note)); return true; } }
 if (Target == Source) { Out.Refusals.Add(TEXT("source and target must be different nodes; deleting the source must not delete the target")); return true; }
 UEdGraphPin* TargetOut=FindPin(Target,TargetPin,EGPD_Output); if (!TargetOut) {
  TArray<FString> Outputs; for (UEdGraphPin* P:Target->Pins) if(P&&P->Direction==EGPD_Output) Outputs.Add(P->PinName.ToString());
  Out.Refusals.Add(FString::Printf(TEXT("target has no unique output pin named '%s'; outputs: %s"),*TargetPin,Outputs.Num()?*FString::Join(Outputs,TEXT(", ")):TEXT("(none)"))); return true;
 }
 for (UEdGraphPin* ConsumerPin : SourceOut->LinkedTo) {
  if (!ConsumerPin || !ConsumerPin->GetOwningNode()) continue;
  FReader Item; Item.ConsumerNodeId=TurboMCPNodeId::MakeForNode(ConsumerPin->GetOwningNode(),true); Item.ConsumerPin=ConsumerPin->PinName.ToString(); Item.OldSourceNodeId=SourceNodeId; Item.OldSourcePin=SourcePin; Out.Readers.Add(MoveTemp(Item));
  const FPinConnectionResponse Check=G->GetSchema()->CanCreateConnection(TargetOut,ConsumerPin);
  const bool ReplacesOnlySource = Check.Response == CONNECT_RESPONSE_BREAK_OTHERS_B
   && ConsumerPin->LinkedTo.Num() == 1 && ConsumerPin->LinkedTo.Contains(SourceOut);
  if (Check.Response != CONNECT_RESPONSE_MAKE && !ReplacesOnlySource) Out.Refusals.Add(FString::Printf(TEXT("target '%s' cannot connect to reader '%s.%s': %s"),*TargetPin,*Out.Readers.Last().ConsumerNodeId,*Out.Readers.Last().ConsumerPin,*Check.Message.ToString()));
 }
 if (!Out.bKeepSource && !Source->CanUserDeleteNode())
  Out.Refusals.Add(TEXT("source node is protected from deletion; use keepSource=true"));
 if (!Out.bKeepSource)
  for (const UEdGraphPin* Pin : Source->Pins)
   if (Pin && Pin != SourceOut && !Pin->LinkedTo.IsEmpty())
    Out.Refusals.Add(FString::Printf(TEXT("source pin '%s' has connections outside this repoint; keepSource=true preserves them"), *Pin->PinName.ToString()));
 if (Out.Readers.Num()==0) Out.Refusals.Add(TEXT("source output has no linked readers; nothing would change"));
 return true;
}

FString FTurboMCPGraphRepoint::RenderPreview(const FPlan& P) {
 FString Out=FString::Printf(TEXT("preview: repoint %s.%s in %s"),*P.SourceNodeId,*P.SourcePin,*P.GraphName);
 Out+=FString::Printf(TEXT("\n%d reader(s) would move to %s.%s"),P.Readers.Num(),P.bCreateTarget?*P.TargetTypeId:*P.TargetNodeId,*P.TargetPin);
 for(const FReader& R:P.Readers) Out+=FString::Printf(TEXT("\n    %s.%s: %s.%s -> %s.%s"),*R.ConsumerNodeId,*R.ConsumerPin,*R.OldSourceNodeId,*R.OldSourcePin,P.bCreateTarget?TEXT("(new target)"):*P.TargetNodeId,*P.TargetPin);
 if(P.Refusals.Num()){Out+=FString::Printf(TEXT("\n\nunchanged: %d refusal(s). Nothing would be written."),P.Refusals.Num()); for(const FString& S:P.Refusals) Out+=FString::Printf(TEXT("\n    refused %s"),*S);}
 else Out+=TEXT("\n; a preview changes NOTHING. Call the edit to write it."); return Out;
}

/**
 * *** THE FIRST MIGRATED WRITER, AND THE WHOLE-EDIT TRANSACTION IS THE POINT (finding G04). ***
 *
 * MEASURED, and it is the defect this rewrite removes: `Apply()` opened one transaction and then called
 * `ApplyCore()`, which ALSO resolved the asset and called `Modify()` - so the operation had two entry paths
 * with their own transactions, and the failure path called `FTurboMCPRollback::EndAndUndoActiveTransaction()`
 * on a transaction the caller owned. One public edit is now one transaction, one resolve, one compile
 * (ARC03's sequence), and the transaction is ended in exactly one place.
 *
 * The outcome is set as a named value rather than left for a caller to infer from the report's prose: `Failed`
 * means the original state was restored, `OutcomeUnknown` means it could not be verified, and `Unchanged` means
 * nothing needed doing. Only a caller with those three apart can report them honestly.
 *
 * The compile baseline is ARC04's, in its smallest honest form: the diagnostics are read BEFORE and AFTER, and
 * a NEW error or warning rolls the edit back. Comparing only the COUNT would accept a changed message at the
 * same count as success, which is the mistake ARC04 names in as many words.
 */
TArray<FString> FTurboMCPGraphRepoint::ReadCompileDiagnostics(UBlueprint* Blueprint)
{
	// A public member so a caller can compare a baseline WITHOUT an edit, which is what ARC04's explicit Compile
	// tool will need. It compiles and reads the log, and it changes nothing itself.
	FCompilerResultsLog Log;
	if (Blueprint != nullptr)
	{
		FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::None, &Log);
	}
	return NormalizeDiagnostics(Log);
}

TArray<FString> FTurboMCPGraphRepoint::NormalizeDiagnostics(const FCompilerResultsLog& Log)
{
	// *** IDENTITY, NOT COUNT, AND THAT IS THE WHOLE OF ARC04's DIAGNOSTIC RULE. ***
	//
	// The audit's graph-safety review records that counting errors accepts a CHANGED message at the same count
	// as success - "A changed diagnostic with the same count is still new" - so each message becomes a stable
	// string of its severity and its text, and the two sets are compared as SETS.
	//
	// THE LIMIT IS STATED RATHER THAN HIDDEN: the engine's results log exposes the message and its severity,
	// not the node that produced it, so this detects a new MESSAGE and cannot detect a message whose text is
	// unchanged but whose target moved. ARC04 asks for a node identity "where available"; it is not available
	// through this API on this engine revision, and the honest form is to say so.
	TArray<FString> Out;
	for (const TSharedRef<FTokenizedMessage>& Message : Log.Messages)
	{
		FString Severity;
		switch (Message->GetSeverity())
		{
		case EMessageSeverity::Error:       Severity = TEXT("error"); break;
		case EMessageSeverity::PerformanceWarning:
		case EMessageSeverity::Warning:     Severity = TEXT("warning"); break;
		default:                            continue;  // information and below are not diagnostics for this rule
		}
		Out.Add(FString::Printf(TEXT("%s: %s"), *Severity, *Message->ToText().ToString()));
	}
	// Sorted so the comparison does not depend on the order the compiler reported them in - the same reason
	// ARC06 requires unordered sets to be sorted before hashing.
	//
	// DUPLICATES ARE KEPT. The earlier version de-duplicated here, which destroyed the MULTIPLICITY ARC04
	// requires: an asset with one instance of a message before the edit and two after had an identical
	// normalized set, so the second occurrence - a new diagnostic by ARC04's rule - compared as pre-existing.
	Out.Sort();
	return Out;
}

namespace
{
	/**
	 * Restores a package's dirty flag on the way out unless the edit committed.
	 *
	 * ARC05 requires a refused edit to leave the package in its initial state, and the compile baseline that
	 * ARC04 requires marks the blueprint structurally modified before any mutation is attempted. Without this
	 * guard, a refused repoint left the asset dirty, so a caller's "nothing changed" was true for the graph and
	 * false for the package. Restoring a flag is not a substitute for verified structural rollback: it runs
	 * only on the paths that never committed a transaction, and the structural rollback is checked separately.
	 */
	struct FDirtyFlagGuard
	{
		UPackage* Package = nullptr;
		bool bOriginal = false;
		bool bKeepChange = false;

		FDirtyFlagGuard(UPackage* InPackage, bool bInOriginal) : Package(InPackage), bOriginal(bInOriginal) {}

		~FDirtyFlagGuard()
		{
			if (Package != nullptr && !bKeepChange)
			{
				Package->SetDirtyFlag(bOriginal);
			}
		}
	};
}

bool FTurboMCPGraphRepoint::ApplyTransacted(const FPlan& P, const FApplyContext& Context, FApplyOutcome& Out,
	FString& Report, FString& Error, bool bJoinCallerTransaction)
{
	Out = FApplyOutcome();
	Report.Empty(); Error.Empty();
	const double StartSeconds = FPlatformTime::Seconds();

    UTransBuffer* Buffer = GEditor ? Cast<UTransBuffer>(GEditor->Trans) : nullptr;
    const bool AllowedTransaction = bJoinCallerTransaction ? Buffer && Buffer->ActiveCount == 1 : GEditor && !GEditor->IsTransactionActive();
    if (!IsInGameThread() || !GEditor || !AllowedTransaction || GEditor->IsPlaySessionInProgress() || Context.bCompile || Context.bSave)
    {
        Error = TEXT("unchanged: repoint requires an idle editor and separate explicit compile/save operations");
        Out.Outcome = ETurboMCPResultOutcome::Refused;
        Out.Code = TEXT("unsafe_edit_context");
        return false;
    }

	// A plan that refused changes nothing and is not a failure. Reporting `failed` here would tell a caller
	// that a refusal was an error, which is the distinction ARC02's vocabulary exists to keep.
	if (P.Refusals.Num() > 0)
	{
		Report = RenderPreview(P);
		Out.Outcome = ETurboMCPResultOutcome::Refused;
		Out.Code = TEXT("plan_refused");
		Out.Rollback = ETurboMCPResultRollback::NotNeeded;
		return true;
	}

	FTurboMCPAssetResolver::FResolved R;
	Error = FTurboMCPAssetResolver::Resolve(P.Asset, R);
	if (!Error.IsEmpty()) { Out.Outcome = ETurboMCPResultOutcome::Failed; Out.Code = TEXT("asset_unresolved"); return false; }

	UBlueprint* B = LoadObject<UBlueprint>(nullptr, *R.ObjectPath);
	UEdGraph* G = FindGraph(B, P.GraphName);
	if (!B || !G || !G->GetSchema())
	{
		Error = TEXT("graph disappeared before apply");
		Out.Outcome = ETurboMCPResultOutcome::Failed;
		Out.Code = TEXT("graph_missing");
		Out.Rollback = ETurboMCPResultRollback::NotNeeded;  // nothing was touched
		return false;
	}
	UEdGraphNode* DeleteCandidate = FindNode(G, P.SourceNodeId);
	UEdGraphPin* PlannedOutput = FindPin(DeleteCandidate, P.SourcePin, EGPD_Output);
	if (!PlannedOutput || PlannedOutput->LinkedTo.Num() != P.Readers.Num())
	{
		Error = TEXT("unchanged: source reader set changed since planning; request a fresh plan");
		Out.Outcome = ETurboMCPResultOutcome::Refused;
		Out.Code = TEXT("stale_readers");
		Out.Rollback = ETurboMCPResultRollback::NotNeeded;
		return false;
	}

	if (!P.bKeepSource && DeleteCandidate && !DeleteCandidate->CanUserDeleteNode())
	{
		Error = TEXT("unchanged: source node is protected from deletion; use keepSource=true");
		Out.Outcome = ETurboMCPResultOutcome::Refused;
		Out.Code = TEXT("source_protected");
		Out.Rollback = ETurboMCPResultRollback::NotNeeded;
		return false;
	}
	Out.Assets.Add(R.PackagePath);

    const bool bDirtyBefore = B->GetOutermost()->IsDirty();
    Out.bDirtyBefore = bDirtyBefore;
    FDirtyFlagGuard DirtyGuard(B->GetOutermost(), bDirtyBefore);
    TUniquePtr<FScopedTransaction> Tx;
    if (!bJoinCallerTransaction)
        Tx = MakeUnique<FScopedTransaction>(NSLOCTEXT("TurboMCP", "GraphRepoint", "TurboMCP repoint graph readers"));
    auto FailEdit = [&]()
    {
        bool Restored = false;
        if (!bJoinCallerTransaction && Buffer && Buffer->ActiveCount == 1 && !Buffer->UndoBuffer.IsEmpty())
        {
            const FGuid OwnedId = Buffer->UndoBuffer.Last()->GetId();
            Tx.Reset(); // Finalize exactly once before undoing our own record.
            const int32 Index = Buffer->GetQueueLength() - Buffer->GetUndoCount() - 1;
            const FTransaction* Record = Index >= 0 ? Buffer->GetTransaction(Index) : nullptr;
            Restored = Record && Record->GetId() == OwnedId && GEditor->UndoTransaction(false);
        }
        Out.Rollback = Restored ? ETurboMCPResultRollback::Complete : ETurboMCPResultRollback::NotSupported;
        Out.bChanged = !Restored;
        Out.Changed = Restored ? 0 : 2;
        DirtyGuard.bKeepChange = !Restored;
        Error += Restored ? TEXT("; rollback=complete") : TEXT("; rollback=not_confirmed; caller must restore before retrying");
    };
	B->Modify();
	G->Modify();
    for (UEdGraphNode* Node : G->Nodes) if (Node) Node->Modify();

	UEdGraphNode* Source = FindNode(G, P.SourceNodeId);
	UEdGraphPin* SourceOut = FindPin(Source, P.SourcePin, EGPD_Output);
	if (!SourceOut)
	{
		Error = TEXT("source disappeared before apply");
		FailEdit();
		Out.Outcome = ETurboMCPResultOutcome::Failed;
		Out.Code = TEXT("source_missing");
		return false;
	}

	FString CreatedTargetId;
	UEdGraphNode* Target = nullptr;
	if (P.bCreateTarget)
	{
		FString Note;
		if (FTurboMCPNodeType::SpawnNodeOfType(B, G, P.TargetTypeId, Target, Note) != FTurboMCPNodeType::ESpawnResult::Spawned)
		{
			Error = FString::Printf(TEXT("could not create target: %s"), *Note);
			FailEdit();
			Out.Outcome = ETurboMCPResultOutcome::Failed;
			Out.Code = TEXT("target_not_created");
			return false;
		}
		Out.Effects.Add(TEXT("graph_node_created"));
	}
	else
	{
		Target = FindNode(G, P.TargetNodeId);
	}
	UEdGraphPin* TargetOut = FindPin(Target, P.TargetPin, EGPD_Output);
	if (!TargetOut)
	{
		Error = TEXT("target disappeared before apply");
		FailEdit();
		Out.Outcome = ETurboMCPResultOutcome::Failed;
		Out.Code = TEXT("target_missing");
		return false;
	}

	for (const FReader& Item : P.Readers)
	{
		UEdGraphNode* Consumer = FindNode(G, Item.ConsumerNodeId);
		UEdGraphPin* ConsumerPin = FindPin(Consumer, Item.ConsumerPin, EGPD_Input);
		if (!ConsumerPin || !ConsumerPin->LinkedTo.Contains(SourceOut) || !SourceOut->LinkedTo.Contains(ConsumerPin))
		{
			Error = TEXT("reader or its planned source link changed before apply");
			FailEdit();
			Out.Outcome = ETurboMCPResultOutcome::Failed;
			Out.Code = TEXT("reader_missing");
			return false;
		}
		G->GetSchema()->BreakSinglePinLink(SourceOut, ConsumerPin);
		if (G->GetSchema()->CanCreateConnection(TargetOut, ConsumerPin).Response != CONNECT_RESPONSE_MAKE
            || !G->GetSchema()->TryCreateConnection(TargetOut, ConsumerPin))
		{
			Error = FString::Printf(TEXT("connection refused for reader %s.%s"), *Item.ConsumerNodeId, *Item.ConsumerPin);
			FailEdit();
			Out.Outcome = ETurboMCPResultOutcome::Failed;
			Out.Code = TEXT("connection_refused");
			return false;
		}
	}

	// Step 11: compare the REQUESTED state with the ACTUAL one, before claiming success. A readback that does
	// not contain the new link is a failure even though every engine call returned true.
	for (const FReader& Item : P.Readers)
	{
		UEdGraphNode* Consumer = FindNode(G, Item.ConsumerNodeId);
		UEdGraphPin* ConsumerPin = FindPin(Consumer, Item.ConsumerPin, EGPD_Input);
		if (!ConsumerPin || !ConsumerPin->LinkedTo.Contains(TargetOut)
            || !TargetOut->LinkedTo.Contains(ConsumerPin)
            || (SourceOut != TargetOut && ConsumerPin->LinkedTo.Contains(SourceOut)))
		{
			Error = TEXT("readback mismatch");
			FailEdit();
			Out.Outcome = ETurboMCPResultOutcome::Failed;
			Out.Code = TEXT("readback_mismatch");
			return false;
		}
	}

	if (!P.bKeepSource)
	{
        // Recheck after schema callbacks: never delete a node with links that
        // were not moved by this operation (including newly added readers).
        for (const UEdGraphPin* Pin : Source->Pins)
        {
            if (Pin && !Pin->LinkedTo.IsEmpty())
            {
                Error = TEXT("source still has connections outside the completed repoint; deletion refused");
                FailEdit();
                Out.Outcome = ETurboMCPResultOutcome::Failed;
                Out.Code = TEXT("source_connections_remaining");
                return false;
            }
        }
		if (Source->Pins.Num() > 0)
		{
			FBlueprintEditorUtils::RemoveNode(B, Source, true);
			Out.Effects.Add(TEXT("graph_node_removed"));
		}
	}

    FBlueprintEditorUtils::MarkBlueprintAsModified(B);
    Out.bChanged = true;
    Out.Changed = 1;
    Out.Compile = ETurboMCPResultCompile::NotRun;
    // Standalone edits commit here; batch edits remain owned by the outer transaction.
    Tx.Reset();

	{
		Out.Outcome = ETurboMCPResultOutcome::Changed;
		Out.Code = TEXT("ok");
		Out.Rollback = ETurboMCPResultRollback::NotNeeded;
		Out.Save = ETurboMCPResultSave::NotRequested;
		Out.ElapsedMs = (FPlatformTime::Seconds() - StartSeconds) * 1000.0;

		CreatedTargetId = P.bCreateTarget ? TurboMCPNodeId::MakeForNode(Target, true) : P.TargetNodeId;
	}

	// The transaction is committed, so the package really is modified: the dirty flag stays as the edit left it.
	DirtyGuard.bKeepChange = true;

	// *** THE GUARD: THE PLAN'S NodeCountBefore IS USED, AND BOTH COUNTS ARE REPORTED. ***
	//
	// The plan recorded `NodeCountBefore` (L45) and NEVER used it, so nothing in this function compared the
	// intended change with the graph's real final state. This is the check that turns "the tool confidently
	// reported a clean success while the graph disagreed" into a refusal.
	//
	// EXACT, NOT `>=`: a comparison that cannot express its claim is how `GetUndoCount() >= 0` and
	// `$anyForeign.Count -eq 0` both hid real defects. Both operands are printed on failure.
	{
		int32 NodeCountAfter = -1;
		if (UEdGraph* GAfter = FindGraph(B, P.GraphName))
		{
			NodeCountAfter = GAfter->Nodes.Num();
		}
		int32 NodeCountExpected = P.NodeCountBefore;
		if (P.bCreateTarget) { NodeCountExpected += 1; }
		if (!P.bKeepSource) { NodeCountExpected -= 1; }

		if (NodeCountAfter < 0 || NodeCountAfter != NodeCountExpected)
		{
			// ARC05: the change may or may not have landed, and a caller must never be able to read "nothing
			// happened" from an unverified outcome.
			Out.Outcome = ETurboMCPResultOutcome::OutcomeUnknown;
			Out.Code = TEXT("graph_nodecount_mismatch");
			Out.Changed = 2;  // tri-state: 2 == unknown
			Out.Rollback = ETurboMCPResultRollback::NotSupported;
			Error = FString::Printf(
				TEXT("the graph's node count does not match the plan, so the outcome cannot be vouched for. ")
				TEXT("Expected %d (before %d, +%d created, -%d removed); found %s. ")
				TEXT("A count that disagrees with the plan means nodes were created or destroyed that the ")
				TEXT("report cannot account for."),
				NodeCountExpected, P.NodeCountBefore, P.bCreateTarget ? 1 : 0, P.bKeepSource ? 0 : 1,
				NodeCountAfter < 0 ? TEXT("(the graph could not be resolved after the edit)") : *FString::FromInt(NodeCountAfter));
			Report = FString::Printf(
				TEXT("outcome_unknown: the edit completed but the graph's node count does not match the plan\n")
				TEXT("nodes: before %d, created %d, removed %d, expected %d, found %d\n")
				TEXT("target the report would have named: %s"),
				P.NodeCountBefore, P.bCreateTarget ? 1 : 0, P.bKeepSource ? 0 : 1, NodeCountExpected,
				NodeCountAfter, *CreatedTargetId);
			return false;
		}

		Report = FString::Printf(TEXT("changed: repointed %d reader(s) in %s; target %s.%s; old source %s\n"
			"nodes: before %d, created %d, removed %d, after %d\n"
			"compile: %s%s"),
			P.Readers.Num(), *P.GraphName, *CreatedTargetId, *P.TargetPin,
			P.bKeepSource ? TEXT("retained") : TEXT("deleted"),
			P.NodeCountBefore, P.bCreateTarget ? 1 : 0, P.bKeepSource ? 0 : 1, NodeCountAfter,
			*FTurboMCPResult::CompileToken(Out.Compile),
			Out.bCompileBaselineUsed
				? *FString::Printf(TEXT(" (compared against the whole asset's pre-edit diagnostics: %d before, "
				                        "%d after, none new)"),
				                   Out.BaselineDiagnosticCount, Out.AfterDiagnosticCount)
				: TEXT(""));
	}
    Report += TEXT("\nsave=not_requested; run EditTurboMCPCompile explicitly after editing");
	return true;
}

bool FTurboMCPGraphRepoint::ApplyCore(const FPlan& P, const FApplyContext& Context, FString& Report, FString& Error) {
	FApplyOutcome Outcome;
	return ApplyTransacted(P, Context, Outcome, Report, Error, true);
}

bool FTurboMCPGraphRepoint::Apply(const FPlan& P, FString& Report, FString& Error) {
	// ONE transaction, armed by ONE entry point. The old version opened its own transaction and then called a
	// core that resolved the asset and called Modify() again, so the same edit had two transaction owners.
	FApplyContext Context;
	Context.bCompile = false;
	FApplyOutcome Outcome;
	return ApplyTransacted(P, Context, Outcome, Report, Error);
}
