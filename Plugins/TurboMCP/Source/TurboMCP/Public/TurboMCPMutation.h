// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// *** THE SMALL SHARED MUTATION COORDINATOR, ARC03. ***
//
// WHY IT EXISTS. Release findings G02, G04, G05, G07 and G11 are the same defect wearing five hats: a writer
// opened its own transaction, called Modify() after it had already changed something, ignored the compile
// result, and reported `changed` without checking that its own rollback had worked. Every family solved that
// separately, and every family solved it differently. This file is the ONE place that decides:
//
//   * whether an edit may start at all (PIE, simulation, shutdown, a conflicting compilation, an unrelated
//     active transaction, another request holding the guard);
//   * the exact sequence transaction -> Modify -> apply -> compile once -> compare -> commit-or-restore;
//   * what a refusal, a partial application and a failed restoration are CALLED, so a caller cannot report
//     `unchanged` about an edit that may have applied.
//
// WHAT IT DELIBERATELY DOES NOT DO. It does not decide what an edit means - that is the family core's job, and
// ARC03 says so: "Family-specific cores own validation and the actual object changes." It does not keep raw
// UObject pointers between calls (ARC06). It does not use global Undo to recover an unrelated transaction. And
// it does not treat "the transaction object went out of scope" as evidence that the edit was committed.
//
// THE ONE INVARIANT THAT MATTERS MOST: `End()` is called EXACTLY ONCE. A `FScopedTransaction` inside this file
// would end the transaction in its destructor, and the failure path would then end it a second time - which is
// release finding G04's shape, where "replace is two committed edits, not one atomic replacement". So the
// transaction is a heap object owned by this class, ended by exactly one of `Commit()` or `Restore()`, and a
// second call is refused with a named diagnostic rather than silently ignored.

#pragma once

#include "CoreMinimal.h"

class UObject;
class FScopedTransaction;

/**
 * What one mutation attempt concluded, in the vocabulary ARC02 publishes.
 *
 * The enum is deliberately the same set of outcomes the result line carries, so a writer cannot invent a
 * sixth state that no client can classify. `OutcomeUnknown` is present because a failed restoration genuinely
 * has no known state, and reporting `failed` for it - as G05 records - tells a caller the asset did not change
 * when it may have.
 */
enum class ETurboMCPMutationOutcome : uint8
{
	/** The edit was applied and verified. */
	Changed,
	/** Nothing needed changing, or a plan refused. Safe: the asset is as it was. */
	Unchanged,
	/** The request was refused BEFORE any mutation. Nothing was written. */
	Refused,
	/** The edit failed and the original state was restored and verified. */
	Failed,
	/** The edit failed and restoration could NOT be verified. The live state is not known. */
	OutcomeUnknown,
};

/** Why a mutation may not begin. Each value is a stable diagnostic code, not prose. */
enum class ETurboMCPReadiness : uint8
{
	Ready,
	/** A play-in-editor session is running. The engine is executing the asset being edited. */
	PieActive,
	/** A simulation session is running. */
	Simulating,
	/** The editor is shutting down. */
	ShuttingDown,
	/** The Blueprint is already compiling, so a second pass would read half-updated state. */
	CompileInProgress,
	/** Another transaction is open that this coordinator does not own. */
	ForeignTransaction,
	/** Another TurboMCP request holds the guard. */
	Busy,
	/** The target could not be resolved unambiguously. */
	TargetUnresolved,
	/** No valid compile baseline exists and none can be established. */
	CompileBaselineRequired,
	/** A commandlet edit asked for editor undo, which a commandlet does not have. */
	UndoUnavailable,
};

/**
 * One thing a mutation changed, as a canonical identity - never a raw UObject pointer.
 *
 * ARC06: "Do not retain raw UObject pointers across independent tool calls." A diagnostic or a fingerprint that
 * holds a pointer is a stale-state bug waiting for the next GC, so this carries the object's path name and its
 * package instead.
 */
struct FTurboMCPTouchedObject
{
	FString PathName;
	FString ClassName;
	FName Identity = NAME_None;

	FTurboMCPTouchedObject() = default;
	FTurboMCPTouchedObject(const UObject* Object);
};

/** A participant the caller wants recorded, taken BEFORE its first mutation (step 8 of ARC03's sequence). */
struct FTurboMCPParticipant
{
	/** Recorded before Modify() is called, so a rollback can name what was touched. */
	TArray<FTurboMCPTouchedObject> Objects;
};

/**
 * The shared coordinator. One instance per public mutation tool; it is cheap and holds no UObject.
 *
 * Usage, in the order ARC03 fixes:
 *
 *     FTurboMCPMutationTransaction Mutation;
 *     ETurboMCPMutationOutcome Outcome = Mutation.Begin(TEXT("TurboMCP repoint"), Refusal, Error);
 *     if (Outcome == ETurboMCPMutationOutcome::Refused) { ... nothing was written ... }
 *     // the family core applies its changes and calls Mutation.NoteParticipant() before each Modify()
 *     Outcome = Mutation.Compile(Targets, Diagnostics, Refusal, Error);   // step 10
 *     Outcome = Mutation.Compare(ActualState, ExpectedState, Refusal, Error);  // step 11
 *     Outcome = Mutation.Commit(Refusal, Error);   // or Mutation.Restore(Refusal, Error)
 *
 * Every step returns an outcome, so a caller that forgets one still has to look at the answer.
 */
