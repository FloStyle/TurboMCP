// Copyright FloStyle. All Rights Reserved.

#include "Panel/STurboMCPPanel.h"

#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "IModelContextProtocolModule.h"
#include "ModelContextProtocolServer.h"
#include "Styling/CoreStyle.h"
#include "ToolsetRegistry/UToolsetRegistry.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/SMultiLineEditableText.h"
#include "Widgets/Text/STextBlock.h"

#include "Tools/TurboMCPToolsets.h"
#include "TurboMCPCallLog.h"
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

			// Status body. Multi-line and scrollable because the toolset list
			// grows as tools are added, and a clipped list would hide a toolset
			// that failed to register, which is exactly what this window is for.
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			[
				SNew(SScrollBox)

				+ SScrollBox::Slot()
				[
					SNew(SMultiLineEditableText)
					.Text(FText::FromString(CachedStatus))
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

void RegisterTurboMCPPanel()
{
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
