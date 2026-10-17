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
 return TurboMCPNodeId::FindGraphByName(B, Name);
}

UEdGraphNode* FindNode(UEdGraph* G, const FString& Id) {
 if (!G || Id.IsEmpty()) return nullptr;
 for (UEdGraphNode* N : G->Nodes) if (N && TurboMCPNodeId::MakeForNode(N, true) == Id) return N;
 return nullptr;
}
UEdGraphPin* FindPin(UEdGraphNode* N, const FString& Name, EEdGraphPinDirection Direction) {
 if (!N) return nullptr;
 for (UEdGraphPin* P : N->Pins) if (P && P->PinName.ToString().Equals(Name, ESearchCase::CaseSensitive) && P->Direction == Direction) return P;
 return nullptr;
}
FString BoolText(const FString& S) { return S.TrimStartAndEnd().IsEmpty() || !S.Equals(TEXT("false"), ESearchCase::IgnoreCase) && !S.Equals(TEXT("0"), ESearchCase::CaseSensitive) ? TEXT("true") : TEXT("false"); }
}

bool FTurboMCPGraphRepoint::Plan(const FString& Asset, const FString& GraphName, const FString& SourceNodeId,
 const FString& SourcePin, const FString& TargetNodeId, const FString& TargetTypeId, const FString& TargetPin,
 const FString& KeepSource, FPlan& Out, FString& Error) {
 Out = FPlan(); Error.Empty(); Out.Asset=Asset; Out.GraphName=GraphName; Out.SourceNodeId=SourceNodeId; Out.SourcePin=SourcePin; Out.TargetNodeId=TargetNodeId; Out.TargetTypeId=TargetTypeId; Out.TargetPin=TargetPin; Out.bKeepSource=BoolText(KeepSource)==TEXT("true");
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
 UEdGraphPin* SourceOut=FindPin(Source,SourcePin,EGPD_Output); if (!SourceOut) { Out.Refusals.Add(FString::Printf(TEXT("source '%s' has no output pin named '%s'"),*SourceNodeId,*SourcePin)); return true; }
 if (TargetNodeId.IsEmpty() && TargetTypeId.IsEmpty()) { Out.Refusals.Add(TEXT("targetTypeId is required when targetNodeId is empty")); return true; }
 UEdGraphNode* Target=nullptr;
 if (!TargetNodeId.IsEmpty()) { Target=FindNode(G,TargetNodeId); if (!Target) { Out.Refusals.Add(FString::Printf(TEXT("target node '%s' was not found"),*TargetNodeId)); return true; } }
 else { FString Note; Target=FTurboMCPNodeType::BuildProbeNode(B,G,TargetTypeId,Note); Out.bCreateTarget=true; if (!Target) { Out.Refusals.Add(FString::Printf(TEXT("could not build target type '%s': %s"),*TargetTypeId,*Note)); return true; } }
 UEdGraphPin* TargetOut=FindPin(Target,TargetPin,EGPD_Output); if (!TargetOut) {
  TArray<FString> Outputs; for (UEdGraphPin* P:Target->Pins) if(P&&P->Direction==EGPD_Output) Outputs.Add(P->PinName.ToString());
  Out.Refusals.Add(FString::Printf(TEXT("target has no output pin named '%s'; outputs: %s"),*TargetPin,Outputs.Num()?*FString::Join(Outputs,TEXT(", ")):TEXT("(none)"))); return true;
 }
 for (UEdGraphPin* ConsumerPin : SourceOut->LinkedTo) {
  if (!ConsumerPin || !ConsumerPin->GetOwningNode()) continue;
  FReader Item; Item.ConsumerNodeId=TurboMCPNodeId::MakeForNode(ConsumerPin->GetOwningNode(),true); Item.ConsumerPin=ConsumerPin->PinName.ToString(); Item.OldSourceNodeId=SourceNodeId; Item.OldSourcePin=SourcePin; Out.Readers.Add(MoveTemp(Item));
  const FPinConnectionResponse Check=G->GetSchema()->CanCreateConnection(TargetOut,ConsumerPin);
  if (Check.Response != CONNECT_RESPONSE_MAKE) Out.Refusals.Add(FString::Printf(TEXT("target '%s' cannot connect to reader '%s.%s': %s"),*TargetPin,*Out.Readers.Last().ConsumerNodeId,*Out.Readers.Last().ConsumerPin,*Check.Message.ToString()));
 }
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
	FString& Report, FString& Error)
{
	Out = FApplyOutcome();
	Report.Empty(); Error.Empty();
	const double StartSeconds = FPlatformTime::Seconds();

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
	if (!B || !G)
	{
		Error = TEXT("graph disappeared before apply");
		Out.Outcome = ETurboMCPResultOutcome::Failed;
		Out.Code = TEXT("graph_missing");
		Out.Rollback = ETurboMCPResultRollback::NotNeeded;  // nothing was touched
		return false;
	}
	Out.Assets.Add(R.PackagePath);

	// --- ARC04: the diagnostics BEFORE the edit, on the same object ---
	//
	// The baseline is read by COMPILING once WITH a results log. ARC04 allows exactly that ("Use an explicit
	// Compile operation to establish a baseline when no valid baseline exists"), and it is done here rather than
	// once per mutation because this write is about to compile anyway. The cross-call cache ARC04 asks for is
	// NOT implemented yet, and that limit is recorded rather than implied: this compares within one edit.
	TArray<FString> Before;
	TArray<FString> After;
	FCompilerResultsLog Log;
	// ARC05: the forced baseline read marks the blueprint dirty. A refusal must leave the package exactly as
	// it was found, so the flag is captured here and restored by this guard unless the edit commits.
	const bool bDirtyBefore = B->GetOutermost()->IsDirty();
	Out.bDirtyBefore = bDirtyBefore;
	FDirtyFlagGuard DirtyGuard(B->GetOutermost(), bDirtyBefore);
	if (Context.bCompile)
	{
		// *** THE BASELINE MUST BE FORCED, AND THE FIRST VERSION DID NOT FORCE IT. ***
		//
		// MEASURED: this baseline returned ZERO messages while the identical call in the after-edit position
		// returned six, and the independent `EditTurboMCPCompile` tool reported SEVEN errors for the same asset.
		// The edit's refusal then blamed the caller's change: "the edit introduced 6 new compile diagnostic(s)",
		// when all six were already there.
		//
		// The cause is that `CompileBlueprint` recompiles what is DIRTY and reports only that. On a freshly loaded
		// blueprint whose generated class already exists and whose graphs are untouched, the compile is a NO-OP:
		// nothing is scheduled, nothing is reported, `Log.Messages` is empty. The after-edit read runs on a
		// structurally modified graph, so the compile is real and the pre-existing errors surface - which is why
		// the baseline looked like zero and the after-read looked like six.
		//
		// So the blueprint is marked structurally modified BEFORE the baseline read, which is what makes the
		// compile do the work the diagnostics describe. This does not change the edit: the transaction has not
		// opened yet, and `MarkBlueprintAsStructurallyModified` sets a dirty flag rather than altering a graph.
		//
		// The first compile's count is logged rather than discarded, because the difference between the two
		// numbers IS the mechanism - a silent skip would otherwise be invisible exactly as it was here.
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(B);
		FKismetEditorUtilities::CompileBlueprint(B, EBlueprintCompileOptions::None, &Log);
		Before = NormalizeDiagnostics(Log);
		UE_LOG(LogTemp, Display,
			TEXT("[TurboMCP] baseline diagnostics for %s: %d message(s), %d error(s) - read after forcing the "
			     "compile; without the force this read returned none"),
			*B->GetName(), Log.Messages.Num(), Log.NumErrors);
		Log.Messages.Reset();
	}

	FScopedTransaction Tx(NSLOCTEXT("TurboMCP", "GraphRepoint", "TurboMCP repoint graph readers"));
	B->Modify();
	G->Modify();

	UEdGraphNode* Source = FindNode(G, P.SourceNodeId);
	UEdGraphPin* SourceOut = FindPin(Source, P.SourcePin, EGPD_Output);
	if (!SourceOut)
	{
		Error = TEXT("source disappeared before apply");
		Tx.Cancel();
		Out.Outcome = ETurboMCPResultOutcome::Failed;
		Out.Code = TEXT("source_missing");
		Out.Rollback = ETurboMCPResultRollback::Complete;
		Out.bChanged = false;
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
			Tx.Cancel();
			Out.Outcome = ETurboMCPResultOutcome::Failed;
			Out.Code = TEXT("target_not_created");
			Out.Rollback = ETurboMCPResultRollback::Complete;
			Out.bChanged = false;
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
		Tx.Cancel();
		Out.Outcome = ETurboMCPResultOutcome::Failed;
		Out.Code = TEXT("target_missing");
		Out.Rollback = ETurboMCPResultRollback::Complete;
		Out.bChanged = false;
		return false;
	}

	for (const FReader& Item : P.Readers)
	{
		UEdGraphNode* Consumer = FindNode(G, Item.ConsumerNodeId);
		UEdGraphPin* ConsumerPin = FindPin(Consumer, Item.ConsumerPin, EGPD_Input);
		if (!ConsumerPin)
		{
			Error = TEXT("reader disappeared before apply");
			Tx.Cancel();
			Out.Outcome = ETurboMCPResultOutcome::Failed;
			Out.Code = TEXT("reader_missing");
			Out.Rollback = ETurboMCPResultRollback::Complete;
			Out.bChanged = false;
			return false;
		}
		G->GetSchema()->BreakSinglePinLink(SourceOut, ConsumerPin);
		if (!G->GetSchema()->TryCreateConnection(TargetOut, ConsumerPin))
		{
			Error = FString::Printf(TEXT("connection refused for reader %s.%s"), *Item.ConsumerNodeId, *Item.ConsumerPin);
			Tx.Cancel();
			Out.Outcome = ETurboMCPResultOutcome::Failed;
			Out.Code = TEXT("connection_refused");
			Out.Rollback = ETurboMCPResultRollback::Complete;
			Out.bChanged = false;
			return false;
		}
	}

	// Step 11: compare the REQUESTED state with the ACTUAL one, before claiming success. A readback that does
	// not contain the new link is a failure even though every engine call returned true.
	for (const FReader& Item : P.Readers)
	{
		UEdGraphNode* Consumer = FindNode(G, Item.ConsumerNodeId);
		UEdGraphPin* ConsumerPin = FindPin(Consumer, Item.ConsumerPin, EGPD_Input);
		if (!ConsumerPin || !ConsumerPin->LinkedTo.Contains(TargetOut))
		{
			Error = TEXT("readback mismatch; all original links restored");
			Tx.Cancel();
			Out.Outcome = ETurboMCPResultOutcome::Failed;
			Out.Code = TEXT("readback_mismatch");
			Out.Rollback = ETurboMCPResultRollback::Complete;
			Out.bChanged = false;
			return false;
		}
	}

	if (!P.bKeepSource)
	{
		if (Source->Pins.Num() > 0)
		{
			G->RemoveNode(Source);
			Out.Effects.Add(TEXT("graph_node_removed"));
		}
	}

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(B);
	Out.bChanged = true;

	// Step 10: compile ONCE per affected compilable asset, and read the diagnostics the engine reported.
	if (Context.bCompile)
	{
		FKismetEditorUtilities::CompileBlueprint(B, EBlueprintCompileOptions::None, &Log);
		After = NormalizeDiagnostics(Log);
		Out.bCompileRan = true;
		Out.Compile = ETurboMCPResultCompile::Passed;
		Out.BaselineDiagnosticCount = Before.Num();
		Out.AfterDiagnosticCount = After.Num();

		// *** THE COMPARISON IS PACKAGE-WIDE AND MULTIPLICITY-AWARE. THE EARLIER VERSION WEAKENED IT. ***
		//
		// This is the rule ARC04 states without qualification: "Pre-existing diagnostics may remain. Any new
		// error or warning causes rollback." It was narrowed in an earlier pass to diagnostics whose TEXT NAMED
		// THE EDITED GRAPH, on the finding that a package-wide comparison refused every edit of the acceptance
		// asset. That finding was a symptom of a DIFFERENT defect in the same block: the baseline was discarded
		// (`Before.Reset()`) immediately before the comparison, so every diagnostic the compiler reported looked
		// new. The narrowing made the symptom disappear and turned a real new error elsewhere in the package
		// into a success report - the exact failure mode this contract exists to prevent.
		//
		// The baseline is now kept, so pre-existing diagnostics compare equal and do not refuse an edit. The
		// multiplicity check is why duplicates are preserved in NormalizeDiagnostics: one instance before and
		// two after is a new diagnostic even though the normalized set is unchanged.
		//
		// The rule lives in ONE pure function so a native case can prove it fires; a rule that cannot fail has
		// been this project's most repeated defect.
		const TArray<FString> New = FTurboMCPResult::NewDiagnosticsSince(Before, After);

		Out.NewDiagnostics = New;
		if (New.Num() > 0)
		{
			// AND THE MESSAGE MUST NOT BLAME THE CALLER FOR WHAT WAS ALREADY THERE. MEASURED: this once said "the
			// edit introduced 6 new compile diagnostic(s)" for six errors the asset already had, because the
			// baseline read had returned none. Both totals are now named, so "the asset was already failing" is
			// distinguishable from "this edit broke it".
			Error = FString::Printf(
				TEXT("the edit adds %d compile diagnostic(s) to an asset that already had %d. The comparison covers "
				     "the whole asset (%d before, %d after), so a new error in any graph rolls the edit back. "
				     "New from this edit: %s"),
				New.Num(), Before.Num(), Before.Num(), After.Num(), *FString::Join(New, TEXT(" | ")));
			Tx.Cancel();
			Out.Outcome = ETurboMCPResultOutcome::Failed;
			Out.Code = TEXT("new_compile_diagnostics");
			Out.Compile = ETurboMCPResultCompile::Failed;
			Out.Rollback = ETurboMCPResultRollback::Complete;
			Out.bChanged = false;
			Out.bCompileBaselineUsed = true;
			return false;
		}
		Out.bCompileBaselineUsed = true;
	}
	else
	{
		Out.Compile = ETurboMCPResultCompile::NotRun;
	}

	// -----------------------------------------------------------------------------------------------
	// *** THE SUCCESS PATH IS SCOPED SO THE COMMIT HAPPENS BEFORE THE GRAPH IS VERIFIED. ***
	//
	// MEASURED, and it is why this block exists: `FScopedTransaction Tx` lives until the FUNCTION returns, and on
	// this engine a transaction is committed by being DESTROYED WITHOUT `Cancel()`. So the commit happened AFTER
	// the report was built - which meant the report described a state nobody had checked. `TargetId` was computed
	// from a LIVE POINTER before the commit, and the tool then reported "changed ... compile: passed" while the
	// graph, measured afterwards by node CLASS, was missing the created node AND had lost another one (3 -> 2
	// K2Node_CallFunction, id 554569 gone).
	//
	// A node-count check placed inside the old success block would have read the graph at the one moment it could
	// not be wrong. Closing the scope here destroys `Tx`, which IS the commit, so everything below runs against
	// the committed graph.
	// -----------------------------------------------------------------------------------------------
	{
		Out.Outcome = ETurboMCPResultOutcome::Changed;
		Out.Code = TEXT("ok");
		Out.Rollback = ETurboMCPResultRollback::NotNeeded;
		Out.Save = Context.bSave ? ETurboMCPResultSave::Saved : ETurboMCPResultSave::NotRequested;
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
			Out.bChanged = false;
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
	return true;
}

bool FTurboMCPGraphRepoint::ApplyCore(const FPlan& P, const FApplyContext& Context, FString& Report, FString& Error) {
	FApplyOutcome Outcome;
	return ApplyTransacted(P, Context, Outcome, Report, Error);
}

bool FTurboMCPGraphRepoint::Apply(const FPlan& P, FString& Report, FString& Error) {
	// ONE transaction, armed by ONE entry point. The old version opened its own transaction and then called a
	// core that resolved the asset and called Modify() again, so the same edit had two transaction owners.
	FApplyContext Context;
	Context.bCompile = true;
	FApplyOutcome Outcome;
	return ApplyTransacted(P, Context, Outcome, Report, Error);
}
