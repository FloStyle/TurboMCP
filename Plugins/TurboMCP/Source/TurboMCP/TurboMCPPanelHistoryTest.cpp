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

	static STurboMCPPanel::ETurboMCPView View(const STurboMCPPanel& Panel)
	{
		return Panel.ActiveView;
	}

	static void SetView(STurboMCPPanel& Panel, STurboMCPPanel::ETurboMCPView InView)
	{
		Panel.SetView(InView);
	}

	static FText InspectorHeader(const STurboMCPPanel& Panel)
	{
		return Panel.GetSelectedCallHeader();
	}

	static FText InspectorDetail(const STurboMCPPanel& Panel)
	{
		return Panel.GetSelectedCallDetail();
	}

	static void SetToolFilter(STurboMCPPanel& Panel, const FString& Tool)
	{
		Panel.TimelineToolFilter = Tool;
		Panel.RebuildTimelineRows();
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

	void AddDetailedCall(FTurboMCPCallLog& Log, const TCHAR* Tool)
	{
		FTurboMCPCallLog::FEntry Entry;
		Entry.Timestamp = TEXT("2026-10-18T00:00:05.000Z");
		Entry.Phase = TEXT("end");
		Entry.Source = TEXT("plugin");
		Entry.Toolset = TEXT("SyntheticToolset");
		Entry.Tool = Tool;
		Entry.Arguments = TEXT("{\"asset\":\"BP_NodeTest\"}");
		Entry.Result = TEXT("known result");
		Entry.ResultChars = 12;
		Entry.TokenEstimate = 3;
		Entry.DurationMs = 5.0;
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

	const bool bLogWasEnabled = Log->IsEnabled();
	TestTrue(TEXT("the default view is Monitor"),
		FTurboMCPPanelHistoryTestAccess::View(*Panel) == STurboMCPPanel::ETurboMCPView::Monitor);
	FTurboMCPPanelHistoryTestAccess::SetView(*Panel, STurboMCPPanel::ETurboMCPView::Tools);
	TestTrue(TEXT("the view switches to Tools"),
		FTurboMCPPanelHistoryTestAccess::View(*Panel) == STurboMCPPanel::ETurboMCPView::Tools);
	TestFalse(TEXT("switching views starts no call"), Panel->GetToolRunner().IsCallPending());
	TestFalse(TEXT("switching views leaves the call result unset"), Panel->GetToolRunner().GetResult().bIsComplete);
	TestTrue(TEXT("switching views does not change the recording setting"), Log->IsEnabled() == bLogWasEnabled);
	FTurboMCPPanelHistoryTestAccess::SetView(*Panel, STurboMCPPanel::ETurboMCPView::Monitor);
	TestTrue(TEXT("the view switches back to Monitor"),
		FTurboMCPPanelHistoryTestAccess::View(*Panel) == STurboMCPPanel::ETurboMCPView::Monitor);

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

	// The selected-call inspector reads actual fields and never executes. Selecting twice must leave
	// the runner and recording setting untouched.
	AddDetailedCall(*Log, TEXT("detailed"));
	TestTrue(TEXT("the detail fixture refreshes"), FTurboMCPPanelHistoryTestAccess::Refresh(*Panel));
	const bool bLogBeforeSelection = Log->IsEnabled();
	const bool bPendingBeforeSelection = Panel->GetToolRunner().IsCallPending();
	const bool bCompleteBeforeSelection = Panel->GetToolRunner().GetResult().bIsComplete;
	if (LastCallRow(*this, *Panel, TEXT("detailed history")) != nullptr)
	{
		const TSharedPtr<FTurboMCPCallTimeline::FListEntry> Selected =
			FTurboMCPPanelHistoryTestAccess::Rows(*Panel).Last();
		Panel->ToggleTimelineRow(Selected);
		TestTrue(TEXT("the detailed inspector shows ok"),
			FTurboMCPPanelHistoryTestAccess::InspectorHeader(*Panel).ToString().Contains(TEXT("ok")));
		const FString Detail = FTurboMCPPanelHistoryTestAccess::InspectorDetail(*Panel).ToString();
		TestTrue(TEXT("the inspector shows the actual request"), Detail.Contains(TEXT("{\"asset\":\"BP_NodeTest\"}")));
		TestTrue(TEXT("the inspector shows the actual result"), Detail.Contains(TEXT("known result")));
		TestTrue(TEXT("the inspector shows known characters"), Detail.Contains(TEXT("characters: 12")));
		TestTrue(TEXT("the inspector labels the token estimate"), Detail.Contains(TEXT("tokens (estimate): 3")));
	}
	for (const TSharedPtr<FTurboMCPCallTimeline::FListEntry>& Row : FTurboMCPPanelHistoryTestAccess::Rows(*Panel))
	{
		if (Row.IsValid() && !Row->bHeading && Row->Call.Tool == TEXT("unfinished"))
		{
			Panel->ToggleTimelineRow(Row);
			TestTrue(TEXT("an unfinished inspector header says completion not observed"),
				FTurboMCPPanelHistoryTestAccess::InspectorHeader(*Panel).ToString().Contains(TEXT("completion not observed")));
			TestTrue(TEXT("an unfinished inspector says the result is unavailable"),
				FTurboMCPPanelHistoryTestAccess::InspectorDetail(*Panel).ToString().Contains(
					TEXT("unavailable (completion not observed)")));
			break;
		}
	}
	for (const TSharedPtr<FTurboMCPCallTimeline::FListEntry>& Row : FTurboMCPPanelHistoryTestAccess::Rows(*Panel))
	{
		if (Row.IsValid() && !Row->bHeading && Row->Call.Tool == TEXT("engine-only"))
		{
			Panel->ToggleTimelineRow(Row);
			TestTrue(TEXT("an engine-only inspector says the request is unavailable"),
				FTurboMCPPanelHistoryTestAccess::InspectorDetail(*Panel).ToString().Contains(
					TEXT("request: unavailable (not recorded)")));
			TestTrue(TEXT("an engine-only inspector says the result is unavailable"),
				FTurboMCPPanelHistoryTestAccess::InspectorDetail(*Panel).ToString().Contains(
					TEXT("result: unavailable (not recorded)")));
			break;
		}
	}
	TestFalse(TEXT("selecting history starts no call"), Panel->GetToolRunner().IsCallPending());
	TestTrue(TEXT("selecting history leaves the call result unset"),
		Panel->GetToolRunner().GetResult().bIsComplete == bCompleteBeforeSelection);
	TestFalse(TEXT("selecting history leaves no call pending"), bPendingBeforeSelection);
	TestTrue(TEXT("selecting history does not change the recording setting"), Log->IsEnabled() == bLogBeforeSelection);

	// The selected call is pinned. An unrelated append must not silently switch the inspector, and a
	// filter, eviction or Clear must say that the pinned call is no longer in the current list.
	if (LastCallRow(*this, *Panel, TEXT("pinned history")) != nullptr)
	{
		Panel->ToggleTimelineRow(FTurboMCPPanelHistoryTestAccess::Rows(*Panel).Last());
		AddCall(*Log, TEXT("unrelated"), TEXT("end"), TEXT("plugin"), 0, 0);
		TestTrue(TEXT("the pinned inspector survives an unrelated append"),
			FTurboMCPPanelHistoryTestAccess::Refresh(*Panel));
		TestTrue(TEXT("the pinned inspector still shows the selected call"),
			FTurboMCPPanelHistoryTestAccess::InspectorDetail(*Panel).ToString().Contains(TEXT("known result")));
		TestFalse(TEXT("an exact match is not reported as missing"),
			FTurboMCPPanelHistoryTestAccess::InspectorHeader(*Panel).ToString().Contains(TEXT("not in the current")));

		FTurboMCPPanelHistoryTestAccess::SetToolFilter(*Panel, TEXT("unfinished"));
		TestTrue(TEXT("a filter that excludes the selected call says so"),
			FTurboMCPPanelHistoryTestAccess::InspectorHeader(*Panel).ToString().Contains(TEXT("not in the current")));
		TestTrue(TEXT("a filter does not replace the pinned detail"),
			FTurboMCPPanelHistoryTestAccess::InspectorDetail(*Panel).ToString().Contains(TEXT("known result")));
		FTurboMCPPanelHistoryTestAccess::SetToolFilter(*Panel, TEXT(""));
		TestFalse(TEXT("clearing the filter reattaches the exact selected call"),
			FTurboMCPPanelHistoryTestAccess::InspectorHeader(*Panel).ToString().Contains(TEXT("not in the current")));

		for (int32 Index = 0; Index < 1001; ++Index)
		{
			AddCall(*Log, *FString::Printf(TEXT("evict-%d"), Index), TEXT("end"), TEXT("plugin"), 0, 0);
		}
		TestTrue(TEXT("eviction of the selected call refreshes the panel"),
			FTurboMCPPanelHistoryTestAccess::Refresh(*Panel));
		TestTrue(TEXT("eviction says the pinned call is no longer in the list"),
			FTurboMCPPanelHistoryTestAccess::InspectorHeader(*Panel).ToString().Contains(TEXT("not in the current")));
		TestTrue(TEXT("eviction leaves the pinned detail intact"),
			FTurboMCPPanelHistoryTestAccess::InspectorDetail(*Panel).ToString().Contains(TEXT("known result")));

		Log->Clear();
		TestTrue(TEXT("Clear refreshes the panel"), FTurboMCPPanelHistoryTestAccess::Refresh(*Panel));
		TestTrue(TEXT("Clear says the pinned call is no longer in the list"),
			FTurboMCPPanelHistoryTestAccess::InspectorHeader(*Panel).ToString().Contains(TEXT("not in the current")));
		TestTrue(TEXT("Clear leaves the pinned detail intact"),
			FTurboMCPPanelHistoryTestAccess::InspectorDetail(*Panel).ToString().Contains(TEXT("known result")));
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
