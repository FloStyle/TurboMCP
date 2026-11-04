// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "Tools/TurboMCPToolsets.h"

#include "TurboMCPAssetResolver.h"
#include "TurboMCPSourceControl.h"

// Planning refreshes provider state; only the Edit wrapper requests checkout.
// Preview and Edit plan separately, so provider state may change between calls.
FString UTurboMCPReadToolset::ReadTurboMCPVcsState(const FString& Assets)
{
	const FTurboMCPSourceControlPlan Plan =
		FTurboMCPSourceControl::Plan(FTurboMCPAssetResolver::SplitList(Assets));
	return FTurboMCPSourceControl::Render(Plan, false);
}

FString UTurboMCPPreviewToolset::PreviewTurboMCPVcsCheckout(const FString& Assets)
{
	const FTurboMCPSourceControlPlan Plan =
		FTurboMCPSourceControl::Plan(FTurboMCPAssetResolver::SplitList(Assets));
	return FTurboMCPSourceControl::Render(Plan, true);
}

FString UTurboMCPEditToolset::EditTurboMCPVcsCheckout(const FString& Assets)
{
	const FTurboMCPSourceControlPlan Plan =
		FTurboMCPSourceControl::Plan(FTurboMCPAssetResolver::SplitList(Assets));
	return FTurboMCPSourceControl::Checkout(Plan);
}

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "ISourceControlModule.h"
#include "ISourceControlProvider.h"

// Exercise provider selection inside the owned editor; never launch a competing editor.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTurboMCPVcsQualityTest,
    "TurboMCP.SourceControl.Quality", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTurboMCPVcsQualityTest::RunTest(const FString& Parameters)
{
    ISourceControlModule& Module = ISourceControlModule::Get();
    const FName Original = Module.GetProvider().GetName();
    ON_SCOPE_EXIT { Module.SetProvider(Original); };
    TArray<FName> Names; Module.GetProviderNames(Names);
    if (!TestTrue(TEXT("Disabled provider registered"), Names.Contains(FName(TEXT("None"))))) return false;
    Module.SetProvider(TEXT("None"));
    const FString Asset = TEXT("/Game/Tests/BP_NodeTest");
    const FString Read = UTurboMCPReadToolset::ReadTurboMCPVcsState(Asset);
    const FString Preview = UTurboMCPPreviewToolset::PreviewTurboMCPVcsCheckout(Asset);
    const FString Edit = UTurboMCPEditToolset::EditTurboMCPVcsCheckout(Asset);
    AddInfo(Read); AddInfo(Preview); AddInfo(Edit);
    TestTrue(TEXT("Read explains disabled provider"), Read.Contains(TEXT("Revision control is not enabled.")));
    TestTrue(TEXT("Preview refuses unavailable checkout"), Preview.Contains(TEXT("Revision control is not enabled.")));
    TestTrue(TEXT("Edit attempts no checkout"), Edit.Contains(TEXT("No checkout was attempted")));
    TestTrue(TEXT("Empty edit is explicit"), UTurboMCPEditToolset::EditTurboMCPVcsCheckout(TEXT("")).Contains(TEXT("No checkout was attempted")));
    TestTrue(TEXT("Unknown asset identified"), UTurboMCPReadToolset::ReadTurboMCPVcsState(TEXT("/Game/NoSuchVcsQualityAsset")).Contains(TEXT("REFUSED")));
    // Unknown state must never be rendered as a measured false source-control flag.
    FTurboMCPSourceControlPlan Unknown;
    FTurboMCPSourceControlAsset Row; Row.Requested = Asset; Row.bStateUnknown = true;
    Row.StateUnknownReason = TEXT("Fixture provider returned no state"); Unknown.Assets.Add(Row);
    const FString UnknownText = FTurboMCPSourceControl::Render(Unknown, true);
    TestTrue(TEXT("Unknown state explicit"), UnknownText.Contains(TEXT("STATE: unknown")));
    TestFalse(TEXT("Unknown state is not untracked"), UnknownText.Contains(TEXT("under source control:")));
    if (!TestTrue(TEXT("Git provider registered"), Names.Contains(FName(TEXT("Git"))))) return false;
    Module.SetProvider(TEXT("Git"));
    Module.GetProvider().Init(false);
    TestEqual(TEXT("Actual provider selected"), Module.GetProvider().GetName(), FName(TEXT("Git")));
    const FTurboMCPSourceControlPlan Git = FTurboMCPSourceControl::Plan({Asset});
    AddInfo(FTurboMCPSourceControl::Render(Git, true));
    TestTrue(TEXT("Git state available in test repository"), Git.bEnabled && Git.bAvailable);
    TestFalse(TEXT("Git has no checkout capability"), Git.bUsesCheckout);
    // A provider without checkout support can reject an operation. Do not equate this with a lock.
    AddExpectedError(TEXT("SourceControl: Operation 'CheckOut' not supported by revision control provider 'Git'"), EAutomationExpectedErrorFlags::Contains, 1);
    const FString GitEdit = FTurboMCPSourceControl::Checkout(Git); AddInfo(GitEdit);
    TestTrue(TEXT("Provider operation result reported"), GitEdit.Contains(TEXT("provider answer, verbatim: ECommandResult::")));
    TestTrue(TEXT("Unsupported checkout is a failed result"), GitEdit.Contains(TEXT("ECommandResult::Failed")));
    TestTrue(TEXT("No exclusive lock claim"), GitEdit.Contains(TEXT("not proof of a lock")));
    Module.SetProvider(Original);
    TestEqual(TEXT("Original provider restored"), Module.GetProvider().GetName(), Original);
    AddInfo(TEXT("Live Perforce/Plastic/SVN checkout and exclusive-lock behavior are not covered by this local Git workflow."));
    return true;
}
#endif
