// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "CoreTypes.h"

/**
 * ROW 10.2: the automatic checkout a write tool performs before it writes.
 *
 * NOTE ON THIS COMMENT: it deliberately contains NO backtick characters. This project has recorded that
 * a doc comment holding a backtick and a later apostrophe is rejected by the build's own comment
 * checker, and warnings are errors here. Identifiers are therefore written bare, in quotes when they
 * need separating from the prose.
 *
 * ---------------------------------------------------------------------------------------------
 * THE TRAP THIS FILE EXISTS TO AVOID, MEASURED BY ROW 10.1
 * ---------------------------------------------------------------------------------------------
 *
 * "USourceControlHelpers::CheckOutFile" ANSWERS "True" ON GIT AND NEVER ISSUES A CHECKOUT. It returns
 * early at SourceControlHelpers.cpp:456-460 when the provider state says "IsCheckedOut() || IsAdded()",
 * AND GIT REPORTS "IsCheckedOut() == true" FOR EVERY TRACKED FILE. So that boolean true means "the state
 * says it is already checked out", NOT "a lock was taken" - and a write tool wired to it would read
 * "checked out, safe to write" on a provider whose "UsesCheckout() == false", which is exactly the
 * "git will not [refuse], and will happily leave a modified file for someone else to find" of
 * Spec/03_TOOLS.md:1348-1351.
 *
 * SO THIS FILE NEVER CALLS CheckOutFile AND NEVER READS ITS BOOLEAN. It reads two things instead, both
 * of them measured answers rather than a helper opinion:
 *
 *   "ISourceControlProvider::UsesCheckout()" - whether a checkout can mean anything on this provider at
 *   all. False on Git (GitSourceControlProvider.cpp:286-289), true on Perforce
 *   (PerforceSourceControlProvider.cpp:622-625), Subversion (:239-242) and Plastic (:498-501).
 *
 *   "ISourceControlProvider::Execute(FCheckOut)" AND THE OPERATION OWN RESULT - the provider answer,
 *   verbatim. On Git that is "ECommandResult::Failed" with
 *   "Operation 'CheckOut' not supported by revision control provider 'Git'", produced before any command
 *   runs, and it is a string a boolean can never carry.
 *
 * ---------------------------------------------------------------------------------------------
 * WHAT A WRITE TOOL DOES WHEN "UsesCheckout() == false", WHICH IS THE ROW OWN DECISION
 * ---------------------------------------------------------------------------------------------
 *
 * D-57 (Spec/05_DECISIONS.md:382-388) answers the ORDERING - E-VCS-CHECKOUT is built and tested before
 * any write tool calls it, which row 10.1 satisfied - and the REASON: "An automatic checkout wired into
 * every write would turn a source control problem into a 'every tool reports failure' problem, and the
 * cause would be invisible from any of those results." IT DOES NOT ANSWER what a write tool should DO on
 * a provider where a checkout can never help. That choice is this row own, and it is stated in every
 * answer the tool gives rather than left implicit.
 *
 * THE THREE OPTIONS, AND THE ONE CHOSEN:
 *
 *   (a) SKIP IT, AND SAY YOU SKIPPED.                      NOT CHOSEN.
 *   (b) ATTEMPT IT AND REPORT THE PROVIDER ANSWER.         *** CHOSEN ***
 *   (c) REFUSE THE WRITE UNLESS THE CALLER OPTS IN.         NOT CHOSEN.
 *
 * WHY (b):
 *   1. IT IS THE SAME ANSWER E-VCS-CHECKOUT ALREADY GIVES. Row 10.1 issued the operation on a no-op
 *      provider and recorded the provider own words rather than predicting a no-op. A write tool that
 *      answered differently about the same asset and the same provider would give a caller two stories.
 *   2. IT NEVER CLAIMS A LOCK WAS TAKEN. The absence of a lock is stated in words, and the provider
 *      refusal is repeated verbatim, so the trap above cannot be reproduced by a reader of this answer.
 *   3. IT CANNOT TURN A SOURCE CONTROL PROBLEM INTO A WRITE FAILURE, WHICH IS THE D-57 WARNING. Option
 *      (c) would refuse every write on Git and on a provider-less editor - that is the "every tool
 *      reports failure" outcome D-57 rejected, arrived at from the other side. So THE CHECKOUT ANSWER IS
 *      REPORTED AND NEVER GATES THE WRITE.
 *   4. IT COSTS NOTHING THAT IS NOT ALREADY BEING PAID. The E-SAVE engine path ALREADY issues a checkout
 *      on every save without reading either fact: UEditorAssetLibrary::SaveLoadedAsset ->
 *      UEditorLoadingAndSavingUtils::SavePackages -> FileHelpers.cpp:6019-6023 ->
 *      InternalCheckoutAndSavePackages, whose FileHelpers.cpp:5925-5929 calls
 *      FEditorFileUtils::CheckoutPackages whenever ISourceControlModule::Get().IsEnabled(), ignoring the
 *      operation result. This file does the same operation EARLIER, with the answer recorded.
 *
 * AND IT IS A MEASURED COST OF ZERO ON THE ONLY PROVIDER THIS MACHINE HAS. On Git the failure is produced
 * at the worker lookup (GitSourceControlProvider.cpp:225-238) before any command runs, and
 * "git status --porcelain" was empty before and after every probe in row 10.1.
 *
 * ---------------------------------------------------------------------------------------------
 * WHERE THE DECISION LIVES
 * ---------------------------------------------------------------------------------------------
 *
 * IN Plan, ALWAYS. Plan reads the module facts, the capability flags and each asset state, and decides
 * for every asset whether an operation would be issued and why. Render prints that decision and reaches
 * no provider with a checkout. EnsureCheckedOut consumes the decision, issues the operation for exactly
 * the assets the plan allowed, and re-decides nothing - so a Preview and its Edit twin cannot disagree
 * about what will happen.
 */

