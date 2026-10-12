// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "Panel/STurboMCPPanel.h"

#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "IModelContextProtocolModule.h"
#include "ModelContextProtocolServer.h"
#include "Styling/CoreStyle.h"
#include "ToolsetRegistry/UToolsetRegistry.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/SMultiLineEditableText.h"
#include "Widgets/Text/STextBlock.h"

#include "Tools/TurboMCPToolsets.h"
#include "TurboMCPCallLog.h"
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

	/** Diameter of the server state dot, in slate units. */
	constexpr float ServerDotSize = 10.0f;

	/** Text size for the window's body, in slate units. */
	constexpr int32 BodyTextSize = 9;
}

void STurboMCPPanel::Construct(const FArguments& InArgs)
{
	// Read once up front so the window is never blank, then on every refresh.
	CachedStatus = GetTurboMCPStatusText();

	// The tool lists come from the registry, so a tool registered by anyone appears here. Done before
	// the widgets are built, because a combo box needs its options to exist when it is constructed.
	ReloadToolLists();

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
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SButton)
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
					SNew(SCheckBox)
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
					.ColorAndOpacity(this, &STurboMCPPanel::GetCallLogSummaryColor)
					.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Mono"), BodyTextSize))
					.ToolTipText(LOCTEXT("CallLogSummaryTip",
						"Read from the call log itself, so it cannot disagree with what was written."))
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
				SNew(SHorizontalBox)

				// Toolset.
				+ SHorizontalBox::Slot()
				.FillWidth(0.35f)
				.VAlign(VAlign_Center)
				.Padding(0.0f, 0.0f, 4.0f, 0.0f)
				[
					SNew(SComboBox<TSharedPtr<FString>>)
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
				+ SHorizontalBox::Slot()
				.FillWidth(0.45f)
				.VAlign(VAlign_Center)
				.Padding(0.0f, 0.0f, 4.0f, 0.0f)
				[
					SNew(SComboBox<TSharedPtr<FString>>)
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

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0.0f, 0.0f, 4.0f, 0.0f)
				[
					SNew(SButton)
					.Text(LOCTEXT("Sample", "Sample"))
					.ToolTipText(LOCTEXT("SampleTip",
						"Fill the arguments box from this tool's own schema, so you do not have to "
						"know one to try it. A field whose type the schema does not declare is "
						"flagged in the result line rather than silently guessed."))
					.OnClicked(this, &STurboMCPPanel::OnSampleClicked)
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SButton)
					.Text(LOCTEXT("Call", "Call"))
					.ToolTipText(LOCTEXT("CallTip",
						"Call the tool through the registry, which is the same entry point the MCP "
						"server uses, so the two cannot disagree."))
					.OnClicked(this, &STurboMCPPanel::OnCallClicked)
				]
			]

			// Arguments, and the result. Both are MULTI-LINE and both are BOUND, because a tool's
			// arguments are JSON and its answer is text, and neither fits on one line.
			+ SVerticalBox::Slot()
			.FillHeight(0.4f)
			.Padding(0.0f, 0.0f, 0.0f, 6.0f)
			[
				SNew(SBorder)
				.Padding(4.0f)
				[
					SNew(SMultiLineEditableText)
					.Text(this, &STurboMCPPanel::GetArgumentsText)
					.OnTextChanged(this, &STurboMCPPanel::OnArgumentsChanged)
					.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Mono"), BodyTextSize))
					.HintText(LOCTEXT("ArgumentsHint", "arguments, as JSON"))
					.ToolTipText(LOCTEXT("ArgumentsTip",
						"The tool's arguments as a JSON object. SAMPLE fills this from the tool's own "
						"schema."))
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
				SNew(SBorder)
				.Padding(4.0f)
				[
					SNew(SScrollBox)

					+ SScrollBox::Slot()
					[
						SNew(SMultiLineEditableText)
						.Text(this, &STurboMCPPanel::GetResultText)
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

				+ SScrollBox::Slot()
				[
					SNew(SMultiLineEditableText)
					// *** BOUND, NOT CAPTURED. `.Text(this, &GetStatusText)` is re-evaluated by Slate, so
					// the pane shows whatever `OnRefreshClicked` last put in `CachedStatus`. Writing
					// `.Text(FText::FromString(CachedStatus))` here evaluated the string ONCE, at build
					// time, and left the refresh button updating a variable nothing displayed. ***
					.Text(this, &STurboMCPPanel::GetStatusText)
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
	return FTurboMCPCallLog::Get().IsEnabled()
		? ECheckBoxState::Checked
		: ECheckBoxState::Unchecked;
}

void STurboMCPPanel::OnCallLogCheckStateChanged(ECheckBoxState InState)
{
	// Through the log object, which persists the choice in the editor settings, exactly as
	// `E-CALL-LOG` does. Writing the setting here directly would be a second writer for one fact.
	FTurboMCPCallLog::Get().SetEnabled(InState == ECheckBoxState::Checked);
}

FText STurboMCPPanel::GetCallLogSummary() const
{
	const FTurboMCPCallLog& Log = FTurboMCPCallLog::Get();
	const TArray<FTurboMCPCallLog::FEntry> Entries = Log.GetEntries();

	// The figures come from the entries themselves rather than from a running counter, so a number
	// shown here is a number that was written.
	int32 EndRecords = 0;
	int32 Tokens = 0;
	int32 Chars = 0;
	for (const FTurboMCPCallLog::FEntry& Entry : Entries)
	{
		if (Entry.Phase == TEXT("end"))
		{
			++EndRecords;
			Tokens += Entry.TokenEstimate;
			Chars += Entry.ResultChars;
		}
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

	return FText::FromString(FString::Printf(
		TEXT("on  |  %d record%s, %d call%s, %d characters, ~%d tokens (estimate)"),
		Entries.Num(), Entries.Num() == 1 ? TEXT("") : TEXT("s"),
		EndRecords, EndRecords == 1 ? TEXT("") : TEXT("s"),
		Chars, Tokens));
}

FSlateColor STurboMCPPanel::GetCallLogSummaryColor() const
{
	// Green while recording, muted when not, so the state reads at a glance without parsing the
	// text. Same intent as the server dot above it.
	return FTurboMCPCallLog::Get().IsEnabled()
		? FSlateColor(FLinearColor(0.1f, 0.8f, 0.1f))
		: FSlateColor(FLinearColor(0.6f, 0.6f, 0.6f));
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
	// can press SAMPLE without choosing anything first.
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

	// The first tool is selected and its sample filled in, so the window is immediately usable and a
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
	// The sample is DERIVED FROM THE SCHEMA rather than stored, so re-pressing it after the registry
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
			? FString(TEXT("sample filled from the schema. AT LEAST ONE PARAMETER'S TYPE IS NOT "
				"DECLARED, so those fields are empty strings rather than guesses."))
			: FString(TEXT("sample filled from the schema.")))
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
		// The menu entry is what makes the window reachable without typing a
		// console command, which a developer debugging an agent needs.
		.SetMenuType(ETabSpawnerMenuType::Enabled);
}

void UnregisterTurboMCPPanel()
{
	if (FSlateApplication::IsInitialized())
	{
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(TurboMCPTabId);
	}
}

#undef LOCTEXT_NAMESPACE
