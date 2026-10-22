// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"
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
 * Read tools return information. ReadTurboMCPPlayCheck is an execution-capable exception: it starts PIE and runs project code. Do not authorize the whole toolset as side-effect-free.
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
	 * Pass the asset, one or more words separated by whitespace, commas or pipes, and
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
	 * The tool waits for the registry scan before it reports a count, including a
	 * later scan of newly mounted content. If the registry is unavailable or its
	 * scan remains incomplete, it returns an error instead of treating that missing
	 * data as zero references.
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

	/**
	 * What the calls COST: which were most expensive, which were slowest, which failed, and the
	 * totals per tool and per asset.
	 *
	 * `R-CALL-LOG` answers "what was called". This answers "what did it cost", and it is a SECOND
	 * VIEW OVER THE SAME DATA RATHER THAN A SECOND PARSER: it reads the very object `R-CALL-LOG`
	 * reads, so the two cannot disagree about what a line says.
	 *
	 * **THE MOST EXPENSIVE AND THE SLOWEST ARE TWO SEPARATE ANSWERS, AND THAT IS DELIBERATE.** A
	 * call that returns a huge payload and a call that blocks the game thread are different problems
	 * with different fixes, so each is reported under its own heading with the basis it ranked by.
	 *
	 * **EVERY TOTAL CARRIES ITS DENOMINATOR** - the call count it was computed from. A total without
	 * one is a number nobody can judge, and a zero-finding answer with no denominator cannot be told
	 * apart from a scan that examined nothing.
	 *
	 * **AN EMPTY ANSWER NAMES WHICH EMPTINESS IT IS**, per `P1`: logging OFF, on with no entries, or
	 * on with entries the filter did not match. Reporting zeros for those would say the tools are
	 * cheap when in fact nothing was recorded. The same rule applies inside the answer: a ranking
	 * over rows that ALL read zero says so rather than naming a winner, because naming one implies a
	 * ranking existed.
	 *
	 * `toolset` NAMES ARE NORMALISED BEFORE TOTALLING. MEASURED: the log's two writers spell the same
	 * toolset two ways - the engine's resolved name arrives already qualified
	 * (`TurboMCP.TurboMCPReadToolset`) while this plugin writes it bare (`TurboMCPReadToolset`) - so
	 * without collapsing them ONE TOOLSET IS REPORTED AS TWO ROWS WITH TWO DENOMINATORS.
	 *
	 * `since` is an ISO-8601 timestamp taken from a previous answer; `filter` matches the toolset,
	 * the tool or the source; `cap` bounds the number of ROWS SHOWN, not the analysis, so a truncated
	 * list never makes a total wrong.
	 *
	 * This is a READ tool: it never writes, and it never clears.
	 */
	UFUNCTION(meta = (AICallable))
	static FString ReadTurboMCPCallCost(
		const FString& Since,
		const FString& Filter,
		const FString& Cap);

	/**
	 * The exact schema of one tool: its signature, one row per parameter, and its trap notes.
	 *
	 * Pass a toolset name and an empty tool to get one line per tool in it. The toolset is accepted in
	 * its FULL form and in its SHORT form, and a name that fits nothing is refused with the CLOSEST
	 * REAL NAMES rather than a bare "not found".
	 *
	 * TRAPS COME FROM A DATA FILE, not from code, so they are corrected without a rebuild. When that
	 * file cannot be read the answer SAYS SO rather than looking like "no traps".
	 *
	 * The ENGINE's own tools are covered by the same code path, because the schemas are read from the
	 * toolset registry. A tool's help is capped at 2000 characters, and what did not fit is named
	 * rather than silently dropped.
	 */
	UFUNCTION(meta = (AICallable))
	static FString ReadTurboMCPToolSchema(const FString& Toolset, const FString& Tool);

	/**
	 * Read the named reroutes of a material: every label, its readers, and the unused ones.
	 *
	 * Named reroutes are the material graph's own way of naming a wire. An UNUSED label is the thing
	 * this reader exists to find, because it is a wire somebody meant to reuse and did not.
	 *
	 * Call this FIRST on any material: the answer is short, and a caller who had to pass an argument
	 * would be tempted to skip it.
	 */
	UFUNCTION(meta = (AICallable))
	static FString ReadTurboMCPMaterialLabels(const FString& Asset);

	/**
	 * Read a whole material: its parameters, then every connected output as a tree.
	 *
	 * Works for a material, a material function and a material instance. An INSTANCE reports its
	 * parent, and `bIncludeInstanceOverrides` adds the parameters it overrides, which is where an
	 * instance's real behaviour lives.
	 *
	 * `TokenBudget` caps the answer. Zero means no cap; a non-zero budget is honoured by NAMING what
	 * was left out rather than by truncating silently.
	 */
	UFUNCTION(meta = (AICallable))
	static FString ReadTurboMCPMaterialLoad(
		const FString& Asset,
		bool bIncludeInstanceOverrides,
		int32 TokenBudget);

	/**
	 * Read ONE output of a material rather than all of them.
	 *
	 * The same reader as `R-MAT-LOAD` restricted to one output, which is what a caller wants when
	 * chasing a single problem rather than paying for the whole graph. The output property is
	 * REQUIRED, and an empty or unconnected one is refused by LISTING the connected ones.
	 */
	UFUNCTION(meta = (AICallable))
	static FString ReadTurboMCPMaterialTrace(
		const FString& Asset,
		const FString& OutputProperty,
		int32 TokenBudget);

	/**
	 * Read a material by BLOCK of logic, one block per comment box.
	 *
	 * A material graph is usually organised into comment boxes, and reading it block by block is how a
	 * caller answers "what does this part do" without reading the whole thing.
	 */
	UFUNCTION(meta = (AICallable))
	static FString ReadTurboMCPMaterialBlocks(const FString& Asset, int32 TokenBudget);

	/**
	 * Read a material INSTANCE: its parent chain, what it overrides, and - the list that matters - what it
	 * inherits from a parent it does not own.
	 *
	 * `03_TOOLS.md` `R-MAT-INST-LOAD` says the inherited list "is the one that matters", because an agent asked to
	 * change a parameter needs to know whether the value is REALLY ON THIS INSTANCE or inherited from a parent it
	 * does not own. A parameter the parent does not declare is reported as ORPHANED rather than dropped, because a
	 * renamed parameter leaves one behind.
	 *
	 * IT IS A SEPARATE READER FROM THE MATERIAL READER AND THAT IS `D-43`: an instance is a list of overrides, and
	 * "a text that hides them invites an edit that changes the wrong asset".
	 *
	 * A BASE MATERIAL IS REFUSED BY NAME rather than answered with everything inherited, because every parameter of
	 * a base material is editable on the material itself and the distinction this tool exists to make would not
	 * apply - say `R-MAT-LOAD` for the graph.
	 */
	UFUNCTION(meta = (AICallable))
	static FString ReadTurboMCPMaterialInstance(const FString& Asset);

	/**
	 * Play the project for a few seconds and report the warnings and errors it raised.
	 *
	 * THIS IS TWO CALLS, AND THAT IS THE ENGINE RATHER THAN A CHOICE. A tool call runs on the editor's game
	 * thread, and PIE is ticked on that same thread inside the editor's own tick, so a call that started a
	 * session and then waited would be blocking the one thread the session needs in order to run. It would
	 * hang, or report a window that never happened.
	 *
	 * So: pass `seconds` to REQUEST a session and return immediately; then call again with `seconds` EMPTY
	 * to collect. The second call answers `pending` while it is still running, and only reports once the
	 * engine has actually torn the session down.
	 *
	 * It uses the engine's own play controls, so what runs is exactly what the Play button runs. A session
	 * already running when you ask is LEFT ALONE and said so, because stopping someone else's play session
	 * is not this tool's decision. It stops only what it started.
	 *
	 * `map` overrides which map plays; empty means the project's default, which is what the Play button
	 * does. `seconds` is capped at 600.
	 *
	 * One session is watched at a time, so a second request while one is pending is refused by name rather
	 * than mixing two runs into one log. Returns warnings and errors only - not ordinary log lines - counted
	 * per source category, and it names anything the list cap left out.
	 */
	UFUNCTION(meta = (AICallable))
	static FString ReadTurboMCPPlayCheck(const FString& Seconds, const FString& Map);

	/**
	 * Read the trace: what is armed, and how often each point has run.
	 *
	 * Pass `action=status` with no node ID. Answers with whether the engine subscription is
	 * live, every armed point with its hit count, and the hits with their time, node and thread.
	 *
	 * This is the READ side, so it cannot arm or clear anything: asking it to do either is
	 * refused by name and the current state is returned instead. Call the Edit twin for changes.
	 *
	 * A zero is reported with its reason, because "nothing has run yet" and "nothing is armed"
	 * need different answers from whoever reads it.
	 */
	UFUNCTION(meta = (AICallable))
	static FString ReadTurboMCPTrace(const FString& Action, const FString& NodeId);

	/**
	 * Lint one or more Blueprints and report what is wrong with them, with each check NAMED.
	 *
	 * `assets` takes one or more asset names. `checks` filters by name - `dead-nodes`,
	 * `unnecessary-casts`, `expensive-tick`, `heavy-hard-references`, `unused-declarations`,
	 * `unconnected-pins` - and empty means every check. **`list` returns the check names and
	 * nothing else**, because `P1` says a filter that matches nothing must say what exists.
	 *
	 * EVERY CHECK REPORTS HOW MANY THINGS IT LOOKED AT, AND THAT COUNT IS THE POINT: a lint
	 * that says "no problems found" without a denominator is indistinguishable from a lint that
	 * examined nothing. A check that could not run says so rather than reporting zero.
	 *
	 * EVERY FINDING CARRIES A NODE ID, A SEVERITY AND ONE LINE SAYING WHY, and where a fix
	 * exists THE FINDING NAMES THE EXISTING TOOL RATHER THAN DESCRIBING ONE. The fix for dead
	 * nodes is `P/E-GRAPH-DEAD`; for backward placement it is `P/E-GRAPH-FIX-BACKWARD`.
	 *
	 * This is a READ tool and it NEVER EDITS - the reader toolset is structurally unable to
	 * write, which is what makes a lint's answer worth trusting.
	 */
	UFUNCTION(meta = (AICallable))
	static FString ReadTurboMCPBPLint(
		const FString& Assets,
		const FString& Checks,
		const FString& TokenBudget);

	/**
	 * The size map and hard references of one asset, SPLIT BY PROVENANCE.
	 *
	 * `D-52`: *"A size is reported with its provenance. Rejected: one total size. Why: the total names no cause and
	 * cannot be acted on. The check that matters is 'a heavy hard reference where a soft one would do', and that fact
	 * does not exist inside a total."* SO THIS PRINTS THE ASSET'S OWN SIZE, THEN WHAT IT PULLS IN SPLIT INTO HARD,
	 * SOFT AND EDITOR-ONLY, THEN THE LARGEST CONTRIBUTORS EACH WITH ITS SIZE AND WHY IT IS REFERENCED.
	 *
	 * `Asset` takes any accepted path form. `Cap` bounds the contributor list and defaults when not positive.
	 *
	 * AND `P1` GOVERNS EVERY FIGURE: a size that cannot be read is reported as UNKNOWN WITH THE REASON, NEVER AS
	 * ZERO - because a zero reads as "free", and an unread package is not a free one.
	 */
	UFUNCTION(meta = (AICallable))
	static FString ReadTurboMCPAssetCost(
		const FString& Asset,
		int32 Cap);

	/**
	 * Read a Widget Blueprint's tree as the text dialect of `04_FORMATS.md` section 8. READS ONLY.
	 *
	 * THE CHILDREN COME OUT IN SLOT ORDER, which is the whole reason this dialect exists: `R-BP-LOAD`'s widget
	 * section lists the same widgets but FLATTENS THE ORDER AWAY, and a tree's child order is its DRAW ORDER -
	 * dialect rule 8.2.3. So an agent that needs to know what draws on top of what reads the tree from here
	 * rather than from the flat list.
	 *
	 * CHILDREN ARE TAKEN THROUGH `UPanelWidget::GetChildrenCount` AND `GetChildAt`
	 * (`Components/PanelWidget.h:28,36`) IN INDEX ORDER, because THAT INDEX IS THE SLOT ORDER. The widget
	 * tree's own flat `AllWidgets` array is deliberately not used: it is the flattening this tool exists to
	 * undo.
	 *
	 * WHAT IT PRINTS IS EXACTLY WHAT `P/E-WIDGET-TREE` ACCEPTS, because the reader and the writer share one
	 * code path (`D-15`): `FTurboMCPWidgetTreeText::Render` is the only printer, and `append`, `replace` and
	 * `patch` all parse the same dialect. A round trip of this text is therefore IDENTITY, and
	 * `test-widget-tree` proves that by walking a live tree, rendering it, parsing it back and comparing -
	 * a comparison that needs no write at all.
	 *
	 * EVERY WIDGET CARRIES ITS PATH (`Root/Overlay/Card/PriceText`), which is the same form the writers
	 * address widgets by, so an agent never invents a name.
	 *
	 * A WIDGET BLUEPRINT WITH NO ROOT SAYS SO rather than printing nothing, because an absent tree and an
	 * empty one are different answers (`P1`). An asset that is not a Widget Blueprint is refused by name.
	 */
	UFUNCTION(meta = (AICallable))
	static FString ReadTurboMCPWidgetTree(
		const FString& Asset);

	/**
	 * Call the two PURE functions behind `R-CALL-COST` with LITERAL inputs, and print what they returned.
	 *
	 * THIS IS A TEST SURFACE AND THE GAP IT CLOSES IS MEASURED. `R-CALL-COST` totals calls by
	 * `FTurboMCPCallCost::NormaliseToolset` and attributes them by `FTurboMCPCallCost::AssetFromArguments`.
	 * ONE OF THE TWO IS UNREACHABLE FROM ANY SUITE THAT READS THE LOG: every entry in the real log is an
	 * ENGINE event, and an engine event carries NO `arguments` field at all - measured over 252 lines, not
	 * one of which had arguments - so nothing can hand `AssetFromArguments` a non-empty input. The
	 * commandlet written to close that gap cannot run: the `Kismet` dependency puts `check(GEditor);` in
	 * the way before any commandlet's `Main`. So the assertions run where every other check in this
	 * project runs: over MCP, from a suite.
	 *
	 * THE OTHER HALF IS REACHABLE, AND THAT IS SAID HERE BECAUSE IT CORRECTS THE RECORD RATHER THAN
	 * FLATTERING THIS TOOL. On a build that demonstrably compiled `return Trimmed;` in the normaliser's
	 * body, `test-call-cost` DOES go red (`no tool row contains the engine's qualification path`), because
	 * the log really does carry qualified names. What this tool adds is therefore not a check that cannot
	 * otherwise fail but ONE THAT CANNOT GO VACUOUS: the log-dependent check bites only while the log
	 * carries a qualified name, which is a fact about the engine's hash resolver, while a literal input is
	 * the same assertion in every session.
	 *
	 * `toolsets` takes one or more toolset SPELLINGS separated by commas or pipes. Pass BOTH spellings of
	 * one toolset to see them collapse: the analytics provider resolves names fully qualified, so an
	 * engine entry records `TurboMCP.TurboMCPReadToolset`, while this plugin's own writer records the bare
	 * `TurboMCPReadToolset`. Each spelling is printed with the key the normaliser gave it, and the number
	 * of DISTINCT keys is printed as a NUMBER, so "one key" is read off a count and never inferred from a
	 * sentence. An empty list says that no spelling was given rather than reporting zero keys (`P1`).
	 *
	 * `arguments` takes an arguments blob exactly as the log records one. The asset the function found is
	 * printed with its LENGTH as a number, so "names no asset" is a measured zero: an engine event has no
	 * arguments at all, and an absent measurement is not zero cost (`P1`). Line breaks in the echo are
	 * written as spaces so one value stays on one line; the function is given the caller's own string.
	 *
	 * IT COMPUTES STRINGS AND WRITES NOTHING - no log is opened, no asset is loaded, no setting is
	 * toggled - so it needs no fixture, it cannot race another suite, and it has nothing to restore.
	 */
	UFUNCTION(meta = (AICallable))
	static FString ReadTurboMCPPureChecks(
		const FString& Toolsets,
		const FString& Arguments);

	/**
	 * Read what source control holds for each asset, and refuse by name when it holds nothing.
	 *
	 * Pass assets separated by commas or pipes, in any path form; pass nothing to read only whether the
	 * project has source control on and which provider is selected.
	 *
	 * BOTH OF THE ENGINE OWN QUESTIONS ARE ASKED, IN ITS OWN ORDER, BEFORE ANYTHING ELSE IS TOUCHED:
	 * whether a provider is selected at all, and whether that provider is available. `SourceControlHelpers.cpp:61-85`
	 * is the engine own gate and it asks exactly these two. BOTH ARE REQUIRED, and that is measured rather than
	 * defensive: Perforce (`PerforceSourceControlProvider.cpp:232-235`), Plastic
	 * (`PlasticSourceControlProvider.cpp:283-286`) and Subversion (`SubversionSourceControlProvider.cpp:79-82`)
	 * answer the FIRST with true UNCONDITIONALLY while the second is false without a server - so a tool that
	 * asked only the first would report a working provider where there is none, which is the one answer this
	 * tool must never give.
	 *
	 * WHEN EITHER ANSWER IS NO, EVERY ASSET IS REFUSED BY NAME, with the engine own wording reproduced
	 * verbatim: `"Revision control is not enabled."` (`SourceControlHelpers.cpp:67`) or
	 * `"Revision control server is currently not available."` (`:76`). NO PROVIDER CALL IS MADE AT ALL,
	 * because with the default provider `GetState` returns Failed with a NULL state
	 * (`DefaultSourceControlProvider.cpp:52-60`) that a reader can mistake for "not under source control".
	 * No state is printed and no zero is printed, for the same reason (`P1`).
	 *
	 * WHEN BOTH PASS, each asset reports: whether it is under source control, whether it is checked out here,
	 * whether it is out of date with respect to the depot, whether it is modified, and the provider own text
	 * for its state. A provider that hands back NO state even so is reported as UNKNOWN WITH THE REASON
	 * rather than as "not under source control", because an absent measurement is not a measured absence.
	 *
	 * ITS EDIT TWIN IS `EditTurboMCPVcsCheckout` and its preview is `PreviewTurboMCPVcsCheckout`; all three
	 * read one plan, so they refuse the same input in the same words.
	 *
	 * THIS IS A READ TOOL. It never checks anything out and it never writes a file.
	 */
	UFUNCTION(meta = (AICallable))
	static FString ReadTurboMCPVcsState(const FString& Assets);
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

    /** Read node IDs, positions and measured sizes without editing the graph. */
    UFUNCTION(meta = (AICallable))
    static FString LookTurboMCPGraphPositions(const FString& Asset, const FString& GraphName);

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
	 * Capture a picture of a Blueprint's graph editor, framed one of seven ways.
	 *
	 * Pass the asset and the graph name, and optionally a mode, a target, and a size.
	 * Empty mode means `graph`.
	 *
	 * Modes taking no target: `graph` frames the whole graph, `screen` frames the whole
	 * editor window at its current size, and `selection` frames the nodes selected on
	 * the graph canvas of the graph named.
	 *
	 * Modes taking a target: `box` takes a comment box INDEX, numbered from 0 in the
	 * same order L-LAYOUT-REPORT lists them, top to bottom then left to right, so an
	 * index means the same thing in both tools. `node` takes a node ID from
	 * R-NODE-FIND. `neighbours` takes a node ID and frames that node plus every node
	 * within N hops, where N is passed in Size as a whole number and empty means 0.
	 * `box-neighbours` takes a comment box index in Target and N in Size, and frames
	 * that box plus the N boxes whose nodes are linked to its nodes.
	 *
	 * Size means the picture size as WxH for `graph`, `box` and `node`, and empty means
	 * 1600x900. FOR `neighbours` AND `box-neighbours` IT MEANS THE HOP COUNT, because
	 * those framings compute their own picture size from the frame, exactly as `node`
	 * and `box` already do.
	 *
	 * `selection` means the GRAPH CANVAS SELECTION and nothing else: it is read from
	 * SGraphEditor::GetSelectedNodes (`GraphEditor.h:394`). The level actor selection
	 * (`GEditor->GetSelectedActors`, `Editor/EditorEngine.h:1969`), the component
	 * selection (`:2000`) and the Content Browser asset selection are DIFFERENT
	 * SELECTIONS and this mode reads none of them, so it is a refusal rather than a
	 * surprising picture. With nothing selected it refuses and says so.
	 *
	 * `neighbours` counts a hop over the graph the READER prints, so a link through a
	 * plain reroute knot is one hop and not two (`D-42`), and a knot is never framed.
	 * Each of the three selectors PRINTS ITS OWN DEFINITION in the result, so a caller
	 * can check the claim instead of trusting it.
	 *
	 * `screen` frames the whole editor window with the Slate renderer, at the window's
	 * current size unless the 4096 edge cap scales it. IT IS REFUSED BY NAME when the
	 * editor runs with `-nullrhi`, because `LaunchEngineLoop.cpp:3343` then installs the
	 * NULL Slate renderer at `:3345-3347` (`GUsingNullRHI`, `RHIGlobals.h:894`) and
	 * there is no rendered pixel to read: a black image would look like a successful
	 * capture of a dark window.
	 *
	 * A framing that cannot be done is an error naming why, and it never falls back to
	 * a wider frame (`D-41`): asking for box 9 of a graph with 4 boxes says exactly
	 * that. An empty frame is refused rather than written as a blank image, because a
	 * blank image looks like a successful capture of nothing.
	 *
	 * The result reports what it framed, the frame's bounds in graph coordinates, the
	 * zoom it chose, and the image size. The PNG goes to a fixed folder under the
	 * project's Saved folder, and its name says which asset, graph and mode produced it.
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

	/**
	 * Work out which nodes are dead and would be removed, without removing them.
	 *
	 * A candidate is a DISCONNECTED GROUP that nothing reaches from any entry AND whose writes are
	 * never read. Every group found is listed with the reason it would be KEPT, when it would be, so
	 * a caller sees the decision rather than only its outcome.
	 *
	 * The spec names three rules and all three are enforced:
	 *  - a group holding an event, a custom event or a function entry is NEVER removed, because a
	 *    hook the user made on purpose looks exactly like one that nothing calls;
	 *  - a group whose write is read by live code is kept;
	 *  - the preview says for each group whether it REPEATS LIVE LOGIC, which is what a reader needs
	 *    before agreeing to delete a copy of something.
	 *
	 * REACHABILITY IS FOLLOWED ACROSS EXEC AND DATA WIRES, deliberately generously. A pure node with
	 * no exec wire reaching it can still be live if a live node reads its value, so judging liveness
	 * on exec alone would delete nodes the Blueprint computes with. The cost is that this tool errs
	 * toward KEEPING things, which is the only safe direction for a delete.
	 */
	UFUNCTION(meta = (AICallable))
	static FString PreviewTurboMCPGraphDead(const FString& Asset, const FString& GraphName);

	/**
	 * Work out which values could become local variables, without promoting any.
	 *
	 * The candidates are values computed by PURE nodes and read MORE THAN ONCE in a function. Each is
	 * marked SAFE or UNSAFE with the reason, which is what the spec asks for.
	 *
	 * SAFE MEANS everything feeding the value is a FUNCTION INPUT or a LITERAL, applied
	 * TRANSITIVELY. Both halves of that are the point rather than pedantry:
	 *  - capturing a value once and reading it twice is only the same as computing it twice when the
	 *    value cannot change between the two reads. A value derived from a member variable can change
	 *    if the exec chain writes it; one derived from an impure node can differ each time.
	 *  - the rule is TRANSITIVE because a one-level check would call `A(input) -> B(A) -> read twice`
	 *    safe for B without ever looking at A.
	 *
	 * ONLY FUNCTION GRAPHS can hold a local, and that is asked of the engine rather than assumed. A
	 * graph that cannot is reported with that as the reason rather than skipped silently.
	 */
	UFUNCTION(meta = (AICallable))
	static FString PreviewTurboMCPLocalVars(const FString& Asset, const FString& GraphName);

	/**
	 * Finds the REAL node type id the engine's node creation tool will accept.
	 *
	 * Give plain words and it matches them against every id this graph accepts, normalising both sides
	 * the same way: case, spaces, underscores, brackets, and the category part before the last `|`.
	 * It finds functions and variables added in this session, and says when the asset must be compiled
	 * first for the engine's own tool to accept one.
	 *
	 * Each match gives the exact id, the class that declares it, whether it is pure, and why it
	 * matched. When the id this plugin's graph text uses differs from the id the engine creates, BOTH
	 * are printed, because one matches graph text and the other creates the node.
	 *
	 * Results are capped at ten unless `limit` says otherwise, and a miss is refused with the closest
	 * real names. `P-NODE-PINS` gives the pins of an id this returns.
	 */
	UFUNCTION(meta = (AICallable))
	static FString PreviewTurboMCPNodeType(
		const FString& Asset,
		const FString& GraphName,
		const FString& Words,
		const FString& Limit);

	/**
	 * The pins a node type produces, WITHOUT TOUCHING the asset's own graph.
	 *
	 * Each pin gives its name, its type, its direction in the spelling the engine's pin tools expect
	 * (`EGPD_Input` or `EGPD_Output`), its default, and its position COUNTED SEPARATELY for inputs and
	 * outputs, which is what the engine does.
	 *
	 * It also says whether the node is pure or latent, and when a latent node is not allowed in this
	 * graph kind it says that instead of leaving you to find out at compile time.
	 *
	 * The node is built in a THROWAWAY graph and discarded, so the asset's node count does not change.
	 * The engine's own reader adds a probe to the real graph and LEAVES IT THERE, which this
	 * deliberately does not repeat.
	 */
	UFUNCTION(meta = (AICallable))
	static FString PreviewTurboMCPNodePins(
		const FString& Asset,
		const FString& GraphName,
		const FString& TypeId);

	/**
	 * Work out which wires cross a node, and where a knot would fix them, without placing any.
	 *
	 * T-LAY-10's contract, and it is narrower than the prose: the preview lists ONLY wires that CROSS A
	 * NODE, and after the edit those wires are straighter than they were.
	 *
	 * A wire is reported ONCE, naming every node it crosses, even when it crosses several: counting once
	 * per crossing would list one wire three times and make the before and after counts incomparable.
	 *
	 * A knot is only offered where the route through it is ONE RUN PLUS ONE BEND. The position is one of
	 * exactly two: level with the source pin, or level with the target pin.
	 *
	 * REFUSED, and the preview says which and why: a wire between neighbouring columns, a wire with no
	 * free space at either candidate position, and a wire already carrying a knot. Sharing is offered
	 * when one pin feeds several distant nodes, and refused with the reason when no single position
	 * suits them all.
	 *
	 * WHEN THE ENGINE HAS MEASURED NO NODE SIZES the answer SAYS SO and judges crossings against a small
	 * square at each node position, which UNDER-REPORTS rather than inventing a crossing. That is not
	 * unusual: NodeWidth is written only by Slate when a node is drawn, so a graph built headlessly has
	 * no measured sizes until something draws it.
	 */
	UFUNCTION(meta = (AICallable))
	static FString PreviewTurboMCPGraphKnots(const FString& Asset, const FString& GraphName);

	/**
	 * Work out a clean layout for one graph, WITHOUT moving anything.
	 *
	 * Assigns each node a COLUMN from what it depends on, spaces the rows, and lays each COMMENT BOX out
	 * AROUND ITS OWN CONTENTS, so a nested box is sized from the boxes inside it.
	 *
	 * MEASURES AND ROLLS BACK, like the knot pass: the total wire length is measured before and after, and
	 * the plan is offered only when the result is SHORTER. `bForce` offers it regardless, so a caller who
	 * disagrees with the measure can still see the plan.
	 *
	 * REFUSES rather than guesses: a graph with a cycle, a graph with no nodes, and a graph whose nodes
	 * have NO MEASURED SIZE are each refused by name. That last one is not unusual - NodeWidth is written
	 * only by Slate when a node is drawn, so a graph built headlessly has no measured sizes until
	 * something draws it.
	 *
	 * `NodeIds` scopes the pass to a block. An empty list tidies the whole graph; otherwise every
	 * unselected node is a fixed anchor and only the named nodes may move. The block is moved to clear
	 * space rather than through its anchors. The preview reports the planned score and the edit reports
	 * the score it actually measured after writing.
	 *
	 * HorizontalGap/VerticalGap set full-detail clearance; LanePitch is minimum Sequence row spacing, expanded for tall pure trees. MainPath accepts "auto" for the longest acyclic execution path, or comma-separated connected reader node IDs for the top exec lane. Auto refuses execution cycles.
	 * The number it prints is the TOTAL WIRE LENGTH, which is what this pass is judged on. `P-GRAPH-KNOTS`
	 * is judged on WIRES CROSSING NODES instead, and the two numbers are deliberately not comparable.
	 */
	UFUNCTION(meta = (AICallable))
	static FString PreviewTurboMCPGraphTidy(
		const FString& Asset,
		const FString& GraphName,
		bool bForce,
		const TArray<FString>& NodeIds,
		const FString& MainPath = TEXT(""), double HorizontalGap = 120.0, double VerticalGap = 100.0, double LanePitch = 280.0);

	/**
	 * Repair the bad spots in a mostly-good graph, WITHOUT moving anything.
	 *
	 * A LOCAL pass, not a layout. It fixes three specific things and leaves the rest alone: an exec wire
	 * that RUNS BACKWARD, a GETTER sitting to the right of the node that reads it, and a LONE NODE left
	 * at the origin next to nothing.
	 *
	 * IT NEVER MOVES A COMMENT BOX, and that is a property of the type rather than a promise: the plan
	 * carries no box position at all, so there is nothing to write one from.
	 *
	 * MEASURES AND ROLLS BACK like the tidy pass: the total wire length is measured on the PLANNED
	 * positions and the plan is offered only when the result is SHORTER. `bForce` offers it anyway, and
	 * it lives HERE rather than on the edit so the preview can state a rollback the edit would perform.
	 *
	 * REFUSES rather than guesses, naming the reason. Where the engine has measured NO NODE SIZES it uses
	 * a documented fallback square and SAYS SO, under-reporting rather than inventing a position. That is
	 * not unusual: NodeWidth is written only by Slate when a node is drawn.
	 *
	 * The number it prints is the TOTAL WIRE LENGTH, the same quantity `P-GRAPH-TIDY` reports, so the two
	 * are comparable. `P-GRAPH-KNOTS` reports WIRES CROSSING NODES and is deliberately not comparable.
	 */
	UFUNCTION(meta = (AICallable))
	static FString PreviewTurboMCPGraphFixBackward(
		const FString& Asset,
		const FString& GraphName,
		bool bForce);

	/**
	 * Check graph text before writing it, and write nothing.
	 *
	 * The gate every write entry point depends on. Feed it exactly the text a reader
	 * printed - there is no second dialect - and it parses, resolves and checks it,
	 * then reports every unknown node type with the closest real IDs, every pin the
	 * node does not have with the real names, every literal that cannot be the pin's
	 * type naming both sides, and every construct it will refuse with the reason.
	 * Each finding carries its line and column.
	 *
	 * Call this BEFORE P-GRAPH-APPEND, P-GRAPH-REPLACE or P-GRAPH-PATCH. Those three
	 * take this same text and reach the same decision through this same check, so a
	 * text that passes here is one they accept - and a text that fails here names
	 * what to fix rather than failing halfway through a write.
	 *
	 * A graph name is required, not optional: a node type and a pin name are both
	 * resolved against a live graph, so an unnamed graph is refused rather than
	 * guessed at.
	 *
	 * Writes nothing, and cannot. It is in this toolset, which holds no tool that can
	 * change an asset.
	 */
	UFUNCTION(meta = (AICallable))
	static FString PreviewTurboMCPGraphCheck(
		const FString& Asset,
		const FString& GraphName,
		const FString& Text);

	/**
	 * Add a block to a graph from graph text, and write nothing.
	 *
	 * Use this before the edit. It reports every way the text cannot be applied, each
	 * naming its line, and it reports what WOULD be created - so a caller can see the
	 * change before agreeing to it.
	 *
	 * It adds nodes and links them to existing nodes BY NODE ID. It CANNOT REMOVE: the
	 * code path that would delete a node does not exist in this capability, which is
	 * what separates it from P-GRAPH-REPLACE. A form carrying a node id is read as a
	 * LINK to that node, never as a second copy of it. To connect two nodes created in one
	 * block, put `; label First` under the first form and write `(-> First:then)` in the
	 * second form's input pin. A local label is unique within that submitted block.
	 *
	 * The text is the same dialect the readers print, checked by P-GRAPH-CHECK first, so
	 * a text this accepts is one the edit accepts.
	 *
	 * Writes nothing, and cannot. It is in this toolset, which holds no tool that can
	 * change an asset.
     * Local labels name the PRECEDING node form. Prefer `(TypeId) ; label Name` for a pinless node,
     * or put `; label Name` on its own line after the closed form. Never put a label before its node.
     * Reference it as `(-> Name:ExactPinName)`. A pinless form needs no dummy pin. Semicolon comments
     * consume the rest of their line: put the closing parenthesis BEFORE a trailing label/comment.
	 */
	UFUNCTION(meta = (AICallable))
	static FString PreviewTurboMCPGraphAppend(
		const FString& Asset,
		const FString& GraphName,
		const FString& Text);

	/**
	 * Rewrite the body of one function, override or event, and list what it removes.
	 *
	 * Replaces the body rather than adding to it, so it is the FIRST writer that can remove
	 * anything. It writes into the EXISTING entry and result nodes and never adds a second
	 * entry, and it keeps the nodes the text does not describe as removals.
	 *
	 * Returns the nodes to create, the nodes to LINK to, and - the part that matters here - the
	 * nodes it would REMOVE, each by key with its class and title. A removal is LISTED rather
	 * than counted, and every entry, result, event and comment is kept, because removing one
	 * would delete a signature or an author's documentation.
	 *
	 * Writes nothing, and cannot. It is in this toolset, which holds no tool that can
	 * change an asset.
	 */
	UFUNCTION(meta = (AICallable))
	static FString PreviewTurboMCPGraphReplace(
		const FString& Asset,
		const FString& GraphName,
		const FString& Text);

	/** Preview latest global undo transaction only if exclusively TurboMCP and the named asset. */
	UFUNCTION(meta = (AICallable))
	static FString PreviewTurboMCPAssetUndo(const FString& Asset);

	/** Translate selected nodes or entire comment groups, including nested boxes; keeps internal layout and wiring. Refuses new collisions or changed external box membership. */
	UFUNCTION(meta = (AICallable))
	static FString PreviewTurboMCPGraphGroupMove(const FString& Asset, const FString& GraphName, const TArray<FString>& NodeIds, int32 DeltaX, int32 DeltaY);

	/** Preview reload of saved Blueprint package; discards all unsaved edits, may reset undo. Edit requires the preview confirmation. */
	UFUNCTION(meta = (AICallable))
	static FString PreviewTurboMCPAssetRevert(const FString& Asset);

	/** Preview call swap. Options is optional JSON: {"pin_map":{"Old":"New"},"new_inputs":{"NewInput":{"node":"reader node ID","pin":"OutputPin"}}}. Unmapped pins retain exact names; keepOld defaults true. */
	UFUNCTION(meta = (AICallable))
	static FString PreviewTurboMCPGraphSwapCall(
		const FString& Asset,
		const FString& GraphName,
		const FString& NodeId,
		const FString& ReplacementTypeId,
		const FString& KeepOld,
		const FString& Options = TEXT(""));

	/** Preview repointing every reader of one output to an existing or newly created source. */
	UFUNCTION(meta = (AICallable))
	static FString PreviewTurboMCPGraphRepoint(
		const FString& Asset, const FString& GraphName, const FString& SourceNodeId,
		const FString& SourcePin, const FString& TargetNodeId, const FString& TargetTypeId,
		const FString& TargetPin, const FString& KeepSource);

	/** Preview graph repoints without edits. Operations accepts a JSON array or an object with an operations array.
     * Example: [{"asset":"/Game/BP_Example","graphName":"EventGraph","sourceNodeId":"reader ID","sourcePin":"then","targetNodeId":"reader ID","targetPin":"execute","keepSource":true}]
     * Field names match GraphRepoint (case-insensitive); graph is an alias for graphName. Optional id defaults to operation-N; optional kind defaults to graphRepoint. Other kinds refuse. Compile/save are deferred. Operations are preflighted in order on transient graph copies; later operations see earlier changes. */
	UFUNCTION(meta = (AICallable))
	static FString PreviewTurboMCPBatchEdit(const FString& Operations);

    /** Connect exact existing node/pin IDs. Data replaces the target input source; exec replaces only the named source output destination and preserves other incoming target paths. No automatic continuation or casts. Same scratch preview and one-undo edit; compile/save deferred. Preview first. */
    UFUNCTION(meta = (AICallable))
    static FString PreviewTurboMCPGraphConnect(const FString& Asset,const FString& GraphName,const FString& SourceNodeId,const FString& SourcePin,const FString& TargetNodeId,const FString& TargetPin);

    /** Remove exact nodes and links with scratch preview and one undo. Nodes is a JSON array of reader node IDs. Links is a JSON array of objects with sourceNodeId, sourcePin, targetNodeId, targetPin. Use [] for either unused list. Node deletion also removes incident links; other nodes/positions are preserved. No automatic reconnection, compile or save. */
    UFUNCTION(meta = (AICallable))
    static FString PreviewTurboMCPGraphRemove(const FString& Asset,const FString& GraphName,const FString& Nodes,const FString& Links);


    /** Copy a pure self/getter/literal per consumer; never impure calls. Preserves defaults and input sources. Run layout afterwards. */
    UFUNCTION(meta = (AICallable))
    static FString PreviewTurboMCPGraphSplitValue(const FString& Asset,const FString& GraphName,const FString& NodeId);

    /** Refactor an explicitly ordered linear chain of synchronous calls into Sequence rows. Groups is JSON [["reader ID 1","reader ID 2"],["reader ID 3"]]. Keeps original execution order and puts continuation last; refuses latent/branching/cyclic groups. Optional trivial-value splitting; one undo, no compile/save. */
    UFUNCTION(meta = (AICallable))
    static FString PreviewTurboMCPGraphSequence(const FString& Asset,const FString& GraphName,const FString& Groups,bool bSplitValues = true);


	/**
	 * Change only what changed: name existing nodes by id and give the pins that differ.
	 *
	 * Read the graph first, then submit ONLY the nodes whose pin values you want to change, each
	 * with its node id from the reader. Nodes you do not mention are LEFT ALONE, and it counts
	 * and names the nodes it would change - which is the number that matters, because a patch
	 * that touches a third node has failed even when the graph still compiles.
	 *
	 * It CHANGES and does not ADD or REMOVE. A form with no node id is refused and told which
	 * tool adds nodes, because "applies only the differences" is not "adds a block" - that is
	 * P/E-GRAPH-APPEND - and removing is P/E-GRAPH-REPLACE. A form that names a node but writes
	 * no pin value changes nothing, and is refused rather than reported as a change of zero.
	 *
	 * Returns the nodes that would change, each by key with its class, and the count of nodes
	 * named against the count that would actually differ. Writes nothing, and cannot. It is in
	 * this toolset, which holds no tool that can change an asset.
	 */
	UFUNCTION(meta = (AICallable))
	static FString PreviewTurboMCPGraphPatch(
		const FString& Asset,
		const FString& GraphName,
		const FString& Text);

	/**
	 * Lay out a material, one comment box per block, and change nothing.
	 *
	 * Use this before `E-MAT-TIDY` to see what would move, which blocks would be boxed, and what the
	 * tool refuses. It reads the material, plans the columns and names every expression whose position
	 * would change. It writes nothing, and it does not claim the compiled result is unchanged - that
	 * claim is made by the edit, which measures it.
	 *
	 * A MATERIAL EXPRESSION STORES NO SIZE AT ALL, so every expression is planned at a documented
	 * fallback size and the positions are RELATIVE. The answer says so, because under-reporting the
	 * space a layout needs is acceptable and inventing a measurement is not.
	 *
	 * Pass anything in `force` to plan even when the material is already as this pass would leave it.
	 */
	UFUNCTION(meta = (AICallable))
	static FString PreviewTurboMCPMaterialTidy(
		const FString& Asset,
		const FString& Force);

	/**
	 * List the values in a material worth a name, and change nothing.
	 *
	 * A VALUE IS WORTH A NAME WHEN IT IS READ IN SEVERAL PLACES, which is what the specification asks
	 * for - used far away or in several boxes. A value read in exactly one place is not shared, so a
	 * label would add a node and a name and save nothing. Distinct READERS are counted rather than
	 * inputs, so one expression that reads the same value into two of its own inputs counts once.
	 *
	 * EACH CANDIDATE CARRIES A PROPOSED NAME TAKEN FROM CONTEXT, which is the part of the specification a
	 * list of values alone would not satisfy. The name comes from the expression's own caption, which is
	 * what the material editor draws, and it is printed with the places that would read it and with
	 * whether the name already exists.
	 *
	 * It writes nothing, and it does not claim the compiled result is unchanged - that claim is made by
	 * the edit, which measures it and rolls back if it is not the same.
	 */
	UFUNCTION(meta = (AICallable))
	static FString PreviewTurboMCPMaterialLabelPromote(
		const FString& Asset);

	/**
	 * `P-MAT-BLOCKS`: make labels follow blocks.
	 *
	 * `Spec/03_TOOLS.md`: "Preview: the labels to add, usage nodes to merge, in-block reroutes to remove,
	 * following the block rule in `05_DECISIONS.md` D-17."
	 *
	 * THE THREE LISTS ARE THE THREE WAYS D-17 CAN BE VIOLATED, and each comes from the SAME geometric block
	 * membership `R-MAT-BLOCKS` computes - read through `FTurboMCPMaterial::BlockIndexOf`, which is public
	 * for exactly this reason. A writer that disagreed with the reader about which box holds what would
	 * move values between blocks the reader does not believe in.
	 *
	 *   - a value read from MORE THAN ONE BLOCK with no label, where D-17 says a value leaving its block
	 *     travels as a named reroute;
	 *   - a label read through TWO OR MORE usage nodes in ONE block, where D-17 says a block reads each
	 *     label through a single usage node;
	 *   - a PLAIN REROUTE whose source and readers are all in ONE block, where D-17 says plain reroute dots
	 *     are for routing long wires and never stand for a value.
	 *
	 * A MATERIAL WITH NO COMMENT BOX IS A THIRD ANSWER RATHER THAN A SILENT SUCCESS. D-17 is about the
	 * boundary between blocks, and one block has no boundary, so the refusal says so and names
	 * `P/E-MAT-TIDY` as the tool that would create the blocks first.
	 *
	 * Every item is printed with the keys the edit would touch, because "changes exactly the items in the
	 * Preview list" is only checkable if the list is in the text.
	 *
	 * It writes nothing, and it does not claim the compiled result is unchanged - that claim is made by the
	 * edit, which measures it and rolls back if it is not the same.
	 */
	UFUNCTION(meta = (AICallable))
	static FString PreviewTurboMCPMaterialBlocks(
		const FString& Asset);

	/**
	 * Preview row operations on a DataTable. WRITES NOTHING.
	 *
	 * `Rows` is one operation per line: `name = field=value, field=value` to set a row, or `- name` to remove one.
	 * The values are the row struct's OWN export text, because they are parsed by the engine's own importer rather
	 * than by a format invented here. A field the struct does not declare is refused with the real field names.
	 *
	 * EVERY REMOVAL IS LISTED BY NAME AND NEVER COUNTED, and every changed field is printed with the value it has
	 * now and the value it would get. A plan with any refusal prints the refusals and writes nothing.
	 */
	UFUNCTION(meta = (AICallable))
	static FString PreviewTurboMCPDataTableRows(
		const FString& Asset,
		const TArray<FString>& Rows);

	/**
	 * Plan copying a block of nodes from one graph to another, and write nothing.
	 *
	 * The boundary comes from the links that CROSS the selection rather than from a pin list
	 * (`D-47`), and there are THREE outcomes rather than two: a crossing link whose far end is
	 * reachable in the target is re-linked, one whose far end is not is REPORTED BY NAME and left
	 * unconnected, and a member the target asset does not declare REFUSES the whole copy and names
	 * every missing one - because half a block is worse than none.
	 *
	 * IT IS IN THIS TOOLSET, WHICH HOLDS NO TOOL THAT CAN CHANGE AN ASSET, so the plan cannot write
	 * even by accident. Its Edit twin reaches the same plan, so everything refused here is refused
	 * there in the same words.
	 */
	UFUNCTION(meta = (AICallable))
	static FString PreviewTurboMCPGraphCopy(
		const FString& SourceAsset,
		const FString& SourceGraph,
		const TArray<FString>& NodeIds,
		const FString& TargetAsset,
		const FString& TargetGraph,
		const FString& AnchorNodeId);

	/**
	 * Plan extracting a selection into a function or a macro, and write nothing.
	 *
	 * `E-GRAPH-COLLAPSE` IS MEASURED REACHABLE BUT NOT YET BUILT, and this half says so rather than
	 * refusing vaguely: the engine's own `FBlueprintEditor::CollapseNodesIntoGraph` is a member of an
	 * ASSET-EDITOR UI OBJECT (`BlueprintEditor.h:1125`), and whether that instance can be constructed
	 * outside an open asset editor is the ONE question the measurement left open. Everything the plan
	 * checks - the boundary, the refusal of an entry, event or result node by name, and the refusal of
	 * a selection with no exec boundary - is decided here, so the Preview and a future Edit twin
	 * cannot disagree about what is acceptable.
	 */
	UFUNCTION(meta = (AICallable))
	static FString PreviewTurboMCPGraphCollapse(
		const FString& Asset,
		const FString& GraphName,
		const TArray<FString>& NodeIds,
		const FString& MemberName,
		const FString& MemberKind);

	/**
	 * Work out what creating a user struct asset would do, without creating it.
	 *
	 * Pass a package path such as `/Game/Tests/MyStruct` and the fields. Each field is
	 * `Name: Type`, followed by any of `default=`, `tooltip=` and `category=`. Fields are
	 * separated by commas at the top level, so a type with a comma in it such as
	 * `Map<String, Int>` stays one field.
	 *
	 * Basic types are bool, int, int64, byte, real, float, double, name, string and text.
	 * Struct names are Vector, Rotator, Transform, Vector2D, LinearColor and Color. For an
	 * enum or a struct asset, pass its path. Containers are `Array<T>`, `Set<T>` and
	 * `Map<K, V>`.
	 *
	 * A path that is not absolute, a field name that is not a usable identifier, a duplicate
	 * field name, an unknown field option, an unbalanced container type and a type that
	 * resolves to nothing are each refused BY NAME. These checks and the ones below are in the
	 * PLAN, so this Preview and its Edit twin refuse exactly the same input (`D-02`), and this
	 * writes nothing.
	 *
	 * It also says whether the asset already exists, because that fact is what decides whether
	 * the Edit will refuse rather than overwrite.
	 */
	UFUNCTION(meta = (AICallable))
	static FString PreviewTurboMCPStructNew(const FString& Path, const FString& Fields);

	/**
	 * Work out what editing a user struct's fields would do, without changing any.
	 *
	 * Pass a user struct asset and one or more changes, separated by commas at the top level.
	 * A change is one of:
	 *
	 *   - `Name: Type` - ADDS the field, or CHANGES its type when the struct already has that
	 *     name. Which one it is follows from the struct, not from the caller's guess, so the
	 *     Preview and the Edit cannot disagree about it.
	 *   - `move:Name, above=Other` or `move:Name, below=Other` - REORDERS.
	 *   - `remove:Name` - REMOVES, and is refused unless it carries `force=<count>`.
	 *
	 * A field is addressed BY ITS DISPLAY NAME, and that is the key. `FStructureEditorUtils`
	 * addresses a member by `FGuid`, but a GUID is generated inside the engine and a caller can
	 * never name one, so the display name is the only key the caller has. Every name is
	 * RE-RESOLVED at apply time, and a name that has gone is REFUSED rather than skipped (`D-45`).
	 *
	 * *** THE REMOVAL RULE IS THE ONE THAT MATTERS. *** A removal breaks every asset that reads
	 * the struct, so it is reported with the LOADED READERS by path and class, never as a bare
	 * count, and it is REFUSED unless `force` names that count exactly. Because the check is in
	 * the PLAN, THIS PREVIEW REFUSES IT TOO and prints the count to pass - so a caller previews,
	 * is told the number, and previews again with it. A caller that guesses a wrong number is
	 * refused with the real one.
	 *
	 * IT ALSO PRINTS THE AUTOMATIC CHECKOUT OF ROW 10.2 (D-57), because its Edit twin now performs
	 * one: the block states, per asset, whether a checkout WOULD be issued and why, so the Preview
	 * and the Edit cannot disagree about what the write will do. THIS PREVIEW REACHES NO PROVIDER
	 * WITH A CHECKOUT - it reads the capability flags and the state only.
	 */
	UFUNCTION(meta = (AICallable))
	static FString PreviewTurboMCPStructFields(const FString& Struct, const FString& Changes);

	/**
	 * Work out what a checkout would do, per asset, without checking anything out.
	 *
	 * Pass assets separated by commas or pipes, or nothing to read only the provider facts.
	 *
	 * IT RUNS THE SAME PLAN AS ITS EDIT TWIN, so every refusal here is the identical refusal there:
	 * with no usable provider, each asset is refused by name with the engine own string and no provider
	 * call is made at all (`SourceControlHelpers.cpp:67`, `:76`). The two questions - is a provider
	 * selected, and is it available - are asked in that order and both are required, because Perforce,
	 * Plastic and Subversion all answer the first with true unconditionally.
	 *
	 * AND IT STATES WHAT A CHECKOUT WOULD MEAN, WHICH IS NOT THE SAME ON EVERY PROVIDER. Where the
	 * provider reports `UsesCheckout() == false`, a checkout is a NO-OP BY CONSTRUCTION: that provider has
	 * no lock command, so every tracked file in the working copy is already checked out
	 * (`GitSourceControlModule.cpp:21`). The preview says so rather than implying a lock would be taken.
	 * Where it reports true - Perforce `:622-625`, Subversion `:239-242`, Plastic `:498-501` - a real lock
	 * is the point of the call.
	 *
	 * WRITES NOTHING AND CANNOT: it is in this toolset, which holds no tool that can change an asset, and
	 * a checkout is issued only by the Edit twin.
	 */
	UFUNCTION(meta = (AICallable))
	static FString PreviewTurboMCPVcsCheckout(const FString& Assets);
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
	 * THE COMPILER'S LOG IS NOT THE WHOLE ANSWER. An engine ensure raised during
	 * the compile reaches the output log and never `FCompilerResultsLog`, so the
	 * log is captured alongside it and both the warnings and the errors the
	 * engine logged are counted and listed per asset. A compile that logged an
	 * engine error is reported as not clean even when the compiler reported
	 * nothing.
	 *
	 * Warnings raised by the compile pass are counted and attributed to the asset
	 * that raised them, and each engine message is listed as its category and its
	 * first line, bounded and trimmed, because a bare count cannot be acted on
	 * and full engine log text does not belong in a result (`P6`).
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
	 *
	 * BEFORE ANY FILE IS WRITTEN IT PERFORMS THE AUTOMATIC CHECKOUT OF ROW 10.2 (D-57). The
	 * project's own provider is asked whether it uses checkout at all, and a checkout is then
	 * issued through ISourceControlProvider::Execute with the provider's own answer repeated
	 * verbatim. It NEVER uses USourceControlHelpers::CheckOutFile, whose true on Git means "the
	 * state says the file is already checked out" rather than "a lock was taken" - the trap row
	 * 10.1 measured. THE CHECKOUT ANSWER IS A REPORT AND NEVER A GATE, so a provider that cannot
	 * take a lock cannot fail this call. The appended block names which of the three policies was
	 * chosen when a provider reports UsesCheckout() == false, and it is (b): attempt the checkout
	 * and repeat the provider's answer.
	 *
	 * `SV-03`'S SECOND BRANCH IS SAID IN THE RESULT, AND THAT IS DELIBERATE. The session report's design
	 * is an OR: "add an option to save without a new thumbnail, OR SAY IN THE RESULT THAT THE THUMBNAIL
	 * DRAW RUNS WIDGET CODE". The first branch needs a parameter, and a defaulted parameter on an
	 * `AICallable` function is REQUIRED by the schema UHT generates - so the option that was tried made
	 * this tool uncallable without it and was withdrawn (`bb6467b`). This tool therefore reports WHICH
	 * MECHANISM RAN, and for a Widget Blueprint it says that the editor's save runs the engine's
	 * thumbnail step and that a thumbnail draw instantiates the widget and runs its design-time code.
	 * THE OTHER BRANCH IS `EditTurboMCPSaveWithoutThumbnail`, a tool of its own, which needs no such
	 * parameter and cannot break a caller.
	 */
	UFUNCTION(meta = (AICallable))
	static FString EditTurboMCPSave(const FString& Assets);

	/**
	 * Save one or more assets WITHOUT the editor's thumbnail step.
	 *
	 * THE SAME SAVE AS `E-SAVE`, MINUS THE ONE PART THAT CAN CRASH THE EDITOR OR RUN THE ASSET'S OWN
	 * CODE. `SV-03` observed the editor CRASH in the engine thumbnail renderer while a save drew a
	 * widget's thumbnail, and its design offers this option as one of two ways to protect a caller.
	 *
	 * IT WRITES THE PACKAGE ITSELF, through `UPackage::SavePackage`, instead of going through the
	 * editor's save. That is the whole point: the editor's save is an `OBJ SAVEPACKAGE` exec
	 * (`FileHelpers.cpp:1332`), and that exec runs the engine's thumbnail step (`UnrealEdSrv.cpp:680-682`),
	 * which renders a thumbnail when the cached one is missing or stale (`:439-446`, `:478-485`). For a
	 * Widget Blueprint the renderer INSTANTIATES THE WIDGET with `Designing | ExecutePreConstruct` and
	 * runs its design-time code (`WidgetBlueprintThumbnailRenderer.cpp:189-191`, frequency `OnAssetSave`
	 * at `:216`). A direct package write calls none of that.
	 *
	 * *** SO IT IS A TOOL OF ITS OWN AND NOT A SECOND PARAMETER ON `E-SAVE`. *** Every caller that passes
	 * only `assets` to `E-SAVE` still works, which is the property the withdrawn version lost.
	 *
	 * REFUSES a package outside the project's `/Game` mount, by name, because it has to name the file
	 * itself: this project MEASURED the engine's own conversion answering a path relative to the ENGINE
	 * directory (`PackageName.cpp:1430`; the consequence is recorded at
	 * `TurboMCPTestMapCommandlet.cpp:323-332`), and a write to a guessed filename is worse than a
	 * refusal. `E-SAVE` handles every mount, so the refusal names it.
	 *
	 * The result says `(written without the thumbnail draw)`, because a caller who chose this and is not
	 * told cannot tell the two saves apart in a log. Everything else - one asset at a time, the retry,
	 * the verdict from the unsaved flag, the counted warnings, the automatic checkout - is `E-SAVE`'s own
	 * behaviour, because both tools run ONE body.
	 */
	UFUNCTION(meta = (AICallable))
	static FString EditTurboMCPSaveWithoutThumbnail(const FString& Assets);

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
	static FString EditTurboMCPFunctionAdd(const FString& Asset, const FString& Name,
		const FString& Inputs = TEXT(""), const FString& Outputs = TEXT(""));

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
	 * THE DELETION NEVER TOUCHES THE NODES, AND ONLY ONE HALF OF THAT IS MEASURED. A box is a label
	 * rather than a container, so removing it removes the label and nothing else. THE RE-MEASUREMENT IS
	 * OF THE BOXES: the result re-counts them afterwards, so what is CHECKED is that the box itself is
	 * gone. The node count is REPORTED, not verified - it is taken before the destroy and states what
	 * the box held. That it stays true rests on `DestroyNode` removing the comment and nothing else,
	 * which is why `Graph->Modify()` runs first and the change can be undone.
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
	 * Remove the dead copies in a graph.
	 *
	 * The same inputs, found and judged the same way, as `P-GRAPH-DEAD`, because the two share one
	 * code path up to the write.
	 *
	 * THE ENTRY RULE IS RE-CHECKED ON THE NODE ABOUT TO GO, not trusted from the plan, because a plan
	 * is made and applied in two calls and the graph can change between them. It is the rule whose
	 * failure destroys work.
	 *
	 * A node key that no longer resolves is REPORTED rather than silently skipped: quietly removing
	 * eight of ten nodes would be a half-applied edit, which `P8` forbids.
	 *
	 * The result reports the node count before and after, so the change can be checked against the
	 * graph rather than believed from a sentence.
	 */
	UFUNCTION(meta = (AICallable))
	static FString EditTurboMCPGraphDead(const FString& Asset, const FString& GraphName);

	/**
	 * Place the knots the preview listed.
	 *
	 * The same inputs, found and judged the same way, as `P-GRAPH-KNOTS`, because the two share one code
	 * path up to the write.
	 *
	 * Each wire is re-resolved BY PIN IDENTITY first, because a plan is made and applied in two calls and
	 * the graph can change between them. A wire whose pins no longer connect is REPORTED and skipped
	 * rather than silently dropped.
	 *
	 * The crossing count is measured before and after, and the whole pass is ROLLED BACK when the result
	 * is not lower. The rollback is then RE-MEASURED to confirm it restored the original count, because a
	 * rollback whose result is not checked is a rollback nobody knows works.
	 */
	UFUNCTION(meta = (AICallable))
	static FString EditTurboMCPGraphKnots(const FString& Asset, const FString& GraphName);

	/**
	 * Apply the layout the preview worked out.
	 *
	 * The same inputs, laid out the same way, as `P-GRAPH-TIDY`, because the two share one code path up to
	 * the write. It re-measures AFTER the write and reports the number it actually achieved rather than
	 * the one it predicted, because a plan and its result are two different claims.
	 *
	 * Comment corners stay fixed during tidy; GraphGroupMove translates a box and its contents together.
	 *
	 * `bForce` writes even when the measure says the layout is not shorter, which is what a caller who
	 * disagrees with the measure asks for. `NodeIds` has the same fixed-anchor scope as the preview.
	 */
	UFUNCTION(meta = (AICallable))
	static FString EditTurboMCPGraphTidy(
		const FString& Asset,
		const FString& GraphName,
		bool bForce,
		const TArray<FString>& NodeIds,
		const FString& MainPath = TEXT(""), double HorizontalGap = 120.0, double VerticalGap = 100.0, double LanePitch = 280.0);

	/**
	 * Apply the repairs the preview listed.
	 *
	 * The same inputs, found and judged the same way, as `P-GRAPH-FIX-BACKWARD`, because the two share one
	 * code path up to the write. Every node is RE-RESOLVED BY KEY before it moves, because a plan is made
	 * and applied in two calls and the graph can change between them.
	 *
	 * A COMMENT BOX IS NEVER MOVED. Nothing in the plan holds a box position, so no code path here can
	 * write one.
	 *
	 * `bForce` writes even when the measure says the layout is not shorter, which is what a caller who
	 * disagrees with the measure asks for.
	 */
	UFUNCTION(meta = (AICallable))
	static FString EditTurboMCPGraphFixBackward(
		const FString& Asset,
		const FString& GraphName,
		bool bForce);

	/**
	 * Promote the SAFE values to local variables.
	 *
	 * The same safety decision as `P-LOCAL-VARS`, made in the same place, because the two share one
	 * code path up to the write. **The edit promotes only what the preview called safe**, and it
	 * re-checks that on the node about to change rather than trusting a plan made in an earlier call.
	 *
	 * AN UNSAFE VALUE IS NEVER PROMOTED, however many times it is read: capturing it once is not the
	 * same as computing it twice.
	 *
	 * A local variable is added through the editor's own `AddLocalVariable`, a getter is placed where
	 * the value was computed, each consumer is re-pointed pin by pin, and **the original is removed
	 * last**, once nothing points at it — removing it first would break the links the rewire needs.
	 *
	 * The result names each value it promoted, how many consumers it re-pointed, and how many
	 * candidates it refused.
	 */
	UFUNCTION(meta = (AICallable))
	static FString EditTurboMCPLocalVars(const FString& Asset, const FString& GraphName);

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

	/**
	 * Mark a node to be traced, or clear the marks.
	 *
	 * `action=arm` with one or more node IDs records that node every time it runs in a play
	 * session, without pausing; `action=clear` with IDs removes those marks and their hits, and
	 * with no IDs removes every mark and every hit. Always answers with the arming summary.
	 *
	 * NOTHING IS ADDED TO ANY GRAPH AND NOTHING IS RECOMPILED. The engine already emits a trace
	 * point into every normally-compiled node and the VM raises it through a delegate, so arming
	 * is an in-memory registry entry. That is why this tool cannot corrupt a graph.
	 *
	 * It is in the Edit toolset although it writes no file, because it changes state a caller
	 * must be able to undo and a Read toolset is the one this plugin promises cannot change
	 * anything.
	 */
	UFUNCTION(meta = (AICallable))
	static FString EditTurboMCPTrace(const FString& Action, const FString& NodeId);

	/**
	 * Add a block to a graph from graph text.
	 *
	 * The same decision as `P-GRAPH-APPEND`, made in the same place, because the two share
	 * one code path up to the write: the plan is built once, and this consumes it. A text
	 * the preview refused is refused here with the same words.
	 *
	 * IT ADDS AND LINKS AND CANNOT REMOVE. There is no code path in this capability that
	 * deletes a node - not a disabled one, an ABSENT one - which is what keeps "cannot
	 * remove anything" true rather than merely intended.
	 *
	 * ONE TRANSACTION, and it rolls back on any failure. If a node the plan accepted is
	 * refused at apply time, NOTHING is written and the report says so.
	 *
	 * AFTER WRITING it reads the graph back and compares, and a node the plan created that
	 * is NOT in the graph is reported as `FAILED COMPARE` - a FAILURE, not a success with a
	 * note. The first line is `changed` or `unchanged`.
     * Local labels name the PRECEDING node form. Prefer `(TypeId) ; label Name` for a pinless node,
     * or put `; label Name` on its own line after the closed form. Never put a label before its node.
     * Reference it as `(-> Name:ExactPinName)`. A pinless form needs no dummy pin. Semicolon comments
     * consume the rest of their line: put the closing parenthesis BEFORE a trailing label/comment.
	 */
	UFUNCTION(meta = (AICallable))
	static FString EditTurboMCPGraphAppend(
		const FString& Asset,
		const FString& GraphName,
		const FString& Text);

	/**
	 * Rewrite the body of one function, override or event, and report every node it removed.
	 *
	 * The Edit twin of PreviewTurboMCPGraphReplace. It reaches the SAME plan, so everything it
	 * refuses the preview refused first, with the same words.
	 *
	 * IT CAN DESTROY, WHICH NO OTHER WRITER IN THIS PLUGIN CAN, so two things are deliberate.
	 * Every removal is LISTED by key with its class and title rather than counted, because a
	 * count says how much is gone and a list says what. And the node list is COPIED before
	 * iterating, because removing while iterating a container skips every other element - which
	 * here would mean a body silently HALF replaced, the worst outcome this capability has.
	 *
	 * One transaction, rolled back on any new compile error, and the result is read back through
	 * the project's own reader and compared rather than reported from memory.
	 */
	UFUNCTION(meta = (AICallable))
	static FString EditTurboMCPGraphReplace(
		const FString& Asset,
		const FString& GraphName,
		const FString& Text);

	/** Undo only the latest global transaction when exclusively TurboMCP and this asset. Confirm is the transaction GUID from preview. Never skips unrelated edits. Editor Redo remains available. */
	UFUNCTION(meta = (AICallable))
	static FString EditTurboMCPAssetUndo(const FString& Asset, const FString& Confirm);

	/** Translate selected nodes or entire comment groups, including nested boxes; keeps internal layout and wiring. Refuses new collisions or changed external box membership. */
	UFUNCTION(meta = (AICallable))
	static FString EditTurboMCPGraphGroupMove(const FString& Asset, const FString& GraphName, const TArray<FString>& NodeIds, int32 DeltaX, int32 DeltaY);

	/** Edit reload of saved Blueprint package; discards all unsaved edits, may reset undo. Edit requires the preview confirmation. */
	UFUNCTION(meta = (AICallable))
	static FString EditTurboMCPAssetRevert(const FString& Asset, const FString& Confirm);

	/** Set node positions in one undo step. Positions is a JSON array of {"node":"exact reader node ID","x":100,"y":200}. Validates the entire batch before writing; does not compile or save. */
	UFUNCTION(meta = (AICallable))
	static FString EditTurboMCPGraphPositions(const FString& Asset, const FString& GraphName, const FString& Positions);

	/** Apply a localized call-node swap. keepOld=true retains the old node unplugged; false deletes it after verification. */
	UFUNCTION(meta = (AICallable))
	static FString EditTurboMCPGraphSwapCall(
		const FString& Asset,
		const FString& GraphName,
		const FString& NodeId,
		const FString& ReplacementTypeId,
		const FString& KeepOld,
		const FString& Options = TEXT(""));

	/** Repoint every reader of one output to an existing or newly created source in one transaction. */
	UFUNCTION(meta = (AICallable))
	static FString EditTurboMCPGraphRepoint(
		const FString& Asset, const FString& GraphName, const FString& SourceNodeId,
		const FString& SourcePin, const FString& TargetNodeId, const FString& TargetTypeId,
		const FString& TargetPin, const FString& KeepSource);

	/** Apply the same Operations JSON array/object accepted by PreviewTurboMCPBatchEdit under one transaction; preview first. Compile/save are deferred. Operations are preflighted in order on transient graph copies; later operations see earlier changes. */
	UFUNCTION(meta = (AICallable))
	static FString EditTurboMCPBatchEdit(const FString& Operations);

    /** Connect exact existing node/pin IDs. Data replaces the target input source; exec replaces only the named source output destination and preserves other incoming target paths. No automatic continuation or casts. Same scratch preview and one-undo edit; compile/save deferred. Preview first. */
    UFUNCTION(meta = (AICallable))
    static FString EditTurboMCPGraphConnect(const FString& Asset,const FString& GraphName,const FString& SourceNodeId,const FString& SourcePin,const FString& TargetNodeId,const FString& TargetPin);

    /** Remove exact nodes and links with scratch preview and one undo. Nodes is a JSON array of reader node IDs. Links is a JSON array of objects with sourceNodeId, sourcePin, targetNodeId, targetPin. Use [] for either unused list. Node deletion also removes incident links; other nodes/positions are preserved. No automatic reconnection, compile or save. */
    UFUNCTION(meta = (AICallable))
    static FString EditTurboMCPGraphRemove(const FString& Asset,const FString& GraphName,const FString& Nodes,const FString& Links);


    /** Copy a pure self/getter/literal per consumer; never impure calls. Preserves defaults and input sources. Run layout afterwards. */
    UFUNCTION(meta = (AICallable))
    static FString EditTurboMCPGraphSplitValue(const FString& Asset,const FString& GraphName,const FString& NodeId);

    /** Refactor an explicitly ordered linear chain of synchronous calls into Sequence rows. Groups is JSON [["reader ID 1","reader ID 2"],["reader ID 3"]]. Keeps original execution order and puts continuation last; refuses latent/branching/cyclic groups. Optional trivial-value splitting; one undo, no compile/save. */
    UFUNCTION(meta = (AICallable))
    static FString EditTurboMCPGraphSequence(const FString& Asset,const FString& GraphName,const FString& Groups,bool bSplitValues = true);


    /** Set one unconnected input pin default by exact reader NodeId and pin name. Validates before mutation, captures undo and marks the Blueprint dirty. Does not compile or save. */
    UFUNCTION(meta = (AICallable))
    static FString EditTurboMCPPinDefault(const FString& Asset, const FString& GraphName,
        const FString& NodeId, const FString& PinName, const FString& Value);


	/**
	 * The Edit twin of PreviewTurboMCPGraphPatch. It reaches the SAME plan, so everything it
	 * refuses the preview refused first, with the same words.
	 *
	 * ONLY A PIN WHOSE VALUE ACTUALLY DIFFERS IS WRITTEN, and that is the capability rather than
	 * an optimisation: setting every described pin to the value it already holds would dirty the
	 * asset and report a change that is not one, which is precisely what "applies only the
	 * differences" forbids.
	 *
	 * The change is MEASURED rather than restated. The node ids and the described pin values are
	 * read BEFORE the write and the ids are read again AFTER it, and the report is computed from
	 * those two sets - so a patch that quietly created or destroyed a node FAILS THE COMPARE
	 * instead of passing on the tool's own summary. That matters because the acceptance row is a
	 * claim about WHICH nodes changed: a patch that touches a third node has failed even when
	 * the graph still compiles.
	 *
	 * One transaction, rolled back on any new compile error, and the first line is `changed` or
	 * `unchanged`.
	 */
	UFUNCTION(meta = (AICallable))
	static FString EditTurboMCPGraphPatch(
		const FString& Asset,
		const FString& GraphName,
		const FString& Text);

	/**
	 * Lay out a material, one comment box per block, and PROVE it changed nothing but geometry.
	 *
	 * "Same compiled result" is the requirement and it is MEASURED. A layout moves expressions and adds
	 * boxes, so it cannot legitimately change what the material computes; if it does, the edit is ROLLED
	 * BACK and reported as a failure with the difference named, never as a success with a note.
	 *
	 * Three statements are made afterwards and the answer says which is which: the SET OF EXPRESSIONS is
	 * identical, which a layout cannot change and which is what the compiled result depends on; the
	 * engine reports no NEW error, so a material that already had one does not make this pass fail; and
	 * the engine's own StateId is printed, REPORTED rather than asserted because it can change for
	 * reasons a layout does not control.
	 *
	 * The whole layout is ONE TRANSACTION, so it is a single undo and a rollback is exact.
	 */
	UFUNCTION(meta = (AICallable))
	static FString EditTurboMCPMaterialTidy(
		const FString& Asset,
		const FString& Force);

	/**
	 * Give a material values names: create the declaration and rewire the readers.
	 *
	 * A MATERIAL LABEL IS A NAMED REROUTE, so this creates a declaration that forwards to the value it
	 * names and re-points every input that read the value directly at the declaration instead. It ADDS
	 * and REWIRES; it never removes anything.
	 *
	 * THE MATERIAL'S COMPILED RESULT IS PROVEN UNCHANGED AND THE EDIT IS ROLLED BACK IF IT IS NOT. A
	 * declaration compiles by forwarding to its input, so reading a value directly and reading it
	 * through a label must compile to the same thing - which is what makes the edit legitimate and what
	 * the check confirms. Three statements are made afterwards and the answer says which is which: no
	 * pre-existing expression was LOST, the expressions GAINED are exactly the declarations this pass
	 * planned, and the engine reports no NEW error. The engine's own StateId is printed, REPORTED rather
	 * than asserted because it can change for reasons a label promotion does not control.
	 *
	 * The whole promotion is ONE TRANSACTION, so it is a single undo and a rollback is exact.
	 */
	UFUNCTION(meta = (AICallable))
	static FString EditTurboMCPMaterialLabelPromote(
		const FString& Asset);

	/**
	 * `E-MAT-BLOCKS`: make labels follow blocks, changing exactly what the Preview listed.
	 *
	 * *** THIS IS THE ONE MATERIAL WRITER THAT REMOVES, SO IT STATES WHAT IT REMOVES AND DELETES BY
	 * IDENTITY. `05_DECISIONS.md` D-45: "a writer that removes anything lists each removal by key with its
	 * class and title, never a count... a count says how much is gone and a list says what." ***
	 *
	 * "Same compiled result check" IS MEASURED, AND THE ASSERTION IS ASYMMETRIC ON PURPOSE because this pass
	 * legitimately adds and legitimately removes: NO PRE-EXISTING EXPRESSION WAS LOST EXCEPT THE ONES ON THE
	 * PLAN'S OWN REMOVAL LIST, the expressions GAINED are exactly the ones planned, and the engine reports
	 * no NEW error - so a material that already had a compiler error does not make this pass fail. A node
	 * that vanished without being listed is a failure EVEN THOUGH THE MATERIAL STILL COMPILES, which is
	 * what "changes exactly the items in the Preview list" means.
	 *
	 * EVERY NODE IS RE-RESOLVED BY KEY AT APPLY TIME rather than trusted from the plan - the other half of
	 * D-45, forced by the two-call shape: a plan is made and applied separately, so the material can change
	 * in between. An input that no longer points where the plan said is a REFUSAL rather than a rewire,
	 * because a stale index would retarget the wrong pin and produce a wrong material that still compiles.
	 *
	 * The whole edit is ONE TRANSACTION, so it is a single undo and a rollback is exact.
	 */
	UFUNCTION(meta = (AICallable))
	static FString EditTurboMCPMaterialBlocks(
		const FString& Asset);

	/**
	 * Set texture overrides on a material instance.
	 *
	 * Each entry is `Parameter = /Asset/Texture.Asset`. The parameter must be declared by the instance's
	 * parent chain, and the texture must be compatible with the parameter's sampler type. The result is
	 * re-read through R-MAT-INST-LOAD so it reports what the instance now stores.
	 */
	UFUNCTION(meta = (AICallable))
	static FString EditTurboMCPMaterialInstanceSet(
		const FString& Asset,
		const TArray<FString>& Overrides);

	/**
	 * Apply row operations to a DataTable in ONE transaction, then re-read the rows back from the table.
	 *
	 * It refuses exactly what the Preview refuses, because both run the same Plan. Every row is named rather than
	 * addressed by position, and every name is looked up again at apply time, so a plan made before another editor
	 * changed the table cannot write into the wrong row. A table with unsaved changes is reported as such.
	 */
	UFUNCTION(meta = (AICallable))
	static FString EditTurboMCPDataTableRows(
		const FString& Asset,
		const TArray<FString>& Rows);

	/**
	 * Copy a block of nodes from one graph to another, and report what could not come with it.
	 *
	 * THE SAME ASSET AND GRAPH ON BOTH SIDES IS REFUSED BY NAME and pointed at the paste-in-place
	 * case, because that is a duplicate rather than a copy. A variable, function or event the target
	 * asset does not have REFUSES THE WHOLE COPY and names every missing member, so a half block is
	 * never written. The copy itself reads the SOURCE NODE AS THE TEMPLATE - which is what keeps a
	 * copied call node calling the function it called - and every link that crosses the selection and
	 * has nothing to point at in the target is REPORTED BY NAME rather than silently dropped.
	 *
	 * One transaction. After writing it reads the graph back and states the node count before and
	 * after, so the first line is `changed`.
	 */
	UFUNCTION(meta = (AICallable))
	static FString EditTurboMCPGraphCopy(
		const FString& SourceAsset,
		const FString& SourceGraph,
		const TArray<FString>& NodeIds,
		const FString& TargetAsset,
		const FString& TargetGraph,
		const FString& AnchorNodeId);

	/**
	 * Extract a selection into a function or a macro.
	 *
	 * NOT BUILT YET, AND IT REFUSES BY SAYING SO rather than by a vague failure. `03_TOOLS.md`
	 * requires the engine's own `FBlueprintEditor::CollapseNodesIntoGraph`, which is a member of an
	 * ASSET-EDITOR UI OBJECT (`BlueprintEditor.h:1125`); the measurement in
	 * `TurboMCPGraphStructureNotes.h` enumerates every other piece as static and reachable, and
	 * leaves exactly one question open - whether that instance can be constructed outside an open
	 * asset editor. A hand-built member would NOT be the thing the editor's own Collapse makes, so
	 * building one would be a different capability wearing this one's name.
	 *
	 * The plan still runs, so a caller learns what the input is missing - a selection containing an
	 * entry, an event or a result node, or one with no exec boundary, is refused by name.
	 */
	UFUNCTION(meta = (AICallable))
	static FString EditTurboMCPGraphCollapse(
		const FString& Asset,
		const FString& GraphName,
		const TArray<FString>& NodeIds,
		const FString& MemberName,
		const FString& MemberKind);

	/**
	 * Create a user struct asset, with its fields in one call.
	 *
	 * The same inputs, parsed and validated the same way, as `P-STRUCT-NEW`, because the two
	 * share one code path up to the write (`D-02`).
	 *
	 * REFUSES an existing path rather than overwriting it: a name someone else chose is not this
	 * tool's to replace. The refusal opens with `unchanged:` so a caller can tell it from a
	 * failure (`P4`).
	 *
	 * *** THE STRUCT IS CREATED THROUGH THE EDITOR'S OWN `FStructureEditorUtils::CreateUserDefinedStruct`,
	 * AND THAT IS A MEASURED CLAIM RATHER THAN A STYLE CHOICE. *** `AddVariable` reaches for the
	 * struct's `UUserDefinedStructEditorData`, and that ONE call installs it
	 * (`StructureEditorUtils.cpp:49-50`) - which is why a `NewObject<UUserDefinedStruct>` built by
	 * hand crashes at `StructureEditorUtils.cpp:256` and this does not. Going through the engine's
	 * creator is also what keeps the asset editable in the struct editor instead of producing a
	 * hand-built asset the editor cannot open.
	 *
	 * THE RESULT PRINTS WHAT WAS ASKED FOR, WHAT WAS ADDED AND WHAT READS BACK from the struct
	 * itself, because a tool reporting its own intention is not evidence that it succeeded. It also
	 * says plainly that the asset is NOT yet saved: an unsaved asset dies with the editor, which has
	 * cost this project three times.
	 */
	UFUNCTION(meta = (AICallable))
	static FString EditTurboMCPStructNew(const FString& Path, const FString& Fields);

	/**
	 * Add, change, reorder and remove fields on an existing user struct.
	 *
	 * The same inputs, checked the same way, as `P-STRUCT-FIELDS`, because the two share one code
	 * path up to the write (`D-02`).
	 *
	 * One transaction, then it RE-READS the field list from the struct and prints it, so the answer
	 * is what the struct holds rather than what this code intended. The report also says the asset
	 * is not saved yet.
	 *
	 * A field that cannot be re-resolved at apply time - because the struct changed between the
	 * preview and this call - is REFUSED BY NAME and is not silently skipped, because skipping it
	 * would make this edit differ from the preview that authorised it (`D-45`). The counts of what
	 * DID land are printed alongside, so the caller knows the exact state.
	 *
	 * BEFORE IT WRITES IT PERFORMS THE AUTOMATIC CHECKOUT OF ROW 10.2 (D-57), and appends what
	 * happened. The provider is asked whether it uses checkout at all, and a checkout is issued
	 * through ISourceControlProvider::Execute with the provider's own answer repeated verbatim. It
	 * NEVER uses USourceControlHelpers::CheckOutFile, whose true on Git means "the state says the
	 * file is already checked out" rather than "a lock was taken" - the trap row 10.1 measured. THE
	 * CHECKOUT ANSWER IS A REPORT AND NEVER A GATE, and the block names the policy chosen when a
	 * provider reports UsesCheckout() == false: (b) attempt it and repeat the provider's answer.
	 */
	UFUNCTION(meta = (AICallable))
	static FString EditTurboMCPStructFields(const FString& Struct, const FString& Changes);

	/**
	 * Take the lock before a write: check one or more assets out through the project own provider.
	 *
	 * `03_TOOLS.md` section 17 is the requirement and `D-57` the ordering decision: this capability is
	 * built and tested BEFORE any write tool calls it, and no automatic checkout happens anywhere until
	 * this tool own tests pass.
	 *
	 * IT GOES THROUGH `ISourceControlModule` OWN PROVIDER rather than shelling out to `p4` or `git`,
	 * exactly as the spec asks. With assets given, its Edit half issues an `FCheckOut` operation through
	 * that provider and repeats the provider own answer verbatim, including the messages the provider
	 * attached to the operation. With no assets it only reads, and nothing is issued.
	 *
	 * IT REFUSES EXACTLY WHAT `PreviewTurboMCPVcsCheckout` REFUSES, because both read one plan: the two
	 * engine questions are asked first, both are required, and with no usable provider every asset is
	 * refused by name with the engine own string while NO PROVIDER IS REACHED - the refusal is not a
	 * failure of a call, it is a decision not to make one.
	 *
	 * *** AND IT SAYS WHEN A CHECKOUT CANNOT MEAN ANYTHING. *** Where the provider reports
	 * `UsesCheckout() == false` the answer states plainly that a checkout is a NO-OP on it, because that
	 * provider has no lock command (`GitSourceControlModule.cpp:21`, `GitSourceControlProvider.cpp:286-289`),
	 * and then records what the provider actually answered rather than predicting one. A tool that implied
	 * it had taken a lock on a provider that has none would be lying about the one thing this capability
	 * exists to guarantee.
	 *
	 * THIS TOOL WRITES NO FILE. A checkout is a source control operation; the asset contents are untouched,
	 * and no save happens here.
	 */
	UFUNCTION(meta = (AICallable))
	static FString EditTurboMCPVcsCheckout(const FString& Assets);
};

/**
 * The identities of the four toolsets, for code that has to find one of them.
 *
 * DECLARED HERE BECAUSE TWO FILES NOW NEED THEM. `TurboMCPCallRecorder.cpp` walks the engine's
 * registry looking for the handler that belongs to one of these classes, and the alternative was a
 * second copy of the list - which is the failure mode `check-mcp-server.py` exists to catch for the
 * analytics provider's hand-kept list.
 */
namespace TurboMCPToolsets
{
	/** Every toolset class this plugin owns, in kind order: Read, Look, Preview, Edit. */
	const TArray<TSubclassOf<UToolsetDefinition>>& GetToolsetClasses();

	/** The `<Module>.<ClassName>` name the engine registers this class under. */
	FString GetToolsetName(const UClass* InClass);
}

/**
 * Registers all four toolsets with the engine's toolset registry.
 *
 * Must be called after every module loading phase is complete
 * (`FCoreDelegates::OnAllModuleLoadingPhasesComplete`), because the registry
 * subsystem may not exist before that. See TurboMCPModule.cpp.
 *
 * It also installs the call recording, so a toolset is registered and recorded by one code path:
 * see `TurboMCPCallRecorder.h` for why the record belongs at the registry's door rather than in 68
 * tool bodies.
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
