// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#include "Containers/Array.h"
#include "Containers/UnrealString.h"

/**
 * `E-VCS-CHECKOUT`: what source control says about an asset, and taking the lock before a write.
 *
 * THE CHECKS ARE IN `Plan` AND NOT IN `Checkout`, so the Preview and its Edit twin refuse the same
 * input. `Checkout` consumes a plan and re-decides nothing.
 *
 * ---------------------------------------------------------------------------------------------
 * WHY BOTH QUESTIONS ARE ASKED, AND WHY ASKING THEM COSTS A PROVIDER NOTHING
 * ---------------------------------------------------------------------------------------------
 *
 * The engine's own gate is `VerifySourceControl` at `Engine/Source/Developer/SourceControl/Private/
 * SourceControlHelpers.cpp:61-85`, and it asks TWO questions in a fixed order:
 *
 *     :65  if (!SCModule.IsEnabled())        -> :67 "Revision control is not enabled."
 *     :74  if (!Provider->IsAvailable())     -> :76 "Revision control server is currently not available."
 *     :84  return Provider;   (nullptr on either refusal)
 *
 * BOTH ARE REQUIRED, AND THAT IS MEASURED RATHER THAN DEFENSIVE. Perforce
 * (`PerforceSourceControlProvider.cpp:232-235`), Plastic (`PlasticSourceControlProvider.cpp:283-286`) and
 * Subversion (`SubversionSourceControlProvider.cpp:79-82`) return `IsEnabled() == true` UNCONDITIONALLY,
 * while `IsAvailable()` is false - so asking only the first reports a working provider where there is none,
 * which is the one answer this tool must never give.
 *
 * AND A PROVIDER IS NEVER REACHED TO FIND OUT. With the default provider, `GetState` returns
 * `ECommandResult::Failed` and a NULL state (`DefaultSourceControlProvider.cpp:52-55`) that a reader can
 * mistake for "not under source control"; `CommandletSourceControlUtils.cpp:184-194` shows what that costs.
 * So the per-asset state is asked ONLY after both checks have passed, and a null state that arrives anyway
 * is reported as an ABSENT MEASUREMENT with its reason, which is `P1`: an absent measurement must be
 * distinguishable from a zero (`TurboMCPAssetCost.cpp:22-23`, `:101-105`).
 *
 * ---------------------------------------------------------------------------------------------
 * `UsesCheckout()` IS THE DIFFERENCE THAT DECIDES WHAT THE ANSWER MEANS
 * ---------------------------------------------------------------------------------------------
 *
 * `Spec/03_TOOLS.md:1348-1351` says "Perforce will refuse a save that is not checked out; git will not".
 * That is correct and it has lines: Perforce `:622-625` returns true, Git `:286-289` returns false with the
 * reason in words at `GitSourceControlModule.cpp:21` ("this provider does not uses the `CheckOut` command,
 * which is a Perforce `lock`, as Git has no lock command"). Subversion `:239-242` and Plastic `:498-501`
 * return true.
 *
 * SO A CHECKOUT ON A PROVIDER THAT DOES NOT USE CHECKOUT IS A NO-OP, AND THE ANSWER SAYS SO RATHER THAN
 * IMPLYING A LOCK WAS TAKEN. It is still asked of the provider and the provider's own answer is repeated
 * verbatim, because the honest form is "this is expected to be a no-op, and here is what it actually
 * answered" rather than either a prediction or a false claim of success.
 */
struct FTurboMCPSourceControlAsset
{
	/** What the caller wrote. */
	FString Requested;

	/** The resolved package path, when the input resolved. */
	FString PackagePath;

	/** The absolute file on disk, when it was found. */
	FString File;

	/** Which conversion produced `File`, so a path that came out wrong is traceable. */
	FString FileRoute;

	/** True when the input resolved to one asset. */
	bool bResolved = false;

	/**
	 * A refusal of THIS ASSET, and the promise that comes with it: NO PROVIDER WAS ASKED ABOUT IT.
	 *
	 * Either the run-level refusal (the two engine strings, verbatim, so the wording is the engine's and
	 * not this tool's) or the input could not be turned into a file on disk.
	 */
	FString Refusal;