struct TURBOMCP_API FTurboMCPMutationTransaction
{
public:
	FTurboMCPMutationTransaction();
	~FTurboMCPMutationTransaction();

	// No copying: it owns a transaction and a guard, and a copy would end both twice.
	FTurboMCPMutationTransaction(const FTurboMCPMutationTransaction&) = delete;
	FTurboMCPMutationTransaction& operator=(const FTurboMCPMutationTransaction&) = delete;

	/**
	 * Steps 1 to 7: take the guard, resolve, refuse an unsafe world state, capture the pre-state, and open ONE
	 * owned transaction.
	 *
	 * `Refusal` is filled with a stable code and a sentence naming the tool the caller must use instead, when
	 * the refusal is one the caller can act on (ARC04: "The client instruction must name the explicit Compile
	 * tool and asset").
	 */
	ETurboMCPMutationOutcome Begin(const FText& Description, FString& RefusalCode, FString& Refusal,
	                               const TArray<UObject*>& Targets = TArray<UObject*>());

	/** Record a participant BEFORE its first mutation, so a rollback can name what it touched. */
	void NoteParticipant(const UObject* Object);

	/** Where the transaction stands. Used by tests and by the second-call refusals below. */
	bool IsOpen() const { return Transaction != nullptr; }

	/** The description the transaction was opened with, for a diagnostic that has to name the edit. */
	const FText& GetDescription() const { return Description; }

	/** The canonical identities recorded so far, sorted, for a stable fingerprint or report. */
	TArray<FString> GetTouchedPaths() const;

	/**
	 * Step 12's success half: mark the Blueprint(s) structurally modified and end the transaction exactly once.
	 *
	 * Refuses when called with no open transaction, and refuses a SECOND call - rather than ignoring it - so a
	 * writer cannot commit twice and believe the second was a no-op.
	 */
	ETurboMCPMutationOutcome Commit(FString& RefusalCode, FString& Refusal);

	/**
	 * Step 12's failure half and step 13: restore the owned transaction, VERIFY the restoration, and report
	 * `OutcomeUnknown` when verification fails.
	 *
	 * *** FINDING G05: "Rollback claims do not depend on rollback success." *** This function's whole job is to
	 * make that impossible. `bRestored` is not assumed from a successful Undo return: the caller passes a
	 * verification callback, and if it cannot confirm the original state the outcome is `OutcomeUnknown` with
	 * `changed` unknown - never `Failed`, which would tell a caller the asset is as it was.
	 */
	ETurboMCPMutationOutcome Restore(bool bVerified, FString& RefusalCode, FString& Refusal);

	/** Step 14: release the guard. Idempotent, because cleanup runs on several paths. */
	void Release();

	/**
	 * Close whatever is still open, without claiming a verdict. For a test, an early return, or any caller that
	 * cannot say whether the edit succeeded.
	 *
	 * *** WHY THIS IS NOT `Release()`. *** MEASURED: a test that called `Release()` after a successful `Begin()`
	 * left the transaction object alive, and the destructor's warning fired - "opened for 'TurboMCP coordinator
	 * test' ... never committed or restored". `Release()` frees the GUARD, which is what stops every later edit;
	 * it cannot free the TRANSACTION, because only `Commit()` (keep) or `Restore()` (revert) may decide that.
	 * A caller that genuinely cannot decide needs a third, honest option: cancel the transaction record, release
	 * the guard, and say in the code that no verdict was claimed. Silence here would leave a transaction for the
	 * engine's frame cleanup and a warning nobody reads.
	 */
	void Finish();

	/** Whether this coordinator still holds the process-wide guard. */
	static bool IsGuardHeld();

	/** For a test or a recovery path: force the guard free and say so, rather than pretending it was never held. */
	static void ForceReleaseGuard(FString& OutNote);

	/** The readiness of the world, without taking anything. Safe to call from a read-only path. */
	static ETurboMCPReadiness CheckReadiness(FString& OutDetail);

	/** A stable code for a readiness value, for the result line's `code` field. */
	static FString ReadinessCode(ETurboMCPReadiness Readiness);

	/** A stable code for an outcome, for the result line's `code` field. */
	static FString OutcomeCode(ETurboMCPMutationOutcome Outcome);

private:
	/** Records one object's canonical identity, once. Never stores a pointer beyond the call. */
	void Touch(const UObject* Object);

	/** The owned transaction, ended EXACTLY ONCE by Commit() or Restore(). */
	FScopedTransaction* Transaction = nullptr;
	FText Description;
	bool bGuardHeld = false;
	TArray<FTurboMCPTouchedObject> Touched;
};
