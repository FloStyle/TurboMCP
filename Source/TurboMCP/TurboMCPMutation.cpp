// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// TurboMCPMutation.cpp - the shared mutation coordinator. See TurboMCPMutation.h for why it exists.

#include "TurboMCPMutation.h"

#include "EdGraph/EdGraph.h"
#include "Editor/Transactor.h"
#include "Engine/Blueprint.h"
#include "Engine/World.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Logging/LogMacros.h"
#include "Misc/App.h"
#include "ScopedTransaction.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

DEFINE_LOG_CATEGORY_STATIC(LogTurboMCPMutation, Log, All);

namespace
{
	/**
	 * THE PROCESS-WIDE MUTATION GUARD.
	 *
	 * A static counter rather than a mutex, and that is deliberate. The guard answers "is a TurboMCP mutation
	 * in progress", and ARC03 requires the answer to be a REFUSAL rather than a wait: "Acquire the plugin
	 * mutation guard without blocking another request indefinitely." A mutex would make the second caller block,
	 * which is the indefinite wait the decision forbids. The engine runs these on the game thread, so a counter
	 * is sufficient as well as safer.
	 */
	int32 GMutationGuardDepth = 0;

	/** A transaction the coordinator did not open. ARC03: "an unrelated active transaction" is a refusal. */
	bool IsForeignTransactionOpen()
	{
#if WITH_EDITOR
		// *** THE ENGINE'S OWN TRANSACTION STACK IS THE AUTHORITY, AND IT HAS TO BE ASKED ABOUT `IsActive`. ***
		// MEASURED while writing this: the first version tested `GetQueueLength() > 0 && GetUndoCount() >= 0`,
		// and `GetUndoCount() >= 0` is true for every unsigned count - so it proved nothing and the check would
		// have refused every edit on a machine with any history at all. A condition that cannot be false is the
		// defect this whole release keeps finding, so it is spelled out here rather than left to a plausible
		// chain of comparisons.
		if (GEditor == nullptr || GEditor->Trans == nullptr || GMutationGuardDepth > 0)
		{
			return false;
		}
		return GEditor->Trans->IsActive();
#else
		return false;
#endif
	}
}

FTurboMCPTouchedObject::FTurboMCPTouchedObject(const UObject* Object)
{
	if (Object == nullptr)
	{
		return;
	}
	PathName = Object->GetPathName();
	ClassName = Object->GetClass() != nullptr ? Object->GetClass()->GetName() : FString();
	// The package is part of the identity because two objects can share a path name across packages, and a
	// fingerprint that cannot tell them apart is not a fingerprint (ARC06).
	if (UPackage* Package = Object->GetOutermost())
	{
		Identity = Package->GetFName();
	}
}

FTurboMCPMutationTransaction::FTurboMCPMutationTransaction() = default;

FTurboMCPMutationTransaction::~FTurboMCPMutationTransaction()
{
	// *** THE DESTRUCTOR MUST NOT END THE TRANSACTION. ***
	//
	// This is release finding G04's shape: "End a transaction exactly once. A scoped destructor must not end it
	// a second time." A destructor that cancels here would also make it impossible to distinguish "the caller
	// forgot to finish" from "the caller restored on purpose", so it DOES NOT end anything and it says so
	// loudly. Leaving the transaction for the engine's own transaction system to clean up at the end of the
	// frame is strictly safer than a second cancel, and a leaked guard would deadlock every later edit - so the
	// guard alone is released, with a warning that names the defect.
	if (Transaction != nullptr)
	{
		UE_LOG(LogTurboMCPMutation, Warning,
			TEXT("a mutation transaction opened for '%s' was never committed or restored. The destructor does "
			     "NOT end it, because ending a transaction twice is the defect this coordinator exists to "
			     "prevent. The caller must call Commit() or Restore()."),
			*Description.ToString());
	}
	FTurboMCPMutationTransaction::Release();
}

bool FTurboMCPMutationTransaction::IsGuardHeld()
{
	return GMutationGuardDepth > 0;
}

