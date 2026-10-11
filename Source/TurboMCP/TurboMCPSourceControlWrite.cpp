// Copyright (c) 2026 Florian ESCHALIER (FloStyle)
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "TurboMCPSourceControlWrite.h"

#include "ISourceControlModule.h"
#include "ISourceControlOperation.h"
#include "ISourceControlProvider.h"
#include "ISourceControlState.h"
#include "Modules/ModuleManager.h"
#include "SourceControlOperations.h"

#include "TurboMCPSourceControl.h"

// NOTE ON COMMENTS IN THIS FILE: no comment line here puts a backtick and a LATER apostrophe on the SAME
// line, because UHT reads that pair as the start of something it never closes and warnings are errors here.
// The header takes the stronger form and uses no backticks at all; this file uses them for identifiers.

namespace
{
	/** true / false as text, so no ternary of two TEXT() literals sits in a variadic argument. */
	const TCHAR* BoolText(const bool bValue)
	{
		return bValue ? TEXT("true") : TEXT("false");
	}

	/** ATTEMPT / DO NOT ATTEMPT as text, for the same reason as BoolText. */
	const TCHAR* DecisionWord(const bool bWillAttempt)
	{
		return bWillAttempt ? TEXT("ATTEMPT") : TEXT("DO NOT ATTEMPT");
	}

	/**
	 * THE POLICY, NAMED IN EVERY ANSWER. Option (b) of the three the brief names.
	 *
	 * It is a pair of constants rather than an enum with unused values because an unreachable switch case
	 * is error C4702 in this build, and because the two rejected options belong in the answer a caller
	 * reads rather than in code that can never run. See the header for the full argument.
	 */
	const TCHAR* const PolicyName = TEXT("AttemptAndReport");
	const TCHAR* const PolicyStatement = TEXT(
		"(b) of the three options: a checkout IS issued through the provider and the provider's own "
		"answer is repeated verbatim. (a) skip-and-say and (c) refuse-unless-opted-in were both "
		"rejected. THIS ANSWER NEVER GATES THE WRITE: it is a report beside the write's own result, so "
		"a provider that cannot take a lock cannot turn a working write into a failed one (D-57).");

	/** The one line that says D-57 does not answer this question, printed with every answer. */
	const TCHAR* const PolicyProvenance = TEXT(
		"D-57 (Spec/05_DECISIONS.md:382-388) answers the ORDERING only - E-VCS-CHECKOUT is built and "
		"tested before any write tool calls it - and it does NOT answer what a write tool does when "
		"UsesCheckout() == false. That choice is row 10.2's own, and it is (b).");

	/** One label : value line. Four spaces of indent, so no line here is one of the writer's own rows. */
	void AddField(TArray<FString>& Out, const TCHAR* InLabel, const FString& InValue)
	{
		Out.Add(FString::Printf(TEXT("    %-20s: %s"), InLabel, *InValue));
	}

	/** The same, for a measured boolean. */
	void AddFlag(TArray<FString>& Out, const TCHAR* InLabel, const bool bValue)
	{
		AddField(Out, InLabel, BoolText(bValue));
	}

