// Copyright FloStyle. All Rights Reserved.

#pragma once

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
 * Phase 0 shows the server row and the toolset list (`08_ROADMAP.md`: the editor
 * window with server row and tool list). The per tool input fields and the
 * asset picker arrive with the tools that need them.
 */
class STurboMCPPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(STurboMCPPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	/** Colour of the server dot: green when the server is running, else red. */
	FSlateColor GetServerDotColor() const;

	/** Re-reads the status and repaints. */
	FReply OnRefreshClicked();

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
