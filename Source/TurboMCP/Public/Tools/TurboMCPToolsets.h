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

	/**
	 * Read every tool the server serves, one line each.
	 *
	 * Lists the MCP server state first, including whether tool search is on,
	 * because that decides what a client can reach. Then one line per tool in
	 * every registered toolset, engine and TurboMCP alike: kind, tool name,
	 * source and the first sentence of its description.
	 *
	 * Use this once at the start of a session instead of guessing tool names.
	 * Pass a tool name to R-TOOL-SCHEMA for its parameters and known traps.
	 *
	 * Returns the counts even when zero, and says why the list is empty when the
	 * toolset registry is unavailable.
	 */
	UFUNCTION(meta = (AICallable))
	static FString ReadTurboMCPToolIndex();

	/**
	 * Read the parameters and known traps of one tool.
	 *
	 * Pass a toolset name and a tool name, or one joined name as tools/list
	 * prints it. Leave the tool name empty to get one line per tool in that
	 * toolset. Name the tool alone to find it in whatever toolset holds it.
	 *
	 * Works for the engine's own tools as well as TurboMCP's, because engine tool
	 * help runs to tens of thousands of characters and the traps are where agents
	 * actually lose time.
	 *
	 * The reply stays under 2000 characters for one tool. A name that fits
	 * nothing lists the real toolset names and the closest matches.
	 *
	 * Both arguments are strings and both appear required in the schema, because
	 * the engine's tool registry marks an FString parameter required whatever its
	 * default. There is no way to express an optional string here, so an empty
	 * string is the documented way to say "not given" (see `07_ENGINE_NOTES.md`
	 * section 2.1). Pass "" rather than omitting the field.
	 */
	UFUNCTION(meta = (AICallable))
	static FString ReadTurboMCPToolHelp(const FString& ToolsetName, const FString& ToolName);

	/**
	 * Read assets matching a query, like the editor's quick open search.
	 *
	 * Pass words to match against asset names and folder paths. Every word must
	 * appear in one or the other. Two filters may appear at any position, alone
	 * or with words: `type:<class words>` and `path:<folder words>`.
	 *
	 * Ranking is exact name first, then names that start with the words, then
	 * names that contain them, then folder only matches, and shorter names first
	 * on a tie.
	 *
	 * Finds assets that are in memory but not saved, not only those on disk.
	 * Every result carries its full object path and class, and a Blueprint also
	 * carries its parent. Results are capped and the count left out is printed.
	 *
	 * Pass "" to see the refusal message. A query matching nothing says how many
	 * assets were searched, so "no match" never looks like "nothing searched".
	 */
	UFUNCTION(meta = (AICallable))
	static FString ReadTurboMCPAssetSearch(const FString& Query);

	/**
	 * Read what one or more assets are, in any path form.
	 *
	 * Pass assets separated by commas or pipes. Every form resolves: a short
	 * name, a package path, an object path, or a generated class path. A short
	 * name that fits more than one asset is refused with the full list rather
	 * than guessed at.
	 *
	 * Each answer gives the package path, object path, class, generated class
	 * path when the asset has one, parent for a Blueprint, whether the asset is
	 * loaded, whether it has unsaved changes, and whether it is on disk.
	 *
	 * A fact that could not be read is reported as unknown with the reason, never
	 * as a confident answer (`D-05`).
	 */
	UFUNCTION(meta = (AICallable))
	static FString ReadTurboMCPAssetDescribe(const FString& Assets);

	/**
	 * Read the exact type path a pin or a variable needs.
	 *
	 * Pass part of a type name, and optionally a kind of struct, enum, class,
	 * interface or any. Every word must appear in the type's name or its full
	 * path.
	 *
	 * Searches native C++ types through reflection and user-made struct and enum
	 * assets, and says which of the two each result is. Each result gives the
	 * exact path to use, its kind, and the module or asset it lives in.
	 *
	 * A type that exists on disk but is not loaded is still listed, marked as not
	 * loaded, with the note that the asset must be loaded before a graph can use
	 * it. Results are capped and the number left out is printed.
	 *
	 * Pass an empty kind for `any`. An unknown kind is refused with the list of
	 * accepted kinds.
	 */
	UFUNCTION(meta = (AICallable))
	static FString ReadTurboMCPTypeFind(const FString& Words, const FString& Kind);

	/**
	 * Read every asset with unsaved changes.
	 *
	 * One line per unsaved asset: its package path and its class. Reads the
	 * package's own unsaved flag rather than a save call's return value, because
	 * `D-33` records that an engine save once reported an error when it worked.
	 *
	 * The class is read from the asset registry, so listing unsaved work never
	 * forces an asset to load. Pass a number to raise the default cap of 50.
	 *
	 * When nothing is unsaved it says so, because a zero that names its reason is
	 * different from a check that did not run (`P1`).
	 */
	UFUNCTION(meta = (AICallable))
	static FString ReadTurboMCPDirtyList(const FString& Limit);

	/**
	 * Turn node IDs into nodes.
	 *
	 * Pass one or more IDs separated by commas or pipes, in the form
	 * `AssetName:GraphName:NodeKey`, for example `WBP_ShopCard:RefreshPrice:3fa91c`.
	 * The full path form `/Game/UI/WBP_ShopCard.WBP_ShopCard:RefreshPrice:3fa91c`
	 * is always accepted too.
	 *
	 * Each ID gives back the node reference the engine's own tools take, its
	 * title, its node type ID — the form the engine's graph writer accepts, not
	 * the C++ class name — its graph, and every pin by name with its direction
	 * and its index within that direction.
	 *
	 * An ID whose asset name fits more than one asset is refused and every full
	 * path it fits is listed. An ID whose node is gone says so and offers the
	 * nodes in that graph whose key starts the same way, so a rebuild can be
	 * survived without guessing. A key that fits more than one node is refused
	 * with the longer IDs that would disambiguate it (`04_FORMATS.md` 3.2).
	 */
	UFUNCTION(meta = (AICallable))
	static FString ReadTurboMCPNodeResolve(const FString& NodeIds);

	/**
	 * Find nodes in a Blueprint by what they say.
	 *
	 * Pass the asset, one or more words separated by commas or pipes, and
	 * optionally one graph name. Every word must appear, which is what makes a
	 * two word search narrow rather than widen.
	 *
	 * The words are matched against each node's title, its called function, its
	 * variable, its class and its pin names. Every graph is searched unless one is
	 * named. Each match prints its node ID, title and node type ID.
	 *
	 * Nothing matching says how many nodes and graphs were searched and what to
	 * try instead; a graph name that does not exist lists the real graph names.
	 */
	UFUNCTION(meta = (AICallable))
	static FString ReadTurboMCPNodeFind(
		const FString& Asset,
		const FString& Words,
		const FString& GraphName);

	/**
	 * Read one graph as text.
	 *
	 * Pass the asset, the graph name, a format and a detail level. Format `read`
	 * is the default and the only one implemented; the engine's own write dialect
	 * is refused by name rather than silently returning the wrong thing.
	 *
	 * Detail levels, each a superset of the one below: `manifest` (entry points by
	 * name and counts), `signatures` (entry points with their output pins),
	 * `bodies` (the default, every entry point with its body as graph text), and
	 * `full` (bodies plus node classes and pin indexes).
	 *
	 * The graph text is S-expressions, one top level form per event or function,
	 * with exec chains reading top to bottom. Every node carries its node ID in a
	 * comment, shared value nodes are defined once and referred to by node ID, and
	 * plain reroute knots are transparent. Nodes not reachable from any entry
	 * point are listed separately with a count.
	 *
	 * A graph name that does not exist lists the real graph names. A node that
	 * cannot be printed becomes an inline `; error:` line rather than a silent gap,
	 * and is also counted at the end so the gap cannot be missed.
	 */
	UFUNCTION(meta = (AICallable))
	static FString ReadTurboMCPGraphLoad(
		const FString& Asset,
		const FString& GraphName,
		const FString& Format,
		const FString& Detail);

	/**
	 * Read whole Blueprints: the main Read tool.
	 *
	 * One call gives everything needed to plan a change. Pass one or more assets
	 * separated by commas or pipes, in any path form.
	 *
	 * Detail levels, each a superset of the one below:
	 *
	 * - `manifest`, the default: asset path, class, parent, interfaces, counts, and
	 *   the names of every graph, variable and component.
	 * - `signatures`: the manifest, plus every variable with its type, default,
	 *   category and flags; every function and event with its full signature;
	 *   components with their classes; and the relation layer.
	 * - `bodies`: the signatures, plus every graph body in the graph text format.
	 * - `full`: the bodies, plus the editing details.
	 *
	 * The relation layer is where bugs become visible without reading a body: which
	 * graph calls which function, which graphs read and write each variable,
	 * functions nothing calls, and events that are bound. A variable nothing touches
	 * is called out, because that is obvious here and invisible in a body.
	 *
	 * The graph filter is a list of name fragments; a graph matches when its name
	 * contains one, case insensitively. A filter that matches nothing is refused and
	 * the real graph names are listed, rather than returning an empty read.
	 *
	 * The token budget is a number of characters. When the read would pass it, whole
	 * sections are dropped — never a line cut in half — and the result ends by
	 * naming what was reduced and the call that gets the rest.
	 *
	 * Format `json` is asked for by the spec but not implemented, and is refused by
	 * name rather than returning graph text a caller would parse as JSON.
	 */
	UFUNCTION(meta = (AICallable))
	static FString ReadTurboMCPBlueprintLoad(
		const FString& Assets,
		const FString& Detail,
		const FString& GraphFilter,
		const FString& Format,
		const FString& TokenBudget);

	/**
	 * Read what an asset references, and what references it.
	 *
	 * Pass one asset in any path form, and optionally a direction: `out` for the
	 * content assets it uses, `in` for the assets that use it, or `both`. Empty
	 * means both.
	 *
	 * Every reference is printed as a full object path with its class. Engine and
	 * script packages are counted but not listed, because `/Script/...` is not
	 * content and listing it would bury the few project assets that matter.
	 *
	 * The edges come from the asset registry, which records them when packages are
	 * saved. An asset that has never been saved therefore has no outgoing edges, and
	 * a user that has never been saved will not appear as an incoming one; the
	 * result says so rather than letting a zero look like a fact about the project.
	 *
	 * A direction that is not one of the three is refused by name, and an asset that
	 * cannot be resolved is refused by the shared resolver.
	 */
	UFUNCTION(meta = (AICallable))
	static FString ReadTurboMCPRefs(
		const FString& Asset,
		const FString& Direction);

	/**
	 * Read what a Blueprint can override, and what it already does.
	 *
	 * Pass one Blueprint in any path form, and optionally an include filter: `all`
	 * for everything, `overridden` for what this Blueprint already overrides, or
	 * `available` for what it has not overridden yet. Empty means all.
	 *
	 * Every function comes from the parent chain or from an implemented interface,
	 * with its full signature and its inputs and outputs marked. Interface
	 * functions are labelled, because a Blueprint that implements an interface has
	 * to answer each of them.
	 *
	 * Each entry says whether it is already overridden here, and which class the
	 * engine says it really comes from, which is what a caller needs in order to
	 * call the parent implementation. Both facts come from the editor's own helpers
	 * rather than from a rule written here.
	 *
	 * Counts are reported before the filter is applied, so asking for `available`
	 * still says how many exist in total. A filter matching nothing says so and says
	 * what would work, and a parent chain with nothing to override says that plainly
	 * rather than looking like a failure.
	 */
	UFUNCTION(meta = (AICallable))
	static FString ReadTurboMCPOverrides(
		const FString& Asset,
		const FString& Include);
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

	/**
	 * Compile one or more Blueprints or widgets, with a real answer each.
	 *
	 * Pass assets separated by commas or pipes. Each asset gets its own line:
	 * clean, clean with warnings, or its errors and warnings with the node that
	 * caused each one. A line also says whether the asset still has unsaved
	 * changes, so a successful compile is never mistaken for saved work.
	 *
	 * The answer comes from the compiler's own message log, not from a return
	 * value, because the engine's compile tool can answer null for success and
	 * for failure alike.
	 *
	 * Warnings raised by the compile pass are counted and named, never pasted,
	 * because engine log text does not belong in a result (`P6`).
	 *
	 * An asset kind that cannot compile is refused with the reason (`P2`).
	 */
	UFUNCTION(meta = (AICallable))
	static FString EditTurboMCPCompile(const FString& Assets);

	/**
	 * Save one or more assets without noise.
	 *
	 * Pass assets separated by commas or pipes, in any path form.
	 *
	 * Refuses while a play or simulate session runs, with that reason, because
	 * saves made then failed or half worked (`D-34`).
	 *
	 * Saves one asset at a time and retries a failure once. Each asset is
	 * reported from its unsaved flag after the save, never from what the save
	 * call returned, because an engine save once reported an error when it had
	 * worked and saved only some of the assets it was given (`D-33`).
	 *
	 * Each line states saved, had no changes, or failed with the reason. Engine
	 * log text is counted and attributed to the asset that raised it, never
	 * pasted (`P6`).
	 */
	UFUNCTION(meta = (AICallable))
	static FString EditTurboMCPSave(const FString& Assets);
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