	/**
	 * The module and policy facts, shared by the Preview and the acting path so they cannot differ.
	 *
	 * bActing only changes the sentence about what the block is; every measured value is printed by this
	 * one function, which is what makes "the Preview and the Edit twin say the same thing" checkable.
	 */
	void AddHeaderLines(TArray<FString>& Out, const FTurboMCPWriteCheckoutPlan& InPlan,
		const FString& InWriteTool, const bool bActing)
	{
		Out.Add(FString::Printf(TEXT("=== automatic checkout before the write (D-57): %s ==="),
			*InWriteTool));
		Out.Add(TEXT("WHAT THIS IS: the source control half of this write tool. D-57 requires the"));
		Out.Add(TEXT("checkout tool to be built and tested before any write tool calls it, and it is."));
		Out.Add(TEXT("This is NOT USourceControlHelpers::CheckOutFile: that helper answers true on Git"));
		Out.Add(TEXT("without issuing anything (SourceControlHelpers.cpp:456-460). It reads"));
		Out.Add(TEXT("UsesCheckout() and the OPERATION's own result instead."));
		// THE TERNARY IS HOISTED INTO A NAMED POINTER ON PURPOSE. UE5's compile-time format checker
		// rejects a ternary of two TEXT() literals sitting inside a variadic argument (error C7595),
		// which this project has already hit once.
		const TCHAR* const WhatThisDoes = bActing
			? TEXT("the operation the plan allowed IS ISSUED here, and no file is written by this section")
			: TEXT("the operation the plan allows WOULD be issued, and none is issued here");
		Out.Add(FString::Printf(TEXT("what this call does: %s."), WhatThisDoes));
		Out.Add(TEXT(""));

		// PRINTF RATHER THAN operator+ ON PURPOSE: the concatenation form of these two lines was not worth
		// a build to discover whether a TCHAR literal can be the left operand of FString addition here.
		AddField(Out, TEXT("policy"), FString::Printf(TEXT("%s - %s"), PolicyName, PolicyStatement));
		AddField(Out, TEXT("policy, provenance"), PolicyProvenance);
		Out.Add(TEXT(""));

		AddFlag(Out, TEXT("module loaded"), InPlan.bModuleLoaded);
		AddField(Out, TEXT("provider in use"), FString::Printf(TEXT("'%s'"), *InPlan.ProviderName));
		FString RegisteredText = FString::Join(InPlan.RegisteredProviders, TEXT(", "));
		if (RegisteredText.IsEmpty())
		{
			RegisteredText = TEXT("(GetProviderNames listed none)");
		}
		AddField(Out, TEXT("registered providers"),
			FString::Printf(TEXT("%d -> %s"), InPlan.RegisteredProviders.Num(), *RegisteredText));
		AddFlag(Out, TEXT("IsEnabled()"), InPlan.bEnabled);
		if (InPlan.bAvailabilityAsked)
		{
			AddFlag(Out, TEXT("IsAvailable()"), InPlan.bAvailable);
		}
		else
		{
			// P1 IN ONE LINE: the second question was NOT asked, and saying so beats printing false,
			// which would read as a measured "the server is down".
			AddField(Out, TEXT("IsAvailable()"), TEXT("NOT ASKED, because IsEnabled() was already false."));
		}
		if (InPlan.bCapabilitiesRead)
		{
			AddFlag(Out, TEXT("UsesCheckout()"), InPlan.bUsesCheckout);
			AddFlag(Out, TEXT("UsesLocalReadOnly()"), InPlan.bUsesLocalReadOnlyState);
		}
		else
		{
			AddField(Out, TEXT("UsesCheckout()"),
				TEXT("NOT READ, because no provider was selected."));
		}

		Out.Add(TEXT(""));
		if (!InPlan.RunRefusal.IsEmpty())
		{
			AddField(Out, TEXT("run refusal"), InPlan.RunRefusal);
			AddField(Out, TEXT("the string is from"), InPlan.RunRefusalSource);
			Out.Add(TEXT("    NO PROVIDER WAS REACHED, so no state is reported and no zero is printed"));
			Out.Add(TEXT("    (P1), and the write itself is NOT gated by this line."));
		}
		else
		{
			AddField(Out, TEXT("run refusal"),
				TEXT("(none) - both engine questions passed, so each asset below was asked for its state."));
		}
	}

	/** The reason a row will not be attempted, or empty when it will be. Set by Plan, read by both paths. */
	FString NotAttemptedReason(const FTurboMCPWriteCheckoutPlan& InPlan,
		const FTurboMCPWriteCheckoutAsset& InRow)
	{
		if (!InRow.Refusal.IsEmpty())
		{
			return InRow.Refusal;
		}
		if (InRow.bStateUnknown)
		{
			return FString::Printf(
				TEXT("the provider returned NO STATE for this file even though it reports itself enabled ")
				TEXT("and available - an absent measurement, not a measured absence (%s)"),
				*InRow.StateUnknownReason);
		}
		if (!InRow.bStateRead)
		{
			return TEXT("no state was readable for this file, so there is nothing for a checkout to act on");
		}
		if (InPlan.bUsesCheckout && (InRow.bCheckedOut || InRow.bAdded))
		{
			return TEXT("the provider's state says this file is ALREADY checked out here (or open for ")
				TEXT("add), which is the condition the engine's own CheckOutFile returns true on ")
				TEXT("(SourceControlHelpers.cpp:456-460). This tool reads the STATE and says so; it does ")
				TEXT("not read that boolean.");
		}
		return FString();
	}

