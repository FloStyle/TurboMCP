// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// *** FOCUSED NATIVE CASES FOR THE SHARED MUTATION COORDINATOR (ARC03). ***
//
// These run as native automation in the editor. They are here rather than in Python because the coordinator
// owns an engine transaction and a process-wide guard, and ARC12 is explicit that a C++ helper requiring
// Unreal is tested "through focused native automation after the plugin build" - not by reimplementing its
// algorithm in Python merely to label the test pure.
//
// WHAT THEY CAN AND CANNOT PROVE. The world-state refusals (PIE, simulation, shutdown) need a play session and
// are covered by M01 at L3; what is proved here is the STATE MACHINE, which is where G04 and G05 live:
//
//   * end the transaction exactly once - a second Commit() is refused, not ignored;
//   * a restore whose verification fails reports OutcomeUnknown and never claims "unchanged";
//   * a restore whose verification succeeds reports Failed, so a caller knows the asset is safe;
//   * the guard is released on every exit path, including the refusal path;
//   * the outcome and readiness codes are the stable snake_case strings the result line publishes, so a
//     writer cannot invent a sixth state no client can classify.

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"

#include "TurboMCPMutation.h"
#include "UObject/Package.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTurboMCPMutationCoordinatorTest,
	"TurboMCP.Mutation.CoordinatorStateMachine",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTurboMCPMutationCoordinatorTest::RunTest(const FString& Parameters)
{
	// --- the published vocabulary ----------------------------------------------------------------
	// A writer cannot invent a code no client can classify, so every outcome and readiness value maps to a
	// stable snake_case string and the mapping is asserted rather than assumed.
	TestEqual(TEXT("Changed maps to changed"),
		FTurboMCPMutationTransaction::OutcomeCode(ETurboMCPMutationOutcome::Changed), TEXT("changed"));
	TestEqual(TEXT("Unchanged maps to unchanged"),
		FTurboMCPMutationTransaction::OutcomeCode(ETurboMCPMutationOutcome::Unchanged), TEXT("unchanged"));
	TestEqual(TEXT("Refused maps to refused"),
		FTurboMCPMutationTransaction::OutcomeCode(ETurboMCPMutationOutcome::Refused), TEXT("refused"));
	TestEqual(TEXT("Failed maps to failed"),
		FTurboMCPMutationTransaction::OutcomeCode(ETurboMCPMutationOutcome::Failed), TEXT("failed"));
	TestEqual(TEXT("OutcomeUnknown maps to outcome_unknown"),
		FTurboMCPMutationTransaction::OutcomeCode(ETurboMCPMutationOutcome::OutcomeUnknown),
		TEXT("outcome_unknown"));
	TestEqual(TEXT("a compile-baseline refusal names the code ARC04 publishes"),
		FTurboMCPMutationTransaction::ReadinessCode(ETurboMCPReadiness::CompileBaselineRequired),
		TEXT("compile_baseline_required"));
	TestEqual(TEXT("Ready maps to ok"),
		FTurboMCPMutationTransaction::ReadinessCode(ETurboMCPReadiness::Ready), TEXT("ok"));

	// --- the guard is free before and after, and a refusal must not leak it ------------------------
	TestFalse(TEXT("the guard is free before any edit"), FTurboMCPMutationTransaction::IsGuardHeld());

	{
		FTurboMCPMutationTransaction Refusing;
		FString Code;
		FString Reason;
		// No targets and a running editor: this either begins or refuses, and EITHER WAY the guard must end up
		// free. What is asserted is the invariant, not which branch this machine takes - the branch depends on
		// whether the test runner has a world, and asserting one of them would make the case fail on a machine
		// that takes the other for a reason unrelated to the coordinator.
		const ETurboMCPMutationOutcome Outcome = Refusing.Begin(
			NSLOCTEXT("TurboMCPTest", "RefusingBegin", "TurboMCP coordinator test"), Code, Reason);
		if (Outcome == ETurboMCPMutationOutcome::Changed)
		{
			TestTrue(TEXT("a begun transaction holds the guard"), FTurboMCPMutationTransaction::IsGuardHeld());
			// FINISH, NOT RELEASE. MEASURED: `Release()` alone left the transaction object alive and the
			// coordinator's own destructor warning fired - which is the diagnostic doing its job on the test
			// that verifies it. `Finish()` closes the transaction record and the guard, and claims no verdict,
			// which is exactly what a case that only wanted to see Begin() succeed owes the process.
			Refusing.Finish();
		}
		else
		{
			TestEqual(TEXT("a refusal reports Refused"), Outcome, ETurboMCPMutationOutcome::Refused);
			TestFalse(TEXT("a refusal names a code"), Code.IsEmpty());
			TestFalse(TEXT("a refusal explains itself"), Reason.IsEmpty());
			TestFalse(TEXT("a refusal does NOT hold the guard"), FTurboMCPMutationTransaction::IsGuardHeld());
		}
	}

	// --- ending the transaction exactly once (finding G04) ------------------------------------------
	{
		FTurboMCPMutationTransaction Mutation;
		FString Code;
		FString Reason;
		const ETurboMCPMutationOutcome Begun = Mutation.Begin(
			NSLOCTEXT("TurboMCPTest", "OnceOnly", "TurboMCP coordinator once-only test"), Code, Reason);

		if (Begun == ETurboMCPMutationOutcome::Changed)
		{
			TestTrue(TEXT("Begin opens exactly one transaction"), Mutation.IsOpen());
			TestEqual(TEXT("the first Commit reports Changed"),
				Mutation.Commit(Code, Reason), ETurboMCPMutationOutcome::Changed);
			TestFalse(TEXT("the transaction is closed after Commit"), Mutation.IsOpen());
			TestFalse(TEXT("the guard is released by Commit"),
				FTurboMCPMutationTransaction::IsGuardHeld());

			// *** THE SECOND COMMIT IS THE DEFECT G04 NAMES: "End a transaction exactly once." ***
			TestEqual(TEXT("a SECOND Commit is refused rather than ignored"),
				Mutation.Commit(Code, Reason), ETurboMCPMutationOutcome::Refused);
			TestEqual(TEXT("the second-commit refusal names its code"), Code, TEXT("transaction_not_open"));
			TestFalse(TEXT("the second-commit refusal explains itself"), Reason.IsEmpty());

			// And a Restore after a Commit is refused too, with an outcome that does NOT claim the asset is
			// unchanged - which is the one thing a caller must never be told here.
			TestEqual(TEXT("a Restore after Commit does not claim a known state"),
				Mutation.Restore(/*bVerified=*/true, Code, Reason),
				ETurboMCPMutationOutcome::OutcomeUnknown);
			TestEqual(TEXT("that refusal names transaction_not_open"), Code, TEXT("transaction_not_open"));
		}
		else
		{
			TestEqual(TEXT("with no world the coordinator refuses rather than beginning"),
				Begun, ETurboMCPMutationOutcome::Refused);
			TestFalse(TEXT("no transaction is left open"), Mutation.IsOpen());
			TestEqual(TEXT("a Commit with no open transaction is refused"),
				Mutation.Commit(Code, Reason), ETurboMCPMutationOutcome::Refused);
			TestEqual(TEXT("a Restore with no open transaction is OutcomeUnknown, never unchanged"),
				Mutation.Restore(/*bVerified=*/true, Code, Reason),
				ETurboMCPMutationOutcome::OutcomeUnknown);
		}
	}

	// --- a failed restoration is OutcomeUnknown, and a verified one is Failed (finding G05) ----------
	// This is the pair that G05 records as wrong: "rollback claims do not depend on rollback success". The two
	// answers must differ, because they tell a caller different things about whether the asset is safe.
	//
	// *** EACH TRANSACTION IS CLOSED IN BOTH BRANCHES, AND THE FIRST VERSION OF THIS CASE WAS NOT. ***
	// MEASURED in the automation log while running this very suite:
	//
	//   LogTurboMCPMutation: Warning: a mutation transaction opened for 'TurboMCP coordinator test' was never
	//   committed or restored.
	//
	// The coordinator's own destructor warning caught a leaked transaction IN THE TEST THAT VERIFIES THE
	// COORDINATOR - which is the diagnostic doing its job, and a reminder that a test which leaves engine state
	// behind fails the next case for a reason of its own making. Every branch below ends its transaction.
	{
		FTurboMCPMutationTransaction Unverified;
		FString Code;
		FString Reason;
		const ETurboMCPMutationOutcome Begun = Unverified.Begin(
			NSLOCTEXT("TurboMCPTest", "Restore", "TurboMCP coordinator restore test"), Code, Reason);
		if (Begun == ETurboMCPMutationOutcome::Changed)
		{
			TestEqual(TEXT("an UNVERIFIED restoration is OutcomeUnknown and never claims unchanged"),
				Unverified.Restore(/*bVerified=*/false, Code, Reason),
				ETurboMCPMutationOutcome::OutcomeUnknown);
			TestEqual(TEXT("the unverified restoration names rollback_unverified"),
				Code, TEXT("rollback_unverified"));
			TestFalse(TEXT("the unverified restoration explains itself"), Reason.IsEmpty());
			TestFalse(TEXT("the guard is released even when restoration is unverified"),
				FTurboMCPMutationTransaction::IsGuardHeld());
		}

		FTurboMCPMutationTransaction Verified;
		const ETurboMCPMutationOutcome BegunAgain = Verified.Begin(
			NSLOCTEXT("TurboMCPTest", "Restore2", "TurboMCP coordinator restore test 2"), Code, Reason);
		if (BegunAgain == ETurboMCPMutationOutcome::Changed)
		{
			TestEqual(TEXT("a VERIFIED restoration is Failed, so the caller knows the asset is safe"),
				Verified.Restore(/*bVerified=*/true, Code, Reason),
				ETurboMCPMutationOutcome::Failed);
			TestTrue(TEXT("a verified restoration carries no refusal code"), Code.IsEmpty());
		}
		else
		{
			// A BEGIN THAT WAS REFUSED OWES NOTHING TO CLEAN UP, and the refusal itself is asserted so this
			// branch cannot hide a coordinator that refused for an unexamined reason.
			TestEqual(TEXT("a refused second Begin reports Refused, not Changed"),
				BegunAgain, ETurboMCPMutationOutcome::Refused);
			TestFalse(TEXT("a refused Begin leaves no transaction open"), Verified.IsOpen());
		}
	}

	// --- the guard cannot be left held by a forgotten Release ----------------------------------------
	// A leaked guard would refuse every later edit, so the recovery path is asserted rather than trusted.
	TestFalse(TEXT("the guard is free at the end of the suite"),
		FTurboMCPMutationTransaction::IsGuardHeld());
	FString Note;
	FTurboMCPMutationTransaction::ForceReleaseGuard(Note);
	TestFalse(TEXT("the force-release says what it did rather than pretending nothing was held"),
		Note.IsEmpty());
	TestTrue(TEXT("the force-release note names the depth it found"), Note.Contains(TEXT("depth was")));

	return true;
}
#endif // WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

