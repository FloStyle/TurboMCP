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

	/**
	 * Read the comment boxes of a Blueprint's graphs as data.
	 *
	 * Pass one Blueprint in any path form, and optionally one graph name. Empty
	 * means every graph, which is usually what a caller wants before choosing one.
	 *
	 * Each box gives its node ID, its title, its colour, its rectangle in graph
	 * coordinates, its parent box when it is nested, and the IDs of the nodes inside
	 * it. A graph with no boxes says so rather than being left out, and a box that
	 * covers no node says that too, because both are facts rather than gaps.
	 *
	 * Membership and nesting are computed from the rectangles, because a comment box
	 * does not store either. A node that falls inside more than one box is reported
	 * with a warning and counted, rather than being assigned to one of them, because
	 * moving it changes the membership of every box that covers it.
	 */
	UFUNCTION(meta = (AICallable))
	static FString ReadTurboMCPCommentList(
		const FString& Asset,
		const FString& GraphName);

	/**
	 * Describe a data table.
	 *
	 * Pass one DataTable in any path form, and optionally an include filter: `all`,
	 * `readers` for which graphs read it, or `schema` for the row struct and rows.
	 * Empty means all.
	 *
	 * The row struct is reported with every field and its type, then the row count
	 * and the row names. **Row values are not printed**: the spec puts them out of
	 * scope because the engine's own tools read and write them, and the result says
	 * so rather than leaving a caller to wonder.
	 *
	 * The readers are found in two steps. The asset registry gives every asset that
	 * depends on the table, which is complete but names no graph; loaded Blueprints
	 * among them are then searched for a node reading this exact table, which is what
	 * turns an asset into a graph name. A reader that is not loaded is still
	 * reported, saying its graph was not searched, rather than being omitted.
	 *
	 * A table with no rows, or a struct with no fields, reports that as a fact. A
	 * table whose row struct was deleted says so instead of printing an empty schema.
	 * A filter that is not one of the three is refused by name.
	 */
	UFUNCTION(meta = (AICallable))
	static FString ReadTurboMCPDataTable(
		const FString& Asset,
		const FString& Include);

	/**
	 * Read the call log: what the server recorded about its own use.
	 *
	 * EVERY tool call is recorded when the log is on, including the ENGINE's own toolsets. The log
	 * has TWO SOURCES and one line shape, so a reader does not have to know which produced a line;
	 * the `source` field says `plugin` or `engine`.
	 *
	 *  - For this plugin's tools the arguments are known, so the line carries them.
	 *  - For the engine's tools the event carries only BLAKE3 hashes of the names and no arguments
	 *    at all, measured from the engine's own `ModelContextProtocolAnalytics.cpp`. This plugin's
	 *    own toolset names are hashed back to names; an engine tool's hash CANNOT be, because
	 *    `UToolsetRegistry` exposes no list of served toolsets or tools. Those lines carry the first
	 *    twelve characters of the hash, labelled as what they are, rather than an invented name.
	 *
	 * `filter` matches the toolset, the tool or the source. `since` is an ISO-8601 timestamp and
	 * returns only later entries, so a caller pages forward instead of re-reading. `limit` defaults
	 * to 50 and is capped at 500.
	 *
	 * An empty log is never just empty: the result says whether logging is OFF, or on with nothing
	 * recorded, or on with entries that do not match the filter.
	 */
	UFUNCTION(meta = (AICallable))
	static FString ReadTurboMCPCallLog(
		const FString& Filter,
		const FString& Limit,
		const FString& Since);
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

	/**
	 * Look at the layout of one graph: measure it and report, without moving anything.
	 *
	 * Pass a Blueprint and a graph name. Every node in the graph is included,
	 * connected or not, and no node is listed twice.
	 *
	 * Reports the extent, the node count, the comment boxes and how many are nested,
	 * and the total wire length with the longest single wire. Wire length is the
	 * straight distance between linked pins and is the main layout measure: a tidy
	 * graph of a given size scores lower than a tangled one. Pin positions are
	 * computed from each node's rectangle and pin order, because the engine exposes
	 * no pin position to a tool call, so the number is consistent and comparable
	 * rather than pixel exact, and the result says so.
	 *
	 * Then reports what is wrong, each finding with node IDs so a caller can act on
	 * it: backward wires with exec and data separated, where backward means the
	 * target node starts to the left of where the source node ends, so a node stacked
	 * under its predecessor counts; overlapping node pairs; a getter placed to the
	 * right of or under its consumer; a node parked at the origin whose nearest link
	 * is far away; and disconnected groups, each node marked with whether its logic
	 * already exists in the connected graph.
	 *
	 * A graph with no wires says so in words rather than reporting a zero. A graph
	 * name that does not exist lists the real ones.
	 */
	UFUNCTION(meta = (AICallable))
	static FString LookTurboMCPLayoutReport(
		const FString& Asset,
		const FString& GraphName);

	/**
	 * Render a Widget Blueprint with sample values and write a PNG.
	 *
	 * Pass the widget, sample values as a JSON object of variable name to value, an
	 * optional function to call after the values are set, and a size as WxH. Empty
	 * sample values means the widget's own defaults; an empty size means 1280x720.
	 *
	 * A transient instance is made off screen, the values are set through reflection,
	 * the function is called, the widget is rendered into a render target and written
	 * to a PNG, and the instance is destroyed before the call returns. No play session
	 * is needed.
	 *
	 * Values are set with the engine's own text import, so the accepted spellings are
	 * the details panel's and not a format invented here. A variable name that does not
	 * exist is reported along with some names that do, rather than being ignored. A
	 * function that takes parameters is refused, because this tool has no way to supply
	 * them.
	 *
	 * The PNG goes to a fixed folder under the project's Saved folder, its alpha is
	 * forced opaque because a widget background is transparent by default, and every
	 * engine warning and error raised while setting values and rendering is reported.
	 *
	 * A widget with no generated class is refused, because an instance of it would not
	 * run its own Construct or its own functions.
	 */
	UFUNCTION(meta = (AICallable))
	static FString LookTurboMCPWidgetPreview(
		const FString& Widget,
		const FString& SampleValuesJson,
		const FString& FunctionName,
		const FString& Size);

	/**
	 * Capture a picture of a Blueprint's graph editor, framed one of three ways.
	 *
	 * Pass the asset and the graph name, and optionally a mode (`graph` for the whole
	 * graph, `box` for one comment box, `node` for one node), a target, and a size as
	 * WxH. Empty mode means `graph`; empty size means 1600x900.
	 *
	 * Mode `box` takes a comment box INDEX, numbered from 0 in the same order
	 * L-LAYOUT-REPORT lists them, top to bottom then left to right, so an index means
	 * the same thing in both tools. Mode `node` takes a node ID from R-NODE-FIND.
	 *
	 * A framing that cannot be done is an error naming why, and it never falls back to
	 * the whole graph: asking for box 9 of a graph with 4 boxes says exactly that. An
	 * empty frame is refused rather than written as a blank image, because a blank
	 * image looks like a successful capture of nothing.
	 *
	 * The graph widget is rendered directly rather than grabbing the screen, so the
	 * image is exactly the size asked for and shows the frame that was set. The result
	 * reports what it framed, the frame's bounds in graph coordinates, the zoom it
	 * chose, and the image size.
	 *
	 * The PNG goes to a fixed folder under the project's Saved folder, and its name
	 * says which asset, graph and mode produced it. Widget Blueprints, materials and
	 * other editors are refused by name for now rather than capturing the wrong tab,
	 * because a wrong picture is worse than no picture.
	 */
	UFUNCTION(meta = (AICallable))
	static FString LookTurboMCPCapture(
		const FString& Asset,
		const FString& GraphName,
		const FString& Mode,
		const FString& Target,
		const FString& Size);
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

	/**
	 * Work out what creating an enum asset would do, without creating it.
	 *
	 * Pass a package path such as `/Game/Tests/MyEnum` and the values, as a comma-separated
	 * list where each entry is `Name` or `Name=Display Name`.
	 *
	 * Prints every value with its display name when it has one, and says whether the asset
	 * already exists, because that fact decides whether the edit will be refused. A value
	 * whose name is not a usable identifier is refused by name, a duplicate value is
	 * refused, and `_MAX` is refused because every user-defined enum carries one already.
	 *
	 * This and its Edit twin share one code path up to the write, so the two cannot disagree
	 * about what would happen (`D-02`). It writes nothing.
	 */
	UFUNCTION(meta = (AICallable))
	static FString PreviewTurboMCPEnumAdd(const FString& Path, const FString& Values);

	/**
	 * Work out what adding Blueprint variables would do, without adding them.
	 *
	 * Pass a Blueprint and a list of variables. Each item is `Name: Type` followed by
	 * optional fields: `default=`, `category=`, `tooltip=`, `private=`, `readonly=`,
	 * `instanceEditable=`, `exposeOnSpawn=`. Items are separated by commas at the top level,
	 * so a type with a comma in it such as `Map<String, Int>` stays one item.
	 *
	 * Basic types are bool, int, int64, byte, real, float, double, name, string and text.
	 * Struct names are Vector, Rotator, Transform, Vector2D, LinearColor and Color. For an
	 * enum or a struct asset, pass its path. Containers are `Array<T>`, `Set<T>` and
	 * `Map<K, V>`.
	 *
	 * A name that is not a usable identifier is refused, a duplicate is refused, an unknown
	 * field is refused rather than ignored, and an unbalanced type is refused rather than
	 * truncated. This and its Edit twin share one code path up to the write (`D-02`), and
	 * this writes nothing.
	 */
	UFUNCTION(meta = (AICallable))
	static FString PreviewTurboMCPVarsAdd(const FString& Asset, const FString& Variables);

	/**
	 * Work out what creating an override would do, without creating it.
	 *
	 * Pass a Blueprint and a function or event name from `R-OVERRIDES`. The preview prints the
	 * exact SIGNATURE, resolved from the real reflected `UFunction` rather than rebuilt from
	 * pins, and says whether the name is an event or a function.
	 *
	 * A name this Blueprint cannot override is refused with BOTH lists of what is still
	 * available, so a caller corrects itself in one step. A name that is ALREADY overridden is
	 * reported in the preview, because a caller that previews and then edits should not be
	 * surprised by the edit.
	 *
	 * The overridable set is the editor's own `ListEvents` and `ListFunctions`, so a name
	 * accepted here is one the editor accepts, including interfaces implemented natively in C++.
	 */
	UFUNCTION(meta = (AICallable))
	static FString PreviewTurboMCPOverrideAdd(
		const FString& Asset,
		const FString& FunctionOrEvent);

	/**
	 * Work out which widget bindings a class swap has left broken, without changing anything.
	 *
	 * When a widget is replaced by one of another class under the same name, its bound events may
	 * still expect the old class. This lists EVERY binding with the widget's current class and says
	 * which ones that class can no longer satisfy.
	 *
	 * VALIDITY IS THE ENGINE'S OWN ANSWER, not a rule invented here: a binding is satisfiable when
	 * the widget's CURRENT class carries a delegate named `<PropertyName>Delegate` or
	 * `<PropertyName>`, which is exactly what `FDelegateEditorBinding::IsBindingValid` looks for
	 * and therefore what the engine's compiler would accept.
	 *
	 * Call nodes naming a function the current class does not have are REPORTED and not touched,
	 * because removing a node is a bigger change than the spec asks for here.
	 */
	UFUNCTION(meta = (AICallable))
	static FString PreviewTurboMCPWidgetRetarget(const FString& Asset);

	/**
	 * Work out what a comment-box change would do, without making it.
	 *
	 * `action` is REQUIRED and one of `create`, `set`, `fit`, `delete`. It is required rather than
	 * guessed from which other fields are present, because a caller who forgot one field would
	 * otherwise get the other operation.
	 *
	 *  - `create` takes `nodes`, one or more node keys or full node ids, and boxes them. The box is
	 *    sized to enclose them; `size=WxH` overrides that.
	 *  - `set` takes `box` and any of `text=`, `color=`, `size=WxH`.
	 *  - `fit` resizes every box in the graph to enclose what is inside it, leaving the box's
	 *    top-left where it is and allowing for the title bar above the rectangle.
	 *  - `delete` takes `box` and removes the BOX ONLY. A box is a label, not a container: nothing
	 *    inside it is removed or moved, and the result says so.
	 *
	 * Boxes are named by their node key, the six hex digits `R-COMMENT-LIST` reports.
	 *
	 * WHAT IS INSIDE A BOX IS THE ENGINE'S ANSWER, not a rectangle compared by hand: membership
	 * comes from `UEdGraphNode::GetNodesUnderComment`, which is what the editor itself uses. A
	 * hand-rolled overlap test would disagree with the editor the first time either changed, and the
	 * disagreement would be invisible.
	 *
	 * EVERY action LISTS THE BOXES THAT HOLD NOTHING, because a box around no nodes is a label with
	 * nothing to label and is usually a leftover. `fit` leaves those alone rather than shrinking
	 * them to nothing, which would look like a deletion.
	 */
	UFUNCTION(meta = (AICallable))
	static FString PreviewTurboMCPComment(
		const FString& Asset,
		const FString& GraphName,
		const FString& Action,
		const FString& Nodes,
		const FString& Box,
		const FString& Text,
		const FString& Color,
		const FString& Size);
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

	/**
	 * Create an enum asset from a list of values.
	 *
	 * Pass a package path such as `/Game/Tests/MyEnum` and the values, as a comma-separated
	 * list where each entry is `Name` or `Name=Display Name`. The same inputs, validated the
	 * same way, as `P-ENUM-ADD`, because the two share one code path up to the write.
	 *
	 * REFUSES an existing path rather than overwriting it: a name someone else chose is not
	 * this tool's to replace, which is what `P8` means by never leaving the user worse off.
	 * The refusal opens with `unchanged:` so a caller can tell it from a failure (`P4`).
	 *
	 * The enum is created through the editor's own `FEnumEditorUtils`, so it stays editable
	 * in the enum editor rather than being a hand-built asset the editor cannot open. The
	 * result says how many names were set and how many display names, and it says plainly
	 * that the asset is NOT yet saved: an unsaved asset dies with the editor, which has cost
	 * this project three times.
	 */
	UFUNCTION(meta = (AICallable))
	static FString EditTurboMCPEnumAdd(const FString& Path, const FString& Values);

	/**
	 * Add Blueprint variables in bulk.
	 *
	 * The same inputs, parsed and validated the same way, as `P-VARS-ADD`, because the two
	 * share one code path up to the write (`D-02`).
	 *
	 * EVERY NAME IS CHECKED BEFORE ANYTHING IS ADDED: a name that is already a variable, or
	 * already on the parent class, refuses the whole call with `unchanged:`. A list that
	 * added three of five and then refused would leave the Blueprint in a state the caller
	 * did not ask for, which is what `P8` forbids.
	 *
	 * The variables are then added, their flags, category and tooltip set, and the Blueprint
	 * compiled ONCE. The single compile is a requirement and not an optimisation: before it
	 * the skeleton class does not carry the new members, so a read-back would report the
	 * previous state.
	 *
	 * The result says how many were added AND how many were read back from the Blueprint,
	 * because a tool reporting its own intention is not evidence that it succeeded.
	 */
	UFUNCTION(meta = (AICallable))
	static FString EditTurboMCPVarsAdd(const FString& Asset, const FString& Variables);

	/**
	 * Mark widgets as variables, or unmark them, in one call.
	 *
	 * Pass a Widget Blueprint, one or more widget names separated by commas, and the value the
	 * flag should end up with. The flag is set through the editor's own
	 * `FWidgetBlueprintOperationUtils::ToggleWidgetAsVariable`, the same call the details
	 * panel's "Is Variable" checkbox makes, so a widget marked here is indistinguishable from
	 * one marked by hand.
	 *
	 * THIS TOOL HAS NO PREVIEW TWIN, deliberately rather than by omission. A preview exists to
	 * show the shape of a change a caller cannot see without making it, and a boolean flag has
	 * no shape. The result carries the OLD and the NEW value of the flag for every widget,
	 * which is what a preview would have shown.
	 *
	 * Every name is resolved before anything is changed, so a misspelled widget in a list
	 * leaves the tree untouched and the refusal lists the real names. A widget already in the
	 * requested state is reported as not touched rather than counted as changed.
	 */
	UFUNCTION(meta = (AICallable))
	static FString EditTurboMCPWidgetsAsVars(
		const FString& Asset,
		const FString& Widgets,
		bool bIsVariable);

	/**
	 * Create an override on purpose.
	 *
	 * The same inputs, resolved and validated the same way, as `P-OVERRIDE-ADD`, because the two
	 * share one code path up to the write (`D-02`).
	 *
	 * REFUSES a name that is already overridden and says WHERE, opening with `unchanged:`. The
	 * engine's own `AddFunctionOverride` does not check: it would quietly add a second graph,
	 * which leaves a Blueprint with two implementations of one function and no way to tell them
	 * apart afterwards.
	 *
	 * The override is created through the editor's own `UBlueprintEditorLibrary`, so it is the
	 * same thing the editor's Override dropdown makes. The result names the graph or node it
	 * created, prints the signature, and READS BACK whether the editor now lists the function as
	 * implemented — because a call that returns a graph is not evidence that the Blueprint
	 * agrees.
	 */
	UFUNCTION(meta = (AICallable))
	static FString EditTurboMCPOverrideAdd(
		const FString& Asset,
		const FString& FunctionOrEvent);

	/**
	 * Remove an override this Blueprint has.
	 *
	 * Pass a Blueprint and a name from `R-OVERRIDES` with `include=overridden`. An event override
	 * is a node in the ubergraph and a function override is a graph of its own, so both are looked
	 * for; a caller does not know which one they made.
	 *
	 * An Edit that can only CREATE is an Edit that cannot be undone, and `P8` says never leave the
	 * user worse off. It also keeps a test suite from consuming the asset: the override suite used
	 * to override one event per run, ran the test Blueprint out of free events, and broke two
	 * unrelated suites whose assertions counted things on the same asset.
	 *
	 * The result reads back whether the editor now reports the function as unoverridden, rather
	 * than trusting the removal, and lists the Blueprint's overrides when the name does not match
	 * one.
	 */
	UFUNCTION(meta = (AICallable))
	static FString EditTurboMCPOverrideRemove(
		const FString& Asset,
		const FString& FunctionOrEvent);

	/**
	 * Add a function to a Blueprint.
	 *
	 * Pass a Blueprint and a name. The name is checked BEFORE anything is created, so a refusal
	 * changes nothing.
	 *
	 * THREE REFUSALS, and the separation between them is the point of the spec:
	 *  - a name already used ON THIS BLUEPRINT, whether by a function graph, an event or a custom
	 *    event, is refused and the result NAMES what holds it;
	 *  - a name that matches a PARENT function is refused differently and points at
	 *    `P/E-OVERRIDE-ADD`, because a caller who wants to implement a parent function wants an
	 *    override, not a new function that would leave the parent's version untouched;
	 *  - a name that is not a usable identifier is refused with the character that is wrong.
	 *
	 * Call nodes and widget bindings that already name it are REPORTED rather than treated as
	 * fatal, because the caller may be about to build exactly that function.
	 *
	 * NO PREVIEW TWIN, per `03_TOOLS.md`. The graph comes from the editor's own
	 * `UBlueprintEditorLibrary::AddFunctionGraph`, and the result reads back whether the editor now
	 * lists the function as implemented rather than trusting the call's return value.
	 */
	UFUNCTION(meta = (AICallable))
	static FString EditTurboMCPFunctionAdd(const FString& Asset, const FString& Name);

	/**
	 * Remove a function from a Blueprint, so its name is free at once.
	 *
	 * Pass a Blueprint and the name of a function it has. The graph is removed through the editor's
	 * own `RemoveFunctionGraph`, which is what the editor's delete does.
	 *
	 * The result says whether the name is FREE AGAIN, checked with the same name test the add uses.
	 * When something else still holds it, that is named rather than left as a surprise on the next
	 * add. A name matching no function is refused with `unchanged:` and lists the functions the
	 * Blueprint does have.
	 */
	UFUNCTION(meta = (AICallable))
	static FString EditTurboMCPFunctionRemove(const FString& Asset, const FString& Name);

	/**
	 * Repair widget bindings after a class swap.
	 *
	 * The same finding as `P-WIDGET-RETARGET`, because the two share one code path (`D-02`).
	 *
	 * KEEPS every binding the widget's current class can still satisfy, and REMOVES each one it
	 * cannot, naming what was removed and why. A widget whose name no longer exists in the tree is
	 * treated as broken for the same reason.
	 *
	 * A removal is by the pair the engine itself uses as a binding's identity — `ObjectName` and
	 * `PropertyName` — rather than by list index, which would shift under the loop.
	 *
	 * The result RE-COUNTS the bindings from the Blueprint afterwards and says how many are still
	 * broken, rather than reporting `N of N`, which would prove nothing. Call nodes are reported
	 * and not touched.
	 */
	UFUNCTION(meta = (AICallable))
	static FString EditTurboMCPWidgetRetarget(const FString& Asset);

	/**
	 * Bind an event on a widget to a function name.
	 *
	 * THIS TOOL EXISTS BECAUSE `P/E-WIDGET-RETARGET` HAS NOTHING TO REPAIR WITHOUT IT. A binding is
	 * a `FDelegateEditorBinding` inside the Widget Blueprint, and the editor only writes one when a
	 * user picks an event in the details panel, which does not exist headless. Without this, the
	 * retarget capability could only be tested against an asset with NO bindings — a check that
	 * passes while proving nothing.
	 *
	 * The property named is looked for as a delegate ON THE WIDGET'S OWN CLASS, so binding a
	 * delegate the class does not carry is possible. That is exactly the state a class swap leaves
	 * behind, and the result SAYS which kind it made: a valid binding or a broken one. A caller
	 * testing the retarget tool wants the broken kind on purpose.
	 *
	 * A binding is identified by the widget and the property together, so binding the same pair
	 * again REPLACES the previous binding rather than adding a second, which is what the engine's
	 * own comparison and the editor's one-binding-per-property rule both require.
	 */
	UFUNCTION(meta = (AICallable))
	static FString EditTurboMCPWidgetBindAdd(
		const FString& Asset,
		const FString& Widget,
		const FString& Property,
		const FString& Function);

	/**
	 * Make a comment-box change.
	 *
	 * The same inputs, read and validated the same way, as `P-COMMENT`, because the two share one
	 * code path up to the write (`D-02`).
	 *
	 * THE DELETION NEVER TOUCHES THE NODES. A box is a label rather than a container, so removing it
	 * removes the label and nothing else. The result says how many nodes it held and re-counts the
	 * boxes afterwards, so the claim is CHECKED rather than asserted — getting that wrong would
	 * destroy a caller's work, which is why it is the one thing this tool states twice.
	 *
	 * `set` captures the old text, colour and size first, so the result reports what CHANGED rather
	 * than only what the values are now (`P4`), and a `set` that changes nothing opens with
	 * `unchanged:`.
	 */
	UFUNCTION(meta = (AICallable))
	static FString EditTurboMCPComment(
		const FString& Asset,
		const FString& GraphName,
		const FString& Action,
		const FString& Nodes,
		const FString& Box,
		const FString& Text,
		const FString& Color,
		const FString& Size);

	/**
	 * Turn the call log on or off.
	 *
	 * The choice is stored in the EDITOR SETTINGS, so it survives a restart, which `03_TOOLS.md`
	 * requires: "It starts unticked, and the choice persists in editor settings." It starts off,
	 * deliberately, because a log that switched itself on would make every user pay for it.
	 *
	 * The value is parsed strictly rather than through `FString::ToBool`, which answers false for
	 * anything it does not recognise — so a caller who typed `on` would silently turn the log OFF
	 * instead of being told their word was not understood. `true`/`on`/`yes`/`1` and
	 * `false`/`off`/`no`/`0` are accepted; anything else is refused by name.
	 *
	 * Calls made BEFORE it is switched on are not in the log, including engine tool calls, because
	 * the analytics provider only sees events from the moment it is installed.
	 */
	UFUNCTION(meta = (AICallable))
	static FString EditTurboMCPCallLog(const FString& Enabled);

	/**
	 * Drop the in-memory call log entries.
	 *
	 * Needs `confirm=yes`. ONLY THE IN-MEMORY ENTRIES ARE DROPPED: the log FILE on disk is left
	 * alone, because it is the record of what happened and erasing it is not this tool's job. The
	 * result says so, so a caller is not left believing the file was cleared too.
	 */
	UFUNCTION(meta = (AICallable))
	static FString EditTurboMCPCallLogClear(const FString& Confirm);
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