/** One asset named to a write tool, and everything source control was asked about it. */
struct FTurboMCPWriteCheckoutAsset
{
	/** What the write tool caller wrote. */
	FString Requested;

	/** The resolved package path, when the input resolved. */
	FString PackagePath;

	/** The absolute file on disk, when it was found. */
	FString File;

	/** Which conversion produced File, so a path that came out wrong is traceable. */
	FString FileRoute;

	/** True when the input resolved to one asset. */
	bool bResolved = false;

	/**
	 * Why NOTHING will be asked of a provider for this asset, or empty.
	 *
	 * Either the run-level engine refusal, or the input could not be turned into a file on disk. It is
	 * the same value the Preview prints, because both read it out of one plan.
	 */
	FString Refusal;

	/** True when a provider was actually asked for this asset state. */
	bool bStateRead = false;

	/** True when the provider gave no state: an ABSENT MEASUREMENT, never "not under source control". */
	bool bStateUnknown = false;
	FString StateUnknownReason;

	bool bSourceControlled = false;
	bool bCheckedOut = false;
	bool bAdded = false;
	bool bModified = false;

	/**
	 * TRUE WHEN THE PROVIDER REPORTS "UsesCheckout() == false", WHICH IS THE TRAP OWN FLAG.
	 *
	 * On such a provider a checkout is a no-op BY CONSTRUCTION, so the answer says so and then repeats
	 * what the provider actually answered. False here is not a promise that a lock was taken either; it
	 * only means this provider has a checkout concept.
	 */
	bool bNoOpExpected = false;

	/** DECIDED IN Plan: true when a checkout operation WOULD be issued for this asset. */
	bool bWillAttempt = false;

	/** WHY, in one sentence. Always filled, whichever way the decision went. */
	FString Intent;

	/** Filled by EnsureCheckedOut only. A Preview leaves these alone and says it did nothing. */
	bool bAttempted = false;
	bool bSucceeded = false;
	FString ProviderAnswer;
	TArray<FString> ProviderErrors;
	TArray<FString> ProviderInfo;
};

/** Everything one automatic checkout decided, worked out once and rendered twice. */
struct FTurboMCPWriteCheckoutPlan
{
	/** True when the engine SourceControl module could be loaded at all. */
	bool bModuleLoaded = false;

	/** ISourceControlModule::IsEnabled() - SourceControlModule.cpp:376. */
	bool bEnabled = false;

	/** ISourceControlProvider::IsAvailable() - asked ONLY when bEnabled. */
	bool bAvailable = false;
	bool bAvailabilityAsked = false;

	/** ISourceControlProvider::GetName() - the provider actually in use. */
	FString ProviderName;

	/** ISourceControlProvider::UsesCheckout() - the fact that decides what a checkout can mean. */
	bool bUsesCheckout = false;
	/** ISourceControlProvider::UsesLocalReadOnlyState(). */
	bool bUsesLocalReadOnlyState = false;
	/** The two capability flags were readable, i.e. a provider exists and the module answered. */
	bool bCapabilitiesRead = false;

	/** The ONE refusal shared by every asset, taken verbatim from the engine own gate. */
	FString RunRefusal;
	FString RunRefusalSource;

	/**
	 * The policy, named in the answer and stated in full.
	 *
	 * There is no enum with unused values here on purpose: an unreachable branch is error C4702 in this
	 * build, and a policy that is documented but not taken is better said in words than compiled as a
	 * case that can never run. The chosen policy is AttemptAndReport, and the two rejected options are
	 * named in PolicyStatement so every answer carries the decision.
	 */
	FString PolicyName;
	FString PolicyStatement;

	/** The registry own list, printed so the provider name above is the engine answer. */
	TArray<FString> RegisteredProviders;

	TArray<FTurboMCPWriteCheckoutAsset> Assets;
};

class FTurboMCPSourceControlWrite
{
public:
	/**
	 * Works out everything: the module facts once, then one row per asset and the decision for each.
	 *
	 * This is the ONLY place a decision is made, which is what makes a Preview and its Edit twin
	 * incapable of disagreeing. It reaches a provider for a STATE only, never for a checkout.
	 */
	static FTurboMCPWriteCheckoutPlan Plan(const TArray<FString>& InAssets);

	/**
	 * Renders a plan for a Preview. Writes nothing and issues no checkout to any provider.
	 *
	 * InWriteTool names the tool the block belongs to, so an answer from E-SAVE and an answer from
	 * E-STRUCT-FIELDS cannot be mistaken for each other.
	 */
	static FString Render(const FTurboMCPWriteCheckoutPlan& InPlan, const FString& InWriteTool);

	/**
	 * Renders the plan and then issues the checkout for every asset the plan allowed.
	 *
	 * *** THE ONLY FUNCTION IN THIS FILE THAT CAN REACH A PROVIDER WITH A WRITE-INTENT OPERATION. ***
	 * The answer it returns is a REPORT: it does not gate the write and it does not fail the caller.
	 * That is D-57 obeyed - a source control problem must not become a write failure - and it is said in
	 * the text rather than left to be inferred.
	 */
	static FString EnsureCheckedOut(const FTurboMCPWriteCheckoutPlan& InPlan, const FString& InWriteTool);
};
