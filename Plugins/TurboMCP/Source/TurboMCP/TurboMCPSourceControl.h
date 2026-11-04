// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#include "Containers/Array.h"
#include "Containers/UnrealString.h"

/** Provider state for asset checkout planning.
 * Planning refreshes state only after checking provider availability. Missing state
 * remains unknown. Checkout reports provider responses, not proof of an exclusive lock.
 * Read/preview/edit wrappers each create a fresh plan; results are point-in-time observations.
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
	 * Read, preview and edit each refresh the plan. Provider state may change between calls.
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
