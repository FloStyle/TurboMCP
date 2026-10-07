// Copyright FloStyle. All Rights Reserved.

#include "Panel/STurboMCPPanel.h"

#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "IModelContextProtocolModule.h"
#include "ModelContextProtocolServer.h"
#include "Styling/CoreStyle.h"
#include "ToolsetRegistry/UToolsetRegistry.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/SMultiLineEditableText.h"
#include "Widgets/Text/STextBlock.h"

#include "Tools/TurboMCPToolsets.h"

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
