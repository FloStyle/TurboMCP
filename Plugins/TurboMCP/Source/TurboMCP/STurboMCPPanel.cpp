// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "Panel/STurboMCPPanel.h"

#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "HAL/IConsoleManager.h"
#include "IModelContextProtocolModule.h"
#include "ModelContextProtocolServer.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Textures/SlateIcon.h"
#include "ToolMenus.h"
#include "ToolsetRegistry/UToolsetRegistry.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/SMultiLineEditableText.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"

#include "Tools/TurboMCPToolsets.h"
#include "TurboMCPCallLog.h"
#include "TurboMCPCallTimeline.h"
#include "TurboMCPPanelToolRunner.h"
#include "TurboMCPTokenEstimate.h"

#define LOCTEXT_NAMESPACE "TurboMCPPanel"

namespace
{
	/**
	 * Tab id for the TurboMCP window.
	 *
	 * A named constant because the tab id is also what a user types in the
	 * Window menu and what a future "open the panel" command needs, so a typo in
	 * a second copy would silently open a second, empty window.
	 */
	const FName TurboMCPTabId(TEXT("TurboMCP"));

	/** The two editor menus this plugin contributes to. */
	const FName TurboMCPWindowMenuName(TEXT("MainFrame.MainMenu.Window"));
	const FName TurboMCPToolsMenuName(TEXT("MainFrame.MainMenu.Tools"));
	const FName TurboMCPMenuSection(TEXT("TurboMCP"));
	const FName TurboMCPWindowEntry(TEXT("TurboMCP.OpenPanel.Window"));
	const FName TurboMCPToolsEntry(TEXT("TurboMCP.OpenPanel.Tools"));

	/** Defined with the console command below; shared by both menu entries. */
	void OpenTurboMCPPanel();

	/** Diameter of the server state dot, in slate units. */
	constexpr float ServerDotSize = 10.0f;

	/** Text size for the window's body, in slate units. */
	constexpr int32 BodyTextSize = 9;

	/**
	 * Height of the live call timeline, in slate units.
	 *
	 * BOUNDED ON PURPOSE, per `P6`: the log holds up to 1000 records and the timeline grows with it, so
	 * a pane that grew with its content would push the tool-calling widgets off the window. It scrolls
	 * instead.
	 */
	constexpr float TimelineHeight = 150.0f;

	/** Full recorded identity, used only to reattach an exact selected call; never a partial key. */
	bool SameRecordedCall(const FTurboMCPCallLog::FEntry& A, const FTurboMCPCallLog::FEntry& B)
	{
		return A.Timestamp == B.Timestamp
			&& A.Phase == B.Phase
			&& A.Source == B.Source
			&& A.SessionId == B.SessionId
			&& A.Toolset == B.Toolset
			&& A.Tool == B.Tool
			&& A.Arguments == B.Arguments
			&& A.Result == B.Result
			&& A.DurationMs == B.DurationMs
			&& A.ResultChars == B.ResultChars
			&& A.TokenEstimate == B.TokenEstimate
			&& A.bError == B.bError;
	}
}