	/** One asset block, shared by both paths up to the point where the acting path adds an answer. */
	void AddAssetLines(TArray<FString>& Out, const FTurboMCPWriteCheckoutAsset& InRow,
		const int32 InIndex, const bool bActing)
	{
		Out.Add(FString::Printf(TEXT("    [%d] %s"), InIndex, *InRow.Requested));
		if (!InRow.PackagePath.IsEmpty())
		{
			Out.Add(FString::Printf(TEXT("        package: %s"), *InRow.PackagePath));
		}
		if (!InRow.File.IsEmpty())
		{
			Out.Add(FString::Printf(TEXT("        file: %s"), *InRow.File));
			Out.Add(FString::Printf(TEXT("        file found by: %s"), *InRow.FileRoute));
		}
		if (InRow.bStateRead)
		{
			Out.Add(FString::Printf(
				TEXT("        the state says: under source control %s, checked out here %s, ")
				TEXT("open for add %s, modified %s"),
				BoolText(InRow.bSourceControlled), BoolText(InRow.bCheckedOut),
				BoolText(InRow.bAdded), BoolText(InRow.bModified)));
		}

		if (InRow.bNoOpExpected)
		{
			Out.Add(TEXT("        UsesCheckout() == false: A CHECKOUT IS A NO-OP ON THIS PROVIDER. It has"));
			Out.Add(TEXT("        no lock command, so nothing here is a claim that a lock was taken. The"));
			Out.Add(TEXT("        operation is issued anyway and the provider's own answer is the record."));
		}

		Out.Add(FString::Printf(TEXT("        decision: %s - %s"),
			DecisionWord(InRow.bWillAttempt), *InRow.Intent));

		if (!InRow.bWillAttempt)
		{
			Out.Add(TEXT("        NOT ATTEMPTED, and no provider call was made for this asset."));
			Out.Add(TEXT(""));
			return;
		}

		if (!bActing)
		{
			Out.Add(TEXT("        THIS PREVIEW REACHED NO PROVIDER WITH A CHECKOUT. It read the"));
			Out.Add(TEXT("        capability flags and the state only, which is what makes the decision"));
			Out.Add(TEXT("        above the same one the write will act on."));
			Out.Add(TEXT(""));
			return;
		}

		Out.Add(FString::Printf(TEXT("        provider answer, verbatim: %s"), *InRow.ProviderAnswer));
		if (InRow.ProviderErrors.Num() == 0 && InRow.ProviderInfo.Num() == 0)
		{
			Out.Add(TEXT("        the provider attached NO message to the operation."));
		}
		for (const FString& Message : InRow.ProviderErrors)
		{
			Out.Add(FString::Printf(TEXT("        provider error: %s"), *Message));
		}
		for (const FString& Message : InRow.ProviderInfo)
		{
			Out.Add(FString::Printf(TEXT("        provider info : %s"), *Message));
		}
		Out.Add(TEXT(""));
	}

	/** The closing statement, identical in both paths, so the contract is never only in the code. */
	void AddClosingLines(TArray<FString>& Out, const bool bActing)
	{
		Out.Add(TEXT("    THE CHECKOUT ANSWER NEVER GATES THE WRITE. It is reported beside the write's"));
		Out.Add(TEXT("    own result, so a provider that cannot take a lock cannot turn a working write"));
		Out.Add(TEXT("    into a failed one - which is the outcome D-57 exists to prevent."));
		if (bActing)
		{
			Out.Add(TEXT("    NO ASSET FILE WAS WRITTEN BY THIS SECTION. A checkout is a source control"));
			Out.Add(TEXT("    operation; the write tool's own result below or above is the file write."));
		}
	}
}

