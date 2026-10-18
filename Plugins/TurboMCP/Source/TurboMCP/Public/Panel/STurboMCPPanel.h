// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#include "Containers/Array.h"
#include "Containers/Set.h"
#include "Containers/UnrealString.h"
#include "TurboMCPCallLog.h"
#include "TurboMCPCallTimeline.h"
#include "TurboMCPPanelToolRunner.h"
#include "Types/SlateEnums.h"
#include "Widgets/SCompoundWidget.h"

template <typename ItemType> class SListView;

/**
 * The TurboMCP editor window.
 *
 * `01_BRIEF.md` says the window is for calling a tool by hand, checking the
 * server and debugging what an agent did. It is deliberately not a graph
 * editor.
 *
 * `P9` in `02_PRINCIPLES.md` requires the window to run exactly the code the MCP
 * runs. This panel therefore calls the registered tool functions themselves
 * rather than reimplementing their text, so the two can never disagree.
 *
 * EVERY TOOL IS REACHABLE FROM HERE, INCLUDING THE ENGINE'S. The toolset and tool
 * lists are read from `UToolsetRegistry`, so a tool added by anyone appears
 * without this file being touched. There is deliberately NO LIST OF TOOLS in the
 * panel: a hardcoded one would silently omit every tool added later, which is the
 * failure this window exists to make visible.
 *
 * WHAT IS IN THE PANEL AND WHAT IS NOT. The tool-calling logic lives in
 * `FTurboMCPPanelToolRunner`, which is plain C++ and can be driven by a test. The
 * panel renders it and owns no logic beyond the widget bindings. `QUESTIONS.md`
 * Q-11 records why that split matters: this plugin has no Slate inspection tool,
 * so nothing about the layout can be checked automatically, and putting logic
 * here would make it unverifiable too.
 */
class STurboMCPPanel : public SCompoundWidget
{
public:
	enum class ETurboMCPView : uint8
	{
		Monitor,
		Tools
	};

	SLATE_BEGIN_ARGS(STurboMCPPanel)
#if WITH_DEV_AUTOMATION_TESTS
		: _TestCallLog(nullptr)
#endif
	{}
#if WITH_DEV_AUTOMATION_TESTS
		SLATE_ARGUMENT(FTurboMCPCallLog*, TestCallLog)
#endif
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/**
	 * Notices a completed call and repaints the timeline.
	 *
	 * *** A LIST VIEW DOES NOT RE-READ ITS SOURCE, WHICH IS THE ONE THING A BOUND TEXT ATTRIBUTE DOES
	 * FOR FREE. *** The summary line above it is bound, so it updates by itself; a list needs to be told,
	 * and the cheapest honest trigger is the log's own entry count - a call that finished wrote an end
	 * record, so the count moved.
	 */
	virtual void Tick(const FGeometry& InAllottedGeometry, const double InCurrentTime,
		const float InDeltaTime) override;

	/** The runner the panel renders. Public so a test can reach the same state. */
	FTurboMCPPanelToolRunner& GetToolRunner() { return ToolRunner; }

	// ---------------------------------------------------------------------------------------
	// THE CALL TIMELINE: ITS TEXT, ITS LIST, ITS FOUR FILTERS AND THE SELECTED-CALL INSPECTOR.
	//
	// *** THE LOGIC IS NOT HERE, AND THESE ARE PUBLIC FOR THE SAME REASON `GetToolRunner` IS. ***
	// `QUESTIONS.md` Q-11 records that the widget tree cannot be inspected. The answers this project
	// uses are (a) leave as little as possible inside the widget - the one-row-per-call assembly, the
	// four filters and the selected-call detail all live in `FTurboMCPCallTimeline`, plain C++ - and
	// (b) let the probe commandlet CONSTRUCT this widget and read back what it renders, so the window
	// is not the one part of the plugin that never runs until a human opens it.
	// ---------------------------------------------------------------------------------------

	/** Rebuilds the rows from the log through the current filter. */
	void RebuildTimelineRows();

	/** One row: the call's line, with a neutral selected marker. */
	TSharedRef<ITableRow> MakeTimelineRow(TSharedPtr<FTurboMCPCallTimeline::FListEntry> InItem,
		const TSharedRef<STableViewBase>& InOwnerTable);

	/** Selects one call for the inspector; it never executes anything. */
	void ToggleTimelineRow(const TSharedPtr<FTurboMCPCallTimeline::FListEntry>& InItem);

	/** How many calls the filter kept, out of how many there are - the denominator `P1` asks for. */
	FText GetTimelineCountText() const;