void STurboMCPPanel::Construct(const FArguments& InArgs)
{
#if WITH_DEV_AUTOMATION_TESTS
	if (InArgs._TestCallLog != nullptr)
	{
		CallLogProvider = InArgs._TestCallLog;
	}
#endif

	// Read once up front so the window is never blank, then on every refresh.
	CachedStatus = GetTurboMCPStatusText();

	// The tool lists come from the registry, so a tool registered by anyone appears here. Done before
	// the widgets are built, because a combo box needs its options to exist when it is constructed.
	ReloadToolLists();

	// The status filter's three options, and "any" selected, so a fresh window hides nothing.
	TimelineStatusOptions.Add(MakeShared<FString>(TEXT("any")));
	TimelineStatusOptions.Add(MakeShared<FString>(TEXT("ok")));
	TimelineStatusOptions.Add(MakeShared<FString>(TEXT("error")));
	SelectedTimelineStatus = TimelineStatusOptions[0];
	RebuildTimelineRows();

	ChildSlot
	[
		SNew(SBorder)
		.Padding(8.0f)
		[
			SNew(SVerticalBox)

			// Server row: state dot, one line of status, and a refresh button.
			// 06_ACCEPTANCE.md T-PANEL-1 wants green when running and red when
			// stopped, so the colour comes from the live server, not from text.
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 6.0f)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0.0f, 0.0f, 6.0f, 0.0f)
				[
					SNew(SBox)
					.WidthOverride(ServerDotSize)
					.HeightOverride(ServerDotSize)
					[
						SNew(SBorder)
						.BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
						.BorderBackgroundColor(this, &STurboMCPPanel::GetServerDotColor)
					]
				]

				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("WindowTitle", "TurboMCP"))
					.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), BodyTextSize + 1))
				]

				+ SHorizontalBox::Slot()
				.FillWidth(1.6f)
				.VAlign(VAlign_Center)
				.Padding(0.0f, 0.0f, 6.0f, 0.0f)
				[
					SNew(STextBlock)
					.Text(this, &STurboMCPPanel::GetStatusSummary)
					.AutoWrapText(true)
					.ToolTipText(this, &STurboMCPPanel::GetStatusText)
					.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Mono"), BodyTextSize))
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SAssignNew(RefreshButton, SButton)
					.Text(LOCTEXT("Refresh", "Refresh"))
					.ToolTipText(LOCTEXT("RefreshTip",
						"Read the server state and the toolset list again."))
					.OnClicked(this, &STurboMCPPanel::OnRefreshClicked)
				]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 6.0f)
			[
				SNew(SSeparator)
			]

			// Call log row: the checkbox `03_TOOLS.md` asks for, and the line count and token
			// estimate `06_ACCEPTANCE.md` T-PANEL-6 asks for.
			//
			// THE CHECKBOX READS AND WRITES THE SAME SETTINGS OBJECT THE TOOL DOES, so the panel and
			// `E-CALL-LOG` cannot disagree about whether recording is on. The summary is read from
			// the log rather than from a counter kept here, for the same reason.
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 6.0f)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0.0f, 0.0f, 6.0f, 0.0f)
				[
					SAssignNew(RecordCheckBox, SCheckBox)
					.IsChecked(this, &STurboMCPPanel::GetCallLogCheckState)
					.OnCheckStateChanged(this, &STurboMCPPanel::OnCallLogCheckStateChanged)
					.ToolTipText(LOCTEXT("CallLogTip",
						"Record every tool call, including the engine's own, to a JSON line file. "
						"The choice is stored in the editor settings and survives a restart."))
					[
						SNew(STextBlock)
						.Text(LOCTEXT("CallLogLabel", "Record every tool call"))
					]
				]

				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(this, &STurboMCPPanel::GetCallLogSummary)
					.AutoWrapText(true)
					.ColorAndOpacity(this, &STurboMCPPanel::GetCallLogSummaryColor)
					.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Mono"), BodyTextSize))
					.ToolTipText(LOCTEXT("CallLogSummaryTip",
						"Read from the call log itself, so it cannot disagree with what was written."))
				]
			]

			// One compact view at a time. The buttons only change which section is visible; they
			// never call a tool or write a setting.
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 6.0f)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 0.0f, 4.0f, 0.0f)
				[
					SAssignNew(MonitorButton, SButton)
					.Text(this, &STurboMCPPanel::GetMonitorButtonText)
					.ToolTipText(LOCTEXT("MonitorViewTip", "Show recent calls and the selected-call inspector."))
					.OnClicked_Lambda([this]()
					{
						SetView(ETurboMCPView::Monitor);
						return FReply::Handled();
					})
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SAssignNew(ToolsButton, SButton)
					.Text(this, &STurboMCPPanel::GetToolsButtonText)
					.ToolTipText(LOCTEXT("ToolsViewTip", "Show tool discovery, example arguments and call results."))
					.OnClicked_Lambda([this]()
					{
						SetView(ETurboMCPView::Tools);
						return FReply::Handled();
					})
				]
			]

			// *** THE LIVE CALL TIMELINE, WHICH IS THE OTHER HALF OF `T02`'s ITEM 3 AND ALL OF ITEM 4b. ***
			// `RenderTimeline` was written for this pane and then left PRIVATE WITH NO CALLER, so the
			// capability was recorded as done while nothing could reach it.
			//
			// ONE ROW PER CALL, FILTERED BY TOOLSET, TOOL, ASSET AND STATUS; ONE ROW IS SELECTED FOR THE
			// INSPECTOR. The rows, the filter and the selected detail all come from
			// `FTurboMCPCallTimeline` - plain C++, exercised by the panel probe commandlet on literal
			// records - so what is here is only which rows are shown and which one is selected.
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 2.0f)
			[
				SNew(SHorizontalBox)
				.Visibility(this, &STurboMCPPanel::GetMonitorVisibility)

				// THE FOUR FILTERS, ONE FIELD EACH, matched against their own column. A single blob
				// matched against everything cannot answer "which calls touched THIS asset".
				+ SHorizontalBox::Slot()
				.FillWidth(0.22f)
				.Padding(0.0f, 0.0f, 4.0f, 0.0f)
				[
					SNew(SEditableTextBox)
					.HintText(LOCTEXT("TimelineFilterToolset", "toolset"))
					.ToolTipText(LOCTEXT("TimelineFilterToolsetTip",
						"Keep the calls whose toolset contains this text."))
					.OnTextChanged_Lambda([this](const FText& InText)
					{
						OnTimelineFilterChanged(InText, &TimelineToolsetFilter);
					})
				]

				+ SHorizontalBox::Slot()
				.FillWidth(0.22f)
				.Padding(0.0f, 0.0f, 4.0f, 0.0f)
				[
					SNew(SEditableTextBox)
					.HintText(LOCTEXT("TimelineFilterTool", "tool"))
					.ToolTipText(LOCTEXT("TimelineFilterToolTip",
						"Keep the calls whose tool name contains this text."))
					.OnTextChanged_Lambda([this](const FText& InText)
					{
						OnTimelineFilterChanged(InText, &TimelineToolFilter);
					})
				]

				+ SHorizontalBox::Slot()
				.FillWidth(0.22f)
				.Padding(0.0f, 0.0f, 4.0f, 0.0f)
				[
					SNew(SEditableTextBox)
					.HintText(LOCTEXT("TimelineFilterAsset", "asset"))
					.ToolTipText(LOCTEXT("TimelineFilterAssetTip",
						"Keep the calls whose ARGUMENTS name this asset. The engine's own event carries "
						"no arguments, so this narrows to the calls that passed this plugin's boundary."))
					.OnTextChanged_Lambda([this](const FText& InText)
					{
						OnTimelineFilterChanged(InText, &TimelineAssetFilter);
					})
				]

				+ SHorizontalBox::Slot()
				.FillWidth(0.16f)
				.Padding(0.0f, 0.0f, 4.0f, 0.0f)
				[
					SAssignNew(TimelineStatusComboBox, SComboBox<TSharedPtr<FString>>)
					.OptionsSource(&TimelineStatusOptions)
					.InitiallySelectedItem(SelectedTimelineStatus)
					.OnSelectionChanged_Lambda([this](TSharedPtr<FString> InItem, ESelectInfo::Type)
					{
						SelectedTimelineStatus = InItem;
						RebuildTimelineRows();
					})
					.OnGenerateWidget_Lambda([](TSharedPtr<FString> InItem)
					{
						return SNew(STextBlock).Text(InItem.IsValid()
							? FText::FromString(*InItem) : FText::GetEmpty());
					})
					.ToolTipText(LOCTEXT("TimelineFilterStatusTip",
						"Keep every call, only the ones that reported an error, or only the ones that did not."))
					[
						SNew(STextBlock)
						.Text(this, &STurboMCPPanel::GetSelectedTimelineStatusText)
					]
				]

				+ SHorizontalBox::Slot()
				.FillWidth(0.18f)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(this, &STurboMCPPanel::GetTimelineCountText)
					.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Mono"), BodyTextSize))
				]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 6.0f)
			[
				SAssignNew(HistoryListBorder, SBorder)
				.Visibility(this, &STurboMCPPanel::GetMonitorVisibility)
				.Padding(4.0f)
				[
					SNew(SBox)
					.HeightOverride(TimelineHeight)
					[
						SNew(SVerticalBox)

						+ SVerticalBox::Slot()
						.FillHeight(1.0f)
						[
							SAssignNew(TimelineListView, SListView<TSharedPtr<FTurboMCPCallTimeline::FListEntry>>)
							.ListItemsSource(&TimelineRows)
							.OnGenerateRow(this, &STurboMCPPanel::MakeTimelineRow)
							// NO SELECTION: a row is opened by clicking it, and a highlight that means
							// nothing would be a second kind of state to explain.
							.SelectionMode(ESelectionMode::None)
						]

						// THE EMPTY STATE, AND ONLY THAT: when the log holds nothing at all, the text
						// timeline says so in its own words - which is also what keeps
						// `RenderTimeline` reachable rather than dead code again.
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(STextBlock)
							.Text(this, &STurboMCPPanel::GetCallTimelineText)
							.Visibility(this, &STurboMCPPanel::GetTimelineTextVisibility)
							.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Mono"), BodyTextSize))
						]
					]
				]
			]

			// ONE selected-call inspector for the Monitor view. It reads only the fields the log
			// actually recorded; selecting a row never calls a tool or changes a setting.
			+ SVerticalBox::Slot()
			.FillHeight(0.55f)
			.Padding(0.0f, 0.0f, 0.0f, 6.0f)
			[
				SAssignNew(InspectorBorder, SBorder)
				.Visibility(this, &STurboMCPPanel::GetMonitorVisibility)
				.Padding(4.0f)
				[
					SNew(SVerticalBox)

					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 0.0f, 0.0f, 2.0f)
					[
						SNew(STextBlock)
						.Text(this, &STurboMCPPanel::GetSelectedCallHeader)
						.AutoWrapText(true)
						.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), BodyTextSize))
					]

					+ SVerticalBox::Slot()
					.FillHeight(1.0f)
					[
						SNew(SScrollBox)

						+ SScrollBox::Slot()
						[
							SNew(STextBlock)
							.Text(this, &STurboMCPPanel::GetSelectedCallDetail)
							.AutoWrapText(true)
							.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Mono"), BodyTextSize))
						]
					]
				]
			]

			// CALL ANY TOOL. This is the section that makes every MCP feature testable by hand,
			// including the engine's own tools, because both lists come from the registry.
			//
			// NOTHING HERE NAMES A TOOL. A hardcoded list would silently omit every tool added
			// later, which is the failure this window exists to make visible.
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 6.0f)
			[
				SNew(SVerticalBox)
				.Visibility(this, &STurboMCPPanel::GetToolsVisibility)

				// Toolset. A full-width row at narrow widths; the old single horizontal row clipped.
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 0.0f, 0.0f, 4.0f)
				[
					SAssignNew(ToolsetComboBox, SComboBox<TSharedPtr<FString>>)
					// `SLATE_ITEMS_SOURCE_ARGUMENT(OptionType, OptionsSource)` -
					// `Slate/Public/Widgets/Input/SComboBox.h:151`.
					.OptionsSource(&ToolsetOptions)
					.InitiallySelectedItem(SelectedToolset)
					// `FOnSelectionChanged` is
					// `TListTypeTraits<NullableOptionType>::FOnSelectionChanged`, so the handler
					// takes the item plus an `ESelectInfo::Type`.
					.OnSelectionChanged(this, &STurboMCPPanel::OnToolsetChanged)
					.OnGenerateWidget(this, &STurboMCPPanel::MakeToolsetRow)
					.ToolTipText(LOCTEXT("ToolsetTip",
						"Every toolset the registry publishes, including the engine's."))
					[
						SNew(STextBlock)
						.Text(this, &STurboMCPPanel::GetSelectedToolsetText)
					]
				]

				// Tool.
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 0.0f, 0.0f, 4.0f)
				[
					SAssignNew(ToolComboBox, SComboBox<TSharedPtr<FString>>)
					.OptionsSource(&ToolOptions)
					.InitiallySelectedItem(SelectedTool)
					.OnSelectionChanged(this, &STurboMCPPanel::OnToolChanged)
					.OnGenerateWidget(this, &STurboMCPPanel::MakeToolRow)
					.ToolTipText(LOCTEXT("ToolTip",
						"Every tool of the chosen toolset, from its published schema."))
					[
						SNew(STextBlock)
						.Text(this, &STurboMCPPanel::GetSelectedToolText)
					]
				]

				// The two actions wrap rather than forcing horizontal overflow.
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SWrapBox)
					.UseAllottedSize(true)
					.InnerSlotPadding(FVector2D(4.0f, 4.0f))

					+ SWrapBox::Slot()
					[
						SAssignNew(FillExampleButton, SButton)
						.Text(LOCTEXT("FillExample", "Fill example arguments"))
						.ToolTipText(LOCTEXT("FillExampleTip",
							"Fill the arguments box with schema scaffolding from this tool. It is a valid "
							"shape, not a validated runnable request; review every field before calling."))
						.OnClicked(this, &STurboMCPPanel::OnSampleClicked)
					]

					+ SWrapBox::Slot()
					[
						SAssignNew(CallButton, SButton)
						.Text(LOCTEXT("Call", "Call"))
						.ToolTipText(LOCTEXT("CallTip",
							"Call the tool through the registry, which is the same entry point the MCP "
							"server uses, so the two cannot disagree."))
						.OnClicked(this, &STurboMCPPanel::OnCallClicked)
					]
				]
			]

			// Arguments, and the result. Both are MULTI-LINE and both are BOUND, because a tool's
			// arguments are JSON and its answer is text, and neither fits on one line.
			+ SVerticalBox::Slot()
			.FillHeight(0.4f)
			.Padding(0.0f, 0.0f, 0.0f, 6.0f)
			[
				SAssignNew(ArgumentsBorder, SBorder)
				.Visibility(this, &STurboMCPPanel::GetToolsVisibility)
				.Padding(4.0f)
				[
					SNew(SMultiLineEditableText)
					.Text(this, &STurboMCPPanel::GetArgumentsText)
					.AutoWrapText(true)
					.OnTextChanged(this, &STurboMCPPanel::OnArgumentsChanged)
					.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Mono"), BodyTextSize))
					.HintText(LOCTEXT("ArgumentsHint", "arguments, as JSON"))
					.ToolTipText(LOCTEXT("ArgumentsTip",
						"The tool's arguments as a JSON object. Fill example arguments creates schema "
						"scaffolding, not a validated runnable request."))
				]
			]

			// The result line: what ran, how long it took, how big the answer was, and whether the
			// schema-derived sample was complete. T-PANEL-6 asks for the line count and the token
			// estimate, and they are here rather than only in the pane because a reader wants the
			// size before deciding to scroll.
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 2.0f)
			[
				SNew(STextBlock)
				.Visibility(this, &STurboMCPPanel::GetToolsVisibility)
				.Text(this, &STurboMCPPanel::GetResultHeader)
				.ColorAndOpacity(this, &STurboMCPPanel::GetResultColor)
				.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Mono"), BodyTextSize))
			]

			// THE RESULT PANE SHOWS THE TOOL'S OWN TEXT, UNTOUCHED. `E-CALL-LOG`'s JSON decoding is
			// done in the runner so what arrives here is what the tool returned, not a serialised
			// wrapper around it.
			+ SVerticalBox::Slot()
			.FillHeight(0.6f)
			.Padding(0.0f, 0.0f, 0.0f, 6.0f)
			[
				SAssignNew(ResultBorder, SBorder)
				.Visibility(this, &STurboMCPPanel::GetToolsVisibility)
				.Padding(4.0f)
				[
					SNew(SScrollBox)

					+ SScrollBox::Slot()
					[
						SNew(SMultiLineEditableText)
						.Text(this, &STurboMCPPanel::GetResultText)
						.AutoWrapText(true)
						.IsReadOnly(true)
						.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Mono"), BodyTextSize))
						.ToolTipText(LOCTEXT("ResultTip",
							"Exactly the text the tool returned to an agent."))
					]
				]
			]

			// Status body. Multi-line and scrollable because the toolset list grows as tools are
			// added, and a clipped list would hide a toolset that failed to register, which is
			// exactly what this window is for.
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			[
				SNew(SScrollBox)
				.Visibility(this, &STurboMCPPanel::GetToolsVisibility)

				+ SScrollBox::Slot()
				[
					SNew(SMultiLineEditableText)
					// *** BOUND, NOT CAPTURED. `.Text(this, &GetStatusText)` is re-evaluated by Slate, so
					// the pane shows whatever `OnRefreshClicked` last put in `CachedStatus`. Writing
					// `.Text(FText::FromString(CachedStatus))` here evaluated the string ONCE, at build
					// time, and left the refresh button updating a variable nothing displayed. ***
					.Text(this, &STurboMCPPanel::GetStatusText)
					.AutoWrapText(true)
					.IsReadOnly(true)
					.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Mono"), BodyTextSize))
					.ToolTipText(LOCTEXT("StatusTip",
						"The exact text the ReadTurboMCPStatus tool returns to an agent."))
				]
			]
		]
	];
}