FTurboMCPWriteCheckoutPlan FTurboMCPSourceControlWrite::Plan(const TArray<FString>& InAssets)
{
	// =============================================================================================
	// THE MODULE FACTS, THE CAPABILITY FLAGS AND EVERY PER-ASSET STATE ARE WORKED OUT BY THE ROW 10.1
	// PLAN, WHICH IS THE ONE CODE PATH THIS PROJECT ALREADY HAS FOR EXACTLY THESE QUESTIONS.
	//
	// Reusing it rather than re-deriving it is the point: the two engine questions, the verbatim refusal
	// strings, the two-route file resolution and the state reading are all the same measurements
	// E-VCS-CHECKOUT reports, so a write tool and the checkout tool cannot answer differently about one
	// asset. What this function adds is the WRITE decision, which is the row's own.
	// =============================================================================================
	const FTurboMCPSourceControlPlan Base = FTurboMCPSourceControl::Plan(InAssets);

	FTurboMCPWriteCheckoutPlan Out;
	Out.bModuleLoaded = Base.bModuleLoaded;
	Out.bEnabled = Base.bEnabled;
	Out.bAvailable = Base.bAvailable;
	Out.bAvailabilityAsked = Base.bAvailabilityAsked;
	Out.ProviderName = Base.ProviderName;
	Out.bUsesCheckout = Base.bUsesCheckout;
	Out.bUsesLocalReadOnlyState = Base.bUsesLocalReadOnlyState;
	Out.bCapabilitiesRead = Base.bCapabilitiesRead;
	Out.RunRefusal = Base.RunRefusal;
	Out.RunRefusalSource = Base.RunRefusalSource;
	Out.RegisteredProviders = Base.RegisteredProviders;
	Out.PolicyName = PolicyName;
	Out.PolicyStatement = PolicyStatement;

	for (const FTurboMCPSourceControlAsset& Row : Base.Assets)
	{
		FTurboMCPWriteCheckoutAsset WriteRow;
		WriteRow.Requested = Row.Requested;
		WriteRow.PackagePath = Row.PackagePath;
		WriteRow.File = Row.File;
		WriteRow.FileRoute = Row.FileRoute;
		WriteRow.bResolved = Row.bResolved;
		WriteRow.Refusal = Row.Refusal;
		WriteRow.bStateRead = Row.bStateRead;
		WriteRow.bStateUnknown = Row.bStateUnknown;
		WriteRow.StateUnknownReason = Row.StateUnknownReason;
		WriteRow.bSourceControlled = Row.bSourceControlled;
		WriteRow.bCheckedOut = Row.bCheckedOut;
		WriteRow.bAdded = Row.bAdded;
		WriteRow.bModified = Row.bModified;

		// -----------------------------------------------------------------------------------------
		// THE DECISION, AND IT IS MADE HERE RATHER THAN IN EnsureCheckedOut, so the Preview and the
		// Edit cannot reach a different one. Four inputs, in this order:
		//   1. a row-level refusal   - nothing is asked of any provider;
		//   2. no state at all       - there is nothing for a checkout to act on;
		//   3. UsesCheckout() false  - the operation is STILL ISSUED, because an expected no-op is not
		//                              a measurement (row 10.1's rule), and the answer says so;
		//   4. already checked out   - nothing is issued, and the answer says which fact said so.
		// -----------------------------------------------------------------------------------------
		const FString Reason = NotAttemptedReason(Out, WriteRow);
		WriteRow.bWillAttempt = Reason.IsEmpty();
		WriteRow.bNoOpExpected = WriteRow.bWillAttempt && !Out.bUsesCheckout;

		if (!WriteRow.bWillAttempt)
		{
			WriteRow.Intent = Reason;
			// *** A DECISION WHOSE REASON IS EMPTY IS THE DEFECT `P1` NAMES, AND A RED-CHECK RUN FOUND IT
			// HERE. *** With the decision made twice (the flag once and the reason afterwards) the reason
			// could be dropped while the decision still printed, which left `decision: DO NOT ATTEMPT - `
			// with nothing after it - an absent explanation reading like a decided one. `NotAttemptedReason`
			// returns empty only when the row WOULD be attempted, so this cannot happen on the current
			// paths; the guard is what makes that a property of the code rather than of the paths.
			if (WriteRow.Intent.IsEmpty())
			{
				WriteRow.Intent = TEXT("refused, and the reason was not recorded - which is itself a defect");
			}
		}
		else if (WriteRow.bNoOpExpected)
		{
			WriteRow.Intent = TEXT("a checkout IS issued through the provider and its own answer is ")
				TEXT("repeated, even though this provider reports UsesCheckout() == false and the ")
				TEXT("operation is therefore expected to be a no-op here.");
		}
		else
		{
			WriteRow.Intent = TEXT("a checkout IS issued through the provider and its own answer is ")
				TEXT("repeated. That is the only thing that can take a lock, and it is the provider's ")
				TEXT("answer that decides whether one was taken.");
		}

		Out.Assets.Add(WriteRow);
	}

	return Out;
}

FString FTurboMCPSourceControlWrite::Render(const FTurboMCPWriteCheckoutPlan& InPlan,
	const FString& InWriteTool)
{
	TArray<FString> Lines;
	AddHeaderLines(Lines, InPlan, InWriteTool, /*bActing=*/false);
	Lines.Add(FString::Printf(TEXT("assets: %d"), InPlan.Assets.Num()));
	Lines.Add(TEXT(""));

	if (InPlan.Assets.Num() == 0)
	{
		Lines.Add(TEXT("NO ASSET WAS NAMED, so there is nothing to check out and nothing was asked of"));
		Lines.Add(TEXT("any provider. A write tool always names at least one."));
	}
	else
	{
		int32 Index = 0;
		for (const FTurboMCPWriteCheckoutAsset& Row : InPlan.Assets)
		{
			AddAssetLines(Lines, Row, ++Index, /*bActing=*/false);
		}
	}

	AddClosingLines(Lines, /*bActing=*/false);
	return FString::Join(Lines, TEXT("\n"));
}

