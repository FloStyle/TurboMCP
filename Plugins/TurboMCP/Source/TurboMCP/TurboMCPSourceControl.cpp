// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "TurboMCPSourceControl.h"

#include "HAL/FileManager.h"
#include "ISourceControlModule.h"
#include "ISourceControlOperation.h"
#include "ISourceControlProvider.h"
#include "ISourceControlState.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "SourceControlOperations.h"

#include "TurboMCPAssetResolver.h"

namespace
{
	// Match the engine readiness messages; check enabled before availability.
	const TCHAR* const NotEnabledText = TEXT("Revision control is not enabled.");
	const TCHAR* const NotAvailableText = TEXT("Revision control server is currently not available.");

	/** `true` / `false` as text, so no ternary of two `TEXT()` literals sits in a variadic argument. */
	const TCHAR* BoolText(const bool bValue)
	{
		return bValue ? TEXT("true") : TEXT("false");
	}

	// Resolve saved package files through mount points, with a checked /Game fallback.
	FString FileForPackagePath(const FString& InPackagePath, FString& OutRoute)
	{
		OutRoute.Reset();

		// ROUTE 1: the engine's own answer, resolved against the mount points rather than the CWD.
		FString EngineAnswer;
		if (FPackageName::DoesPackageExist(InPackagePath, &EngineAnswer))
		{
			const FString Full = FPaths::ConvertRelativePathToFull(EngineAnswer);
			if (IFileManager::Get().FileExists(*Full))
			{
				OutRoute = TEXT("FPackageName::DoesPackageExist (PackageName.h:450)");
				return Full;
			}
		}

		// ROUTE 2: built by hand for /Game, exactly as the two commandlets above do it.
		const FString GameRoot = TEXT("/Game");
		if (InPackagePath.StartsWith(GameRoot + TEXT("/")))
		{
			const FString Full = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir())
				+ InPackagePath.RightChop(GameRoot.Len())
				+ FPackageName::GetAssetPackageExtension();
			if (IFileManager::Get().FileExists(*Full))
			{
				OutRoute = TEXT("hand-built /Game path under FPaths::ProjectContentDir()");
				return Full;
			}
		}

		return FString();
	}

	/** One `IsX(): true` line, so a reader compares values rather than parsing prose. */
	void AddFlag(TArray<FString>& Out, const TCHAR* InLabel, const bool bValue)
	{
		Out.Add(FString::Printf(TEXT("      %-22s %s"), InLabel, BoolText(bValue)));
	}

	/** The per-asset section, shared by every renderer so they cannot describe one row two ways. */
	void AddAssetLines(TArray<FString>& Out, const FTurboMCPSourceControlPlan& InPlan,
		const FTurboMCPSourceControlAsset& InRow, const int32 InIndex, const bool bShowCheckoutIntent)
	{
		Out.Add(FString::Printf(TEXT("  [%d] %s"), InIndex, *InRow.Requested));

		if (!InRow.PackagePath.IsEmpty())
		{
			Out.Add(FString::Printf(TEXT("      package: %s"), *InRow.PackagePath));
		}
		if (!InRow.File.IsEmpty())
		{
			Out.Add(FString::Printf(TEXT("      file: %s"), *InRow.File));
			Out.Add(FString::Printf(TEXT("      file found by: %s"), *InRow.FileRoute));
		}

		// -----------------------------------------------------------------------------------------
		// A REFUSAL IS PER ASSET AND IT IS THE ONLY THING SAID ABOUT IT. No state line, no zero, and
		// nothing that could be read as "not under source control" (P1).
		// -----------------------------------------------------------------------------------------
		if (!InRow.Refusal.IsEmpty())
		{
			Out.Add(FString::Printf(TEXT("      REFUSED: %s"), *InRow.Refusal));
			Out.Add(TEXT("      No usable state is available for this asset; the reason is shown above."));
			if (bShowCheckoutIntent)
			{
				Out.Add(TEXT("      Checkout is currently blocked by the reason above."));
				Out.Add(TEXT("      A later EditTurboMCPVcsCheckout call refreshes state again."));
			}
			Out.Add(TEXT(""));
			return;
		}

		if (InRow.bStateUnknown)
		{
			Out.Add(FString::Printf(TEXT("      STATE: unknown - %s"), *InRow.StateUnknownReason));
			Out.Add(TEXT("      Source-control status could not be determined. This does not mean the file is"));
			Out.Add(TEXT("      untracked. Refresh the provider state before deciding how to proceed."));
		}
		else if (InRow.bStateRead)
		{
			AddFlag(Out, TEXT("under source control:"), InRow.bSourceControlled);
			AddFlag(Out, TEXT("checked out here:"), InRow.bCheckedOut);
			AddFlag(Out, TEXT("added (open for add):"), InRow.bAdded);
			AddFlag(Out, TEXT("current (up to date):"), InRow.bCurrent);
			AddFlag(Out, TEXT("out of date:"), !InRow.bCurrent);
			AddFlag(Out, TEXT("modified:"), InRow.bModified);
			AddFlag(Out, TEXT("ignored:"), InRow.bIgnored);
			AddFlag(Out, TEXT("CanCheckout():"), InRow.bCanCheckout);
			Out.Add(FString::Printf(TEXT("      the provider's own state text: %s"), *InRow.StateDisplay));
		}

		if (bShowCheckoutIntent)
		{
			Out.Add(TEXT("      Checkout plan:"));
			if (!InPlan.bUsesCheckout)
			{
				Out.Add(FString::Printf(
					TEXT("        Provider '%s' reports UsesCheckout() == false."),
					*InPlan.ProviderName));
				Out.Add(TEXT("        Checkout is not supported as a provider capability; no lock is promised."));
				Out.Add(TEXT("        Edit still submits the operation and reports the provider response."));
				Out.Add(TEXT("        Preview has not submitted that operation."));
			}
			else if (InRow.bCheckedOut || InRow.bAdded)
			{
				Out.Add(TEXT("        No checkout is needed: the file is already checked out here"));
				Out.Add(TEXT("        or open for add."));
				Out.Add(TEXT("        The edit will leave this file unchanged."));
			}
			else if (!InRow.bStateRead)
			{
				Out.Add(TEXT("        nothing would be attempted: there is no state to act on."));
			}
			else if (!InRow.bCanCheckout)
			{
				Out.Add(TEXT("        The provider currently reports that this file cannot be checked out."));
				Out.Add(TEXT("        The edit will still request checkout and report the operation result;"));
				Out.Add(TEXT("        the preview does not guarantee success."));
			}
			else
			{
				Out.Add(TEXT("        an FCheckOut would be issued through the provider, and its own"));
				Out.Add(TEXT("        answer reported. Provider operations may change file or source-control state."));
			}
		}

		Out.Add(TEXT(""));
	}
}