FSlateColor STurboMCPPanel::GetServerDotColor() const
{
	// The dot reports the live server, so it is green only when a caller could
	// actually connect (06_ACCEPTANCE.md T-PANEL-1).
	//
	// Not const: IModelContextProtocolModule::GetServer() is a non-const virtual,
	// so a const pointer cannot call it. This runs on the game thread only.
	IModelContextProtocolModule* Module = IModelContextProtocolModule::Get();
	const bool bRunning = (Module != nullptr) && (Module->GetServer() != nullptr);

	return bRunning
		? FSlateColor(FLinearColor(0.1f, 0.8f, 0.1f))
		: FSlateColor(FLinearColor(0.9f, 0.2f, 0.2f));
}

FReply STurboMCPPanel::OnRefreshClicked()
{
	CachedStatus = GetTurboMCPStatusText();
	return FReply::Handled();
}

ECheckBoxState STurboMCPPanel::GetCallLogCheckState() const
{
	// Read from the log, which reads the settings, so there is ONE source of truth rather than the
	// panel keeping its own copy that a tool call could invalidate.
	return GetCallLog().IsEnabled()
		? ECheckBoxState::Checked
		: ECheckBoxState::Unchecked;
}

void STurboMCPPanel::OnCallLogCheckStateChanged(ECheckBoxState InState)
{
	// Through the log object, which persists the choice in the editor settings, exactly as
	// `E-CALL-LOG` does. Writing the setting here directly would be a second writer for one fact.
	GetCallLog().SetEnabled(InState == ECheckBoxState::Checked);
}

