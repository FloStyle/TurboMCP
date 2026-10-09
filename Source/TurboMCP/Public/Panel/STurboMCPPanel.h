// Copyright FloStyle. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "TurboMCPPanelToolRunner.h"
#include "Types/SlateEnums.h"
#include "Widgets/SCompoundWidget.h"

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
	SLATE_BEGIN_ARGS(STurboMCPPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** The runner the panel renders. Public so a test can reach the same state. */
	FTurboMCPPanelToolRunner& GetToolRunner() { return ToolRunner; }

private:
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

	/** Fills the arguments box from the selected tool's own schema. */
	FReply OnSampleClicked();

	/** Starts the call, without blocking the game thread. */
	FReply OnCallClicked();

	/** What the result pane shows, and how it is coloured. */
	FText GetResultText() const;
	FSlateColor GetResultColor() const;

	/** One line above the result: what ran, how long it took, how big it was. */
	FText GetResultHeader() const;

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
