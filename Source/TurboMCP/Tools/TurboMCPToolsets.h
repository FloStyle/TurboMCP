// Copyright FloStyle. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolsetDefinition.h"

#include "TurboMCPToolsets.generated.h"

/**
 * The four toolset classes, one per tool kind.
 *
 * `03_TOOLS.md` section 1.1 requires a toolset that holds only Read and Look
 * tools to be unable to change an asset, and requires that this stays true by
 * structure rather than by care.
 *
 * So the kind is the class, not a label on a function:
 *
 * | Kind    | Class                  | May write |
 * |---------|------------------------|-----------|
 * | Read    | UTurboMCPReadToolset   | no        |
 * | Look    | UTurboMCPLookToolset   | no        |
 * | Preview | UTurboMCPPreviewToolset| no        |
 * | Edit    | UTurboMCPEditToolset   | yes       |
 *
 * A write tool can only live in the Edit class, and that class is registered
 * last. Nothing enforces this by convention: putting a write in a Read class is
 * a code change in a file whose whole job is reading, which is visible in review.
 *
 * The toolset name an agent sees is `<Module>.<ClassName>`, built by the engine
 * in FFunctionLibraryToolset::GetToolsetClassName. Ours are therefore
 * `TurboMCP.UTurboMCPReadToolset` and so on. The engine does not strip the `U`,
 * so the class names are chosen to read well with it.
 *
 * Each tool is a static UFUNCTION with `meta = (AICallable)`. The engine builds
 * the tool's schema from reflection and uses the doc comment as the tool
 * description the agent reads. `07_ENGINE_NOTES.md` section 2.1 says a UFUNCTION
 * in a toolset that is not a tool needs `meta = (AIIgnore)`, or the registry
 * reports an invalid tool function.
 */

/**
 * Read tools: return text about assets. Cannot change anything.
 *
 * Phase 1 fills this class with R-TOOL-INDEX, R-TOOL-SCHEMA, R-ASSET-SEARCH,
 * R-ASSET-DESCRIBE, R-TYPE-FIND, R-DIRTY-LIST and E-COMPILE's read half
 * (`08_ROADMAP.md` phase 1).
 */
UCLASS()
class UTurboMCPReadToolset : public UToolsetDefinition
{
	GENERATED_BODY()

public:
	/**
	 * Read the MCP server state and this plugin's registration state.
	 *
	 * Use this first in a session to find out whether the server is running, on
	 * which address, and whether this plugin registered its toolsets. It costs
	 * nothing and changes nothing.
	 *
	 * Returns one line for the server and one line per TurboMCP toolset, or a
	 * line saying why a toolset is missing.
	 */
	UFUNCTION(meta = (AICallable))
	static FString ReadTurboMCPStatus();
};

/**
 * Look tools: return layout facts or images. Cannot change anything.
 *
 * Phase 3 fills this class with L-LAYOUT-REPORT, L-CAPTURE and
 * L-WIDGET-PREVIEW.
 */
UCLASS()
class UTurboMCPLookToolset : public UToolsetDefinition
{
	GENERATED_BODY()

public:
	/**
	 * Look for a live editor window that a capture could use.
	 *
	 * Answers one question: is there an editor window to look at right now. It
	 * reads the live Slate window list and names the windows a future capture
	 * could frame. It costs nothing and changes nothing.
	 *
	 * Returns the window count, one line per window, and says so in words when
	 * the list is empty. It does not capture an image yet.
	 */
	UFUNCTION(meta = (AICallable))
	static FString LookTurboMCPToolset();
};

/**
 * Preview tools: work out a change and report it, without making it.
 *
 * Phase 4 fills this class with the Preview half of every P/E pair. A Preview
 * and its Edit twin share one code path up to the write step, so they cannot
 * disagree (`D-02`).
 */
UCLASS()
class UTurboMCPPreviewToolset : public UToolsetDefinition
{
	GENERATED_BODY()

public:
	/**
	 * Preview the size of this plugin's own tool help.
	 *
	 * Reads every toolset schema from the registry and reports its size, so a
	 * toolset that is too large to help anyone is visible before an agent pays
	 * to read it. R-TOOL-SCHEMA sets a 2000 character budget for help output,
	 * and this flags any toolset over it. It works out the numbers and reports
	 * them, and writes nothing.
	 *
	 * Returns the total characters across all toolsets, then one line per
	 * TurboMCP toolset with its size and whether it is over the budget, or a
	 * line naming a toolset the registry refused.
	 */
	UFUNCTION(meta = (AICallable))
	static FString PreviewTurboMCPToolset();
};

/**
 * Edit tools: make the change. Every Edit is one undo (`P4`).
 *
 * Phase 4 fills this class with the Edit half of every P/E pair.
 */
UCLASS()
class UTurboMCPEditToolset : public UToolsetDefinition
{
	GENERATED_BODY()

public:
	/**
	 * Read whether the editor can accept a write right now.
	 *
	 * Every Edit tool must be one transaction, and an Edit must not run during
	 * play or simulate (`D-34`). This reports the current state so a caller
	 * knows before it plans a write, and it writes nothing itself.
	 *
	 * Returns whether play or simulate is running, and whether a transaction
	 * could be opened.
	 */
	UFUNCTION(meta = (AICallable))
	static FString ReadTurboMCPEditQueue();
};

/**
 * Registers all four toolsets with the engine's toolset registry.
 *
 * Must be called after every module loading phase is complete
 * (`FCoreDelegates::OnAllModuleLoadingPhasesComplete`), because the registry
 * subsystem may not exist before that. See TurboMCPModule.cpp.
 */
void RegisterTurboMCPToolsets();

/** Unregisters all four toolsets, so a hot reload leaves nothing stale. */
void UnregisterTurboMCPToolsets();

/**
 * The same text `UTurboMCPReadToolset::ReadTurboMCPStatus` returns.
 *
 * The panel calls this, so what a developer sees in the editor window is
 * exactly what an agent gets. `P9` in `02_PRINCIPLES.md` requires one code path,
 * and the cheapest way to keep that promise is for the window to call the
 * tool's own code rather than a copy of it.
 */
FString GetTurboMCPStatusText();