FTurboMCPSourceControlPlan FTurboMCPSourceControl::Plan(const TArray<FString>& InAssets)
{
	FTurboMCPSourceControlPlan Plan;

	// =============================================================================================
	// STEP 1: THE MODULE FACTS, ASKED ONCE AND BEFORE ANYTHING ELSE.
	// =============================================================================================
	ISourceControlModule* Module = FModuleManager::LoadModulePtr<ISourceControlModule>(TEXT("SourceControl"));

	if (Module == nullptr)
	{
		Plan.RunRefusal = TEXT("the engine's SourceControl module could not be loaded, so no revision "
			"control provider exists to ask.");
		Plan.RunRefusalSource = TEXT("TurboMCP (no engine string exists for this case)");
	}
	else
	{
		Plan.bModuleLoaded = true;

		// THE REGISTRY'S OWN LIST, copied out. It is what proves the provider name below is the engine's
		// answer rather than a guess, and it is the list a caller would have to validate against before
		// SetProvider - which terminates the process on an unmatched name (SourceControlModule.cpp:404-417).
		TArray<FName> ProviderNames;
		Module->GetProviderNames(ProviderNames);
		for (const FName& Name : ProviderNames)
		{
			Plan.RegisteredProviders.Add(Name.ToString());
		}

		// *** QUESTION ONE: IS A PROVIDER SELECTED AT ALL? *** `SourceControlModule.cpp:376` is
		// `TemporarilyDisabledCount == 0 && GetProvider().IsEnabled()`, and the default provider answers
		// false (`DefaultSourceControlProvider.cpp:41-44`).
		Plan.bEnabled = Module->IsEnabled();
		Plan.ProviderName = Module->GetProvider().GetName().ToString();

		if (!Plan.bEnabled)
		{
			Plan.RunRefusal = NotEnabledText;
			Plan.RunRefusalSource = TEXT("SourceControlHelpers.cpp:67 (the engine's own gate)");
		}
		else
		{
			// *** QUESTION TWO: IS THAT PROVIDER USABLE? *** Perforce, Plastic and Subversion all answer
			// `IsEnabled() == true` unconditionally and `IsAvailable() == false` without a server, so this
			// second question is the one that separates "selected" from "working".
			Plan.bAvailabilityAsked = true;
			Plan.bAvailable = Module->GetProvider().IsAvailable();

			// The capability flags are PURE PREDICATES, not commands, so they are safe to read even when
			// the provider is unavailable - and they are what makes the refusal legible: the reason this
			// capability exists is that Perforce's UsesCheckout() is true (:622-625) while Git's is false
			// (:286-289).
			Plan.bCapabilitiesRead = true;
			Plan.bUsesCheckout = Module->GetProvider().UsesCheckout();
			Plan.bUsesLocalReadOnlyState = Module->GetProvider().UsesLocalReadOnlyState();

			if (!Plan.bAvailable)
			{
				Plan.RunRefusal = NotAvailableText;
				Plan.RunRefusalSource = TEXT("SourceControlHelpers.cpp:76 (the engine's own gate)");
			}
		}
	}

	// =============================================================================================
	// STEP 2: ONE ROW PER ASSET. THE REFUSAL IS WRITTEN INTO EVERY ROW, PER ASSET, WHICH IS WHAT
	// `T-VCS-1` MEANS BY "THE ANSWER SAYS SO PER ASSET".
	// =============================================================================================
	for (const FString& Requested : InAssets)
	{
		FTurboMCPSourceControlAsset Row;
		Row.Requested = Requested;

		FTurboMCPAssetResolver::FResolved Resolved;
		const FString ResolveError = FTurboMCPAssetResolver::Resolve(Requested, Resolved);
		if (!ResolveError.IsEmpty())
		{
			Row.Refusal = TEXT("the input did not resolve to one asset: ") + ResolveError;
			Plan.Assets.Add(Row);
			continue;
		}

		Row.bResolved = true;
		Row.PackagePath = Resolved.PackagePath;

		Row.File = FileForPackagePath(Resolved.PackagePath, Row.FileRoute);
		if (Row.File.IsEmpty())
		{
			Row.Refusal = FString::Printf(
				TEXT("'%s' resolved to package '%s', but NO FILE FOR IT WAS FOUND ON DISK - so there is "
					"no file for source control to have an opinion about"),
				*Requested, *Resolved.PackagePath);
			Plan.Assets.Add(Row);
			continue;
		}

		// THE REFUSAL, BEFORE ANY PROVIDER CALL. This is the whole rule: with no provider, a state cannot
		// be reported at all, so what is reported is the refusal and the asset is left unmeasured.
		if (!Plan.RunRefusal.IsEmpty())
		{
			Row.Refusal = Plan.RunRefusal;
			Plan.Assets.Add(Row);
			continue;
		}

		// =========================================================================================
		// ONLY NOW IS A PROVIDER ASKED, AND ONLY BECAUSE BOTH CHECKS PASSED.
		// =========================================================================================
		ISourceControlProvider& Provider = Module->GetProvider();
		const FSourceControlStatePtr State = Provider.GetState(Row.File, EStateCacheUsage::ForceUpdate);

		if (!State.IsValid())
		{
			// AN ABSENT STATE IS AN ABSENT MEASUREMENT. `DefaultSourceControlProvider.cpp:52-60` returns
			// Failed with a null state, and a reader that folded that into "not under source control"
			// would report a fact it never measured (P1).
			Row.bStateUnknown = true;
			Row.StateUnknownReason = FString::Printf(
				TEXT("provider '%s' returned NO STATE for this file even though it reports itself enabled "
					"and available - an absent measurement, not a measured absence"),
				*Plan.ProviderName);
			Plan.Assets.Add(Row);
			continue;
		}

		Row.bStateRead = true;
		Row.StateDisplay = State->GetDisplayName().ToString();
		Row.bSourceControlled = State->IsSourceControlled();
		Row.bCheckedOut = State->IsCheckedOut();
		Row.bAdded = State->IsAdded();
		Row.bCurrent = State->IsCurrent();
		Row.bModified = State->IsModified();
		Row.bIgnored = State->IsIgnored();
		Row.bCanCheckout = State->CanCheckout();

		Plan.Assets.Add(Row);
	}

	return Plan;
}

