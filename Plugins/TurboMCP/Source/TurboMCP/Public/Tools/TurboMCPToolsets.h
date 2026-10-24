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
 * Reflected MCP tools grouped by their primary purpose. A toolset name is not a
 * side-effect guarantee: PlayCheck and WidgetPreview execute project code.
 * Consult each tool's execution and mutation contract before invoking it.
 * The registry publishes names such as TurboMCP.TurboMCPReadToolset.
 * UFUNCTION documentation is exposed to MCP clients as the tool description.
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
	 * Both fields are required by this tool's live schema. Pass an empty string
	 * for an unused field; requiredness of other tools comes from their own schemas.
	 *
	 * @param ToolsetName Registered toolset name, e.g. TurboMCP.TurboMCPReadToolset. Empty searches all toolsets for ToolName. Supply an empty string when unused.
	 * @param ToolName Bare tool name, e.g. ReadTurboMCPStatus. Empty lists the named toolset; use the returned list:<offset> for another page. find:0:<words> searches names and descriptions.
	*/
	UFUNCTION(meta = (AICallable))
	static FString ReadTurboMCPToolHelp(const FString& ToolsetName, const FString& ToolName);

	/**
	 * Read assets matching a query, like the editor's quick open search.
	 *
	 * For gameplay-tag usage/referencers, discover the stock GameplayTagsToolset
	 * FindReferencersByTag tool and pass tagName (the exact registered tag).
	 * It returns asset-registry package referencers, not proof of runtime execution.
	 * This text search is not a tag-reference search; the stock toolset must be enabled.
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
	 *
	 * @param Query Nonempty search words with optional type: and path: filters, e.g. type:Blueprint path:/Game/UI. All words must match. Empty input and unsupported filters are refused.
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
	 *
	 * @param Assets One or more existing assets separated by commas or pipes, e.g. /Game/Tests/BP_NodeTest. Short names must resolve uniquely; use full paths to disambiguate.
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
	 *
	 * @param Words Words matched against type names and paths, e.g. Vector. Empty or whitespace-only input is refused.
	 * @param Kind struct, enum, class, interface or any; empty means any. Example: struct.
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
	 *
	 * @param Limit Positive decimal maximum number of listed packages, e.g. 100. Empty uses 50; zero, non-digits and values above 2147483647 are refused.
	*/
	UFUNCTION(meta = (AICallable))
	static FString ReadTurboMCPDirtyList(const FString& Limit);

	/**
	 * Resolve node identities and inspect pins; may load assets, without requesting edits, compile or save.
	 * Reports engine references, titles, type identities and up to 40 pins per node. Type identities do not guarantee writer support. Mixed results include per-ID refusals; similar key prefixes are suggestions, not replacement identities.
	 * @param NodeIds Required comma- or pipe-separated list of 1..25 reader IDs, e.g. WBP_ShopCard:RefreshPrice:3fa91c. Full asset paths are accepted. Empty input is refused. Keys need at least six hexadecimal digits and must resolve uniquely; prefer the exact full ID from a fresh reader result.
	 */
	UFUNCTION(meta = (AICallable))
	static FString ReadTurboMCPNodeResolve(const FString& NodeIds);

	/**
	 * Inspect static execution context and input sources; may load assets, without requesting edits, compile or save.
	 * Lists reachable predecessors and bounded downstream execution hops, not runtime traces. Dynamic and delegate dispatch are not inferred. NextHops does not cap upstream traversal or total output.
	 * @param Asset Required Blueprint path or unique short name, e.g. /Game/Tests/BP_NodeTest. Empty, missing or ambiguous assets are refused.
	 * @param GraphName Required exact unique graph name, e.g. EventGraph. Empty, missing or ambiguous names are refused; graph paths are not selectors.
	 * @param NodeId Required exact full or short node ID emitted by the reader for this graph. Empty, stale or ambiguous IDs are refused; arbitrary GUID prefixes are not accepted here.
	 * @param NextHops Optional number of downstream execution edges to follow, 0..32; default 3. Zero includes the selected node and upstream context but no downstream hops. Values outside the range are refused.
	 */
	UFUNCTION(meta = (AICallable))
	static FString ReadTurboMCPNodeContext(const FString& Asset,const FString& GraphName,const FString& NodeId,int32 NextHops = 3);

	/**
	 * Search Blueprint nodes by text; may load assets, without requesting edits, compile or save.
	 * Returns up to 40 matches. Searches local graphs, including nested graphs when unscoped, but not inherited assets or native implementations. This is not a gameplay-tag reference search or proof of runtime execution.
	 * @param Asset Required Blueprint path or unique short name, e.g. /Game/Tests/BP_NodeTest. Empty, missing or ambiguous assets are refused.
	 * @param Words Required search terms separated by whitespace, commas or pipes, e.g. Print String. Every term must match a title, function, variable, class, comment or pin name; matching uses case-insensitive substrings, not phrases. Empty input is refused.
	 * @param GraphName Required string; pass an empty string to search all local graphs, or one exact unique graph name such as EventGraph. Missing or ambiguous names are refused. Graph paths are not accepted selectors.
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
	 *
	 * EntryFilter optionally matches event/function names by case-insensitive substring. Empty reads all entries. Filtered views are inspection-only. Large reads return inspection pages. Use nextOffset and Revision for subsequent pages; keep all other arguments identical. A changed read refuses the stale token. Partial pages are not standalone write-back input.
	 *
	 * @param Asset Existing Blueprint path or unique short name, e.g. /Game/Tests/BP_NodeTest. Empty is refused.
	 * @param GraphName Exact existing graph name, e.g. EventGraph. Empty is refused; no graph is guessed.
	 * @param Format read is the only supported format and the default; empty also means read. write is refused.
	 * @param Detail manifest, signatures, bodies or full; default or empty means bodies. Each level adds detail.
	 * @param Offset Zero-based character offset. Start at 0; for later pages use the returned nextOffset and Revision together. Negative values are refused.
	 * @param MaxChars Maximum page characters, 256..24000 inclusive; default 24000. Partial pages are inspection-only, not standalone write-back input.
	 * @param Revision Empty for page zero. Later pages require the revision token from page zero with unchanged arguments; stale tokens are refused.
	 * @param EntryFilter Case-insensitive entry-name substring, e.g. BeginPlay. Empty includes all entries. Filtered views are inspection-only.
	*/
	UFUNCTION(meta = (AICallable))
	static FString ReadTurboMCPGraphLoad(
		const FString& Asset,
		const FString& GraphName,
		const FString& Format = TEXT("read"),
		const FString& Detail = TEXT("bodies"),
        int32 Offset = 0, int32 MaxChars = 24000, const FString& Revision = TEXT(""), const FString& EntryFilter = TEXT(""));

	/**
	 * Inspect Blueprint structure and selected graph content; loads assets without requesting edits, compile or save.
	 * This is not a complete export or proof of runtime behavior. Relation analysis is static; omitted content and budget reductions are reported. Widget details are included only for unfiltered text reads above manifest detail.
	 * @param Assets Required comma- or pipe-separated list of 1..8 Blueprint paths or unique short names, e.g. /Game/Tests/BP_NodeTest. Empty input is refused. Per-asset failures do not discard other results.
	 * @param Detail Required string: manifest, signatures, bodies or full (case-insensitive). Empty uses manifest. Higher levels add signatures/relations, graph bodies, then editing details; text output may fall back to lower detail to reduce size.
	 * @param GraphFilter Required comma- or pipe-separated graph-name fragments. Empty selects all local graphs; otherwise any case-insensitive substring match is included. No match produces a per-asset refusal listing graph names. This does not filter all asset metadata.
	 * @param Format Required string: read or json (case-insensitive); empty uses read. JSON is embedded per asset in a text response with headings/status, not returned as one standalone JSON document. Both formats support all detail levels.
	 * @param TokenBudget Required string of positive decimal digits, up to 2147483647; empty uses 60000. Despite the name, this is a per-asset character budget, not tokens or a hard response limit. Text drops detail but retains an oversized minimum manifest with a warning; headings and notes add overhead. JSON refuses an oversized document. Use smaller graph scopes or ReadTurboMCPGraphLoad for bounded graph reads.
	 */
	UFUNCTION(meta = (AICallable))
	static FString ReadTurboMCPBlueprintLoad(
		const FString& Assets,
		const FString& Detail,
		const FString& GraphFilter,
		const FString& Format,
		const FString& TokenBudget);

	/**
	 * Read saved registry references into or out of an asset. Unsaved reference changes are not authoritative registry data; no explicit compile or save. Waits for registry scanning before reporting counts.
	 * @param Asset Required asset path or unique short name, e.g. /Game/Tests/BP_NodeTest. Empty or unresolved values are refused; use a full path for ambiguous names.
	 * @param Direction Case-insensitive out (dependencies), in (referencers), or both. Empty defaults to both; other values are refused.
	 */
	UFUNCTION(meta = (AICallable))
	static FString ReadTurboMCPRefs(
		const FString& Asset,
		const FString& Direction);

	/**
	 * Read available and implemented Blueprint overrides from parent classes and interfaces. Loads the Blueprint; no explicit compile or save. Counts precede filtering.
	 * @param Asset Required Blueprint path or unique short name, e.g. /Game/Tests/BP_NodeTest. Empty or non-Blueprint targets are refused.
	 * @param Include Case-insensitive all, overridden, or available; not-overridden is an alias for available. Empty defaults to all. Unknown filters are refused.
	 */
	UFUNCTION(meta = (AICallable))
	static FString ReadTurboMCPOverrides(
		const FString& Asset,
		const FString& Include);

	/**
	 * Read Blueprint comment rectangles, membership and nesting. Membership is inferred geometrically, not a stored ownership relation; overlapping membership is reported. Loads the Blueprint; no explicit compile or save.
	 * @param Asset Required Blueprint path or unique short name, e.g. /Game/Tests/BP_NodeTest. Empty or non-Blueprint targets are refused.
	 * @param GraphName Existing graph name, e.g. EventGraph. Empty lists all collected graphs; unknown names are refused with available names.
	 */
	UFUNCTION(meta = (AICallable))
	static FString ReadTurboMCPCommentList(
		const FString& Asset,
		const FString& GraphName);

	/**
	 * Read DataTable schema, row names, selected values and registry-based readers. Displays up to 80 fields and 60 sorted row names, with omitted counts; no paging parameter. Reader graph details are searched only in loaded Blueprints. Registry results do not prove all unsaved or dynamic uses. Loads the table; no explicit edit or save.
	 * @param Asset Required DataTable path or unique short name, e.g. /Game/Tests/DT_Items. Empty or non-DataTable targets are refused.
	 * @param Include Case-insensitive all, readers, or schema. Empty defaults to all. schema includes row names and selected values; readers omits rows. Unknown filters are refused.
	 * @param Columns Required field; pass an empty string to omit values. Otherwise comma-separated reflected field names from the schema, e.g. Price,Count. Empty omits values. Matching ignores case, duplicates are collapsed and unknown fields are refused. Requires a row struct and Include=all or schema; values are limited to the displayed rows.
	 */
	UFUNCTION(meta = (AICallable))
	static FString ReadTurboMCPDataTable(
		const FString& Asset,
		const FString& Include,
		const FString& Columns = TEXT(""));

	/**
	 * Read retained call-log records, newest first. Does not clear the log or change recording settings; this request may itself be recorded.
	 * Engine records may lack arguments or resolved names. Unknown sizes are not zero. Records are not the same as completed calls; use ReadTurboMCPCallCost for call totals.
	 * @param Filter Required string. Case-insensitive substring of toolset, tool, source, arguments or status (ok/error). Empty matches all records; e.g. BP_NodeTest. This call can match its own recorded filter argument.
	 * @param Limit Required string of positive decimal digits, e.g. 50. Empty uses 50; values above 500 are capped at 500. Zero, signs and fractions are refused.
	 * @param Since Required timestamp string copied exactly from a log entry, e.g. 2026-10-24T09:30:00.000Z. Empty has no lower bound. Returns records strictly after this timestamp, using text comparison; other date formats are not normalized or validated.
	 */
	UFUNCTION(meta = (AICallable))
	static FString ReadTurboMCPCallLog(
		const FString& Filter,
		const FString& Limit,
		const FString& Since);

	/**
	 * Summarize retained call costs, durations and failures. Does not clear the log or change recording settings; this request may itself be recorded.
	 * Related records are grouped into calls. Rankings and totals cover matching retained calls, not all historical activity. Unknown measurements are not zero; token counts are estimates.
	 * @param Since Required timestamp string copied exactly from a log entry, e.g. 2026-10-24T09:30:00.000Z. Empty has no lower bound. Selects calls strictly after the timestamp using text comparison; other date formats are not normalized or validated.
	 * @param Filter Required string. Case-insensitive substring of the tool label, toolset, tool or source; e.g. TurboMCPReadToolset. Empty includes all retained calls. Unlike CallLog, does not search arguments or status.
	 * @param Cap Required string of positive decimal digits, e.g. 10. Empty uses 10; values above 50 are capped at 50. Limits displayed rows per list, not analysis or totals. Zero, signs and fractions are refused.
	 */
	UFUNCTION(meta = (AICallable))
	static FString ReadTurboMCPCallCost(
		const FString& Since,
		const FString& Filter,
		const FString& Cap);

	/**
	 * Inspect registered tool metadata and local trap notes without invoking the described tool.
	 * This is a compact rendering, not the complete JSON schema. Single-tool output is capped at 2000 characters; use describe_toolset for the complete schema. Missing trap notes are reported, not treated as proof that a tool has no limitations.
	 * @param Toolset Required string. Empty lists registered toolsets. Prefer the full registry name, e.g. TurboMCP.TurboMCPReadToolset. Short names and suffixes are matched after normalization; the first matching toolset is selected, so use full names to avoid ambiguity.
	 * @param Tool Required string. Empty lists tools in the selected toolset. Otherwise use a bare or fully qualified registered tool name, e.g. ReadTurboMCPNodeFind. Matching ignores case, spaces and underscores. Unknown names return suggestions; Toolset must be supplied to select a tool.
	 */
	UFUNCTION(meta = (AICallable))
	static FString ReadTurboMCPToolSchema(const FString& Toolset, const FString& Tool);

	/**
	 * Read named material reroute labels, writers and reader counts; no explicit edit, compile or save.
	 * @param Asset Required Material or Material Function path or unique short name, e.g. /Game/Tests/M_TurboMCPFixture. Empty/unresolved assets are refused. Instances have no expression graph; read their parent or use ReadTurboMCPMaterialInstance.
	 */
	UFUNCTION(meta = (AICallable))
	static FString ReadTurboMCPMaterialLabels(const FString& Asset);

	/**
	 * Read material parameters and connected output trees, or an instance override summary. Loads the asset; no explicit edit, compile or save. TokenBudget is advisory, not a hard cap.
	 * @param Asset Required Material, Material Function or constant Material Instance path or unique short name. Empty/unresolved values are refused; e.g. /Game/Tests/M_TurboMCPFixture.
	 * @param bIncludeInstanceOverrides Required boolean. true permits constant Material Instances and lists their overrides; false refuses instances with parent guidance. Ignored for base materials/functions.
	 * @param TokenBudget Required integer advisory threshold in estimated tokens, e.g. 3000. Zero or negative disables the overrun warning. Output is not capped or reduced to this threshold; use Trace to narrow large graphs.
	 */
	UFUNCTION(meta = (AICallable))
	static FString ReadTurboMCPMaterialLoad(
		const FString& Asset,
		bool bIncludeInstanceOverrides,
		int32 TokenBudget);

	/**
	 * Read one connected material output tree; no explicit edit, compile or save. TokenBudget is advisory, not a hard cap.
	 * @param Asset Required Material or Material Function path or unique short name, e.g. /Game/Tests/M_TurboMCPFixture. Empty/unresolved assets are refused. Instances have no expression graph; read their parent or use ReadTurboMCPMaterialInstance.
	 * @param OutputProperty Required connected output identifier from MaterialLoad, e.g. MP_BaseColor. Empty or unconnected output is refused; use MaterialLoad to discover connected outputs.
	 * @param TokenBudget Required integer advisory threshold in estimated tokens, e.g. 3000. Zero or negative disables the overrun warning. Output is not capped or reduced to this threshold; use Trace to narrow large graphs.
	 */
	UFUNCTION(meta = (AICallable))
	static FString ReadTurboMCPMaterialTrace(
		const FString& Asset,
		const FString& OutputProperty,
		int32 TokenBudget);

	/**
	 * Read material logic grouped by comment boxes; no explicit edit, compile or save. TokenBudget is advisory, not a hard cap.
	 * @param Asset Required Material or Material Function path or unique short name, e.g. /Game/Tests/M_TurboMCPFixture. Empty/unresolved assets are refused. Instances have no expression graph; read their parent or use ReadTurboMCPMaterialInstance.
	 * @param TokenBudget Required integer advisory threshold in estimated tokens, e.g. 3000. Zero or negative disables the overrun warning. Output is not capped or reduced to this threshold; use Trace to narrow large graphs.
	 */
	UFUNCTION(meta = (AICallable))
	static FString ReadTurboMCPMaterialBlocks(const FString& Asset, int32 TokenBudget);

	/**
	 * Read material-instance parent chain, local overrides, inherited values and orphaned overrides. Loads the asset; no explicit edit, compile or save.
	 * @param Asset Required material-instance path or unique short name, e.g. /Game/Tests/MI_TurboMCPFixture. Empty and base-material targets are refused; use ReadTurboMCPMaterialLoad for a base graph.
	 */
	UFUNCTION(meta = (AICallable))
	static FString ReadTurboMCPMaterialInstance(const FString& Asset);

	/**
	 * Start or poll a timed Play In Editor session that executes project code with editor permissions, is not sandboxed or automatically undone, and does not explicitly save assets.
	 * Starting returns immediately. Poll with empty Seconds until the owned session has stopped; pending is not a completed result. Existing play sessions are left alone. Captured warnings/errors do not prove gameplay correctness.
	 * @param Seconds Required string. A positive numeric duration up to 600 seconds starts a run; e.g. 2.5. Empty or whitespace polls the current run. Zero, negative, invalid and over-limit durations are refused.
	 * @param Map Required string. On start, a loadable map package path, e.g. /Game/Maps/TestMap; empty uses the editor's default play request. Ignored when polling. Invalid map paths are refused before requesting PIE.
	 */
	UFUNCTION(meta = (AICallable))
	static FString ReadTurboMCPPlayCheck(const FString& Seconds, const FString& Map);

	/**
	 * Report the trace subscription, armed points and retained hits. Does not arm or clear points, edit graphs, compile or save assets. Zero hits alone do not prove a node never ran.
	 * @param Action Required string. status (case-insensitive) reports the registry; empty also selects status. arm and clear are refused with EditTurboMCPTrace guidance. Other actions are refused. Example: status.
	 * @param NodeId Required string. Ignored for status; use an empty string. This reader reports the full registry rather than filtering by node ID.
	 */
	UFUNCTION(meta = (AICallable))
	static FString ReadTurboMCPTrace(const FString& Action, const FString& NodeId);

	/**
	 * Inspect Blueprints for selected lint findings; loads assets but does not explicitly edit, compile or save them. Findings are review suggestions, not proof of defects or guaranteed fixes.
	 * @param Assets Required comma-separated Blueprint paths or unique short names, e.g. /Game/Tests/BP_NodeTest. Empty is refused except when Checks=list. Each asset is processed independently; failures are reported alongside successful results.
	 * @param Checks Required comma-separated check names; empty runs all checks. list returns names and descriptions without loading assets. Example: dead-nodes,unconnected-pins. Unknown names are refused.
	 * @param TokenBudget Required string of decimal digits from 0 to 2147483647. Empty or 0 disables the budget. Positive values bound finding text approximately at four characters per estimated token per asset; complete lines may exceed it. Counts and headings are outside this budget. Example: 2000. Fractions and signs are refused.
	 */
	UFUNCTION(meta = (AICallable))
	static FString ReadTurboMCPBPLint(
		const FString& Assets,
		const FString& Checks,
		const FString& TokenBudget);

	/**
	 * Report recorded package disk sizes and direct registry dependencies; does not explicitly edit, compile or save assets. This is not runtime memory, cooked size or recursive load cost.
	 * Categories may overlap; unknown sizes remain unknown. Registry data may omit unsaved or dynamic references.
	 * @param Asset Required asset path or unique short name, e.g. /Game/Tests/BP_NodeTest. Empty or unresolved assets are refused. Use a full path to disambiguate.
	 * @param Cap Required integer maximum contributor rows shown. Zero or negative uses 10; positive values are used directly. Example: 20. Does not limit dependency scanning, category counts or the separate unknown-size list.
	 */
	UFUNCTION(meta = (AICallable))
	static FString ReadTurboMCPAssetCost(
		const FString& Asset,
		int32 Cap);

	/**
	 * Inspect selected Widget Blueprint hierarchy data; loads the asset without explicitly editing, compiling or saving it. This is not a complete or lossless export.
	 * Reports names, classes, paths, variable flags, non-default visibility and selected padding. Panel children retain slot order; this alone does not establish visual draw order. Other properties, bindings and animations are omitted. Do not assume write-back preserves omitted state.
	 * @param Asset Required Widget Blueprint path or unique short name, e.g. /Game/Tests/WBP_NodeTest. Empty, unresolved or non-widget assets are refused. A widget without a root returns an explicit no-root result.
	 */
	UFUNCTION(meta = (AICallable))
	static FString ReadTurboMCPWidgetTree(
		const FString& Asset);

	/**
	 * Inspect call-cost string normalization and asset extraction for literal inputs; does not run the named tools or read the call log.
	 * The result shows computed strings, not a pass/fail verdict or evidence that an asset exists. The tool body does not load assets, edit, compile or save; normal tool-call recording may still record this invocation.
	 * @param Toolsets Required comma- or pipe-separated spellings, e.g. TurboMCP.TurboMCPReadToolset,TurboMCPReadToolset. Whitespace around entries is trimmed; empty entries are ignored. Empty input computes no toolset keys. Names need not exist in the registry.
	 * @param Arguments Required literal string to inspect for an asset key, e.g. {"asset":"/Game/Tests/BP_NodeTest"}. Empty is accepted and yields no asset key. This is heuristic string extraction, not JSON validation or asset resolution. Echoed line breaks and tabs become spaces; extraction uses the original input.
	 */
	UFUNCTION(meta = (AICallable))
	static FString ReadTurboMCPPureChecks(
		const FString& Toolsets,
		const FString& Arguments);

	/**
	 * Query the selected source-control provider; may refresh cached state and contact its server, but does not request checkout, save or submission.
	 * Reports provider availability and per-asset state. Unavailable or missing provider state is unknown, not evidence that a file is clean or untracked. Results describe the provider response at query time.
	 * @param Assets Required comma- or pipe-separated asset paths or unique short names, e.g. /Game/Tests/BP_NodeTest. Empty queries provider status only. Invalid or ambiguous assets are reported individually; other entries are still processed. Use full paths to disambiguate.
	 */
	UFUNCTION(meta = (AICallable))
	static FString ReadTurboMCPVcsState(const FString& Assets);
};