FText STurboMCPPanel::GetCallLogSummary() const
{
	const FTurboMCPCallLog& Log = GetCallLog();
	const FTurboMCPCallLog::FSnapshot Snapshot = Log.GetSnapshot();
	const TArray<FTurboMCPCallLog::FEntry>& Entries = Snapshot.Entries;

	// *** TWO NUMBERS CALLED "CALLS" IS THE DEFECT THIS METHOD HAD, AND THE CAPTURE SHOWED IT: the
	// summary counted `end` RECORDS and printed `15 calls` while the timeline below it printed `11 of
	// 11 call(s)`, because one call now leaves up to four records. *** A reader had no way to tell which
	// was right, and BOTH were - they were answers to different questions with the same word on them.
	// The assembly in `FTurboMCPCallTimeline` is the one definition, so the summary uses it too.
	const TArray<FTurboMCPCallLog::FEntry> Calls = FTurboMCPCallTimeline::RepresentativeRecords(Entries);
	// The figures come from the calls themselves rather than from a running counter, so a number shown
	// here is a number that was measured.
	int32 UnfinishedCalls = 0;
	int32 Tokens = 0;
	int32 Chars = 0;
	int32 UnknownChars = 0;
	int32 UnknownTokens = 0;
	int32 MeasuredZeroCalls = 0;
	int32 SlowCalls = 0;
	int32 LargeCalls = 0;
	for (const FTurboMCPCallLog::FEntry& Call : Calls)
	{
		// A `start` WITH NO `end` MEANS COMPLETION WAS NOT OBSERVED. The log cannot prove the
		// tool is alive, so it is counted separately and contributes no size.
		if (Call.Phase == TEXT("start"))
		{
			++UnfinishedCalls;
			continue;
		}
		if (Call.ResultChars == FTurboMCPCallLog::FEntry::NotMeasured) { ++UnknownChars; }
		else { Chars += Call.ResultChars; }
		if (Call.TokenEstimate == FTurboMCPCallLog::FEntry::NotMeasured) { ++UnknownTokens; }
		else { Tokens += Call.TokenEstimate; }
		if (Call.ResultChars == 0 && Call.TokenEstimate == 0) { ++MeasuredZeroCalls; }
		// MARKED HERE SO THE SUMMARY CAN SAY HOW MANY, RATHER THAN LEAVING A READER TO SCAN THE TIMELINE FOR THEM.
		if (Call.IsSlow()) { ++SlowCalls; }
		if (Call.IsLarge()) { ++LargeCalls; }
	}

	// `P1`: never show nothing. A window that says only "0" leaves a reader unsure whether the log
	// is off, empty, or broken.
	if (!Log.IsEnabled())
	{
		return FText::FromString(FString::Printf(
			TEXT("off  |  %d entr%s held, nothing new is being recorded"),
			Entries.Num(), Entries.Num() == 1 ? TEXT("y") : TEXT("ies")));
	}

	if (Entries.Num() == 0)
	{
		return FText::FromString(TEXT("on  |  no calls recorded yet this session"));
	}

	// THE RECORD COUNT IS PRINTED BESIDE THE CALL COUNT, because that difference is a fact about how
	// the log is written - two writers, up to four records a call - and hiding it would make the file's
	// own line count look wrong to whoever opens it next.
	FString Out = FString::Printf(
		TEXT("on  |  %d record%s, %d call%s, %d measured zero, %d chars known, %d chars unknown, ~%d tokens estimated, %d tokens unknown, %d completion not observed, %d slow, %d large"),
		Entries.Num(), Entries.Num() == 1 ? TEXT("") : TEXT("s"),
		Calls.Num(), Calls.Num() == 1 ? TEXT("") : TEXT("s"),
		MeasuredZeroCalls, Chars, UnknownChars, Tokens, UnknownTokens, UnfinishedCalls, SlowCalls, LargeCalls);
	return FText::FromString(Out);
}

FSlateColor STurboMCPPanel::GetCallLogSummaryColor() const
{
	// Neutral: recording on/off is an operation state, not a success or failure. The checkbox and
	// the summary text carry the state; green is reserved for successful outcomes.
	return FSlateColor(FLinearColor(0.72f, 0.72f, 0.72f));
}

FText STurboMCPPanel::GetCallTimelineText() const
{
	// BOUND RATHER THAN CAPTURED, for the reason the result and status panes record: Slate re-evaluates
	// a bound attribute, so a call made a moment ago appears without the window being rebuilt. The
	// read is bounded by the log's own `MaxEntries`, which is the same bound the summary above pays.
	//
	// *** IT IS THE PANE'S EMPTY STATE NOW THAT THE ROWS ARE A LIST, AND THAT KEEPS IT REACHABLE. ***
	return FText::FromString(GetCallLog().RenderTimeline());
}