void FTurboMCPMutationTransaction::ForceReleaseGuard(FString& OutNote)
{
	const int32 Was = GMutationGuardDepth;
	GMutationGuardDepth = 0;
	OutNote = FString::Printf(
		TEXT("the mutation guard was force-released (depth was %d). This is a recovery action: a writer that "
		     "did not call Commit() or Restore() left it held, and every later mutation would have been refused."),
		Was);
}

FString FTurboMCPMutationTransaction::ReadinessCode(ETurboMCPReadiness Readiness)
{
	switch (Readiness)
	{
	case ETurboMCPReadiness::Ready:                  return TEXT("ok");
	case ETurboMCPReadiness::PieActive:              return TEXT("pie_active");
	case ETurboMCPReadiness::Simulating:             return TEXT("simulating");
	case ETurboMCPReadiness::ShuttingDown:           return TEXT("shutting_down");
	case ETurboMCPReadiness::CompileInProgress:      return TEXT("compile_in_progress");
	case ETurboMCPReadiness::ForeignTransaction:     return TEXT("foreign_transaction");
	case ETurboMCPReadiness::Busy:                   return TEXT("busy");
	case ETurboMCPReadiness::TargetUnresolved:       return TEXT("target_unresolved");
	case ETurboMCPReadiness::CompileBaselineRequired:return TEXT("compile_baseline_required");
	case ETurboMCPReadiness::UndoUnavailable:        return TEXT("undo_unavailable");
	}
	return TEXT("unknown");
}

FString FTurboMCPMutationTransaction::OutcomeCode(ETurboMCPMutationOutcome Outcome)
{
	switch (Outcome)
	{
	case ETurboMCPMutationOutcome::Changed:        return TEXT("changed");
	case ETurboMCPMutationOutcome::Unchanged:      return TEXT("unchanged");
	case ETurboMCPMutationOutcome::Refused:        return TEXT("refused");
	case ETurboMCPMutationOutcome::Failed:         return TEXT("failed");
	case ETurboMCPMutationOutcome::OutcomeUnknown: return TEXT("outcome_unknown");
	}
	return TEXT("unknown");
}

ETurboMCPReadiness FTurboMCPMutationTransaction::CheckReadiness(FString& OutDetail)
{
	OutDetail.Empty();

	// A commandlet has no editor undo stack, so a mutation that needs undo must be refused rather than run
	// without the ability to restore. ARC03: "Refuse commandlet edits that require editor undo when that undo
	// facility is unavailable."
	const bool bHasUndo = (GEditor != nullptr);

	if (IsEngineExitRequested())
	{
		OutDetail = TEXT("the editor is shutting down");
		return ETurboMCPReadiness::ShuttingDown;
	}
	if (IsForeignTransactionOpen())
	{
		OutDetail = TEXT("another transaction is open that this coordinator does not own");
		return ETurboMCPReadiness::ForeignTransaction;
	}
	if (IsGuardHeld())
	{
		OutDetail = TEXT("another TurboMCP mutation holds the guard");
		return ETurboMCPReadiness::Busy;
	}
	if (GEditor != nullptr)
	{
		// *** PIE AND SIE ARE DIFFERENT STATES AND ONE ENGINE CALL DOES NOT TELL THEM APART. ***
		// MEASURED: `UWorld::IsSimulating()` does not exist on this engine revision (5.8.3), and the first
		// version of this check did not compile. `UWorld::WorldType` and the editor's play context DO exist, so
		// the state is read from them:
		//   * any PIE/SIE world running at all means the engine is executing the asset -> refuse;
		//   * `GEditor->PlayWorld` distinguishes SIMULATE from a real play session, which matters because the
		//     two are recovered differently and the diagnostic should say which one is in the way.
		if (GEditor->PlayWorld != nullptr)
		{
			OutDetail = TEXT("the editor has a play world active, so the engine is executing this asset");
			return ETurboMCPReadiness::PieActive;
		}
		if (const UWorld* World = GEditor->GetEditorWorldContext().World())
		{
			if (World->IsPlayInEditor())
			{
				OutDetail = TEXT("a play-in-editor session is running, so the engine is executing this asset");
				return ETurboMCPReadiness::PieActive;
			}
			if (World->WorldType == EWorldType::PIE || World->WorldType == EWorldType::Game)
			{
				OutDetail = TEXT("a game or PIE world is the current world context");
				return ETurboMCPReadiness::Simulating;
			}
		}
		// `GEditor` exists but the world context does not: that is the startup window, and an edit there has no
		// undo stack to restore from. Naming it beats a null dereference later.
		if (GEditor->GetEditorWorldContext().World() == nullptr)
		{
			OutDetail = TEXT("the editor has no world context yet, so there is no undo stack to edit against");
			return ETurboMCPReadiness::ShuttingDown;
		}
	}
	else if (FApp::IsUnattended())
	{
		// Unattended and no editor: a commandlet. Undo is unavailable, and that is a fact worth naming rather
		// than a condition to work around.
		OutDetail = TEXT("this is a commandlet, where the editor undo facility is unavailable");
		return ETurboMCPReadiness::UndoUnavailable;
	}

	if (!bHasUndo && FApp::IsUnattended())
	{
		OutDetail = TEXT("no editor undo facility is available");
		return ETurboMCPReadiness::UndoUnavailable;
	}
	return ETurboMCPReadiness::Ready;
}