/**
 * Look tools return layout facts or images. WidgetPreview requires per-call consent and runs project code; capture may open editor UI and writes an image file.
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
	 * Inspect graph geometry and layout findings; loads the Blueprint and measures Slate geometry without requesting graph edits, compile or save.
	 * Requires editor rendering. Wire lengths use estimated pin positions, not rendered wire curves. Findings are heuristic review aids, not proof of correctness; some lists and output are capped.
	 * @param Asset Required Blueprint path or unique short name, e.g. /Game/Tests/BP_NodeTest. Empty or unresolved input is refused.
	 * @param GraphName Required exact graph name, e.g. EventGraph. Empty or unresolved names are refused; use the Blueprint reader to list graphs.
	 * @param AlignmentTolerance Optional finite vertical pin-offset tolerance in graph units, 0..1024; default 64. Use 8 for stricter alignment review. Does not suppress overlaps or backward-wire findings. Values outside the range are refused.
	 */
	UFUNCTION(meta = (AICallable))
	static FString LookTurboMCPLayoutReport(const FString& Asset, const FString& GraphName, double AlignmentTolerance = 64.0);

	/**
	 * Return layout findings and capture a graph image; may open editor UI and writes an image file, but does not request asset edits, compile or save.
	 * Requires rendering. This is a human review aid, not an automatic done/pass verdict. A capture failure remains visible alongside any successful text report.
	 * @param Asset Required Blueprint path or unique short name, e.g. /Game/Tests/BP_NodeTest. Empty or unresolved input is refused.
	 * @param GraphName Required exact graph name, e.g. EventGraph. The same graph is measured and captured; empty or unresolved names are refused.
	 * @param AlignmentTolerance Optional finite vertical pin-offset tolerance in graph units, 0..1024; default 64. Use 8 for stricter review. Affects alignment findings, not capture framing, overlaps or backward-wire findings.
	 */
	UFUNCTION(meta = (AICallable))
	static FString LookTurboMCPLayoutDoneCheck(const FString& Asset,const FString& GraphName,double AlignmentTolerance = 64.0);

	/**
	 * Read node IDs, stored positions and measured sizes; loads the Blueprint and uses rendering without requesting graph edits, compile or save.
	 * Coordinates and sizes are in graph units. A capped response marks omitted nodes and must not be treated as a complete inventory.
	 * @param Asset Required Blueprint path or unique short name, e.g. /Game/Tests/BP_NodeTest. Empty or unresolved input is refused.
	 * @param GraphName Required exact graph name, e.g. EventGraph. Empty or unresolved names are refused; list graphs with ReadTurboMCPBlueprintLoad first.
	 */
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
     * Execution is per-call consent: empty/deny refuses; allow permits project code.
     * Script effects are not sandboxed or reversible, even without a named callback.
	 * @param Widget Required Widget Blueprint path or unique short name, e.g. /Game/Tests/WBP_NodeTest. Empty is refused; the widget needs a compiled generated class.
     * @param SampleValuesJson JSON object encoded as a string, mapping variable names to values, e.g. {"Label":"Hello"}. Empty uses defaults. Values must be strings, numbers or booleans; structs/arrays use engine text-import strings. Nested JSON and null are refused before construction. Unknown variables are reported individually.
     * @param FunctionName Optional zero-parameter widget function to execute after setting sample values. Empty skips the explicit callback; widget construction can still execute code.
     * @param Size Pixel dimensions as WxH or W,H, e.g. 1280x720. Empty defaults to 1280x720. Each dimension must contain digits only and be 1..4096; surrounding whitespace is accepted. Fractions, signs and oversized values are refused before construction.
     * @param Execution Per-call consent: allow permits project code; empty or deny refuses before loading. Other values are refused. Defaults to deny; effects are not sandboxed or reversible.
     */
	UFUNCTION(meta = (AICallable))
	static FString LookTurboMCPWidgetPreview(
		const FString& Widget,
		const FString& SampleValuesJson,
		const FString& FunctionName,
		const FString& Size,
        const FString& Execution = TEXT("deny"));

	/**
	 * Capture PNG; may load assets and change editor UI/view/selection; editor callbacks may run; no explicit asset compile/save; no automatic undo; image is not proof of graph correctness.
	 * Requires a rendered editor. May load assets, open editor UI and change the view or selection.
	 * Does not explicitly compile or save assets; editor callbacks may run. no automatic undo.
	 * Refusals can occur after opening UI. An image is not proof of graph correctness.
	 * All five string arguments are required by the schema; supply empty strings where unused.
	 * @param Asset Asset path or unique name, e.g. /Game/Tests/BP_NodeTest. Empty: allowed for screen; asset-editor lists supported kinds.
	 * @param GraphName Exact graph name, e.g. EventGraph; required for graph and diagnostic modes. For asset-editor, optional editor kind from its support table (empty auto-detects). Ignored by screen.
	 * @param Mode Case-insensitive: graph (also empty), box, node, region, selection, neighbours, box-neighbours, screen, asset-editor. Selection uses graph-canvas nodes only. Diagnostics: debug-window, rhi-state, select-nodes (changes graph selection).
	 * @param Target Node ID from ReadTurboMCPNodeFind for node/neighbours; zero-based comment index from LookTurboMCPLayoutReport for box/box-neighbours. Region: x,y,width,height, finite values within +/-10000000, positive dimensions. select-nodes: comma-separated node keys. Otherwise pass empty.
	 * @param Size Graph/box/node/region: WxH or W,H, each 1..4096 pixels, e.g. 1600x900. Empty uses native crop sizing, not a guaranteed resolution. Selection/neighbours/box-neighbours: digits 0..4096 (empty 0); neighbourhood modes use this as traversal depth/count and selectors size the image automatically. Screen/asset-editor ignore Size. Diagnostics accept dimensions except rhi-state (ignored).
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
	 * Search available node types; loads Blueprint context and queries editor node actions, without requesting graph edits, compilation or saving.
	 * Results are ranked and capped; PreviewTurboMCPNodePins inspects a returned type.
	 * @param Asset Required Blueprint path or unique short name, e.g. /Game/Tests/BP_NodeTest. Empty or unresolved input is refused.
	 * @param GraphName Exact graph name, e.g. EventGraph. Empty selects the Blueprint event graph; missing or ambiguous graphs are refused.
	 * @param Words Search text or candidate type ID, e.g. PrintString. Empty is allowed only with an explicit Limit for bounded browsing.
	 * @param Limit Maximum results as digits from 1 to 200, e.g. 10. Empty defaults to 10; signs, fractions, zero and larger values are refused.
	 */
	UFUNCTION(meta = (AICallable))
	static FString PreviewTurboMCPNodeType(
		const FString& Asset,
		const FString& GraphName,
		const FString& Words,
		const FString& Limit);

	/**
	 * Inspect pins by constructing a temporary node; loads assets and executes editor probe code, without requesting compilation or saving.
	 * Reports pin directions, defaults and graph restrictions; this is not proof of runtime behavior or absence of all side effects.
	 * @param Asset Required Blueprint path or unique short name, e.g. /Game/Tests/BP_NodeTest. Empty or unresolved input is refused.
	 * @param GraphName Exact graph name, e.g. EventGraph. Empty selects the Blueprint event graph; missing or ambiguous graphs are refused.
	 * @param TypeId Required exact, case-sensitive ID returned by PreviewTurboMCPNodeType. Copy the complete ID; empty or approximate names are refused.
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
	 * NodeIds accepts "@unboxed": alone selects all unboxed nodes; with node IDs filters that selection to unboxed nodes. Boxed nodes and comments stay fixed.
	 * `NodeIds` scopes the pass to a block. An empty list tidies the whole graph; otherwise every
	 * unselected node is a fixed anchor and only the named nodes may move. The block is moved to clear
	 * space rather than through its anchors. The preview reports the planned score and the edit reports
	 * the score it actually measured after writing.
	 *
	 * WrapAfter optionally lists comma-separated execution node IDs at natural breaks (e.g. a latent call); their continuations start a new row. No links change.
	 * HorizontalGap/VerticalGap set full-detail clearance; LanePitch is minimum Sequence row spacing, expanded for tall pure trees. MainPath accepts "auto" for the longest acyclic execution path, or comma-separated connected reader node IDs for the top exec lane. Auto refuses execution cycles.
	 * Structural quality (including requested row breaks) takes priority; total wire length breaks ties.
	 *
	 * @param Asset Existing Blueprint path or unique short name, e.g. /Game/Tests/BP_NodeTest. Use a full path to disambiguate.
	 * @param GraphName Exact existing graph name, e.g. EventGraph. Select it from the reader; do not guess.
	 * @param bForce Required boolean. false keeps quality-based acceptance; true permits the proposed layout even when its score does not improve. Does not bypass scope or collision checks.
	 * @param NodeIds Array of reader node IDs; [] selects the whole graph. Unselected nodes remain fixed. ["@unboxed"] selects unboxed nodes; combine it with IDs to restrict those IDs to unboxed nodes.
	 * @param MainPath Empty uses normal layout. auto chooses the longest acyclic execution path; otherwise comma-separated connected execution node IDs in desired order. Cycles are refused in auto mode.
	 * @param HorizontalGap Finite horizontal clearance in graph units, 40..2000; default 120. Example: 160.
	 * @param VerticalGap Finite vertical clearance in graph units, 40..2000; default 100. Example: 120.
	 * @param LanePitch Finite minimum Sequence-row spacing in graph units, 100..10000; default 280. Rows expand when needed for tall trees.
	 * @param WrapAfter Empty means no requested row breaks. Otherwise comma-separated unique execution node IDs within scope, each with a continuation. Following execution starts a new row; links are unchanged.
	 */
	UFUNCTION(meta = (AICallable))
	static FString PreviewTurboMCPGraphTidy(
		const FString& Asset,
		const FString& GraphName,
		bool bForce,
		const TArray<FString>& NodeIds,
		const FString& MainPath = TEXT(""), double HorizontalGap = 120.0, double VerticalGap = 100.0, double LanePitch = 280.0, const FString& WrapAfter = TEXT(""));

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
	 *
	 * @param Asset Existing Blueprint path or unique short name, e.g. /Game/Tests/BP_NodeTest. Use a full path to disambiguate.
	 * @param GraphName Exact existing graph name, e.g. EventGraph. Select it from the reader; do not guess.
	 * @param bForce Required boolean. false requires the proposed wire-length improvement; true permits applying the planned placement without that improvement. Other safety checks still apply.
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
	 * take this same text and reach the same decision through this same check, but each operation also has its own scope and edit guards. A text that fails here names
	 * what to fix rather than failing halfway through a write.
	 *
	 * A graph name is required, not optional: a node type and a pin name are both
	 * resolved against a live graph, so an unnamed graph is refused rather than
	 * guessed at.
	 *
	 * Plans this operation without applying it to the target graph. A successful preview
	 * does not guarantee a later edit: the target state and edit guards can change.
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
	 * the edit rechecks the current graph and its mutation guards.
	 *
	 * Plans this operation without applying it to the target graph. A successful preview
	 * does not guarantee a later edit: the target state and edit guards can change.
     * Local labels name the PRECEDING node form. Prefer `(TypeId) ; label Name` for a pinless node,
     * or put `; label Name` on its own line after the closed form. Never put a label before its node.
     * Reference it as `(-> Name:ExactPinName)`. A pinless form needs no dummy pin. Semicolon comments
     * consume the rest of their line: put the closing parenthesis BEFORE a trailing label/comment.
	 *
	 * Continuation: auto (legacy unique exit), all (all reachable new unconnected exec exits), or label:pin (one named new node output). Applies to displaced existing exec wires; explicit submitted target links take precedence. Compile/save deferred.
	 *
	 * @param Asset Existing Blueprint path or unique short name, e.g. /Game/Tests/BP_NodeTest. Use a full path when names are ambiguous.
	 * @param GraphName Exact existing graph name, e.g. EventGraph. Use the reader to select the intended graph; do not guess.
	 * @param Text Nonempty append graph text. New forms create nodes; reader-derived IDs refer to existing nodes. Use Preview first and preserve exact type IDs and pin names. Partial reader pages are not standalone append input.
	 * @param Continuation auto (default or empty), all, or label:pin. Controls reconnection of displaced execution wires to new exits; explicit target links take precedence. Example: Done:then for a submitted local label.
	*/
	UFUNCTION(meta = (AICallable))
	static FString PreviewTurboMCPGraphAppend(
		const FString& Asset,
		const FString& GraphName,
		const FString& Text,
        const FString& Continuation = TEXT("auto"));

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
	 * Plans this operation without applying it to the target graph. A successful preview
	 * does not guarantee a later edit: the target state and edit guards can change.
	 */
	UFUNCTION(meta = (AICallable))
	static FString PreviewTurboMCPGraphReplace(
		const FString& Asset,
		const FString& GraphName,
		const FString& Text);

	/**
	 * Inspect the latest global undo entry for the named Blueprint and return its confirmation GUID; does not undo. Refuses unrelated or multi-package entries.
	 * @param Asset Required Blueprint path or unique short name, e.g. /Game/Tests/BP_NodeTest. Empty or ambiguous values are refused; prefer the full path.
	 */
	UFUNCTION(meta = (AICallable))
	static FString PreviewTurboMCPAssetUndo(const FString& Asset);

	/** Translate selected nodes or entire comment groups, including nested boxes; keeps internal layout and wiring. Refuses new collisions or changed external box membership unless DestinationBox explicitly names a comment to receive selected unboxed nodes; they must fit inside without resizing it. */
	UFUNCTION(meta = (AICallable))
	static FString PreviewTurboMCPGraphGroupMove(const FString& Asset, const FString& GraphName, const TArray<FString>& NodeIds, int32 DeltaX, int32 DeltaY, const FString& DestinationBox = TEXT(""));

    /** Propose a collision-free translation near outside nodes linked to the selected nodes/comment groups. Returns DeltaX/DeltaY for GraphGroupMove and per-wire before/after lengths. Preserves internal layout; searches free positions and connected comment boxes with room for the unboxed selection. Returns explicit DestinationBox when transferring into a box. Never edits, saves or compiles. */
    UFUNCTION(meta = (AICallable))
    static FString PreviewTurboMCPGraphGroupPlacement(const FString& Asset, const FString& GraphName, const TArray<FString>& NodeIds);


	/**
	 * Preview discarding all unsaved changes in a saved Blueprint package; does not reload or save. Returns a confirmation valid for 60 seconds. The edit can reinstance references and reset global undo history.
	 * @param Asset Required Blueprint path or unique short name, e.g. /Game/Tests/BP_NodeTest. Empty or ambiguous values are refused; prefer the full path. A saved package must exist.
	 */
	UFUNCTION(meta = (AICallable))
	static FString PreviewTurboMCPAssetRevert(const FString& Asset);

	/** Preview call swap. Options is optional JSON: {"pin_map":{"Old":"New"},"new_inputs":{"NewInput":{"node":"reader node ID","pin":"OutputPin"}}}. Unmapped pins retain exact names; keepOld defaults true.
	 * @param Asset Existing Blueprint path or unique short name, e.g. /Game/Tests/BP_NodeTest. Use a full path when names are ambiguous.
	 * @param GraphName Exact existing graph name, e.g. EventGraph. Use the reader to select the intended graph; do not guess.
	 * @param NodeId Exact reader ID of the existing call node to replace; empty is invalid.
	 * @param ReplacementTypeId Exact replacement call type ID returned by node-type discovery for this graph. Do not use a display name or guessed C++ name.
	 * @param KeepOld true or yes (also empty) keeps the original node disconnected; false or no removes it after verification. Other values are refused.
	 * @param Options JSON object encoded as a string, or empty for exact-name pin matching. Optional pin_map maps old pin names to new names; new_inputs maps new input names to objects with node and pin. Example: {"pin_map":{"Old":"New"}}. Unknown fields are refused.
	*/
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

    /** Connect exact existing node/pin IDs. Data replaces the target input source; exec replaces only the named source output destination and preserves other incoming target paths. No automatic continuation or casts. Same scratch preview and one-undo edit; compile/save deferred. Preview first.
	 * @param Asset Existing Blueprint path or unique short name, e.g. /Game/Tests/BP_NodeTest. Use a full path when names are ambiguous.
	 * @param GraphName Exact existing graph name, e.g. EventGraph. Use the reader to select the intended graph; do not guess.
	 * @param SourceNodeId Exact reader ID of the existing output node in this graph; empty or unresolved IDs are refused.
	 * @param SourcePin Exact output pin name from the reader, e.g. then. No automatic continuation is added.
	 * @param TargetNodeId Exact reader ID of the existing input node in this graph; empty or unresolved IDs are refused.
	 * @param TargetPin Exact input pin name from the reader, e.g. execute. Data replaces this input source; execution replaces only the named source output destination, preserving other incoming paths.
	*/
    UFUNCTION(meta = (AICallable))
    static FString PreviewTurboMCPGraphConnect(const FString& Asset,const FString& GraphName,const FString& SourceNodeId,const FString& SourcePin,const FString& TargetNodeId,const FString& TargetPin);

    /** Remove exact nodes and links with scratch preview and one undo. Nodes is a JSON array of reader node IDs. Links is a JSON array of objects with sourceNodeId, sourcePin, targetNodeId, targetPin. Use [] for either unused list. Node deletion also removes incident links; other nodes/positions are preserved. No automatic reconnection, compile or save.
	 *
	 * @param Asset Existing Blueprint path or unique short name, e.g. /Game/Tests/BP_NodeTest. Use a full path to disambiguate.
	 * @param GraphName Exact existing graph name, e.g. EventGraph. Select it from the reader; do not guess.
	 * @param Nodes JSON array encoded as a string, e.g. ["exact reader node ID"]. Use [] when removing only links, not an empty string. Duplicate, missing and non-deletable nodes are refused.
	 * @param Links JSON array encoded as a string. Each object requires sourceNodeId, sourcePin, targetNodeId and targetPin for an existing link. Use [] when removing only nodes. Do not repeat links already removed by a selected node deletion.
	 */
    UFUNCTION(meta = (AICallable))
    static FString PreviewTurboMCPGraphRemove(const FString& Asset,const FString& GraphName,const FString& Nodes,const FString& Links);

    /** Preview a Create Event node bound to a named function and connect it to the exact delegate input.
     * Optional ObjectNodeId and ObjectPin select an object-reference output; both empty means self.
     * Signature must match. Replaces that input's previous source. Uses transient preview and owned undo;
     * does not create the function, compile or save. */
    UFUNCTION(meta = (AICallable))
    static FString PreviewTurboMCPBindFunction(const FString& Asset,const FString& GraphName,const FString& TargetNodeId,const FString& TargetPin,const FString& FunctionName,const FString& ObjectNodeId = TEXT(""),const FString& ObjectPin = TEXT(""));

    /** Preview a new custom event whose parameters come from the exact target delegate input.
     * EventName must be unused. Event graph only; replaces the delegate input source.
     * The event body starts empty. Does not compile or save; preview uses a transient copy. */
    UFUNCTION(meta = (AICallable))
    static FString PreviewTurboMCPBindNewEvent(const FString& Asset,const FString& GraphName,const FString& TargetNodeId,const FString& TargetPin,const FString& EventName);




    /** Copy a pure self/getter/literal per consumer; never impure calls. Preserves defaults and input sources. Run layout afterwards.
	 *
	 * @param Asset Existing Blueprint path or unique short name, e.g. /Game/Tests/BP_NodeTest. Use a full path to disambiguate.
	 * @param GraphName Exact existing graph name, e.g. EventGraph. Select it from the reader; do not guess.
	 * @param NodeId Exact reader ID of a supported pure self, getter or literal node to copy per consumer. Impure calls are refused. Run layout after the edit; no compile or save.
	 */
    UFUNCTION(meta = (AICallable))
    static FString PreviewTurboMCPGraphSplitValue(const FString& Asset,const FString& GraphName,const FString& NodeId);

    /** Refactor an explicitly ordered linear chain of synchronous calls into Sequence rows. Groups is JSON [["reader ID 1","reader ID 2"],["reader ID 3"]]. Keeps original execution order and puts continuation last; refuses latent/branching/cyclic groups. Optional trivial-value splitting; one undo, no compile/save.
	 *
	 * @param Asset Existing Blueprint path or unique short name, e.g. /Game/Tests/BP_NodeTest. Use a full path to disambiguate.
	 * @param GraphName Exact existing graph name, e.g. EventGraph. Select it from the reader; do not guess.
	 * @param Groups JSON array encoded as a string: nonempty arrays of reader node IDs, e.g. [["first ID","second ID"],["third ID"]]. Groups must form an ordered linear chain of synchronous calls; latent, branching and cyclic groups are refused.
	 * @param bSplitValues Optional boolean, default true. Split supported trivial values for the selected groups when true. false leaves those shared values in place; execution ordering checks still apply.
	 */
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
	 *
	 * @param Asset Existing Blueprint path or unique short name, e.g. /Game/Tests/BP_NodeTest. Use a full path when names are ambiguous.
	 * @param GraphName Exact existing graph name, e.g. EventGraph. Use the reader to select the intended graph; do not guess.
	 * @param Text Nonempty graph text containing existing reader node IDs and only changed literal pin defaults. No node creation, removal or link rewiring. Example: a reader-derived PrintString form with its original ID and changed InString value.
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
	 * Inspect checkout eligibility; refreshes provider state and may contact the server, but does not request checkout, asset compilation or saving.
	 * This is a current-state report, not a reservation or guarantee that a later checkout will succeed.
	 * @param Assets Comma- or pipe-separated asset paths or unique names, e.g. /Game/Tests/BP_NodeTest. Assets must have files on disk. Empty reports provider status only. Each unresolved asset is reported separately.
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
	 * Compile up to 25 named assets and report compiler and engine diagnostics; does not save.
	 * Compilation can reinstance classes and affect dependent objects. Refuses during an active editor transaction. Inspect each asset result before saving; this is not an atomic batch.
	 * @param Assets Required comma- or pipe-separated asset paths or unique short names, e.g. /Game/Tests/BP_NodeTest|/Game/Tests/WBP_NodeTest. Empty lists and lists over 25 entries are refused. Unsupported asset types are reported individually.
	 */
	UFUNCTION(meta = (AICallable))
	static FString EditTurboMCPCompile(const FString& Assets);

	/**
	 * Write named asset packages to disk, including packages whose dirty flag is clear; does not explicitly compile.
	 * Widget Blueprints use direct package serialization without refreshing their thumbnail. Other assets use the editor save path, which may render thumbnails and execute asset code. Saving is not an undoable graph edit.
	 * Refuses during play, simulation or an active editor transaction. Each asset is saved separately, with one retry on failure; earlier writes are not rolled back if another asset fails. Source-control checkout results are reported and do not guarantee a lock or block the save. Inspect each result.
	 * @param Assets Required comma- or pipe-separated asset paths or unique short names, e.g. /Game/Tests/BP_NodeTest. Empty lists and lists over 25 entries are refused. This is an explicit save list, not a dirty-asset filter.
	 */
	UFUNCTION(meta = (AICallable))
	static FString EditTurboMCPSave(const FString& Assets);

	/**
	 * Write named asset packages directly to disk without refreshing thumbnails; does not explicitly compile.
	 * Skips the editor thumbnail draw, but package serialization still runs engine and asset save callbacks. This is not a sandbox or an undoable graph edit. Existing thumbnails may remain stale. Mounted package filenames must resolve successfully.
	 * Refuses during play, simulation or an active editor transaction. Saves each asset separately with one retry on failure; earlier writes are not rolled back. Source-control checkout results are reported, not a guarantee that a lock was acquired.
	 * @param Assets Required comma- or pipe-separated asset paths or unique short names, e.g. /Game/Tests/BP_NodeTest. Empty lists and lists over 25 entries are refused. Explicitly saves even packages whose dirty flag is clear.
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
	 * Include "@unboxed" to leave boxed nodes fixed (alone: all unboxed; with IDs: only unboxed IDs).
	 * WrapAfter is the same comma-separated execution-node row-break option as the preview; links remain unchanged.
	 *
	 * @param Asset Existing Blueprint path or unique short name, e.g. /Game/Tests/BP_NodeTest. Use a full path to disambiguate.
	 * @param GraphName Exact existing graph name, e.g. EventGraph. Select it from the reader; do not guess.
	 * @param bForce Required boolean. false keeps quality-based acceptance; true permits the proposed layout even when its score does not improve. Does not bypass scope or collision checks.
	 * @param NodeIds Array of reader node IDs; [] selects the whole graph. Unselected nodes remain fixed. ["@unboxed"] selects unboxed nodes; combine it with IDs to restrict those IDs to unboxed nodes.
	 * @param MainPath Empty uses normal layout. auto chooses the longest acyclic execution path; otherwise comma-separated connected execution node IDs in desired order. Cycles are refused in auto mode.
	 * @param HorizontalGap Finite horizontal clearance in graph units, 40..2000; default 120. Example: 160.
	 * @param VerticalGap Finite vertical clearance in graph units, 40..2000; default 100. Example: 120.
	 * @param LanePitch Finite minimum Sequence-row spacing in graph units, 100..10000; default 280. Rows expand when needed for tall trees.
	 * @param WrapAfter Empty means no requested row breaks. Otherwise comma-separated unique execution node IDs within scope, each with a continuation. Following execution starts a new row; links are unchanged.
	 */
	UFUNCTION(meta = (AICallable))
	static FString EditTurboMCPGraphTidy(
		const FString& Asset,
		const FString& GraphName,
		bool bForce,
		const TArray<FString>& NodeIds,
		const FString& MainPath = TEXT(""), double HorizontalGap = 120.0, double VerticalGap = 100.0, double LanePitch = 280.0, const FString& WrapAfter = TEXT(""));

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
	 *
	 * @param Asset Existing Blueprint path or unique short name, e.g. /Game/Tests/BP_NodeTest. Use a full path to disambiguate.
	 * @param GraphName Exact existing graph name, e.g. EventGraph. Select it from the reader; do not guess.
	 * @param bForce Required boolean. false requires the proposed wire-length improvement; true permits applying the planned placement without that improvement. Other safety checks still apply.
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
	 * Enable or disable call recording and request persistence in editor settings. Existing records are retained. Calls made while recording was off are not recovered.
	 * @param Enabled Required boolean text: true/on/yes/1 enables recording; false/off/no/0 disables it. Case-insensitive with surrounding whitespace ignored. Empty and other values are refused. Example: true.
	 */
	UFUNCTION(meta = (AICallable))
	static FString EditTurboMCPCallLog(const FString& Enabled);

	/**
	 * Clear retained in-memory call records after confirmation. Leaves the disk log and recording setting unchanged. This request may itself leave new records when recording is enabled.
	 * @param Confirm Required confirmation text: yes or true, case-insensitive with surrounding whitespace ignored. Empty and other values are refused. Example: yes.
	 */
	UFUNCTION(meta = (AICallable))
	static FString EditTurboMCPCallLogClear(const FString& Confirm);

	/**
	 * Manage in-memory trace points; does not edit graph nodes, compile or save assets, and clearing recorded hits is not undoable.
	 * Arming registers IDs without verifying that the nodes exist or will emit trace events. It does not start gameplay.
	 * Hits require execution and available Blueprint instrumentation; zero hits do not prove that a node did not run.
	 * @param Action Required command: arm, clear or status (case-insensitive). Empty is refused. Arm adds points; clear removes points and their recorded hits; status reports current trace state.
	 * @param NodeId Comma-separated exact node IDs from ReadTurboMCPNodeFind, e.g. Asset:EventGraph:NodeKey. Arm requires at least one; clear with empty removes ALL points and hits. Status ignores this argument. Supply an empty string when unused.
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
	 *
	 * Continuation: auto (legacy unique exit), all (all reachable new unconnected exec exits), or label:pin (one named new node output). Applies to displaced existing exec wires; explicit submitted target links take precedence. Compile/save deferred.
	 *
	 * @param Asset Existing Blueprint path or unique short name, e.g. /Game/Tests/BP_NodeTest. Use a full path when names are ambiguous.
	 * @param GraphName Exact existing graph name, e.g. EventGraph. Use the reader to select the intended graph; do not guess.
	 * @param Text Nonempty append graph text. New forms create nodes; reader-derived IDs refer to existing nodes. Use Preview first and preserve exact type IDs and pin names. Partial reader pages are not standalone append input.
	 * @param Continuation auto (default or empty), all, or label:pin. Controls reconnection of displaced execution wires to new exits; explicit target links take precedence. Example: Done:then for a submitted local label.
	*/
	UFUNCTION(meta = (AICallable))
	static FString EditTurboMCPGraphAppend(
		const FString& Asset,
		const FString& GraphName,
		const FString& Text,
        const FString& Continuation = TEXT("auto"));

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

	/**
	 * Undo the latest global TurboMCP transaction for the named Blueprint; does not explicitly compile or save. Never skips unrelated entries. Editor Redo remains available after successful undo.
	 * @param Asset Required Blueprint path or unique short name, e.g. /Game/Tests/BP_NodeTest. Empty or ambiguous values are refused; prefer the full path.
	 * @param Confirm Required exact transaction GUID returned by PreviewTurboMCPAssetUndo for this asset. Empty or stale confirmation is refused; preview again if the undo stack changes.
	 */
	UFUNCTION(meta = (AICallable))
	static FString EditTurboMCPAssetUndo(const FString& Asset, const FString& Confirm);

	/** Translate selected nodes or entire comment groups, including nested boxes; keeps internal layout and wiring. Refuses new collisions or changed external box membership unless DestinationBox explicitly names a comment to receive selected unboxed nodes; they must fit inside without resizing it. */
	UFUNCTION(meta = (AICallable))
	static FString EditTurboMCPGraphGroupMove(const FString& Asset, const FString& GraphName, const TArray<FString>& NodeIds, int32 DeltaX, int32 DeltaY, const FString& DestinationBox = TEXT(""));

	/**
	 * Reload a saved Blueprint package, discarding ALL unsaved changes, including edits made after preview; does not save. May reinstance references and reset global undo history. Re-read asset IDs and references after reload.
	 * @param Asset Required Blueprint path or unique short name, e.g. /Game/Tests/BP_NodeTest. Empty or ambiguous values are refused; prefer the full path. A saved package must exist.
	 * @param Confirm Required exact DISCARD confirmation string from the latest PreviewTurboMCPAssetRevert for this asset. Expires after 60 seconds and is consumed before reload. Empty, expired or replaced tokens are refused; preview again. Do not construct a token yourself.
	 */
	UFUNCTION(meta = (AICallable))
	static FString EditTurboMCPAssetRevert(const FString& Asset, const FString& Confirm);

	/**
	 * Set node positions in one undo step. No compile or save.
	 * Positions is a STRING containing a JSON array, not lines of text or a JSON object.
	 * Example: [{"node":"exact reader node ID","x":100,"y":200}]
	 * Each object must contain exactly node, x, y (case-sensitive); not id or nodeId.
	 * Use IDs from LookTurboMCPGraphPositions or the graph reader, not display titles.
	 * x and y are absolute integer graph coordinates, including negative coordinates,
	 * in the signed 32-bit range. Supply 1..4096 unique nodes; omit unchanged nodes.
	 * Validates the entire batch before writing; unlisted nodes stay in place.
	 * @param Positions JSON array encoded as a string. Example: [{"node":"exact reader node ID","x":100,"y":200}]
	 *
	 * @param Asset Existing Blueprint path or unique short name, e.g. /Game/Tests/BP_NodeTest. Use a full path when names are ambiguous.
	 * @param GraphName Exact existing graph name, e.g. EventGraph. Use the reader to select the intended graph; do not guess.
	*/
	UFUNCTION(meta = (AICallable))
	static FString EditTurboMCPGraphPositions(const FString& Asset, const FString& GraphName, const FString& Positions);

	/** Apply a localized call-node swap. keepOld=true retains the old node unplugged; false deletes it after verification.
	 * @param Asset Existing Blueprint path or unique short name, e.g. /Game/Tests/BP_NodeTest. Use a full path when names are ambiguous.
	 * @param GraphName Exact existing graph name, e.g. EventGraph. Use the reader to select the intended graph; do not guess.
	 * @param NodeId Exact reader ID of the existing call node to replace; empty is invalid.
	 * @param ReplacementTypeId Exact replacement call type ID returned by node-type discovery for this graph. Do not use a display name or guessed C++ name.
	 * @param KeepOld true or yes (also empty) keeps the original node disconnected; false or no removes it after verification. Other values are refused.
	 * @param Options JSON object encoded as a string, or empty for exact-name pin matching. Optional pin_map maps old pin names to new names; new_inputs maps new input names to objects with node and pin. Example: {"pin_map":{"Old":"New"}}. Unknown fields are refused.
	*/
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

    /** Connect exact existing node/pin IDs. Data replaces the target input source; exec replaces only the named source output destination and preserves other incoming target paths. No automatic continuation or casts. Same scratch preview and one-undo edit; compile/save deferred. Preview first.
	 * @param Asset Existing Blueprint path or unique short name, e.g. /Game/Tests/BP_NodeTest. Use a full path when names are ambiguous.
	 * @param GraphName Exact existing graph name, e.g. EventGraph. Use the reader to select the intended graph; do not guess.
	 * @param SourceNodeId Exact reader ID of the existing output node in this graph; empty or unresolved IDs are refused.
	 * @param SourcePin Exact output pin name from the reader, e.g. then. No automatic continuation is added.
	 * @param TargetNodeId Exact reader ID of the existing input node in this graph; empty or unresolved IDs are refused.
	 * @param TargetPin Exact input pin name from the reader, e.g. execute. Data replaces this input source; execution replaces only the named source output destination, preserving other incoming paths.
	*/
    UFUNCTION(meta = (AICallable))
    static FString EditTurboMCPGraphConnect(const FString& Asset,const FString& GraphName,const FString& SourceNodeId,const FString& SourcePin,const FString& TargetNodeId,const FString& TargetPin);

    /** Remove exact nodes and links with scratch preview and one undo. Nodes is a JSON array of reader node IDs. Links is a JSON array of objects with sourceNodeId, sourcePin, targetNodeId, targetPin. Use [] for either unused list. Node deletion also removes incident links; other nodes/positions are preserved. No automatic reconnection, compile or save.
	 *
	 * @param Asset Existing Blueprint path or unique short name, e.g. /Game/Tests/BP_NodeTest. Use a full path to disambiguate.
	 * @param GraphName Exact existing graph name, e.g. EventGraph. Select it from the reader; do not guess.
	 * @param Nodes JSON array encoded as a string, e.g. ["exact reader node ID"]. Use [] when removing only links, not an empty string. Duplicate, missing and non-deletable nodes are refused.
	 * @param Links JSON array encoded as a string. Each object requires sourceNodeId, sourcePin, targetNodeId and targetPin for an existing link. Use [] when removing only nodes. Do not repeat links already removed by a selected node deletion.
	 */
    UFUNCTION(meta = (AICallable))
    static FString EditTurboMCPGraphRemove(const FString& Asset,const FString& GraphName,const FString& Nodes,const FString& Links);

    /** Edit a Create Event node bound to a named function and connect it to the exact delegate input.
     * Optional ObjectNodeId and ObjectPin select an object-reference output; both empty means self.
     * Signature must match. Replaces that input's previous source. Uses transient preview and owned undo;
     * does not create the function, compile or save. */
    UFUNCTION(meta = (AICallable))
    static FString EditTurboMCPBindFunction(const FString& Asset,const FString& GraphName,const FString& TargetNodeId,const FString& TargetPin,const FString& FunctionName,const FString& ObjectNodeId = TEXT(""),const FString& ObjectPin = TEXT(""));

    /** Edit a new custom event whose parameters come from the exact target delegate input.
     * EventName must be unused. Event graph only; replaces the delegate input source.
     * The event body starts empty. Does not compile or save; preview uses a transient copy. */
    UFUNCTION(meta = (AICallable))
    static FString EditTurboMCPBindNewEvent(const FString& Asset,const FString& GraphName,const FString& TargetNodeId,const FString& TargetPin,const FString& EventName);




    /** Copy a pure self/getter/literal per consumer; never impure calls. Preserves defaults and input sources. Run layout afterwards.
	 *
	 * @param Asset Existing Blueprint path or unique short name, e.g. /Game/Tests/BP_NodeTest. Use a full path to disambiguate.
	 * @param GraphName Exact existing graph name, e.g. EventGraph. Select it from the reader; do not guess.
	 * @param NodeId Exact reader ID of a supported pure self, getter or literal node to copy per consumer. Impure calls are refused. Run layout after the edit; no compile or save.
	 */
    UFUNCTION(meta = (AICallable))
    static FString EditTurboMCPGraphSplitValue(const FString& Asset,const FString& GraphName,const FString& NodeId);

    /** Refactor an explicitly ordered linear chain of synchronous calls into Sequence rows. Groups is JSON [["reader ID 1","reader ID 2"],["reader ID 3"]]. Keeps original execution order and puts continuation last; refuses latent/branching/cyclic groups. Optional trivial-value splitting; one undo, no compile/save.
	 *
	 * @param Asset Existing Blueprint path or unique short name, e.g. /Game/Tests/BP_NodeTest. Use a full path to disambiguate.
	 * @param GraphName Exact existing graph name, e.g. EventGraph. Select it from the reader; do not guess.
	 * @param Groups JSON array encoded as a string: nonempty arrays of reader node IDs, e.g. [["first ID","second ID"],["third ID"]]. Groups must form an ordered linear chain of synchronous calls; latent, branching and cyclic groups are refused.
	 * @param bSplitValues Optional boolean, default true. Split supported trivial values for the selected groups when true. false leaves those shared values in place; execution ordering checks still apply.
	 */
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
	 *
	 * @param Asset Existing Blueprint path or unique short name, e.g. /Game/Tests/BP_NodeTest. Use a full path when names are ambiguous.
	 * @param GraphName Exact existing graph name, e.g. EventGraph. Use the reader to select the intended graph; do not guess.
	 * @param Text Nonempty graph text containing existing reader node IDs and only changed literal pin defaults. No node creation, removal or link rewiring. Example: a reader-derived PrintString form with its original ID and changed InString value.
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
	 * Request provider checkout per asset; may contact the server and change source-control or file state, with no automatic rollback of partial success and no asset compile/save request.
	 * Provider results are reported per file; a successful command is not independent proof of an exclusive lock. Providers without checkout support may reject the request. Refresh with ReadTurboMCPVcsState afterward.
	 * @param Assets Comma- or pipe-separated asset paths or unique names, e.g. /Game/Tests/BP_NodeTest. Assets must have files on disk. Empty reports provider status without checking out files. Invalid entries are reported individually; other entries may proceed.
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