EVisibility STurboMCPPanel::GetTimelineTextVisibility() const
{
	// ONLY WHEN THE LOG HOLDS NOTHING. If the filter matched nothing there ARE calls, and the count
	// line says so; printing the whole timeline there would show rows the filter had just removed.
	return TimelineCallCountBeforeFilter == 0 ? EVisibility::Visible : EVisibility::Collapsed;
}

void STurboMCPPanel::Tick(const FGeometry& InAllottedGeometry, const double InCurrentTime,
	const float InDeltaTime)
{
	SCompoundWidget::Tick(InAllottedGeometry, InCurrentTime, InDeltaTime);
	RefreshTimelineIfChanged();
}

bool STurboMCPPanel::RefreshTimelineIfChanged()
{
	const uint64 Revision = GetCallLog().GetRevision();
	if (bHasTimelineRevision && Revision == LastTimelineRevision)
	{
		return false;
	}
	RebuildTimelineRows();
	return true;
}

void STurboMCPPanel::OnTimelineFilterChanged(const FText& InText, FString* InField)
{
	if (InField == nullptr)
	{
		return;
	}
	*InField = InText.ToString();
	RebuildTimelineRows();
}

void STurboMCPPanel::RebuildTimelineRows()
{
	const FTurboMCPCallLog::FSnapshot Snapshot = GetCallLog().GetSnapshot();
	const TArray<FTurboMCPCallLog::FEntry>& Records = Snapshot.Entries;
	LastTimelineRevision = Snapshot.Revision;
	bHasTimelineRevision = true;
	++TimelineRebuildCount;

	// THE DENOMINATOR FIRST, because a list that is short looks the same whether the filter removed the
	// rows or nothing was recorded - and those two need different actions from a reader.
	TimelineCallCountBeforeFilter = FTurboMCPCallTimeline::Select(Records,
		FTurboMCPCallTimeline::FFilter()).Num();

	FTurboMCPCallTimeline::FFilter Filter;
	Filter.Toolset = TimelineToolsetFilter;
	Filter.Tool = TimelineToolFilter;
	Filter.Asset = TimelineAssetFilter;
	Filter.Status = SelectedTimelineStatus.IsValid() ? *SelectedTimelineStatus : FString();

	// ONE ENTRY PER SESSION HEADING AND ONE PER CALL, from the timeline rather than assembled here: the
	// grouping rule is part of what `T02` asks for, so it is asserted by the probe beside the filters.
	const TArray<FTurboMCPCallTimeline::FListEntry> Entries =
		FTurboMCPCallTimeline::ListEntries(Records, Filter);

	TimelineRows.Reset();
	TimelineRows.Reserve(Entries.Num());
	for (const FTurboMCPCallTimeline::FListEntry& Entry : Entries)
	{
		// A shared pointer per row because that is what `SListView` hands back to `OnGenerateRow`.
		TimelineRows.Add(MakeShared<FTurboMCPCallTimeline::FListEntry>(Entry));
	}

	RestoreSelectedCall();

	if (TimelineListView.IsValid())
	{
		TimelineListView->RequestListRefresh();
	}
}

bool STurboMCPPanel::IsTimelineRowSelected(
	const TSharedPtr<FTurboMCPCallTimeline::FListEntry>& InItem) const
{
	return InItem.IsValid() && !InItem->bHeading && SelectedTimelineRow.Get() == InItem.Get();
}

void STurboMCPPanel::ToggleTimelineRow(const TSharedPtr<FTurboMCPCallTimeline::FListEntry>& InItem)
{
	// A HEADING IS NOT A CALL AND HAS NOTHING TO INSPECT.
	if (!InItem.IsValid() || InItem->bHeading)
	{
		return;
	}
	SelectedTimelineRow = InItem;
	SelectedTimelineCall = InItem->Call;
	bHasSelectedCall = true;
	bSelectedCallInCurrentView = true;
	bSelectedCallAmbiguous = false;
	if (TimelineListView.IsValid())
	{
		TimelineListView->RequestListRefresh();
	}
}

void STurboMCPPanel::RestoreSelectedCall()
{
	SelectedTimelineRow.Reset();
	bSelectedCallInCurrentView = false;
	bSelectedCallAmbiguous = false;
	if (!bHasSelectedCall)
	{
		return;
	}

	TSharedPtr<FTurboMCPCallTimeline::FListEntry> Match;
	int32 Matches = 0;
	for (const TSharedPtr<FTurboMCPCallTimeline::FListEntry>& Row : TimelineRows)
	{
		if (Row.IsValid() && !Row->bHeading && SameRecordedCall(Row->Call, SelectedTimelineCall))
		{
			++Matches;
			Match = Row;
		}
	}
	if (Matches == 1)
	{
		SelectedTimelineRow = Match;
		bSelectedCallInCurrentView = true;
	}
	else if (Matches > 1)
	{
		// Two records identical in every available field cannot be told apart. Keep the pinned
		// snapshot and say so rather than silently pointing at one of them.
		bSelectedCallAmbiguous = true;
	}
}

FText STurboMCPPanel::GetMonitorButtonText() const
{
	return FText::FromString(ActiveView == ETurboMCPView::Monitor ? TEXT("Monitor (current)") : TEXT("Monitor"));
}

FText STurboMCPPanel::GetToolsButtonText() const
{
	return FText::FromString(ActiveView == ETurboMCPView::Tools ? TEXT("Tools (current)") : TEXT("Tools"));
}

FText STurboMCPPanel::GetSelectedCallHeader() const
{
	if (!bHasSelectedCall)
	{
		return LOCTEXT("NoInspectorSelection", "No call selected");
	}
	const FTurboMCPCallLog::FEntry& Call = SelectedTimelineCall;
	const FString Status = Call.Phase == TEXT("start")
		? TEXT("completion not observed")
		: (Call.bError ? TEXT("error") : TEXT("ok"));
	FString Header = FString::Printf(TEXT("%s  %s.%s  %s  source %s"),
		*Call.Timestamp, *Call.Toolset, *Call.Tool, *Status,
		Call.Source.IsEmpty() ? TEXT("unavailable") : *Call.Source);
	if (!bSelectedCallInCurrentView)
	{
		Header += bSelectedCallAmbiguous
			? TEXT("  |  selected call has multiple matches; pinned snapshot")
			: TEXT("  |  selected call is not in the current filtered/retained history");
	}
	return FText::FromString(Header);
}