FString FTurboMCPSourceControl::Render(const FTurboMCPSourceControlPlan& InPlan, const bool bShowCheckoutIntent)
{
	TArray<FString> Lines;
	Lines.Add(TEXT("=== source control state ==="));
	Lines.Add(TEXT("Current provider state for each requested asset. If the provider is unavailable,"));
	Lines.Add(TEXT("each asset reports why its state could not be read."));
	Lines.Add(TEXT(""));

	// ---------------------------------------------------------------------------------------------
	// THE MODULE FACTS, ALWAYS PRINTED. A refusal that does not say which provider was asked, and
	// which providers exist, is a refusal a caller cannot act on.
	// ---------------------------------------------------------------------------------------------
	Lines.Add(FString::Printf(TEXT("module loaded        : %s"), BoolText(InPlan.bModuleLoaded)));
	Lines.Add(FString::Printf(TEXT("provider in use      : '%s'"), *InPlan.ProviderName));
	// THE LIST IS BUILT FIRST RATHER THAN INLINED AS A TERNARY. `UE5`'s compile-time format checker
	// rejects a ternary of two `TEXT()` literals inside a variadic argument, so the shape is avoided
	// rather than tested.
	FString RegisteredText = FString::Join(InPlan.RegisteredProviders, TEXT(", "));
	if (RegisteredText.IsEmpty())
	{
		RegisteredText = TEXT("(GetProviderNames listed none)");
	}
	Lines.Add(FString::Printf(TEXT("registered providers : %d -> %s"), InPlan.RegisteredProviders.Num(),
		*RegisteredText));
	Lines.Add(FString::Printf(TEXT("  IsEnabled()        : %s"), BoolText(InPlan.bEnabled)));
	if (InPlan.bAvailabilityAsked)
	{
		Lines.Add(FString::Printf(TEXT("  IsAvailable()      : %s"), BoolText(InPlan.bAvailable)));
	}
	else
	{
		// `P1` IN ONE LINE: the second question was NOT asked, and saying so beats printing `false`,
		// which would read as a measured "the server is down".
		Lines.Add(TEXT("  IsAvailable()      : NOT ASKED, because IsEnabled() was already false."));
	}
	if (InPlan.bCapabilitiesRead)
	{
		Lines.Add(FString::Printf(TEXT("  UsesCheckout()     : %s"), BoolText(InPlan.bUsesCheckout)));
		Lines.Add(FString::Printf(TEXT("  UsesLocalReadOnly(): %s"), BoolText(InPlan.bUsesLocalReadOnlyState)));
		if (!InPlan.bUsesCheckout)
		{
			Lines.Add(TEXT("  Checkout is not supported as a provider capability; no lock is promised."));
			Lines.Add(TEXT("    UsesCheckout() alone does not establish the outcome of an operation."));
			Lines.Add(TEXT("    Check per-file state and provider results."));
		}
	}
	else
	{
		Lines.Add(TEXT("  UsesCheckout()     : NOT READ, because no provider was selected."));
	}

	if (!InPlan.RunRefusal.IsEmpty())
	{
		Lines.Add(TEXT(""));
		Lines.Add(FString::Printf(TEXT("Provider unavailable: %s"), *InPlan.RunRefusal));
		Lines.Add(FString::Printf(TEXT("  Diagnostic source: %s"), *InPlan.RunRefusalSource));
		Lines.Add(TEXT("  Per-file state was not requested because provider readiness checks failed."));
	}
	else
	{
		Lines.Add(TEXT(""));
		Lines.Add(TEXT("The provider is enabled and available. Per-asset results below show whether state was obtained."));
	}

	Lines.Add(TEXT(""));
	Lines.Add(FString::Printf(TEXT("assets: %d"), InPlan.Assets.Num()));
	Lines.Add(TEXT(""));

	if (InPlan.Assets.Num() == 0)
	{
		Lines.Add(TEXT("No assets were supplied. The report above describes the selected provider."));
		Lines.Add(TEXT("Supply saved asset paths to inspect per-file state."));
	}
	else
	{
		int32 Index = 0;
		for (const FTurboMCPSourceControlAsset& Row : InPlan.Assets)
		{
			AddAssetLines(Lines, InPlan, Row, ++Index, bShowCheckoutIntent);
		}
	}

	return FString::Join(Lines, TEXT("\n"));
}