	/** The rows the list currently shows. Read by the probe so the count has a witness. */
	int32 GetTimelineRowCount() const { return TimelineRows.Num(); }

private:
#if WITH_DEV_AUTOMATION_TESTS
	friend struct FTurboMCPPanelHistoryTestAccess;
#endif

	/** The log this panel renders; production defaults to the singleton. */
	FTurboMCPCallLog& GetCallLog() const { return CallLogProvider ? *CallLogProvider : FTurboMCPCallLog::Get(); }

	/** Switches between the compact monitor and the tool workbench; no tool or setting is touched. */
	void SetView(ETurboMCPView InView)
	{
		if (ActiveView != InView)
		{
			ActiveView = InView;
			Invalidate(EInvalidateWidgetReason::Layout);
		}
	}
	EVisibility GetMonitorVisibility() const { return ActiveView == ETurboMCPView::Monitor ? EVisibility::Visible : EVisibility::Collapsed; }
	EVisibility GetToolsVisibility() const { return ActiveView == ETurboMCPView::Tools ? EVisibility::Visible : EVisibility::Collapsed; }
	FText GetMonitorButtonText() const;
	FText GetToolsButtonText() const;

	/** The selected call's actual fields; absent fields are named unavailable. */
	FText GetSelectedCallHeader() const;
	FText GetSelectedCallDetail() const;

	/** True when that row is the one selected for the inspector. */
	bool IsTimelineRowSelected(const TSharedPtr<FTurboMCPCallTimeline::FListEntry>& InItem) const;

	/** Rebuilds only when the log revision changed, and says whether it did. */
	bool RefreshTimelineIfChanged();

	/** Colour of the server dot: green when the server is running, else red. */
	FSlateColor GetServerDotColor() const;

	/** Re-reads the status and repaints. */
	FReply OnRefreshClicked();

	/**
	 * The call log's check state.
	 *
	 * Read from the SAME settings object the tool writes, so the box and `E-CALL-LOG` cannot
	 * disagree. `03_TOOLS.md` requires the log to be "turned on by a checkbox in the panel" and
	 * the choice to persist in editor settings.
	 */
	ECheckBoxState GetCallLogCheckState() const;

	/** Turns the log on or off through the shared log object, which persists the choice. */
	void OnCallLogCheckStateChanged(ECheckBoxState InState);

	/**
	 * One line about the log: whether it is on, how many entries are held, and the token estimate.
	 *
	 * `06_ACCEPTANCE.md` T-PANEL-6 wants a line count and a token estimate shown. Reading it from
	 * the log rather than from a counter kept here means the number cannot drift from the log.
	 */
	FText GetCallLogSummary() const;

	/** Colour for the summary: the accent when recording, muted when not. */
	FSlateColor GetCallLogSummaryColor() const;

	/**
	 * The live call timeline's TEXT, which the pane shows only when the log holds nothing at all.
	 *
	 * `T02`'s item 3. It calls `FTurboMCPCallLog::RenderTimeline` rather than formatting rows here, so
	 * the pane and `R-CALL-LOG` cannot disagree about what a call is or how one is shown.
	 */
	FText GetCallTimelineText() const;

	/**
	 * What identifies one call in the list, for the expansion set.
	 *
	 * The stamp and the name. Two calls to one tool inside the same millisecond would collide, and the
	 * honest fix would be a record index; a collision costs one row that opens with another's detail,
	 * which is a smaller defect than an index that has to be kept in step with a filtered list.
	 */
	/** The selected call's inspector state. */
	TSharedPtr<FTurboMCPCallTimeline::FListEntry> SelectedTimelineRow;

	/** The status combo's face. */
	FText GetSelectedTimelineStatusText() const;

	/** Shown only when the log holds NOTHING, so an empty list says why rather than looking broken. */
	EVisibility GetTimelineTextVisibility() const;

	/** One filter changed: the rows are rebuilt from the log, not filtered in place. */
	void OnTimelineFilterChanged(const FText& InText, FString* InField);

	/** The status combo's options and its selection. */
	TArray<TSharedPtr<FString>> TimelineStatusOptions;
	TSharedPtr<FString> SelectedTimelineStatus;

	/** The rows the list shows, and the list itself so a rebuild can ask it to repaint. */
	TArray<TSharedPtr<FTurboMCPCallTimeline::FListEntry>> TimelineRows;
	TSharedPtr<SListView<TSharedPtr<FTurboMCPCallTimeline::FListEntry>>> TimelineListView;

	/** The current compact view; Monitor is the default. */
	ETurboMCPView ActiveView = ETurboMCPView::Monitor;

	/** The three text filters, as typed. */
	FString TimelineToolsetFilter;
	FString TimelineToolFilter;
	FString TimelineAssetFilter;

	/** The log this panel reads; an isolated test replaces it before the first history read. */
	FTurboMCPCallLog* CallLogProvider = nullptr;