FString FTurboMCPSourceControlWrite::EnsureCheckedOut(const FTurboMCPWriteCheckoutPlan& InPlan,
	const FString& InWriteTool)
{
	TArray<FString> Lines;
	AddHeaderLines(Lines, InPlan, InWriteTool, /*bActing=*/true);
	Lines.Add(FString::Printf(TEXT("assets: %d"), InPlan.Assets.Num()));
	Lines.Add(TEXT(""));

	if (InPlan.Assets.Num() == 0)
	{
		Lines.Add(TEXT("NO ASSET WAS NAMED, so there is nothing to check out and nothing was asked of"));
		Lines.Add(TEXT("any provider."));
		AddClosingLines(Lines, /*bActing=*/true);
		return FString::Join(Lines, TEXT("\n"));
	}

	// THE REFUSAL IS A DECISION TO MAKE NO CALL, NOT A FAILED CALL: with no usable provider the engine
	// own gate stops here, exactly as it does in E-VCS-CHECKOUT.
	if (!InPlan.RunRefusal.IsEmpty())
	{
		Lines.Add(TEXT("REFUSED FOR EVERY ASSET, AND NO PROVIDER WAS REACHED: this is the engine's own"));
		Lines.Add(TEXT("string and the same one the Preview printed, because both read it out of Plan."));
		int32 Index = 0;
		for (const FTurboMCPWriteCheckoutAsset& Row : InPlan.Assets)
		{
			AddAssetLines(Lines, Row, ++Index, /*bActing=*/true);
		}
		AddClosingLines(Lines, /*bActing=*/true);
		return FString::Join(Lines, TEXT("\n"));
	}

	ISourceControlModule* Module = FModuleManager::LoadModulePtr<ISourceControlModule>(TEXT("SourceControl"));
	if (Module == nullptr)
	{
		Lines.Add(TEXT("REFUSED: the SourceControl module could not be loaded, so there is no provider to"));
		Lines.Add(TEXT("ask. NOTHING WAS ATTEMPTED."));
		AddClosingLines(Lines, /*bActing=*/true);
		return FString::Join(Lines, TEXT("\n"));
	}
	ISourceControlProvider& Provider = Module->GetProvider();

	int32 Index = 0;
	for (const FTurboMCPWriteCheckoutAsset& Row : InPlan.Assets)
	{
		FTurboMCPWriteCheckoutAsset Result = Row;
		++Index;

		if (!Row.bWillAttempt)
		{
			AddAssetLines(Lines, Result, Index, /*bActing=*/true);
			continue;
		}

		// THE OPERATION IS CREATED AND KEPT, so its own result info can be read back after Execute. This
		// is the measured path: on Git the answer is Failed with the provider's own
		// "Operation 'CheckOut' not supported by revision control provider 'Git'", produced before any
		// command runs (GitSourceControlProvider.cpp:225-238).
		const TSharedRef<FCheckOut, ESPMode::ThreadSafe> Operation = ISourceControlOperation::Create<FCheckOut>();
		const ECommandResult::Type Execution = Provider.Execute(Operation, Row.File);

		Result.bAttempted = true;
		Result.bSucceeded = (Execution == ECommandResult::Succeeded);

		FString ResultWord = TEXT("Failed");
		if (Execution == ECommandResult::Succeeded)
		{
			ResultWord = TEXT("Succeeded");
		}
		else if (Execution == ECommandResult::Cancelled)
		{
			ResultWord = TEXT("Cancelled");
		}
		Result.ProviderAnswer = FString::Printf(TEXT("ECommandResult::%s"), *ResultWord);

		const FSourceControlResultInfo& ResultInfo = Operation->GetResultInfo();
		for (const FText& Message : ResultInfo.ErrorMessages)
		{
			Result.ProviderErrors.Add(Message.ToString());
		}
		for (const FText& Message : ResultInfo.InfoMessages)
		{
			Result.ProviderInfo.Add(Message.ToString());
		}

		AddAssetLines(Lines, Result, Index, /*bActing=*/true);
	}

	AddClosingLines(Lines, /*bActing=*/true);
	return FString::Join(Lines, TEXT("\n"));
}