FString FTurboMCPSourceControl::Checkout(const FTurboMCPSourceControlPlan& InPlan)
{
	// The state half is rendered first and unchanged, so the Edit answer and the Preview answer can be
	// compared line for line up to the point where the Edit one acts.
	TArray<FString> Lines;
	Lines.Add(Render(InPlan, false));
	Lines.Add(TEXT("This state was measured before checkout. Refresh with ReadTurboMCPVcsState after reviewing per-file results."));
	Lines.Add(TEXT(""));
	Lines.Add(TEXT("=== checkout (EditTurboMCPVcsCheckout) ==="));

	if (InPlan.Assets.Num() == 0)
	{
		Lines.Add(TEXT("No assets were supplied. No checkout was attempted."));
		return FString::Join(Lines, TEXT("\n"));
	}

	if (!InPlan.RunRefusal.IsEmpty())
	{
		Lines.Add(FString::Printf(TEXT("REFUSED FOR EVERY ASSET: %s"), *InPlan.RunRefusal));
		Lines.Add(FString::Printf(TEXT("  Diagnostic source: %s"), *InPlan.RunRefusalSource));
		Lines.Add(TEXT("  No checkout was attempted; provider readiness checks failed."));
		Lines.Add(TEXT("  Configure or reconnect revision control, then refresh with ReadTurboMCPVcsState."));
		return FString::Join(Lines, TEXT("\n"));
	}

	ISourceControlModule* Module = FModuleManager::LoadModulePtr<ISourceControlModule>(TEXT("SourceControl"));
	if (Module == nullptr)
	{
		Lines.Add(TEXT("REFUSED: the SourceControl module could not be loaded, so there is no provider to ask."));
		return FString::Join(Lines, TEXT("\n"));
	}
	ISourceControlProvider& Provider = Module->GetProvider();

	int32 Index = 0;
	for (const FTurboMCPSourceControlAsset& Row : InPlan.Assets)
	{
		Lines.Add(FString::Printf(TEXT("  [%d] %s"), ++Index, *Row.Requested));

		if (!Row.Refusal.IsEmpty())
		{
			Lines.Add(FString::Printf(TEXT("      REFUSED: %s"), *Row.Refusal));
			Lines.Add(TEXT("      Checkout was not attempted for this asset."));
			Lines.Add(TEXT(""));
			continue;
		}

		if (!Row.bStateRead)
		{
			Lines.Add(TEXT("      NOT ATTEMPTED: no state was readable for this file, so there is nothing to"));
			Lines.Add(TEXT("      act on. An unreadable state is not a state that says 'check me out'."));
			Lines.Add(TEXT(""));
			continue;
		}

		// Capability flags do not prove an operation outcome or exclusive ownership.
		// Preserve the provider response even when checkout is unsupported.
		const bool bCheckoutUnsupported = !InPlan.bUsesCheckout;
		if (bCheckoutUnsupported)
		{
			Lines.Add(TEXT("      UsesCheckout() == false: checkout is not a supported provider capability."));
			Lines.Add(TEXT("      No lock is promised. The operation is submitted below;"));
			Lines.Add(TEXT("      its reported result does not independently verify exclusive ownership."));
			Lines.Add(TEXT("      Provider messages follow the result."));
		}
		else if (Row.bCheckedOut || Row.bAdded)
		{
			Lines.Add(TEXT("      Unchanged: this file is already checked out here or open for add."));
			Lines.Add(TEXT("      No checkout operation was issued."));
			Lines.Add(TEXT("      No compile or save was requested."));
			Lines.Add(TEXT(""));
			continue;
		}

		// THE OPERATION IS CREATED AND KEPT, so its own result info can be read back after Execute.
		const TSharedRef<FCheckOut, ESPMode::ThreadSafe> Operation = ISourceControlOperation::Create<FCheckOut>();
		const ECommandResult::Type Result = Provider.Execute(Operation, Row.File);

		FString ResultWord = TEXT("Failed");
		if (Result == ECommandResult::Succeeded)
		{
			ResultWord = TEXT("Succeeded");
		}
		else if (Result == ECommandResult::Cancelled)
		{
			ResultWord = TEXT("Cancelled");
		}

		Lines.Add(FString::Printf(TEXT("      provider answer, verbatim: ECommandResult::%s"), *ResultWord));
		if (bCheckoutUnsupported)
		{
			Lines.Add(TEXT("      This is the selected provider response, not proof of a lock."));
		}

		const FSourceControlResultInfo& ResultInfo = Operation->GetResultInfo();
		if (ResultInfo.ErrorMessages.Num() == 0 && ResultInfo.InfoMessages.Num() == 0)
		{
			Lines.Add(TEXT("      The provider supplied no additional message."));
		}
		for (const FText& Message : ResultInfo.ErrorMessages)
		{
			Lines.Add(FString::Printf(TEXT("      provider error: %s"), *Message.ToString()));
		}
		for (const FText& Message : ResultInfo.InfoMessages)
		{
			Lines.Add(FString::Printf(TEXT("      provider info : %s"), *Message.ToString()));
		}

		Lines.Add(TEXT("      No asset compile or save was requested; provider operations may change file state."));
		Lines.Add(TEXT("      Successful earlier operations are not rolled back if another asset fails."));
		Lines.Add(TEXT(""));
	}

	return FString::Join(Lines, TEXT("\n"));
}