	/** True when a provider was actually asked for this asset's state. */
	bool bStateRead = false;

	/** True when the provider gave no state: an ABSENT MEASUREMENT, never "not under source control". */
	bool bStateUnknown = false;
	FString StateUnknownReason;

	/** The provider's own words for the state, when there is a state. */
	FString StateDisplay;

	bool bSourceControlled = false;
	bool bCheckedOut = false;
	bool bAdded = false;
	bool bCurrent = false;
	bool bModified = false;
	bool bIgnored = false;
	bool bCanCheckout = false;

	/** Checkout result. Only ever filled by `Checkout`, never by `Plan` and never by a renderer. */
	bool bAttempted = false;
	bool bSucceeded = false;
	FString ProviderAnswer;
	TArray<FString> ProviderErrors;
	/** True when the attempt was made against a provider that has no checkout concept. */
	bool bCheckoutIsNoOp = false;
	/** Set when the plan decided not to attempt, with the reason. */
	FString NotAttemptedReason;
};

/** Everything one call decided, worked out once and rendered twice. */
struct FTurboMCPSourceControlPlan
{
	/** True when the engine's `SourceControl` module could be loaded at all. */
	bool bModuleLoaded = false;

	/** `ISourceControlModule::IsEnabled()` - `SourceControlModule.cpp:376`. */
	bool bEnabled = false;

	/** `ISourceControlModule::GetProvider().IsAvailable()` - asked ONLY when `bEnabled`. */
	bool bAvailable = false;
	bool bAvailabilityAsked = false;

	/** `ISourceControlProvider::GetName()` - the provider actually in use. */
	FString ProviderName;

	/** `ISourceControlProvider::UsesCheckout()` - the fact that decides what a checkout MEANS here. */
	bool bUsesCheckout = false;
	/** `ISourceControlProvider::UsesLocalReadOnlyState()` - whether read-only is the edit signal. */
	bool bUsesLocalReadOnlyState = false;
	/** `UsesCheckout()` was readable, i.e. a provider exists and the module answered. */
	bool bCapabilitiesRead = false;

	/**
	 * The ONE refusal shared by every asset, taken verbatim from the engine's own gate.
	 *
	 * Empty means a provider was asked. Non-empty means nothing was asked of any provider.
	 */
	FString RunRefusal;

	/** The source of the refusal, so the string cannot be mistaken for this tool's own wording. */
	FString RunRefusalSource;

	/**
	 * `ISourceControlModule::GetProviderNames()`, copied out.
	 *
	 * Printed so that the provider name above is read from the registry the engine itself uses. It is
	 * also the guard the fatal-`SetProvider` hazard needs: `SourceControlModule.cpp:404-417` terminates
	 * the process on an unmatched name, while `CreateProvider` (`:384-397`) returns null. This tool never
	 * calls either - it uses the provider the project selected - so the list is reported, not used to set.
	 */
	TArray<FString> RegisteredProviders;

	TArray<FTurboMCPSourceControlAsset> Assets;
};

class FTurboMCPSourceControl
{
public:
	/**
	 * Works out everything: the module facts once, then one row per asset.
	 *
	 * This is the ONLY place a decision is made, which is what makes a Preview and its Edit twin
	 * incapable of disagreeing.
	 */
	static FTurboMCPSourceControlPlan Plan(const TArray<FString>& InAssets);

	/**
	 * Renders a plan. Writes nothing and asks no provider anything.
	 *
	 * `bShowCheckoutIntent` adds the section a Preview needs: what a checkout WOULD do per asset, read
	 * off the same plan fields `Checkout` acts on.
	 */
	static FString Render(const FTurboMCPSourceControlPlan& InPlan, bool bShowCheckoutIntent);

	/**
	 * Renders the plan and then attempts the checkout for every asset the plan allows.
	 *
	 * *** THE ONLY FUNCTION IN THIS FILE THAT CAN REACH A PROVIDER WITH A WRITE-INTENT OPERATION. ***
	 * It refuses every asset whose plan carries a `Refusal`, so its refusals are the Preview's refusals.
	 */
	static FString Checkout(const FTurboMCPSourceControlPlan& InPlan);
};
