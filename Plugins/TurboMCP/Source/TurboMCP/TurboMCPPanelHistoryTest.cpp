// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"

#include "Framework/Application/SlateApplication.h"
// The panel owns FTurboMCPPanelToolRunner by value, and that runner owns
// TStrongObjectPtr<UToolCallAsyncResultString>. The defining UCLASS header must
// be complete before the panel header is instantiated; the forward declaration
// in TurboMCPPanelToolRunner.h is intentionally not enough for TStrongObjectPtr.
#include "ToolsetRegistry/ToolCallAsyncResultString.h"
#include "Panel/STurboMCPPanel.h"
#include "Templates/UniquePtr.h"
#include "TurboMCPCallLog.h"
#include "TurboMCPCallTimeline.h"

struct FTurboMCPPanelHistoryTestAccess
{
	static bool Refresh(STurboMCPPanel& Panel)
	{
		return Panel.RefreshTimelineIfChanged();
	}

	static const TArray<TSharedPtr<FTurboMCPCallTimeline::FListEntry>>& Rows(const STurboMCPPanel& Panel)
	{
		return Panel.TimelineRows;
	}

	static FText Summary(const STurboMCPPanel& Panel)
	{
		return Panel.GetCallLogSummary();
	}

	static uint64 Rebuilds(const STurboMCPPanel& Panel)
	{
		return Panel.TimelineRebuildCount;
	}

	static uint64 RenderedRevision(const STurboMCPPanel& Panel)
	{
		return Panel.LastTimelineRevision;
	}
};

namespace
{
	int32 CallRows(const STurboMCPPanel& Panel)
	{
		int32 Count = 0;
		for (const TSharedPtr<FTurboMCPCallTimeline::FListEntry>& Row : FTurboMCPPanelHistoryTestAccess::Rows(Panel))
		{
			if (Row.IsValid() && !Row->bHeading)
			{
				++Count;
			}
		}
		return Count;
	}

	const FTurboMCPCallTimeline::FListEntry* LastCallRow(
		FAutomationTestBase& Test, const STurboMCPPanel& Panel, const TCHAR* Context)
	{
		const TArray<TSharedPtr<FTurboMCPCallTimeline::FListEntry>>& Rows =
			FTurboMCPPanelHistoryTestAccess::Rows(Panel);
		if (Rows.Num() == 0)
		{
			Test.AddError(FString::Printf(TEXT("%s: the panel history has no rows"), Context));
			return nullptr;
		}
		const TSharedPtr<FTurboMCPCallTimeline::FListEntry>& Row = Rows.Last();
		if (!Row.IsValid())
		{
			Test.AddError(FString::Printf(TEXT("%s: the last history row is invalid"), Context));
			return nullptr;
		}
		if (Row->bHeading)
		{
			Test.AddError(FString::Printf(TEXT("%s: the last history row is a heading, not a call"), Context));
			return nullptr;
		}
		return Row.Get();
	}