FText STurboMCPPanel::GetSelectedCallDetail() const
{
	if (!bHasSelectedCall)
	{
		return LOCTEXT("InspectorEmpty",
			"Select one call to inspect its actual request and result. No tool is called by selecting a row.");
	}
	const FTurboMCPCallLog::FEntry& Call = SelectedTimelineCall;

	FString Out = TEXT("request: ");
	if (Call.Arguments.IsEmpty())
	{
		Out += TEXT("unavailable (not recorded)");
	}
	else
	{
		Out += Call.Arguments;
	}
	Out += TEXT("\nresult: ");
	if (Call.Phase == TEXT("start"))
	{
		Out += TEXT("unavailable (completion not observed)");
	}
	else if (Call.ResultChars == FTurboMCPCallLog::FEntry::NotMeasured)
	{
		Out += TEXT("unavailable (not recorded)");
	}
	else if (Call.Result.IsEmpty() && Call.ResultChars == 0)
	{
		Out += TEXT("(empty result)");
	}
	else if (Call.Result.IsEmpty())
	{
		Out += TEXT("unavailable (not recorded)");
	}
	else
	{
		TArray<FString> Lines;
		Call.Result.ParseIntoArrayLines(Lines, /*bCullEmpty=*/false);
		const int32 Shown = FMath::Min(8, Lines.Num());
		for (int32 Index = 0; Index < Shown; ++Index)
		{
			if (Index > 0)
			{
				Out += TEXT("\n  ");
			}
			Out += Lines[Index];
		}
		if (Lines.Num() > Shown)
		{
			Out += FString::Printf(TEXT("\n  ... and %d more line(s)"), Lines.Num() - Shown);
		}
	}
	Out += TEXT("\nsession: ");
	if (Call.SessionId.IsEmpty())
	{
		Out += TEXT("unavailable (not recorded)");
	}
	else
	{
		Out += Call.SessionId;
	}
	Out += TEXT("\nduration: ");
	if (Call.Phase == TEXT("start"))
	{
		Out += TEXT("unavailable (completion not observed)");
	}
	else
	{
		Out += FString::Printf(TEXT("%.1f ms"), Call.DurationMs);
	}
	Out += TEXT("\ncharacters: ");
	Out += FTurboMCPCallLog::FEntry::DescribeSize(Call.ResultChars);
	Out += TEXT("\ntokens (estimate): ");
	Out += FTurboMCPCallLog::FEntry::DescribeSize(Call.TokenEstimate);
	return FText::FromString(Out);
}

TSharedRef<ITableRow> STurboMCPPanel::MakeTimelineRow(
	TSharedPtr<FTurboMCPCallTimeline::FListEntry> InItem, const TSharedRef<STableViewBase>& InOwnerTable)
{
	if (!InItem.IsValid())
	{
		return SNew(STableRow<TSharedPtr<FTurboMCPCallTimeline::FListEntry>>, InOwnerTable)
			[ SNew(STextBlock).Text(LOCTEXT("TimelineBadRow", "(a row with no record)")) ];
	}

	// *** THE SESSION HEADING: "the calls of one batch script are grouped", WHICH IS A ROW RATHER THAN A
	// COLUMN. *** It appears once per batch, so a reader sees where one script ended without comparing
	// twenty session strings, and it is not clickable because there is nothing to open.
	if (InItem->bHeading)
	{
		return SNew(STableRow<TSharedPtr<FTurboMCPCallTimeline::FListEntry>>, InOwnerTable)
			[
				SNew(STextBlock)
				.Text(FText::FromString(FString::Printf(TEXT("; session %s"),
					InItem->SessionId.IsEmpty() ? TEXT("(none)") : *InItem->SessionId)))
				.AutoWrapText(true)
				.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Mono"), BodyTextSize))
				.ColorAndOpacity(FSlateColor(FLinearColor(0.65f, 0.65f, 0.65f)))
			];
	}

	const bool bSelected = IsTimelineRowSelected(InItem);
	FString RowText = bSelected ? TEXT("  > ") : TEXT("    ");
	RowText += FTurboMCPCallTimeline::RenderRow(InItem->Call);

	// ONE LINE, WITH A NEUTRAL TEXT MARKER WHEN SELECTED. The line is
	// `FTurboMCPCallTimeline::RenderRow`, the same text the timeline prints; selection only chooses
	// which call the single inspector reads.
	TSharedRef<SVerticalBox> Body = SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(STextBlock)
			.Text(FText::FromString(RowText))
			.AutoWrapText(true)
			.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Mono"), BodyTextSize))
		];

	return SNew(STableRow<TSharedPtr<FTurboMCPCallTimeline::FListEntry>>, InOwnerTable)
		[
			// A BORDERLESS BUTTON IS HOW A ROW IS MADE CLICKABLE, rather than a `SButton` that looks
			// like a button: the row already reads as a row, and the click selects its inspector.
			SNew(SButton)
			.ButtonStyle(FCoreStyle::Get(), "NoBorder")
			.ContentPadding(FMargin(2.0f, 1.0f))
			.HAlign(HAlign_Left)
			.ToolTipText(LOCTEXT("TimelineSelectTip",
				"Select this call for the inspector. Selecting a row never calls a tool or changes a setting."))
			.OnClicked_Lambda([this, InItem]()
			{
				ToggleTimelineRow(InItem);
				return FReply::Handled();
			})
			[
				Body
			]
		];
}

FText STurboMCPPanel::GetTimelineCountText() const
{
	// `P1`: the count carries its denominator, so "0 rows" is never ambiguous between "nothing was
	// recorded" and "the filter removed everything". HEADINGS ARE NOT CALLS, so they are not counted.
	int32 Calls = 0;
	for (const TSharedPtr<FTurboMCPCallTimeline::FListEntry>& Row : TimelineRows)
	{
		if (Row.IsValid() && !Row->bHeading)
		{
			++Calls;
		}
	}
	return FText::FromString(FString::Printf(TEXT("%d of %d call(s)"), Calls,
		TimelineCallCountBeforeFilter));
}

FText STurboMCPPanel::GetSelectedTimelineStatusText() const
{
	return SelectedTimelineStatus.IsValid()
		? FText::FromString(*SelectedTimelineStatus)
		: FText::FromString(TEXT("any"));
}

void STurboMCPPanel::ReloadToolLists()
{
	ToolsetOptions.Reset();
	ToolOptions.Reset();

	TArray<FTurboMCPPanelToolRunner::FToolsetInfo> Toolsets;
	FString Error;
	if (!FTurboMCPPanelToolRunner::ListToolsets(Toolsets, Error))
	{
		// `P1`: an empty list must say WHY it is empty. A reader who sees nothing cannot tell a
		// registry that is not up yet from one that answered with nothing.
		DiscoveryErrorText = FText::FromString(Error);
		SelectedToolset.Reset();
		SelectedTool.Reset();
		ResultText = FText::FromString(FString::Printf(
			TEXT("no toolsets could be listed.\n\n%s\n\nThe engine registers its toolsets after every ")
			TEXT("module has finished loading, so this is expected for the first moments after the ")
			TEXT("editor starts. Refresh reads it again."),
			*Error));
		return;
	}

	DiscoveryErrorText = FText::GetEmpty();
	for (const FTurboMCPPanelToolRunner::FToolsetInfo& Toolset : Toolsets)
	{
		ToolsetOptions.Add(MakeShared<FString>(Toolset.Name));
	}

	// The first toolset is selected, so the tool list is never empty on a fresh window and a caller
	// can press Fill example arguments without choosing anything first.
	if (ToolsetOptions.Num() > 0)
	{
		SelectedToolset = ToolsetOptions[0];
		OnToolsetChanged(SelectedToolset, ESelectInfo::Direct);
	}
}