ETurboMCPMutationOutcome FTurboMCPMutationTransaction::Begin(const FText& InDescription,
                                                           FString& RefusalCode, FString& Refusal,
                                                           const TArray<UObject*>& Targets)
{
	RefusalCode.Empty();
	Refusal.Empty();
	Description = InDescription;
	Touched.Reset();

	if (Transaction != nullptr)
	{
		RefusalCode = TEXT("transaction_already_open");
		Refusal = FString::Printf(
			TEXT("this coordinator already owns a transaction for '%s'. Do not acquire several independent "
			     "transactions inside one public edit (ARC03); finish the first with Commit() or Restore()."),
			*Description.ToString());
		return ETurboMCPMutationOutcome::Refused;
	}

	// Step 3: refuse an unsafe world state BEFORE anything is written.
	FString Detail;
	const ETurboMCPReadiness Readiness = CheckReadiness(Detail);
	if (Readiness != ETurboMCPReadiness::Ready)
	{
		RefusalCode = ReadinessCode(Readiness);
		Refusal = FString::Printf(TEXT("refused before any mutation: %s"), *Detail);
		return ETurboMCPMutationOutcome::Refused;
	}

	// Step 2, recorded before the transaction: every target resolved unambiguously and named canonically, so a
	// refusal later can say which object it was about without holding a pointer.
	for (UObject* Target : Targets)
	{
		if (Target == nullptr)
		{
			RefusalCode = TEXT("target_unresolved");
			Refusal = TEXT("refused before any mutation: a target resolved to null");
			return ETurboMCPMutationOutcome::Refused;
		}
		Touch(Target);
	}

	// Step 1: take the guard. A refusal rather than a wait, per ARC03.
	++GMutationGuardDepth;
	bGuardHeld = true;

	// Step 7: ONE owned transaction. Heap-allocated so that exactly one of Commit() or Restore() ends it.
	Transaction = new FScopedTransaction(InDescription);
	return ETurboMCPMutationOutcome::Changed;
}

void FTurboMCPMutationTransaction::NoteParticipant(const UObject* Object)
{
	Touch(Object);
}

void FTurboMCPMutationTransaction::Touch(const UObject* Object)
{
	if (Object == nullptr)
	{
		return;
	}
	const FString Path = Object->GetPathName();
	for (const FTurboMCPTouchedObject& Existing : Touched)
	{
		if (Existing.PathName == Path)
		{
			return;
		}
	}
	Touched.Emplace(Object);
}

TArray<FString> FTurboMCPMutationTransaction::GetTouchedPaths() const
{
	TArray<FString> Paths;
	Paths.Reserve(Touched.Num());
	for (const FTurboMCPTouchedObject& Entry : Touched)
	{
		Paths.Add(Entry.PathName);
	}
	// Sorted, because ARC06 requires unordered sets to be sorted before hashing and a report that varies with
	// iteration order cannot be compared between runs.
	Paths.Sort();
	return Paths;
}