	void AddCall(FTurboMCPCallLog& Log, const TCHAR* Tool, const TCHAR* Phase,
		const TCHAR* Source, int32 ResultChars, int32 TokenEstimate)
	{
		FTurboMCPCallLog::FEntry Entry;
		Entry.Timestamp = TEXT("2026-10-18T00:00:00.000Z");
		Entry.Phase = Phase;
		Entry.Source = Source;
		Entry.Toolset = TEXT("SyntheticToolset");
		Entry.Tool = Tool;
		Entry.ResultChars = ResultChars;
		Entry.TokenEstimate = TokenEstimate;
		Log.Write(Entry);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTurboMCPPanelHistoryTest,
	"TurboMCP.Panel.History",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTurboMCPPanelHistoryTest::RunTest(const FString& Parameters)
{
	if (!FSlateApplication::IsInitialized())
	{
		AddError(TEXT("Slate is not initialized; the panel history fixture cannot run"));
		return false;
	}

	TUniquePtr<FTurboMCPCallLog> Log(FTurboMCPCallLog::CreateForTest(true));
	if (!Log.IsValid())
	{
		AddError(TEXT("the isolated test log could not be created"));
		return false;
	}

	for (int32 Index = 0; Index < 1000; ++Index)
	{
		AddCall(*Log, *FString::Printf(TEXT("tool-%d"), Index), TEXT("end"), TEXT("plugin"), 0, 0);
	}
	const uint64 SeededRevision = Log->GetRevision();
	TestEqual(TEXT("the isolated log has one revision per seeded write"), SeededRevision, uint64(1000));
	const FTurboMCPCallLog::FSnapshot Seeded = Log->GetSnapshot();
	TestEqual(TEXT("the snapshot entries match the isolated log"), Seeded.Entries.Num(), Log->NumEntries());
	TestEqual(TEXT("the snapshot revision matches the log revision"), Seeded.Revision, SeededRevision);

	TSharedRef<STurboMCPPanel> Panel = SNew(STurboMCPPanel).TestCallLog(Log.Get());
	TestEqual(TEXT("the panel bound the isolated log before its initial read"), CallRows(*Panel), 1000);

	// At the cap a new write changes the revision but not the entry count.
	AddCall(*Log, TEXT("tool-newest"), TEXT("end"), TEXT("plugin"), 4, 1);
	TestEqual(TEXT("the log stays capped"), Log->NumEntries(), 1000);
	TestTrue(TEXT("the revision moves at the cap"), Log->GetRevision() > SeededRevision);
	TestTrue(TEXT("the panel refreshes at the cap"), FTurboMCPPanelHistoryTestAccess::Refresh(*Panel));
	TestEqual(TEXT("the panel rendered the new revision"), FTurboMCPPanelHistoryTestAccess::RenderedRevision(*Panel), Log->GetRevision());
	TestEqual(TEXT("the capped list still holds one call per record"), CallRows(*Panel), 1000);
	if (const FTurboMCPCallTimeline::FListEntry* Newest = LastCallRow(*this, *Panel, TEXT("capped history")))
	{
		TestEqual(TEXT("the newest capped row is visible"), Newest->Call.Tool, FString(TEXT("tool-newest")));
	}

	// An unchanged revision must not rebuild.
	const uint64 CapRebuilds = FTurboMCPPanelHistoryTestAccess::Rebuilds(*Panel);
	TestFalse(TEXT("an unchanged revision does not refresh"), FTurboMCPPanelHistoryTestAccess::Refresh(*Panel));
	TestEqual(TEXT("an unchanged revision does not rebuild"), FTurboMCPPanelHistoryTestAccess::Rebuilds(*Panel), CapRebuilds);

	// Clear and refill to the same count; a count-only signal would miss this.
	const uint64 BeforeClear = Log->GetRevision();
	Log->Clear();
	TestTrue(TEXT("Clear advances the revision"), Log->GetRevision() > BeforeClear);
	for (int32 Index = 0; Index < 1000; ++Index)
	{
		AddCall(*Log, *FString::Printf(TEXT("refill-%d"), Index), TEXT("end"), TEXT("plugin"), 0, 0);
	}
	TestEqual(TEXT("the refill returns to the same entry count"), Log->NumEntries(), 1000);
	TestTrue(TEXT("a clear/refill revision change refreshes"), FTurboMCPPanelHistoryTestAccess::Refresh(*Panel));
	if (const FTurboMCPCallTimeline::FListEntry* Refilled = LastCallRow(*this, *Panel, TEXT("refilled history")))
	{
		TestEqual(TEXT("the refill rows are the new calls"), Refilled->Call.Tool, FString(TEXT("refill-999")));
	}

	// Measured zero, unknown sizes and completion-not-observed are distinct.
	Log->Clear();
	AddCall(*Log, TEXT("measured-zero"), TEXT("end"), TEXT("plugin"), 0, 0);
	AddCall(*Log, TEXT("unknown"), TEXT("end"), TEXT("plugin"), FTurboMCPCallLog::FEntry::NotMeasured,
		FTurboMCPCallLog::FEntry::NotMeasured);
	AddCall(*Log, TEXT("engine-only"), TEXT("end"), TEXT("engine"), FTurboMCPCallLog::FEntry::NotMeasured,
		FTurboMCPCallLog::FEntry::NotMeasured);
	AddCall(*Log, TEXT("unfinished"), TEXT("start"), TEXT("plugin"), FTurboMCPCallLog::FEntry::NotMeasured,
		FTurboMCPCallLog::FEntry::NotMeasured);
	TestTrue(TEXT("the summary refreshes for the state fixture"), FTurboMCPPanelHistoryTestAccess::Refresh(*Panel));
	const FText Summary = FTurboMCPPanelHistoryTestAccess::Summary(*Panel);
	TestTrue(TEXT("measured zero has its own count"), Summary.ToString().Contains(TEXT("1 measured zero")));
	TestTrue(TEXT("measured zero is not counted as known characters"), Summary.ToString().Contains(TEXT("0 chars known")));
	TestTrue(TEXT("unknown character counts are stated"), Summary.ToString().Contains(TEXT("2 chars unknown")));
	TestTrue(TEXT("token estimates are labelled"), Summary.ToString().Contains(TEXT("~0 tokens estimated")));
	TestTrue(TEXT("unknown tokens are stated"), Summary.ToString().Contains(TEXT("2 tokens unknown")));
	TestTrue(TEXT("an unmatched start is completion not observed"),
		Summary.ToString().Contains(TEXT("1 completion not observed")));
	TestFalse(TEXT("an unmatched start is not reported as running"),
		Summary.ToString().Contains(TEXT("running")));
	if (const FTurboMCPCallTimeline::FListEntry* Unfinished = LastCallRow(*this, *Panel, TEXT("unfinished history")))
	{
		TestTrue(TEXT("the unfinished row is rendered honestly"),
			FTurboMCPCallTimeline::RenderRow(Unfinished->Call).Contains(TEXT("completion not observed")));
	}

	// A disabled write changes neither entries nor revision.
	Log->SetEnabled(false);
	const uint64 DisabledRevision = Log->GetRevision();
	const int32 DisabledEntries = Log->NumEntries();
	AddCall(*Log, TEXT("disabled"), TEXT("end"), TEXT("plugin"), 1, 1);
	TestEqual(TEXT("a disabled write does not change the revision"), Log->GetRevision(), DisabledRevision);
	TestEqual(TEXT("a disabled write does not change the entries"), Log->NumEntries(), DisabledEntries);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