TSharedRef<SWidget> STurboMCPPanel::MakeToolsetRow(TSharedPtr<FString> InItem)
{
	return SNew(STextBlock).Text(InItem.IsValid()
		? FText::FromString(*InItem)
		: FText::GetEmpty());
}

TSharedRef<SWidget> STurboMCPPanel::MakeToolRow(TSharedPtr<FString> InItem)
{
	return SNew(STextBlock).Text(InItem.IsValid()
		? FText::FromString(*InItem)
		: FText::GetEmpty());
}

FText STurboMCPPanel::GetSelectedToolsetText() const
{
	// A blank face would be indistinguishable from a failed load, which is the confusion `P1` exists
	// to prevent, so each empty case says which it is.
	if (SelectedToolset.IsValid())
	{
		return FText::FromString(*SelectedToolset);
	}
	return DiscoveryErrorText.IsEmpty()
		? LOCTEXT("NoToolsetSelected", "(no toolset chosen)")
		: LOCTEXT("NoToolsetLoaded", "(no toolset: see the result pane)");
}

FText STurboMCPPanel::GetSelectedToolText() const
{
	if (SelectedTool.IsValid())
	{
		return FText::FromString(*SelectedTool);
	}
	return ToolOptions.Num() == 0
		? LOCTEXT("NoToolsInToolset", "(this toolset publishes no tools)")
		: LOCTEXT("NoToolSelected", "(no tool chosen)");
}

void STurboMCPPanel::OnToolsetChanged(TSharedPtr<FString> InItem, ESelectInfo::Type InSelectInfo)
{
	SelectedToolset = InItem;
	SelectedTool.Reset();
	ToolOptions.Reset();

	if (!InItem.IsValid())
	{
		return;
	}

	TArray<FTurboMCPPanelToolRunner::FToolInfo> Tools;
	FString Error;
	if (!FTurboMCPPanelToolRunner::ListTools(*InItem, Tools, Error))
	{
		ResultText = FText::FromString(FString::Printf(
			TEXT("the tools of '%s' could not be listed.\n\n%s"), **InItem, *Error));
		return;
	}

	for (const FTurboMCPPanelToolRunner::FToolInfo& Tool : Tools)
	{
		ToolOptions.Add(MakeShared<FString>(Tool.BareName));
	}

	// The first tool is selected and its example arguments filled in, so the window is immediately usable and a
	// human sees what an argument list looks like before typing one.
	if (ToolOptions.Num() > 0)
	{
		SelectedTool = ToolOptions[0];
		OnToolChanged(SelectedTool, ESelectInfo::Direct);
	}
}

void STurboMCPPanel::OnToolChanged(TSharedPtr<FString> InItem, ESelectInfo::Type InSelectInfo)
{
	SelectedTool = InItem;
	if (!InItem.IsValid() || !SelectedToolset.IsValid())
	{
		return;
	}

	// Picking a tool fills the arguments box from that tool's OWN schema, so a caller never faces an
	// empty box and has to guess what the tool wants.
	bool bIsPartial = false;
	FString Error;
	const FString Sample = FTurboMCPPanelToolRunner::SampleJsonForTool(
		*SelectedToolset, *InItem, bIsPartial, Error);
	if (!Error.IsEmpty())
	{
		ResultText = FText::FromString(Error);
		return;
	}

	ArgumentsText = FText::FromString(Sample);
	ResultText = FText::FromString(bIsPartial
		? TEXT("This tool's schema does not declare a type for every parameter, so those fields are "
			"empty strings and are FLAGGED as a guess rather than filled with a plausible value. "
			"Type what the parameter wants.")
		: TEXT("The arguments above were built from this tool's own schema. Edit them, then press "
			"Call."));
}

void STurboMCPPanel::OnArgumentsChanged(const FText& InText)
{
	// The member is the single source of truth for what will be sent, so the box and the call cannot
	// disagree about the arguments.
	ArgumentsText = InText;
}

FReply STurboMCPPanel::OnSampleClicked()
{
	// The example arguments are DERIVED FROM THE SCHEMA rather than stored, so re-pressing it after the registry
	// changed gives the current shape rather than a stale one.
	if (!SelectedToolset.IsValid() || !SelectedTool.IsValid())
	{
		ResultText = FText::FromString(
			TEXT("choose a toolset and a tool first; there is no schema to sample."));
		return FReply::Handled();
	}

	bool bIsPartial = false;
	FString Error;
	const FString Sample = FTurboMCPPanelToolRunner::SampleJsonForTool(
		*SelectedToolset, *SelectedTool, bIsPartial, Error);
	ResultText = FText::FromString(Error.IsEmpty()
		? (bIsPartial
			? FString(TEXT("example arguments filled from the schema; scaffolding, not a validated "
				"runnable request. AT LEAST ONE PARAMETER'S TYPE IS NOT DECLARED, so those fields "
				"are empty strings rather than guesses."))
			: FString(TEXT("example arguments filled from the schema; scaffolding, not a validated "
				"runnable request.")))
		: Error);

	if (Error.IsEmpty())
	{
		ArgumentsText = FText::FromString(Sample);
	}
	return FReply::Handled();
}

FReply STurboMCPPanel::OnCallClicked()
{
	if (!SelectedToolset.IsValid() || !SelectedTool.IsValid())
	{
		ResultText = FText::FromString(
			TEXT("choose a toolset and a tool before calling: there is nothing to call."));
		return FReply::Handled();
	}

	// THE CALL DOES NOT BLOCK. The registry answers through a future and the editor's own tick
	// completes it, so waiting here would stall the window. The result is read on the next paint,
	// which `GetResultText` does.
	ToolRunner.BeginCall(*SelectedToolset, *SelectedTool, ArgumentsText.ToString());
	ResultText = FText::FromString(TEXT("calling..."));

	return FReply::Handled();
}

FText STurboMCPPanel::GetResultText() const
{
	const FTurboMCPPanelToolRunner::FCallResult& Result = ToolRunner.RefreshIfPending();

	if (!Result.bIsComplete)
	{
		return ResultText;
	}

	return Result.bIsError
		? FText::FromString(FString::Printf(TEXT("ERROR\n%s"), *Result.Error))
		: FText::FromString(Result.Value);
}

FSlateColor STurboMCPPanel::GetResultColor() const
{
	// IT REFRESHES TOO, AND THAT IS NOT REDUNDANT. Slate evaluates bound attributes in its own order,
	// so relying on the result PANE having been painted first would make the colour and the header
	// lag a frame behind the text on some paths and not others. Each reader notices the completion
	// itself, and noticing twice costs nothing.
	const FTurboMCPPanelToolRunner::FCallResult& Result = ToolRunner.RefreshIfPending();
	if (!Result.bIsComplete)
	{
		return FSlateColor(FLinearColor(0.7f, 0.7f, 0.7f));
	}
	return Result.bIsError
		? FSlateColor(FLinearColor(0.9f, 0.3f, 0.3f))
		: FSlateColor(FLinearColor(0.4f, 0.8f, 0.4f));
}