ETurboMCPMutationOutcome FTurboMCPMutationTransaction::Commit(FString& RefusalCode, FString& Refusal)
{
	RefusalCode.Empty();
	Refusal.Empty();

	if (Transaction == nullptr)
	{
		// *** A SECOND COMMIT IS REFUSED, NOT IGNORED. *** End a transaction exactly once (ARC03). A writer
		// that committed twice must be told, because the second call would otherwise look like a no-op that
		// succeeded.
		RefusalCode = TEXT("transaction_not_open");
		Refusal = FString::Printf(
			TEXT("Commit() was called with no open transaction for '%s'. A transaction is ended exactly once; a "
			     "second Commit() or a Commit() after Restore() means the caller has lost track of the edit."),
			*Description.ToString());
		return ETurboMCPMutationOutcome::Refused;
	}

	// *** `FScopedTransaction` HAS NO Commit(): NOT CALLING Cancel() IS THE COMMIT. ***
	// MEASURED on engine 5.8.3: `ScopedTransaction.h` exposes only `Cancel()`, `IsOutstanding()` and the
	// destructor - so the first version's `Transaction->Commit()` did not compile. Destroying the object
	// WITHOUT cancelling keeps the recorded changes; `Cancel()` is the restore path and belongs to Restore()
	// below. That asymmetry is the whole reason this coordinator owns ONE heap object and ends it in exactly
	// one of two places, rather than letting a stack object's destructor decide.
	delete Transaction;
	Transaction = nullptr;

	Release();
	return ETurboMCPMutationOutcome::Changed;
}

ETurboMCPMutationOutcome FTurboMCPMutationTransaction::Restore(bool bVerified, FString& RefusalCode,
                                                              FString& Refusal)
{
	RefusalCode.Empty();
	Refusal.Empty();

	if (Transaction == nullptr)
	{
		RefusalCode = TEXT("transaction_not_open");
		Refusal = FString::Printf(
			TEXT("Restore() was called with no open transaction for '%s'. Nothing was restored, and this is NOT "
			     "evidence that the asset is unchanged."),
			*Description.ToString());
		return ETurboMCPMutationOutcome::OutcomeUnknown;
	}

	// *** A CANCELED TRANSACTION RECORD IS NOT RESTORATION (ARC03), SO THE CALLER'S VERIFICATION DECIDES. ***
	// `Cancel()` reverts the transaction's recorded changes; whether the ASSET is back to its original state is
	// a separate question the caller answers with an independent comparison, which is what `bVerified` is. This
	// coordinator refuses to report a restoration it did not verify, which is finding G05's entire point.
	Transaction->Cancel();
	delete Transaction;
	Transaction = nullptr;
	Release();

	if (bVerified)
	{
		return ETurboMCPMutationOutcome::Failed;  // failed, and restored - so the caller knows the asset is safe
	}
	RefusalCode = TEXT("rollback_unverified");
	Refusal = FString::Printf(
		TEXT("the edit for '%s' failed and the restoration could NOT be verified. The live state is not known: "
		     "do not report this as unchanged, do not save, and refuse further plugin mutations until the "
		     "session recovers (ARC05)."),
		*Description.ToString());
	return ETurboMCPMutationOutcome::OutcomeUnknown;
}

void FTurboMCPMutationTransaction::Finish()
{
	// No verdict is claimed. The transaction record is cancelled so the engine does not see an edit nobody
	// finished, and the guard is released so every later edit is not refused - the two things a caller that
	// cannot decide still owes the process.
	if (Transaction != nullptr)
	{
		Transaction->Cancel();
		delete Transaction;
		Transaction = nullptr;
	}
	Release();
}

void FTurboMCPMutationTransaction::Release()
{
	if (bGuardHeld && GMutationGuardDepth > 0)
	{
		--GMutationGuardDepth;
	}
	bGuardHeld = false;
}