	/** The log revision the rows were built from. */
	uint64 LastTimelineRevision = 0;
	bool bHasTimelineRevision = false;

	/** Test-visible evidence that an unchanged revision did not rebuild the list. */
	uint64 TimelineRebuildCount = 0;

	/** How many calls there are altogether, so the count line can carry its denominator. */
	int32 TimelineCallCountBeforeFilter = 0;

	// ---------------------------------------------------------------------------------------
	// The tool-call section. Every one of these is a thin binding over `ToolRunner`, which is
	// where the work and the testability live.
	// ---------------------------------------------------------------------------------------

	/** Re-reads the toolset and tool lists from the registry. */
	void ReloadToolLists();

	/** The widgets that generate a row for a toolset and for a tool. */
	TSharedRef<SWidget> MakeToolsetRow(TSharedPtr<FString> InItem);
	TSharedRef<SWidget> MakeToolRow(TSharedPtr<FString> InItem);

	/** A selection changed in either combo box. */
	void OnToolsetChanged(TSharedPtr<FString> InItem, ESelectInfo::Type InSelectInfo);
	void OnToolChanged(TSharedPtr<FString> InItem, ESelectInfo::Type InSelectInfo);

	/**
	 * What each combo box shows on its face.
	 *
	 * Bound rather than passed once. A combo box with no selection must say so rather than showing a
	 * blank, because a blank face and a missing list look the same (`P1`).
	 */
	FText GetSelectedToolsetText() const;
	FText GetSelectedToolText() const;

	/** Fills the arguments box from the selected tool's own schema. */
	FReply OnSampleClicked();

	/** Starts the call, without blocking the game thread. */
	FReply OnCallClicked();

	/** What the result pane shows, and how it is coloured. */
	FText GetResultText() const;
	FSlateColor GetResultColor() const;

	/** One line above the result: what ran, how long it took, how big it was. */
	FText GetResultHeader() const;

	/** The status pane's text, BOUND RATHER THAN CAPTURED. `OnRefreshClicked` replaces `CachedStatus`,
	 *  and a bound attribute is what makes the pane show the replacement - the one-time
	 *  `.Text(FText::FromString(CachedStatus))` it used to carry kept the string the panel was built
	 *  with, so a refresh could never change what was on screen. */
	FText GetStatusText() const { return FText::FromString(CachedStatus); }

	/** The first status line for the compact header; the full body lives in Tools. */
	FText GetStatusSummary() const;

	/** The toolset combo box's options. */
	TArray<TSharedPtr<FString>> ToolsetOptions;

	/** The tool combo box's options, for the selected toolset. */
	TArray<TSharedPtr<FString>> ToolOptions;

	/** The selection of each combo box, invalid when nothing is chosen. */
	TSharedPtr<FString> SelectedToolset;
	TSharedPtr<FString> SelectedTool;

	/**
	 * The arguments box's content.
	 *
	 * An `FText` member BOUND to the widget rather than a value passed once, because Slate
	 * re-evaluates a bound attribute and does not re-read a value. A plain `FString` would leave the
	 * view showing whatever it had when it was built, which for a sample would be nothing.
	 */
	FText ArgumentsText;

	/** The result pane's content. An `FText` for the same reason as the arguments box. */
	FText ResultText;

	/** Why the registry could not be read, shown in place of an empty list. */
	FText DiscoveryErrorText;

	/**
	 * The arguments box's text, bound rather than passed once.
	 *
	 * Slate RE-EVALUATES a bound attribute and does not re-read a value, so a plain `FString` handed
	 * over at construction would leave the box showing whatever it had then - and the SAMPLE button,
	 * whose whole job is to replace that text, would appear to do nothing.
	 */
	FText GetArgumentsText() const { return ArgumentsText; }

	/**
	 * Keeps the member in step with what was typed.
	 *
	 * `FOnTextChanged` is `DECLARE_DELEGATE_OneParam(FOnTextChanged, const FText&)` -
	 * `Engine/Source/Runtime/Slate/Public/Framework/SlateDelegates.h:92`.
	 */
	void OnArgumentsChanged(const FText& InText);

	/** The runner: discovery, samples and execution, all of it testable without Slate. */
	FTurboMCPPanelToolRunner ToolRunner;

	/** Cached status text, refreshed on construct and on demand. */
	FString CachedStatus;
};

/**
 * Adds the TurboMCP window to the editor's Window menu.
 *
 * Safe to call twice: the tab manager replaces an existing spawner for the same
 * id, so a hot reload cannot register a second window.
 */
void RegisterTurboMCPPanel();

/** Removes the window from the Window menu. */
void UnregisterTurboMCPPanel();