FText STurboMCPPanel::GetStatusSummary() const
{
	FString FirstLine = CachedStatus;
	int32 Newline = INDEX_NONE;
	if (FirstLine.FindChar(TEXT('\n'), Newline))
	{
		FirstLine = FirstLine.Left(Newline);
	}
	return FText::FromString(FirstLine);
}

FText STurboMCPPanel::GetResultHeader() const
{
	// Refreshes for the same reason the colour does: Slate's evaluation order is not this panel's to
	// choose, so each reader notices the completion itself.
	const FTurboMCPPanelToolRunner::FCallResult& Result = ToolRunner.RefreshIfPending();

	if (!Result.bIsComplete)
	{
		return ToolRunner.IsCallPending()
			? LOCTEXT("CallInFlight", "calling, waiting for the registry...")
			: LOCTEXT("NoCallYet", "no call yet");
	}

	const FString Where = FString::Printf(TEXT("%s.%s"), *Result.ToolsetName, *Result.ToolName);

	if (Result.bNeverRan)
	{
		// A distinct sentence, because the tool DID NOT RUN and an empty pane would look like a tool
		// that returned nothing.
		return FText::FromString(FString::Printf(TEXT("NOT RUN: %s"), *Result.Error));
	}

	// T-PANEL-6 wants a line count and a token estimate. Both are here, with the duration, because a
	// reader deciding whether to scroll wants the size first.
	int32 Lines = 0;
	if (!Result.Value.IsEmpty())
	{
		Lines = 1;
		for (const TCHAR Character : Result.Value)
		{
			if (Character == TEXT('\n')) { ++Lines; }
		}
	}

	return FText::FromString(FString::Printf(
		TEXT("%s  |  %s  |  %d line(s), %d characters, ~%d tokens (estimate)  |  %.0f ms"),
		Result.bIsError ? TEXT("error") : TEXT("ok"),
		*Where, Lines, Result.CharacterCount, Result.TokenEstimate, Result.DurationMs));
}

namespace
{
	void AddTurboMCPMenuEntry(const FName MenuName, const FName EntryName)
	{
		UToolMenus* ToolMenus = UToolMenus::Get();
		if (ToolMenus == nullptr)
		{
			return;
		}
		UToolMenu* Menu = ToolMenus->ExtendMenu(MenuName);
		if (Menu == nullptr)
		{
			return;
		}
		FToolMenuSection& Section = Menu->FindOrAddSection(
			TurboMCPMenuSection, LOCTEXT("TurboMCPMenuSection", "TurboMCP"));
		if (Section.FindEntry(EntryName) != nullptr)
		{
			return;
		}
		Section.AddMenuEntry(
			EntryName,
			LOCTEXT("TurboMCPMenuEntry", "TurboMCP"),
			LOCTEXT("TurboMCPMenuEntryTip", "Open the TurboMCP tools and call log."),
			// Existing editor style: StarshipStyle.cpp:470 registers Icons.Tools under the
			// editor style set that FAppStyle::GetAppStyleSetName() resolves.
			FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("Icons.Tools")),
			FUIAction(FExecuteAction::CreateStatic(&OpenTurboMCPPanel)));
	}

	void RemoveTurboMCPMenuSection()
	{
		UToolMenus* ToolMenus = UToolMenus::Get();
		if (ToolMenus == nullptr)
		{
			return;
		}
		if (UToolMenu* WindowMenu = ToolMenus->FindMenu(TurboMCPWindowMenuName))
		{
			WindowMenu->RemoveSection(TurboMCPMenuSection);
		}
		if (UToolMenu* ToolsMenu = ToolMenus->FindMenu(TurboMCPToolsMenuName))
		{
			ToolsMenu->RemoveSection(TurboMCPMenuSection);
		}
	}
}

void RegisterTurboMCPPanel(){
	// A nomad tab is dockable next to the other editor tabs and needs no
	// workspace layout entry, which is what a debugging window wants.
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
		TurboMCPTabId,
		FOnSpawnTab::CreateLambda([](const FSpawnTabArgs&) -> TSharedRef<SDockTab>
		{
			return SNew(SDockTab)
				.TabRole(ETabRole::NomadTab)
				[
					SNew(STurboMCPPanel)
				];
		}))
		.SetDisplayName(LOCTEXT("TabTitle", "TurboMCP"))
		.SetTooltipText(LOCTEXT("TabTooltip",
			"Call TurboMCP tools by hand and check the MCP server."))
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("Icons.Tools")))
		// The automatic orphan entry is disabled: the explicit Window and Tools entries below
		// replace its old placement under Programming and cannot duplicate on re-registration.
		.SetAutoGenerateMenuEntry(false)
		.SetMenuType(ETabSpawnerMenuType::Enabled);

	AddTurboMCPMenuEntry(TurboMCPWindowMenuName, TurboMCPWindowEntry);
	AddTurboMCPMenuEntry(TurboMCPToolsMenuName, TurboMCPToolsEntry);
}

void UnregisterTurboMCPPanel()
{
	RemoveTurboMCPMenuSection();
	if (FSlateApplication::IsInitialized())
	{
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(TurboMCPTabId);
	}
}

namespace
{
	/** Opens the window. See the command below for why this exists at all. */
	void OpenTurboMCPPanel()
	{
		if (FSlateApplication::IsInitialized())
		{
			FGlobalTabmanager::Get()->TryInvokeTab(TurboMCPTabId);
		}
	}

	/**
	 * `TurboMCP.OpenPanel` - opens the window, so a SCRIPT can open it and photograph it.
	 *
	 * *** THE FILE ALREADY EXPECTED THIS COMMAND TO EXIST: `RegisterTurboMCPPanel`'s own comment
	 * justifies the menu entry by "the window reachable without typing a console command", and until
	 * now there was none to type. *** It is added for a measured reason rather than for symmetry:
	 * THE PANEL IS THE ONE PART OF THIS PLUGIN THAT RUNS ONLY WHEN A HUMAN OPENS IT, so a mistake in
	 * its widgets - a bad lambda binding, a list reading from an array that no longer exists - survives
	 * every suite, every build and every commandlet until somebody clicks the menu. With this command a
	 * harness can start the editor with `-ExecCmds="TurboMCP.OpenPanel"`, after which
	 * `LookTurboMCPCapture` in `screen` mode can frame the window and a reader can LOOK at it.
	 *
	 * A CONSOLE COMMAND AND NOT AN `AICallable` TOOL, for the reason `UTurboMCPPanelProbeCommandlet` is
	 * not one either: `P9` says the panel and the MCP share a code path, and it does not say a widget
	 * concern belongs in the surface an agent reads.
	 */
	FAutoConsoleCommand GTurboMCPOpenPanelCommand(
		TEXT("TurboMCP.OpenPanel"),
		TEXT("Opens the TurboMCP window, so a harness can photograph it."),
		FConsoleCommandDelegate::CreateStatic(&OpenTurboMCPPanel));
}

#undef LOCTEXT_NAMESPACE
