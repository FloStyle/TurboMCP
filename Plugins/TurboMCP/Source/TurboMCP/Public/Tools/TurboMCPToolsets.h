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
    /** Inspect currently mounted Slate text and ancestor visibility without waiting, focusing or sending input. Searches visible top-level windows, including hidden descendants when requested. Text comes from STextBlock or available accessibility text; custom text without either source is not covered. Visibility is not proof of clipping, hit testing, enabled input or OS foreground. Results are JSON; incomplete traversal cannot prove absence.
     * @param Text Required nonempty case-insensitive literal substring, at most 256 characters. No wildcard syntax.
     * @param IncludeHidden Include matches hidden by the widget or an ancestor. False returns effectively visible matches only.
     * @param MatchLimit Maximum returned matches, 1..100. More matches set resultsTruncated.
     * @param MaxDepth Maximum tree depth, 1..128. Children beyond it set traversalTruncated. At most 20000 widgets are visited.
     */
    UFUNCTION(meta = (AICallable))
    static FString ReadTurboMCPSlateText(const FString& Text, bool IncludeHidden = true, int32 MatchLimit = 32, int32 MaxDepth = 64);


    /** Inspect one exact Blueprint-callable function on an existing PIE runtime object without invoking it.
     * @param WorldPath Exact current PIE world object path, not an editor world or guessed player index.
     * @param ObjectPath Exact existing runtime object path belonging to that world, including local-player subsystems. No assets are loaded.
     * @param FunctionName Exact case-sensitive callable function name. Reports parameter types and directions; latent, RPC and editor-only calls are refused.
     */
    UFUNCTION(meta = (AICallable))
    static FString ReadTurboMCPPlayFunction(const FString& WorldPath,const FString& ObjectPath,const FString& FunctionName);

    /** Read bounded editor/PIE output captured during this enabled plugin session, not tool-call history or a disk file. Returns UTC timestamps, categories, severity, cursor and explicit retention/message truncation. Does not run commands or change assets.
     * @param Category Empty for all categories, otherwise exact case-insensitive category name, such as LogBlueprintUserMessages.
     * @param Contains Empty for all messages, otherwise a case-insensitive substring of stored text (at most 2048 characters).
     * @param Since Empty for all retained times, otherwise inclusive ISO-8601 UTC time ending in Z, such as 2026-10-27T10:00:00Z.
     * @param Cursor Empty starts at oldest retained record. Otherwise copy nextCursor from the previous response and keep filters unchanged. Another-session or future cursor refuses; expired retained records report loss.
     * @param Limit Maximum returned records, 1..100, default 50. Whole records also have a 24000-character response budget. Messages are stored up to 2048 UTF-16 characters; truncated tails are not searchable.
     */
    UFUNCTION(meta = (AICallable))
    static FString ReadTurboMCPEditorLog(const FString& Category, const FString& Contains, const FString& Since, const FString& Cursor, int32 Limit = 50);

    /** Read reflected properties of data assets or already-loaded Blueprint class defaults. The first line of a paged read is a JSON object with rows, totals, errors and continuation. The complete flag is true when the first page covers the whole request without errors. Data asset loads may run load hooks and load dependencies. Blueprint defaults use an existing generated class and default object. No property write, save or compile is requested.
     * @param Asset Data asset path or unique name. In paged modes, pass up to 64 separated by commas or pipes. Blueprint defaults require full Blueprint package, object or generated class paths.
     * @param Property Exact reflected path such as Settings.Number. In paged modes, pass up to eight names separated by commas or pipes. Paths can traverse structs and owned instanced children. Empty lists editable fields in legacy mode.
     * @param Mode legacy preserves the single data asset read and edit revision. values pages named values and entries in a selected collection. keys pages map keys or zero-based array indices without reading their values. Default legacy.
     * @param Source asset reads data assets. blueprint_defaults reads defaults from the existing loaded generated class in a paged mode. It never loads or constructs the class or default object. Defaults can reflect the last compile.
     * @param Offset Start at 0. In paged modes copy nextOffset until it is -1. Keep all other inputs and the live data unchanged between pages.
     * @param PageSize Maximum returned rows in paged modes, 1 to 100. Default 32. A response budget can reduce this count.
     */
    UFUNCTION(meta = (AICallable))
    static FString ReadTurboMCPDataAssetProperty(const FString& Asset, const FString& Property,
        const FString& Mode = TEXT("legacy"), const FString& Source = TEXT("asset"), int64 Offset = 0, int32 PageSize = 32);

    /** Inspect the selected PIE player's focus, held synthetic input and Enhanced Input context priorities; no input or asset mutation. Reports optional CommonInput device type and unordered active CommonUI widgets when loaded; unavailable is not zero. Does not determine the routing owner.
     * @param Target Use list to discover in-process local PIE players, then copy an exact session-bound target token. Tokens expire when their world/player ends. The pie:0 alias requires exactly one PIE world and local player. External-process clients are not supported.
     * @param ButtonFilter Optional case-insensitive runtime object path substring; empty matches all candidates. Local widget visibility does not prove ancestor/screen visibility or input eligibility.
     * @param ButtonLimit Optional maximum button paths, 0..64, default 16. Use 0 for diagnostics only. Counts report matching and omitted candidates; narrow ButtonFilter to find a specific menu. Diagnostics precede the list.
     */
    UFUNCTION(meta = (AICallable))
    static FString ReadTurboMCPPlayInputState(const FString& Target,const FString& ButtonFilter = TEXT(""),int32 ButtonLimit = 16);

	/**
	 * Read the MCP server state and this plugin's registration state.
	 *
	 * Use this first in a session to find out whether the server is running, on
	 * which address, and whether this plugin registered its toolsets. It does not
	 * edit assets, start the server or change preferences.
	 *
	 * Returns one line for the server and one line per TurboMCP toolset, or a
	 * line saying why a toolset is missing.
	 */
	UFUNCTION(meta = (AICallable))
	static FString ReadTurboMCPStatus();

	/**
	 * Read a compact summary of the registered toolsets and their available tool counts.
	 *
	 * Includes the server and tool search state. Use ReadTurboMCPToolHelp with
	 * an exact ToolsetName and empty ToolName to list one toolset. Continue with
	 * list:<offset>, or search with find:0:<words>. Pass a bare tool name for
	 * its parameters and known traps. Use describe_toolset for the full schema.
	 *
	 * The summary reports any toolsets omitted to fit the character limit.
	 */
	UFUNCTION(meta = (AICallable))
	static FString ReadTurboMCPToolIndex();

	/**
	 * Read the parameters and known traps of one tool.
	 *
	 * Pass a toolset name and a tool name, or one joined name as tools/list
	 * prints it. Leave the tool name empty for the first page of the named
	 * toolset. Name the tool alone to find its registered toolset.
	 *
	 * Works for the engine's own tools as well as TurboMCP's, because engine tool
	 * help runs to tens of thousands of characters and the traps are where agents
	 * actually lose time.
	 *
	 * Compact help targets 2000 characters for one tool and keeps the
	 * required arguments visible. An unknown name points to discovery.
	 *
	 * Both fields are optional. Omit both to list available toolsets; omit ToolName
	 * to list the named toolset. Other tools retain their own required arguments.
	 *
	 * @param ToolsetName Optional; * or empty searches all toolsets. With both arguments omitted, lists toolsets. Registered toolset name, e.g. TurboMCP.TurboMCPReadToolset. Empty searches all toolsets for ToolName. Supply an empty string when unused.
	 * @param ToolName Optional; * or empty lists tools in the named toolset. ToolSchema uses argument names Toolset and Tool instead. Bare tool name, e.g. ReadTurboMCPStatus. Empty lists the named toolset; use the returned list:<offset> for another page. find:0:<words> searches names and descriptions.
	*/
	UFUNCTION(meta = (AICallable))
	static FString ReadTurboMCPToolHelp(const FString& ToolsetName = TEXT("*"), const FString& ToolName = TEXT("*"));

	/**
	 * Read assets matching a query, like the editor's quick open search.
	 *
	 * For gameplay-tag usage/referencers, discover the stock GameplayTagsToolset
	 * FindReferencersByTag tool and pass tagName (the exact registered tag).
	 * It returns asset-registry package referencers, not proof of runtime execution.
	 * This text search is not a tag-reference search; the stock toolset must be enabled.
	 *
	 * Pass words to match against asset names and folder paths. Every word must
	 * appear in one or the other. The query filters type: and path: match
	 * substrings in the class name and package folder respectively.
	 * Path limits the search to an exact folder and, by default, its subfolders.
	 * Query filters also apply within that scope.
	 *
	 * Ranking is exact name first, then names that start with the words, then
	 * names that contain them, then folder only matches, and shorter names first
	 * on a tie.
	 *
	 * Searches registry data, including registered unsaved assets, without
	 * loading or editing assets. Shows at most 40 matches and reports omissions.
	 *
	 * @param Query Search words with optional type: and path: substring filters, e.g. type:Blueprint menu. Pass an empty string to list matches within Path.
	 * @param Path Optional full mounted content folder, e.g. /Game/UI. Omit or pass * or an empty string for an unscoped search. A single trailing slash is accepted.
	 * @param Recursive Include subfolders when Path is set. Default true. Set false for assets directly in Path.
	*/
	UFUNCTION(meta = (AICallable))
	static FString ReadTurboMCPAssetSearch(const FString& Query, const FString& Path = TEXT("*"), bool Recursive = true);

    /**
     * Export bounded pages from one bulk reader into a single JSON report under Saved/TurboMCP/Exports.
     * Writes a report file, never saves project assets. Package loads may run PostLoad and dirty assets;
     * original reader safety checks and per-page effects are preserved. Complete is false for skipped
     * assets, capped evidence, errors or limits. Time is checked between pages, not during a load.
     * @param Reader Exact ReadTurboMCPRefsBatch, ReadTurboMCPNodeFind or ReadTurboMCPWidgetInventory.
     * @param QueryJSON Reader arguments as one JSON object using lower-camel names. Omit cursor and format; export starts at the first page and forces NodeFind JSON. Unknown fields are refused.
     * @param MaxPages Maximum pages, 1..1000. Default 100. A partial report retains its nextCursor.
     * @param MaxSeconds Time budget checked between pages, 1..120. Default 30. A single page can exceed it.
     * @param MaxMegabytes Maximum UTF-8 report size, 1..64. Default 16. No arbitrary output path or overwrite.
     */
    UFUNCTION(meta = (AICallable))
    static FString ReadTurboMCPBulkExport(const FString& Reader, const FString& QueryJSON,
        int32 MaxPages = 100, int32 MaxSeconds = 30, int32 MaxMegabytes = 16);

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
	 * Find reflected type paths without loading or editing assets; matches do not guarantee pin or writer support.
	 * Searches loaded reflected types and registered user-defined structs/enums. Blueprint generated classes and unloaded native modules are excluded; use AssetSearch/AssetDescribe for Blueprint classes.
	 * Every whitespace-separated word must match the name or path. Lists at most 30 matches with omitted counts. Registry scanning reports partial rather than a complete search; unloaded registry paths are not validated.
	 * @param Words Required nonempty words, e.g. CoreUObject Vector. All words must match, ignoring case; spaces, tabs and newlines separate words.
	 * @param Kind Required filter: struct, enum, class, interface or any. Empty means any. Example: struct. Unknown values are refused.
	 */
	UFUNCTION(meta = (AICallable))
	static FString ReadTurboMCPTypeFind(const FString& Words, const FString& Kind);

	/**
	 * List loaded content packages marked dirty; does not include unsaved maps or other editor state.
	 *
	 * Reports package paths and registry classes without loading assets. A clean dirty
	 * flag is not proof that memory matches disk. The result reports omitted packages
	 * when the requested limit is reached; empty Limit defaults to 50.
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
	 * @param Asset Blueprint path or unique short name. For Context, an empty string uses the asset from a complete NodeId; conflicting selectors refuse.
	 * @param GraphName Exact unique graph name, or an empty string to use the graph from a complete NodeId. Conflicting or ambiguous selectors refuse; graph paths are not selectors.
	 * @param NodeId Required exact full or short node ID emitted by the reader for this graph. Empty, stale or ambiguous IDs are refused; arbitrary GUID prefixes are not accepted here.
	 * @param NextHops Optional number of downstream execution edges to follow, 0..32; default 3. Zero includes the selected node and upstream context but no downstream hops. Values outside the range are refused.
	 */
	UFUNCTION(meta = (AICallable))
	static FString ReadTurboMCPNodeContext(const FString& Asset,const FString& GraphName,const FString& NodeId,int32 NextHops = 3);

	/**
	 * Search Blueprint nodes by text; may load assets, without requesting edits, compile or save.
	 * Returns up to PageSize matches with continuation; scans at most 16 assets and 10000 nodes per call. Searches local graphs, including nested graphs when unscoped, but not inherited assets or native implementations. This is not a gameplay-tag reference search or proof of runtime execution.
	 * @param Asset Blueprint path for asset scope, content folder for folder scope, or empty for project scope.
	 * @param Words Required search terms separated by whitespace, commas or pipes, e.g. Print String. By default every term must match a title, function, variable, class, comment or pin name; matching uses case-insensitive substrings, not phrases. Empty input is refused.
	 * @param GraphName Required string; pass an empty string to search all local graphs, or one exact unique graph name such as EventGraph. Missing or ambiguous names are refused. Graph paths are not accepted selectors.
	 * @param MatchMode all (default) requires every term; any accepts at least one term.
	 * @param IncludeLiterals Include stored defaults on unlinked input pins. Does not evaluate runtime values.
	 * @param NodeClass Exact node class name or full class path; any (default) disables this filter. No subclass expansion.
	 * @param Scope asset (default), folder, or project (/Game and enabled project content plugins).
	 * @param Recursive Include folder descendants; default true.
	 * @param PageSize Maximum result rows per call, 1..40.
	 * @param Cursor first (default) or empty for the first page, then the returned next_cursor. Keep the query and assets unchanged.
	 * @param Format text (default) or json. JSON returns one document including errors, hits, skips, coverage and next_cursor (null at end). Graph searches exclude Data Asset properties; use RefsBatch and DataAssetProperty before concluding an asset is unused.
	 * @param CountOnly Omit rows and count matches within the same scan budget. Follow all cursors for final totals. Skipped assets prevent exhaustive conclusions.
	 */
	UFUNCTION(meta = (AICallable))
	static FString ReadTurboMCPNodeFind(
		const FString& Asset,
		const FString& Words,
		const FString& GraphName,
		const FString& MatchMode = TEXT("all"),
		bool IncludeLiterals = false,
		const FString& NodeClass = TEXT("any"),
		const FString& Scope = TEXT("asset"),
		bool Recursive = true,
		int32 PageSize = 40,
		const FString& Cursor = TEXT("first"),
		bool CountOnly = false,
		const FString& Format = TEXT("text"));

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
	 * reroute references retain their identity while the reader follows the wire. Nodes not reachable from any entry
	 * point are listed separately with a count.
	 *
	 * An event-backed function shows its authored event body and identifies its graph and node.
	 * A missing name lists the existing graphs. Read-only node descriptions and
	 * display labels supplement the exact node and pin identities in the body.
	 *
	 * Omit EntryFilter or supply * or an empty string for all entries. A non-empty EntryFilter matches event/function names by case-insensitive substring. Empty reads all entries. Filtered views are inspection-only. Large reads return inspection pages. Use nextOffset and Revision for subsequent pages; keep all other arguments identical. A changed read refuses the stale token. Partial pages are not standalone write-back input.
	 *
	 * @param Asset Existing Blueprint path or unique short name, e.g. /Game/Tests/BP_NodeTest. Empty is refused.
	 * @param GraphName Exact graph or authored event function name, e.g. EventGraph or ReceiveBeginPlay. Empty is refused.
	 * @param Format read (default, including empty) or json. JSON returns a standalone object per page with body text, body_notes, revision, offset/count/total and nextOffset (-1 at end). Event-backed reads also identify body_graph and event_node. Concatenate body strings; this is inspection data, not write-back input. MaxChars bounds unescaped UTF-16 body units, not JSON envelope or escaping. write is refused.
	 * @param Detail manifest, signatures, bodies or full; default or empty means bodies. Each level adds detail.
	 * @param Offset Zero-based character offset. Start at 0; for later pages use the returned nextOffset and Revision together. Negative values are refused.
	 * @param MaxChars Maximum page characters, 256..24000 inclusive; default 24000. Partial pages are inspection-only, not standalone write-back input.
	 * @param Revision Omit or supply first or an empty string for page zero. Later pages require the revision token from page zero with unchanged arguments; stale tokens are refused.
	 * @param EntryFilter Case-insensitive entry-name substring, e.g. BeginPlay. Empty includes all entries. Filtered views are inspection-only.
	*/
	UFUNCTION(meta = (AICallable))
	static FString ReadTurboMCPGraphLoad(
		const FString& Asset,
		const FString& GraphName,
		const FString& Format = TEXT("read"),
		const FString& Detail = TEXT("bodies"),
        int32 Offset = 0, int32 MaxChars = 24000, const FString& Revision = TEXT("first"), const FString& EntryFilter = TEXT("*"));

	/**
	 * Inspect Blueprint structure and selected graph content; loads assets without requesting edits, compile or save.
	 * This is not a complete export or proof of runtime behavior. Relation analysis is static; omitted content and budget refusals are reported. Widget details are included only for unfiltered text reads above manifest detail.
	 * @param Assets Required comma- or pipe-separated list of 1..8 Blueprint paths or unique short names, e.g. /Game/Tests/BP_NodeTest. Empty input is refused. Per-asset failures do not discard other results. Split the list into smaller calls or read one asset per call if the combined response exceeds the client's capacity.
	 * @param Detail Optional string (default manifest): manifest, signatures, bodies or full (case-insensitive). Empty uses manifest. Higher levels add signatures/relations, graph bodies, then editing details; oversized documents are refused without lowering detail. Text relations distinguish explicit setters, stock container mutation sites and possible non-const reference writes; tracing follows getters through reroutes, not arbitrary aliases.
	 * @param GraphFilter Optional; * (default) or empty selects all graphs. Comma- or pipe-separated graph-name fragments. Empty selects all local graphs; otherwise any case-insensitive substring match is included. No match produces a per-asset refusal listing graph names. Filters graph names, signatures, relations and directly referenced variable/component declarations. Asset identity, parent and interfaces remain asset-level context; filtered reads do not establish whole-asset nonuse.
	 * @param Format Optional string (default read): read or json (case-insensitive); empty uses read. JSON is embedded per asset in a text response with headings/status, not returned as one standalone JSON document. Both formats support all detail levels.
	 * @param TokenBudget Optional string (default 60000) of positive decimal digits, up to 2147483647; empty uses 60000. This limits each asset document in characters. Several asset documents and response headings can together exceed this value. Text and JSON refuse oversized documents with the required character count while preserving requested detail. Raise TokenBudget to that count as a decimal string, narrow GraphFilter, or use ReadTurboMCPGraphLoad for paged graph reads.
	 */
	UFUNCTION(meta = (AICallable))
	static FString ReadTurboMCPBlueprintLoad(
		const FString& Assets,
		const FString& Detail = TEXT("manifest"),
		const FString& GraphFilter = TEXT("*"),
		const FString& Format = TEXT("read"),
		const FString& TokenBudget = TEXT("60000"));

	/**
	 * Read saved registry references into or out of an asset. Unsaved reference changes are not authoritative registry data; no explicit compile or save. Waits for registry scanning before reporting counts.
	 * @param Asset Required asset path or unique short name, e.g. /Game/Tests/BP_NodeTest. Empty or unresolved values are refused; use a full path for ambiguous names.
	 * @param Direction Case-insensitive out (dependencies), in (referencers), or both. Empty defaults to both.
	 * @param Kinds All or a comma-separated list of hard, soft, searchable-name, management.
	 * @param PageSize Maximum rows in a page, 1 to 500. The response size limit can reduce this count.
	 * @param Cursor Omit, pass first, or pass an empty string to start. Continue with nextCursor from the refs-page line.
	 */
	UFUNCTION(meta = (AICallable))
	static FString ReadTurboMCPRefs(
		const FString& Asset, const FString& Direction,
		const FString& Kinds = TEXT("all"), int32 PageSize = 100, const FString& Cursor = TEXT("first"));

	/**
	 * Read saved registry edges for multiple packages in one call. Results describe registry evidence only and can miss unsaved, config or dynamic references.
	 * @param Assets Comma- or pipe-separated asset paths or unique short names. Provide assets or a folder.
	 * @param Folder Mounted content path such as /Game/Art. Used as the seed set when Assets is empty.
	 * @param Recursive Include descendant folders when selecting seeds.
	 * @param ClassFilter Optional exact asset class name or full class path for folder seeds.
	 * @param Direction Dependencies, referencers, or both. Out and in are aliases.
	 * @param Kinds All or a comma-separated list of hard, soft, searchable-name, management.
	 * @param Depth Number of hops, 1 to 6. Each seed and direction expands a package once.
	 * @param Compact Use short class names and omit representative object paths.
	 * @param StopAtRoot For referencers, include one shortest chain to each reached saved package with zero incoming edges of the selected kinds.
	 * @param PageSize Maximum records per page, 1 to 500. The response size limit can reduce this count.
	 * @param Cursor Omit, pass first, or pass an empty string to start. Continue with nextCursor until it is empty.
	 * @param IncludeSeeds Include input-asset records (default true). False returns only reference edges. total, returned and offset count visible records; seedCount still counts inputs. Keep this option unchanged while paging.
	 */
	UFUNCTION(meta = (AICallable))
	static FString ReadTurboMCPRefsBatch(
		const FString& Assets = TEXT("<none>"), const FString& Folder = TEXT("<none>"), bool Recursive = false,
		const FString& ClassFilter = TEXT("<none>"), const FString& Direction = TEXT("both"),
		const FString& Kinds = TEXT("all"), int32 Depth = 1, bool Compact = true,
		bool StopAtRoot = false, int32 PageSize = 100, const FString& Cursor = TEXT("first"), bool IncludeSeeds = true);

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
	 * Read Blueprint comment rectangles, membership and nesting. Membership uses node origins inside box rectangles, not full rendered bounds or a stored ownership relation; overlapping membership is reported. Loads the Blueprint; no explicit compile or save.
	 * @param Asset Required Blueprint path or unique short name, e.g. /Game/Tests/BP_NodeTest. Empty or non-Blueprint targets are refused.
	 * @param GraphName Existing graph name, e.g. EventGraph. Empty lists all collected graphs; unknown names are refused with available names.
     * @param Scope Optional empty for all boxes, node:<reader ID or unique hex key> for boxes containing that node's origin, box:<comment ID> for intersecting boxes, or region:x,y,width,height in graph units. Region dimensions must be finite and positive; edge-only contact is excluded. Nonempty scope requires GraphName. Relationships are still computed against all boxes.
     * @param Offset Optional zero-based matched-box offset, default 0. Use nextOffset from the response; -1 means complete. Pages are live, so restart after graph edits.
     * @param Limit Optional maximum whole box records per page, 1..80, default 80. The text budget can shorten a page; omitted counts remain explicit.
	 */
	UFUNCTION(meta = (AICallable))
	static FString ReadTurboMCPCommentList(
		const FString& Asset,
		const FString& GraphName, const FString& Scope = TEXT(""), int32 Offset = 0, int32 Limit = 80);

	/**
	 * Read DataTable schema, row names, selected values and registry-based readers. Displays up to 80 fields and pages of up to 60 sorted rows with revision-checked continuation. Oversized values are explicitly omitted; request fewer columns to inspect them. Loaded Blueprint input literals are inspected, including unsaved graphs. GetDataTableRow sites are distinguished from generic literal references; registry dependencies alone do not prove a read. Dynamic references and shared row-struct parameter flow are not resolved. Loads the table; no explicit edit or save.
	 * @param Asset Required DataTable path or unique short name, e.g. /Game/Tests/DT_Items. Empty or non-DataTable targets are refused.
	 * @param Include Case-insensitive all, readers, or schema. Empty defaults to all. schema includes row names and selected values; readers omits rows. Unknown filters are refused.
	 * @param Columns Empty or * selects all values; none selects row names only. Otherwise comma-separated reflected, authored or unique display names. Exact reflected names take precedence; ambiguous/unknown names are refused. Values require Include=all or schema.
	 * @param Offset Zero-based offset in sorted, filtered rows. Continuations require Revision.
	 * @param Limit Maximum rows per page, 1..60. Character limits can shorten a page; use next_offset (-1 means finished).
	 * @param RowFilter Case-insensitive row-name substring; empty matches all rows.
	 * @param Revision Token from the first page. A changed table or query is refused; restart at Offset=0.
	 * @param TextMode lossless (default) retains localized text identity; display emits the current localized string for inspection only, not write-back.
	 */
	UFUNCTION(meta = (AICallable))
	static FString ReadTurboMCPDataTable(
		const FString& Asset,
		const FString& Include,
		const FString& Columns = TEXT(""), const FString& TextMode = TEXT("lossless"),
		int32 Offset = 0, int32 Limit = 60, const FString& RowFilter = TEXT(""), const FString& Revision = TEXT(""));

	/**
	 * Read retained call-log records, newest first. Does not clear the log or change recording settings; this request may itself be recorded.
	 * Engine records may lack arguments or resolved names. Unknown sizes are not zero. Arguments and results are escaped single-line excerpts capped at 200 characters with an explicit truncation marker. Records are not the same as completed calls; use ReadTurboMCPCallCost for call totals.
	 * @param Filter Required string. Case-insensitive substring of toolset, tool, source, arguments or status (ok/error/incomplete). Empty matches all records; e.g. BP_NodeTest. This call can match its own recorded filter argument.
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
	 * Read named material reroute labels, declarations, source expressions and usage counts. Duplicate declaration names are refused with node identities. No explicit edit, compile or save.
	 * @param Asset Required Material or Material Function path or unique short name, e.g. /Game/Tests/M_TurboMCPFixture. Empty/unresolved assets are refused. Instances have no expression graph; read their parent or use ReadTurboMCPMaterialInstance.
	 */
	UFUNCTION(meta = (AICallable))
	static FString ReadTurboMCPMaterialLabels(const FString& Asset);

	/**
	 * Read material parameters and connected output trees, or the dedicated instance report. Loads the asset; no explicit edit, compile or save. TokenBudget is advisory, not a hard cap.
	 * @param Asset Required Material, Material Function or constant Material Instance path or unique short name. Empty/unresolved values are refused; e.g. /Game/Tests/M_TurboMCPFixture.
	 * @param bIncludeInstanceOverrides Required boolean. true permits constant Material Instances and reports their parent chain, overrides and inherited values; false refuses instances with parent guidance. Ignored for base materials/functions.
	 * @param TokenBudget Required integer advisory threshold in estimated tokens, e.g. 3000. Zero or negative disables the overrun warning. Output is not capped or reduced to this threshold; use Trace to narrow large graphs.
	 */
	UFUNCTION(meta = (AICallable))
	static FString ReadTurboMCPMaterialLoad(
		const FString& Asset,
		bool bIncludeInstanceOverrides,
		int32 TokenBudget);

	/**
	 * Read one connected material output tree and its label definitions; unused-expression findings consider all connected outputs. No explicit edit, compile or save. TokenBudget is advisory, not a hard cap.
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
	 * Read material logic grouped by comment boxes, with incoming and outgoing values. Membership uses node positions and the smallest containing box, not visual node bounds. No explicit edit, compile or save. TokenBudget is advisory, not a hard cap.
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
	 * @param Cap Required integer maximum rows in each contributor and unknown-size list. Zero or negative uses 10; positive values are used directly. Example: 20. Does not limit dependency scanning or category counts; omitted rows are reported.
	 */
	UFUNCTION(meta = (AICallable))
	static FString ReadTurboMCPAssetCost(
		const FString& Asset,
		int32 Cap);

	/**
     * Read supported widget or parent-slot properties without compiling or saving.
     * Supports editable scalar values, simple value structs, and exact hard object/class references. Dotted paths descend named struct fields only. Containers, delegates, localized text, ownership and instanced references are excluded. Reference values are exact paths or None when unset. This is not a full widget export.
     * @param Asset Required Widget Blueprint path or unique name; prefer /Game/UI/WBP_Menu.
     * @param Widget Required designer object name from ReadTurboMCPWidgetTree, not display text or a runtime instance path.
     * @param Target Required widget or slot. Slot means the named widget's parent slot; a root without a slot refuses.
     * @param Property Exact name or dotted struct-field path, such as Visibility, Brush.ResourceObject or EntryWidgetClass. Empty lists up to 160 supported paths, types and values. Navigation is read-only on Target=widget and reports stored Designer rules; it does not observe runtime focus.
     */
    UFUNCTION(meta = (AICallable))
    static FString ReadTurboMCPWidgetProperty(const FString& Asset, const FString& Widget, const FString& Target, const FString& Property);
    /**
     * Inspect or change explicit shared CommonButtonStyle Blueprint defaults.
     * @param Asset Required compiled CommonButtonStyle Blueprint path, not a widget. All buttons using this shared asset may change. No compile/save/runtime refresh is performed; recreate affected widgets after compiling to verify visuals.
     * @param Property Exact reflected path. Empty lists readable fields for read only. Examples: ButtonPadding or NormalHovered.TintColor.SpecifiedColor. Paths descend structs, never objects/containers. Edit conditions and color-use rules still apply. Native references and localized text are read-only.
     */
    UFUNCTION(meta = (AICallable))
    static FString ReadTurboMCPCommonButtonStyle(const FString& Asset, const FString& Property);

    /**
	 * Inspect selected Widget Blueprint hierarchy data; loads the asset without explicitly editing, compiling or saving it. This is not a complete or lossless export.
	 * Reports names, classes, paths, variable flags, non-default visibility and selected padding. Panel children retain slot order; this alone does not establish visual draw order. Common Button context adds the assigned style class and inherited hover-event names as inspection comments; style values and runtime overrides remain omitted. Other properties, bindings and animations are omitted. Do not assume write-back preserves omitted state.
	 * @param Asset Required Widget Blueprint path or unique short name, e.g. /Game/Tests/WBP_NodeTest. Empty, unresolved or non-widget assets are refused. A widget without a root returns an explicit no-root result.
	 */
	UFUNCTION(meta = (AICallable))
	static FString ReadTurboMCPWidgetTree(
		const FString& Asset);

    /**
     * Find Widget Blueprints by class, references, overrides and graph nodes. Uses a bounded page of sorted registry candidates and returns JSON evidence.
     * Searches the owned designer tree, including embedded widget instances and local named-slot content, and owned Blueprint graphs. Embedded internals, inherited trees, native implementations, maps, sets and dynamic references are outside scope. Values compare against existing class defaults. Loading an unloaded candidate may run PostLoad or dependency compilation and dirty packages. No explicit compilation, save, checkout, map opening or PIE.
     * @param Rules JSON object with at least one exact string rule. class accepts a Widget or RichTextBlockDecorator class path and matches subclasses. asset accepts a full object path. assetFolder matches a content folder recursively. override and call accept function names. nodeClass accepts an EdGraphNode class path and matches subclasses.
     * @param Folder Mounted content folder without trailing slash, e.g. /Game/UI. Default "project" searches /Game and enabled project plugin content recursively.
     * @param Recursive Include candidate subfolders when Folder is explicit. Default true.
     * @param MatchMode all requires every rule somewhere in the same Blueprint. any requires at least one rule. Default all.
     * @param PageSize Maximum Widget Blueprint candidates to inspect per call, 1 to 16. Default 8.
     * @param Cursor Use first initially. Copy nextCursor with the same filters to continue. An empty nextCursor ends the scan. Totals remain unknown until all candidates are inspected.
     * @param LoadUnloaded Allow bounded candidate package loads. Set false for loaded candidates only, with unloaded candidates reported as skipped. Default true.
     */
    UFUNCTION(meta = (AICallable))
    static FString ReadTurboMCPWidgetInventory(const FString& Rules, const FString& Folder = TEXT("project"),
        bool Recursive = true, const FString& MatchMode = TEXT("all"), int32 PageSize = 8,
        const FString& Cursor = TEXT("first"), bool LoadUnloaded = true);

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
	 * reads the live Slate window list without capturing images or changing assets.
	 *
	 * Returns the total visible count and up to six titles. An unavailable Slate
	 * application is reported separately from zero windows. No image is captured.
	 */
	UFUNCTION(meta = (AICallable))
	static FString LookTurboMCPToolset();

	/**
	 * Inspect graph geometry and layout findings; loads the Blueprint and measures Slate geometry without requesting graph edits, compile or save.
	 * Requires editor rendering. Wire lengths use estimated pin positions, not rendered wire curves. Each finding is an intended control-flow handoff with rule R3/R4/R6, or a defect candidate needing visual review. An intended edge does not approve the whole layout. Cross-box wires remain candidates because a valid handoff may still cross unrelated boxes. Output is paged and some source lists are capped.
	 * @param Asset Required Blueprint path or unique short name, e.g. /Game/Tests/BP_NodeTest. Empty or unresolved input is refused.
	 * @param GraphName Required exact graph name, e.g. EventGraph. Empty or unresolved names are refused; use the Blueprint reader to list graphs.
	 * @param AlignmentTolerance Optional finite vertical pin-offset tolerance in graph units, 0..1024; default 64. Use 8 for stricter alignment review. Does not suppress overlaps or backward-wire findings. Values outside the range are refused.
     * @param Scope Required string: pass an empty string for the whole graph, or node:<reader ID or unique hex key>, box:<comment ID>, or region:x,y,width,height in graph units. The tool schema requires this field even for the whole graph. Box/region scopes include intersecting node rectangles, excluding edge-only contact. A finding is included if any involved node is selected, including links to outside nodes. Measurement still covers the graph.
     * @param Offset Optional zero-based matched-finding offset, default 0. Use nextOffset from the response; -1 means complete. Pages are live; restart at 0 after edits.
     * @param Limit Optional maximum whole findings per page, 1..120, default 120. Defect candidates appear before intended handoffs; overlaps lead the defect candidates. Text-budget limits never split a finding. Use targeted capture separately for visual review.
	 */
	UFUNCTION(meta = (AICallable))
	static FString LookTurboMCPLayoutReport(const FString& Asset, const FString& GraphName, double AlignmentTolerance = 64.0, const FString& Scope = TEXT(""), int32 Offset = 0, int32 Limit = 120);

	/**
	 * Return layout findings and capture the same graph scope; may open editor UI and writes an image file, but does not request asset edits, compile or save.
	 * Requires rendering. This is a human review aid, not an automatic done/pass verdict. A capture failure remains visible alongside any successful text report. Native zoom can still refuse a large frame; select a node, box or smaller region instead.
	 * @param Asset Required Blueprint path or unique short name, e.g. /Game/Tests/BP_NodeTest. Empty or unresolved input is refused.
	 * @param GraphName Required exact graph name, e.g. EventGraph. The same graph is measured and captured; empty or unresolved names are refused.
	 * @param AlignmentTolerance Optional finite vertical pin-offset tolerance in graph units, 0..1024; default 64. Use 8 for stricter review. Affects alignment findings, not capture framing, overlaps or backward-wire findings.
	 * @param Scope Required string in the tool schema: pass an empty string for the whole graph, node:<reader ID or unique hex key>, box:<comment ID>, or region:x,y,width,height. A box captures its current rectangle; a node captures the resolved node. Invalid or ambiguous scopes refuse before capture. Report findings remain paged by LookTurboMCPLayoutReport defaults; use that tool for further pages.
	 */
	UFUNCTION(meta = (AICallable))
	static FString LookTurboMCPLayoutDoneCheck(const FString& Asset,const FString& GraphName,double AlignmentTolerance = 64.0,const FString& Scope = TEXT(""));

	/**
	 * Read node IDs, stored positions and measured sizes; loads the Blueprint and uses rendering without requesting graph edits, compile or save.
     * Positions are stored top-left coordinates; sizes are rendered node bodies in graph units, not screen pixels or wire bounds.
     * Rendering measures the graph; NodeIds filters returned rows. Output reports selected, listed, omitted and excluded counts.
     * @param NodeIds Optional newline-separated exact reader IDs or unique hexadecimal keys of at least six digits, maximum 256 entries. Empty selects all nodes. Missing, ambiguous or blank entries refuse the entire call. Duplicates are listed once.
     * @param MaxNodes Optional output row limit, 1..256, default 128. Rows are sorted by ID and also bounded by the text budget. Continue with the returned next_offset until complete=true.
     * @param Offset Optional zero-based row offset in the ID-sorted selection, default 0. Must be 0..selected count. Use the same selection across pages and restart at 0 after graph changes; offsets are not snapshot tokens.
	 * @param Asset Required Blueprint path or unique short name, e.g. /Game/Tests/BP_NodeTest. Empty or unresolved input is refused.
	 * @param GraphName Required exact graph name, e.g. EventGraph. Empty or unresolved names are refused; list graphs with ReadTurboMCPBlueprintLoad first.
	 */
	UFUNCTION(meta = (AICallable))
	static FString LookTurboMCPGraphPositions(const FString& Asset, const FString& GraphName, const FString& NodeIds = TEXT(""), int32 MaxNodes = 128, int32 Offset = 0);

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
     * @param FunctionName Optional zero-parameter widget function to execute after setting sample values and constructing the widget. Construction can overwrite samples; the callback runs afterwards. This does not simulate pointer or focus input. Empty skips the explicit callback; widget construction can still execute code.
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
	 * Capture a graph, asset editor, active window or live PIE game layer as a PNG.
	 * Start with Mode=graph for an overview. ReadTurboMCPNodeFind supplies node IDs;
	 * then use Mode=node, Size=1200x800 for detail or Mode=neighbours, Size=1 for connected context.
	 * A fitted overview may make text too small to read. Inspect the image and request a smaller target;
	 * resolution and successful capture do not prove readability or graph correctness.
	 * Requires a rendered editor. May load assets and change editor view, focus or selection.
	 * Does not explicitly compile or save assets; editor callbacks may run. No automatic undo.
	 * All five string arguments are required. Pass empty strings for unused arguments.
	 * @param Asset Asset path or unique name. Required for graph and asset-editor captures. Empty for viewport; optional for screen.
	 * @param GraphName Exact graph name for graph modes. Asset-editor optionally accepts an editor kind; empty auto-detects. Empty for viewport; ignored by screen.
	 * @param Mode Case-insensitive. Graph modes: graph (default), node, box, region, selection, neighbours, box-neighbours. Other modes: screen, asset-editor, viewport. Diagnostics: debug-window, rhi-state, select-nodes (changes selection). Viewport supports one in-process PIE world only; inspect after normal game ticks.
	 * @param Target Node ID for node/neighbours; zero-based comment index from LookTurboMCPLayoutReport for box/box-neighbours; x,y,width,height for region (finite, within +/-10000000, positive dimensions). Asset-editor: empty keeps the view, fit frames a Material graph. Viewport requires pie. select-nodes takes comma-separated node keys. Otherwise empty.
	 * @param Size Graph/node/box/region: WxH or W,H, each 1..4096 pixels; empty means 1600x900. Selection/neighbours/box-neighbours: integer hop count 0..4096, empty means 0; image dimensions are automatic, not set by this number. Screen/asset-editor ignore Size. Viewport requires empty Size and uses native pixels up to a 4096-pixel edge. Diagnostics accept dimensions except rhi-state, which ignores Size.
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
    /** Check one saved asset for deletion without deleting it; loading and garbage collection may run. Refuses referenced, dirty, open, read-only or unsafe source-control targets, maps, redirectors and multi-asset packages. Never force-deletes or removes imported source files.
     * @param AssetPath Exact saved /Game asset object path, for example /Game/Tests/Unused.Unused. No short names, folders or package-only paths.
     */
    UFUNCTION(meta = (AICallable))
    static FString PreviewTurboMCPAssetDelete(const FString& AssetPath);


public:
    /** Validate a new Blueprint function library at an unused destination; no disk save or overwrite. Never copies another asset. Preview creates nothing; edit uses the stock factory, which initializes the Blueprint class. No disk save or reliable creation-undo guarantee. Refuses PIE and active transactions.
     * @param PackagePath Exact new /Game package path, for example /Game/Utilities/BFL_Purchase. Do not pass an object suffix, filename, short name or existing package. Created assets remain unsaved; save separately after inspection.
     */
    UFUNCTION(meta = (AICallable))
    static FString PreviewTurboMCPFunctionLibraryCreate(const FString& PackagePath);

    /** Create a fresh typed Data Asset, preserving class defaults. Preview validates without allocation. Edit creates in memory; saving and source-control add are explicit. Constructors may execute project code. Never overwrites an existing package or compiles Blueprints.
     * @param ClassPath Exact already loaded concrete UDataAsset class path; Blueprint-generated paths end in _C. Unloaded, abstract, transient and superseded classes are refused.
     * @param PackagePath New /Game package path without object suffix or extension, for example /Game/Data/DA_Menu.
     * @param InitialFieldsJSON JSON object of editable field names to Unreal property-text strings. Empty means class defaults. Scalar/value-struct values and loaded external references are supported; containers and owned children use subsequent property edits.
     * @param Save Explicit disk save after creation. Default false. A saved file is not removed by Undo.
     * @param AddToSourceControl Add the saved file through the current provider; requires Save=true. Default false. Failure preserves the asset and reports partial completion. Never checks out or submits.
     */
    UFUNCTION(meta = (AICallable))
    static FString PreviewTurboMCPDataAssetCreate(const FString& ClassPath, const FString& PackagePath, const FString& InitialFieldsJSON = TEXT(""), bool Save = false, bool AddToSourceControl = false);



public:
    /** Validate a scoped PIE runtime call without executing it. Reference-bearing parameters, RPCs and latent functions are refused. No save or undo guarantee.
     * @param WorldPath Exact current PIE world object path.
     * @param ObjectPath Exact existing runtime object path in that world; local-player subsystems are supported.
     * @param FunctionName Exact Blueprint-callable function name. Inspect ReadTurboMCPPlayFunction first.
     * @param Arguments JSON object with every input parameter by exact name; use {} for no inputs. Use JSON numbers for numbers (integers must be exact and within +/-9007199254740991), booleans for bools, and strings for text/name/enum names. Arrays and sets use JSON arrays; reflected structs and maps use JSON objects. Map keys must be text/name/enum values. Object references, custom-imported structs and null values are unsupported. Supply all inputs, including defaults; omit output-only and return parameters. Example: {"Amount":3,"Label":"debug event"}. Unknown, missing or unsupported arguments refuse before dispatch.
     */
    UFUNCTION(meta = (AICallable))
    static FString PreviewTurboMCPPlayInvoke(const FString& WorldPath,const FString& ObjectPath,const FString& FunctionName,const FString& Arguments);


    /** Validate creation of a standalone typed custom event in an existing event graph. Local, non-replicated; not an override or delegate binding. No compile or save. Duplicate names refuse. Loading assets can run engine load hooks.
     * @param Asset Blueprint object path or unique asset name.
     * @param GraphName Exact existing event graph name. Function and macro graphs refuse.
     * @param Name New event identifier, at most 128 characters; must not collide with existing or inherited callables.
     * @param Inputs Empty for no inputs, otherwise comma-separated Name:type pairs, for example Amount:float, Label:string. Container type commas stay inside angle brackets. At most 32 unique data parameters; no execution, delegate or wildcard types. Event parameters become output pins for subsequent append links.
     */
    UFUNCTION(meta = (AICallable))
    static FString PreviewTurboMCPCustomEventAdd(const FString& Asset, const FString& GraphName, const FString& Name, const FString& Inputs);


    /** Preview a diagnostic query or process-wide change without executing it; obtain a single-use confirmation for changes. Log, Slate debugger and CVar effects are process-wide, even with a PIE target. No shell, file operations or arbitrary console commands. Dispatch is not proof of asynchronous effects; use ReadTurboMCPEditorLog.
     * @param Target Exact editor or pie:<instance number>. Missing or ambiguous worlds refuse.
     * @param Command One command: Log list [category], Log <exact category> <verbosity>, SlateDebugger.Event.Start/Stop, or an existing CVar name with an optional single value. No chaining or newlines. Preview reads current state without changing it.
     */
    UFUNCTION(meta = (AICallable))
    static FString PreviewTurboMCPDiagnosticConsole(const FString& Target, const FString& Command);


    /** Preview one data-asset property using temporary validation. Requires the current reader revision. No implicit save or compile. Preview does not modify live properties or run edit notifications. create_child runs constructors on temporary objects; project constructors can have external effects.
     * @param Asset Exact asset path or uniquely resolved asset name; must resolve to UDataAsset.
     * @param Property Exact reflected property path from ReadTurboMCPDataAssetProperty. External objects are never implicit edit targets.
     * @param Operation set, array_remove, array_insert, map_upsert, map_remove, create_child or child_set. array_remove removes one existing array element and preserves all remaining values and their order; array_insert inserts before Key (0..count; count appends), preserving existing order. Whole-array set is unsupported. child_set edits an existing owned map child selected by Key. create_child creates a compatible instanced object in an empty property/map entry; it never replaces an existing child. Set changes one scalar/value-struct or existing object reference; map operations change one entry, preserving unrelated entries.
     * @param Key For array_remove/array_insert, an exact zero-based decimal index (e.g. 64 removes the 65th element). Otherwise, exact loaded object path for object keys or Unreal property text. Required for map operations and map-child creation; empty for set or direct child creation.
     * @param Value Unreal property text for value fields, or exact loaded object path for references. Empty for map_remove/array_remove. For array_insert, empty inserts a default element; explicit values support scalar/value structs or loaded compatible hard references, not reference-containing struct text. Use quoted empty string for a string element. Example integer: 42. Object example: /Game/Input/IA_Jump.IA_Jump. For child_set, use JSON: {"property":"Number","value":"42"}. For create_child, use JSON: {"class":"/Script/EnhancedInput.PlayerMappableKeySettings","fields":{"Name":"Jump"}}. Initial fields are scalar/value-struct property-text strings. Other operations do not accept stock engine JSON.
     * @param Revision Exact revision returned by the reader or preview; stale revisions refuse without writing. Read again after any change.
     */
    UFUNCTION(meta = (AICallable))
    static FString PreviewTurboMCPDataAssetProperty(const FString& Asset, const FString& Property, const FString& Operation, const FString& Key, const FString& Value, const FString& Revision);

    /**
     * Remove a directly implemented interface while keeping its function bodies as ordinary functions.
     *
     * Calls that require this interface may stop working. First preview, inspect affected callers, and pass the returned confirmation unchanged to the edit tool. Confirmation identifies the asset/interface; it is not a snapshot or stale-state lock.
     * Preview only: may load assets but does not edit them. An unchanged response means no work is needed.
     * Requires the editor game thread outside PIE and active transactions.
     *
     * Example: Preview the same Asset and Interface first; copy the complete confirmation value from its response into Confirm.
     *
     * @param Asset Required target Blueprint asset path, such as /Game/UI/WBP_Target.WBP_Target, or an unambiguous asset name. Prefer the full path.
     * @param Interface Required exact directly implemented interface class path, asset object path, or unique name. Inherited interfaces cannot be removed.
     *
     * @return A preview/edited/unchanged summary or an error with the reason. Structural refresh is not proof of successful compilation. Read back and compile before saving.
     */
    UFUNCTION(meta = (AICallable))
    static FString PreviewTurboMCPInterfaceRemove(const FString& Asset, const FString& Interface);

    /**
     * Declare an event dispatcher with named, typed input parameters.
     *
     * Any existing member with this name refuses, including an identical dispatcher. This creates the signature only; it does not bind listeners, create call nodes or invoke the dispatcher.
     * Preview only: may load assets but does not edit them. An unchanged response means no work is needed.
     * Requires the editor game thread outside PIE and active transactions.
     *
     * Example: Name=OnPurchase; Inputs=Accepted:bool,Buyer:object</Script/Engine.Actor>.
     *
     * @param Asset Required target Blueprint asset path, such as /Game/UI/WBP_Target.WBP_Target, or an unambiguous asset name. Prefer the full path.
     * @param Name Required unused member identifier, for example OnPurchase. Letters, digits and underscores only; start with a letter or underscore. None is reserved.
     * @param Inputs Required string: comma-separated Name:Type pairs. Use an empty string for no parameters. Example: Accepted:bool,Buyer:object</Script/Engine.Actor>. Parameter names must be distinct; nested container types use shared type syntax.
     *
     * @return A preview/edited/unchanged summary or an error with the reason. Structural refresh is not proof of successful compilation. Read back and compile before saving.
     */
    UFUNCTION(meta = (AICallable))
    static FString PreviewTurboMCPDispatcherAdd(const FString& Asset, const FString& Name, const FString& Inputs);
    /**
     * Rename an owned Blueprint function and discovered callers. Preview loads candidates; edit repeats discovery using the same arguments. Requires a compiled owner and idle editor. Direct dependent maps and more than 4096 candidate packages refuse. No save or guaranteed rollback. Compile affected assets before saving; dynamic/native references are not verified.
     * Requires an idle editor outside play/transactions and a successfully compiled defining Blueprint. Direct dependent maps refuse; discovery refuses above 4096 candidate packages. Preview does not reserve a plan: edit discovers references again.
     * Example: Asset=/Game/UI/WBP_Menu, Function=RefreshState, NewName=RefreshLegacyState. Preview first, inspect affected assets, then edit with the same arguments.
     * Preview lists affected assets and call/delegate nodes. Dependencies are loaded; load callbacks may run. Edit repeats discovery, renames child overrides through stock editor APIs and updates references. No save or guaranteed rollback; compile affected assets before saving. Inherited/interface functions and name conflicts refuse. Dynamic strings, native code and unavailable content are not verified.
     * @param Asset Required defining Blueprint path or unique name. Example: /Game/UI/WBP_Menu with Function=RefreshState and NewName=RefreshLegacyState. Preview first and inspect affected assets. Requires a compiled owner and idle editor. Discovery inspects direct referencers of the owner and derived classes; dependent maps and more than 4096 direct candidate packages refuse.
     * @param Function Required exact owned function name; inherited overrides are not independently renamed.
     * @param NewName Required valid new member name, 1 to 100 characters. Same name is a no-op. Edit repeats discovery with the same arguments. No save or guaranteed rollback; compile affected assets before saving. Dynamic/native references are not verified.
     */
    UFUNCTION(meta = (AICallable))
    static FString PreviewTurboMCPFunctionRename(const FString& Asset, const FString& Function, const FString& NewName);
    /**
     * Inspect or change owned function purity and receiver constness. Preview first; edit repeats preflight. Requires an idle editor and compiled owner. Inspects direct referencers of the owner and derived classes, with a 4096-package limit. Refuses unsafe connected callers, overrides and delegate references. Candidate assets are loaded and callbacks may run. No automatic save; compile affected assets before saving. A failed postcondition explicitly reports partial changes.
     * @param Asset Required Blueprint path or unique asset name.
     * @param Function Required exact owned function name. Use FunctionAdd first to create a new helper.
     * @param Pure Required keep, true or false. Removing connected execution pins or introducing an unexecuted data producer refuses.
     * @param Const Required keep, true or false. Const constrains this receiver, not all external objects. Unsupported macro-body analysis refuses.
     */
    UFUNCTION(meta = (AICallable))
    static FString PreviewTurboMCPFunctionFlags(const FString& Asset, const FString& Function, const FString& Pure, const FString& Const);

    /**
     * Prepare missing member declarations before copying graph bodies between Blueprints.
     *
     * Creates declared variables with defaults, function signatures, dispatcher signatures and custom-event declarations on a new named event graph. Exact-name exclusions skip creation without deleting existing target members. GraphTransfer can replace matching empty event declarations with their bodies; connected or commented target events refuse. Compatible existing members are retained; differing variable types or callable signatures refuse. Compile both assets first. Does not copy widget trees, interfaces, function bodies, animations or bindings. Add required interfaces and copy widget subtrees separately, then use GraphTransfer. A failed edit can leave earlier declarations created; inspect the target before retrying.
     * Preview only: may load assets but does not edit them. An unchanged response means no work is needed.
     * Requires the editor game thread outside PIE and active transactions.
     *
     * Example: SourceAsset=/Game/UI/WBP_Source.WBP_Source; TargetAsset=/Game/UI/WBP_Target.WBP_Target.
     *
     * @param SourceAsset Required source Blueprint asset path or unambiguous name. Prefer its full object path; the source is not edited.
     * @param TargetAsset Required different target Blueprint asset path or unambiguous name. Its parent class must equal or derive from the source parent class.
     *
     * @param Exclude Optional comma- or pipe-separated exact source member names to skip, case-insensitive. Empty includes all. Unknown names refuse before changes. Skipped dependencies may need separate preparation before transferring bodies.
     * @return A preview/edited/unchanged summary or an error with the reason. Structural refresh is not proof of successful compilation. Read back and compile before saving.
     */
    UFUNCTION(meta = (AICallable))
    static FString PreviewTurboMCPMergeMembers(const FString& SourceAsset, const FString& TargetAsset, const FString& Exclude = TEXT(""));
    /**
     * Copy one complete owned graph, including its nodes, connections, layout, comments and locals.
     *
     * Compile both assets and prepare referenced members/widgets first. Existing executable logic is never overwritten. Source-owned macro dependencies must already exist in the target; unresolved calls, missing self members and conflicting custom events refuse. Matching empty custom-event declarations can be replaced; connected, commented or disabled declarations are protected. This does not perform a whole-asset merge or automatically tidy the copied layout.
     * Preview only: may load assets but does not edit them. An unchanged response means no work is needed.
     * Requires the editor game thread outside PIE and active transactions.
     *
     * Example: SourceGraph=CalculatePurchase; TargetGraph=""; use exact source/target asset paths.
     *
     * @param SourceAsset Required source Blueprint asset path or unambiguous name. Prefer its full object path; the source is not edited.
     * @param SourceGraph Required exact source function, interface-function, macro or event graph name. This selects the whole graph, not node IDs or an event label.
     * @param TargetAsset Required different target Blueprint asset path or unambiguous name. Its parent class must equal or derive from the source parent class.
     * @param TargetGraph Pass an empty string to keep SourceGraph, or a valid destination identifier. Existing event pages receive the whole source group below existing nodes, preserving internal links and relative positions; unmeasured nodes use a conservative 512-unit height. This does not splice execution chains or tidy. Nested source graphs refuse append. Existing macro graphs refuse. An existing function must contain only entry/result nodes, no locals, and have a compiled compatible signature.
     *
     * @return A preview/edited/unchanged summary or an error with the reason. Structural refresh is not proof of successful compilation. Read back and compile before saving.
     */
    UFUNCTION(meta = (AICallable))
    static FString PreviewTurboMCPGraphTransfer(const FString& SourceAsset, const FString& SourceGraph, const FString& TargetAsset, const FString& TargetGraph);
    /**
     * Validate and preview one widget or parent-slot property; returns before/after values without changing the widget. Exact hard reference validation can load the named asset and run native load hooks. Compile/save and rendered/runtime verification are separate.
     * @param Asset Required Widget Blueprint path or unique name; prefer /Game/UI/WBP_Menu.
     * @param Widget Required designer object name from ReadTurboMCPWidgetTree, not display text or a runtime instance path.
     * @param Target Required widget or slot. Slot means the named widget's parent slot; a root without a slot refuses.
     * @param Property Exact name or dotted struct-field path returned by ReadTurboMCPWidgetProperty, for example Visibility, LayoutData, Brush.ResourceObject or EntryWidgetClass. Paths cannot descend objects or containers.
     * @param Value Required Unreal property text for values (for example Hidden), or None/exact Unreal object or generated-class path for a hard reference. The referenced asset must exist and satisfy the field's type and metadata constraints. Other struct fields are preserved. Maximum 65536 characters. Requires an idle editor outside PIE.
     */
    UFUNCTION(meta = (AICallable))
    static FString PreviewTurboMCPWidgetProperty(const FString& Asset, const FString& Widget, const FString& Target, const FString& Property, const FString& Value);
    /**
     * Inspect or change explicit shared CommonButtonStyle Blueprint defaults.
     * @param Asset Required compiled CommonButtonStyle Blueprint path, not a widget. All buttons using this shared asset may change. No compile/save/runtime refresh is performed; recreate affected widgets after compiling to verify visuals.
     * @param Property Exact reflected path. Empty lists readable fields for read only. Examples: ButtonPadding or NormalHovered.TintColor.SpecifiedColor. Paths descend structs, never objects/containers. Edit conditions and color-use rules still apply. Native references and localized text are read-only.
     * @param Value Required Unreal property text, not JSON; up to 65536 characters. Example padding: (Left=8,Top=4,Right=8,Bottom=4). Example color: (R=0.2,G=0.5,B=0.8,A=1). Partial structs retain other fields. Preview first; edit reparses current state. Invalid/non-finite values refuse. Requires an idle editor outside PIE.
     */
    UFUNCTION(meta = (AICallable))
    static FString PreviewTurboMCPCommonButtonStyle(const FString& Asset, const FString& Property, const FString& Value);

    /**
     * Preview adding a designer widget; loads classes but constructs no widget and makes no explicit edit.
     * Requires an idle editor outside PIE. Refuses missing/full parents, name conflicts and circular widget dependencies. Native construction/palette hooks and structural refresh may run on edit; loaded dependent Blueprints may be affected. No full compile or save is requested. Read back, configure layout separately and compile before saving.
     * @param Asset Required destination Widget Blueprint path or unique name; prefer /Game/UI/WBP_Menu.
     * @param Parent Required existing designer panel name from ReadTurboMCPWidgetTree, such as CardList. Empty does not select or create a root. Named-slot insertion is not supported.
     * @param WidgetClass Required compiled Widget Blueprint path/name, such as /Game/UI/WBP_Card, or concrete native class path such as /Script/UMG.TextBlock. Dirty source Blueprints must be compiled first.
     * @param Name Required unused identifier: 1 to 100 letters, digits or underscores, starting with a letter or underscore. Example: NewCard. Duplicate names refuse rather than adding a numbered copy.
     */
    UFUNCTION(meta = (AICallable))
    static FString PreviewTurboMCPWidgetAdd(const FString& Asset, const FString& Parent, const FString& WidgetClass, const FString& Name);

    /**
     * Preview removal; no mutation is performed.
     * Refuses graph references, designer bindings, animation bindings and explicit navigation targets in this Blueprint. External assets, dynamic string lookups and custom reference systems are not verified. Removing the root leaves an empty tree. Edit checks current state again; loaded dependent Blueprints may be refreshed. Compile and inspect before saving.
     * @param Asset Required Widget Blueprint path or unique name; prefer /Game/UI/WBP_Menu. Requires an idle editor outside PIE and transactions.
     * @param Widget Required exact designer object name from ReadTurboMCPWidgetTree. Includes its descendants in this tree, not the internals of embedded UserWidgets. Missing names refuse. Example: ObsoleteCard. Preview lists every affected name.
     */
    UFUNCTION(meta = (AICallable))
    static FString PreviewTurboMCPWidgetRemove(const FString& Asset, const FString& Widget);

    /**
     * Copy one widget and its descendants into another Widget Blueprint.
     *
     * Widget names must be unused unless AutoRename is enabled. Copies widget properties, internal slots and the root slot layout; preserves direct/struct/array text identities. Designer bindings refuse. Graph logic and animations are not copied. Detected references outside the selected subtree refuse; prefer copying a common ancestor. Complex custom widget references and text map/set properties are not fully validated. Read the target tree and compile before saving.
     * Preview only: may load assets but does not edit them. An unchanged response means no work is needed.
     * Requires the editor game thread outside PIE and active transactions.
     *
     * Example: SourceWidget=PurchasePanel; Parent=RootCanvas; use exact source/target Widget Blueprint paths.
     *
     * @param SourceAsset Required source Blueprint asset path or unambiguous name. Prefer its full object path; the source is not edited.
     * @param SourceWidget Required exact designer object name, such as PurchasePanel. Use the widget-tree reader; this is not displayed text or a runtime PIE path.
     * @param TargetAsset Required different target Blueprint asset path or unambiguous name. Its parent class must equal or derive from the source parent class.
     * @param Parent Required string naming an existing target panel, such as RootCanvas. Pass an empty string only when the target widget tree is empty. A panel must have room; its class must match the source parent class when preserving the root slot.
     * @param AutoRename Optional, default false. Rename colliding copied widgets with _Copy, _Copy2, etc.; preview/edit list every mapping. The source and existing target widgets are untouched. Explicit navigation inside the subtree is remapped; outside navigation refuses. Graphs, animations and custom string references are not migrated. Edit recalculates names, so inspect its returned mapping.
     *
     * @return A preview/edited/unchanged summary or an error with the reason. Structural refresh is not proof of successful compilation. Read back and compile before saving.
     */
    UFUNCTION(meta = (AICallable))
    static FString PreviewTurboMCPWidgetSubtreeCopy(const FString& SourceAsset, const FString& SourceWidget, const FString& TargetAsset, const FString& Parent, bool AutoRename = false);

    /**
     * Add a Blueprint interface contract and its required function declarations.
     *
     * An already implemented or inherited interface is unchanged. Existing graph-name conflicts and animation-layer interfaces refuse. This does not copy source event bodies or function logic.
     * Preview only: may load assets but does not edit them. An unchanged response means no work is needed.
     * Requires the editor game thread outside PIE and active transactions.
     *
     * Example: Asset=/Game/UI/WBP_Target.WBP_Target; Interface=/Game/Interfaces/BPI_Menu.BPI_Menu.
     *
     * @param Asset Required target Blueprint asset path, such as /Game/UI/WBP_Target.WBP_Target, or an unambiguous asset name. Prefer the full path.
     * @param Interface Required compiled Blueprint interface asset path/name, such as /Game/Interfaces/BPI_Menu.BPI_Menu, or a Blueprint-implementable native /Script/Module.Interface class path.
     *
     * @return A preview/edited/unchanged summary or an error with the reason. Structural refresh is not proof of successful compilation. Read back and compile before saving.
     */
    UFUNCTION(meta = (AICallable))
    static FString PreviewTurboMCPInterfaceAdd(const FString& Asset, const FString& Interface);

    /**
     * Declare a typed local variable in one function; do not create getter/setter nodes.
     *
     * An existing local with the same type and default is unchanged; a conflicting declaration refuses. This declares storage only, not function behavior.
     * Preview only: may load assets but does not edit them. An unchanged response means no work is needed.
     * Requires the editor game thread outside PIE and active transactions.
     *
     * Example: Function=CalculatePurchase; Name=ItemCount; Type=int; Default=7.
     *
     * @param Asset Required target Blueprint asset path, such as /Game/UI/WBP_Target.WBP_Target, or an unambiguous asset name. Prefer the full path.
     * @param Function Required owned function graph name, for example CalculatePurchase, including implemented interface functions. Matching is case-insensitive and must be unique. Event graphs and inherited functions are not accepted.
     * @param Name Required identifier starting with a letter or underscore and containing only letters, digits or underscores. Must not conflict with parameters or members; None is reserved.
     * @param Type Required shared Blueprint type syntax, for example int, float, vector, or object</Script/Engine.Actor>. For structs, pass the plain object path, for example /Script/CoreUObject.Vector or /Game/Types/ST_Item.ST_Item; do not add a struct: prefix. The struct must support Blueprint variables.
     * @param Default Pass an empty string for the Unreal type default. Otherwise provide a schema-valid scalar/struct literal, for example 7 for int. Explicit object/class/container defaults refuse; assign them in graph logic.
     *
     * @return A preview/edited/unchanged summary or an error with the reason. Structural refresh is not proof of successful compilation. Read back and compile before saving.
     */
    UFUNCTION(meta = (AICallable))
    static FString PreviewTurboMCPLocalVariableAdd(const FString& Asset, const FString& Function, const FString& Name, const FString& Type, const FString& Default);

    /**
     * Report registered schema sizes without invoking tools or changing assets.
     * Returns the total schema characters across all registered toolsets, then
     * the size of each registered TurboMCP toolset or its missing-registration notice.
     * Whole-toolset sizes are not compared with the 2000-character compact-help
     * budget. This is metadata inspection, not an edit preview or image capture.
     */
	UFUNCTION(meta = (AICallable))
	static FString PreviewTurboMCPToolset();

	/** Inspect a proposed new enum and check for an existing asset; does not create, compile or save an asset.
     * Existing assets are not overwritten. Preview does not guarantee creation or that the engine will accept every display name; edit reads names, numeric values and requested display names back. Failed creation may leave an unsaved asset for inspection.
     * @param Path Required new package path including the asset name, for example /Game/Enums/E_Colour; do not supply an object suffix or filesystem extension. Surrounding whitespace is trimmed; empty and relative paths are refused. Use a writable content mount.
     * @param Values Required comma- or pipe-separated entries in Name or Name=Display Name format, for example Red,Green=Dark Green,Blue. Names start with a letter or underscore and contain only letters, digits or underscores; duplicates ignoring case and _MAX are refused. Values receive consecutive integers starting at zero. Empty input is refused; empty separators are skipped. Empty display text uses the engine-generated display name. Commas and pipes cannot be escaped inside display text.
     */
	UFUNCTION(meta = (AICallable))
	static FString PreviewTurboMCPEnumAdd(const FString& Path, const FString& Values);

	/**
	 * Parse a variable creation request; may load referenced types but does not add variables, compile the target or save.
	 * Preview does not resolve the target Blueprint or check existing names. Edit performs those checks; preview acceptance does not guarantee edit success. Defaults are validated as Unreal property text before adding any variables; compile separately before saving.
	 * @param Asset Required Blueprint name or package/object path, e.g. /Game/Tests/BP_NodeTest; empty is refused. Preview only retains this identifier and does not check existing names; edit resolves it and checks conflicts.
	 * @param Variables Required comma-separated Name: Type declarations, e.g. Health: float default=100 category=Stats. Optional space-separated key=value fields: default (alias value), category, tooltip, private, readonly, instanceEditable, exposeOnSpawn. Flags default false and accept true/false, 1/0, yes/no, on/off. Other fields default empty; empty list is refused. Values cannot contain ungrouped spaces or commas. Types include basic scalars, named engine structs, enum/struct assets, object/class/softobject/softclass<ClassPath>, Array<T>, Set<T>, Map<K,V>; map values must be basic scalar types. Duplicate names/options are refused. Defaults are validated as Unreal property text before adding any variables; compile separately before saving.
	 */
	UFUNCTION(meta = (AICallable))
	static FString PreviewTurboMCPVarsAdd(const FString& Asset, const FString& Variables);
    /** Remove one unused Blueprint-owned member variable after reference inspection; never deletes graph nodes intentionally. Inspection loads dependent Blueprints and may run callbacks. Refuses referenced/inherited/generated variables and incomplete inspection. Dynamic/native/string references are outside proof. No save; structural refresh can compile dependencies and invalidate undo.
     * @param Asset Exact Blueprint asset path or unique name; prefer the full path.
     * @param Name Exact case-sensitive owned member name, not a local or widget-generated variable.
     */
    UFUNCTION(meta = (AICallable))
    static FString PreviewTurboMCPVariableRemove(const FString& Asset, const FString& Name);

    /** Preview an owned member-variable rename and list affected Blueprint assets and getter/setter references. Preserves identity, effective default and replication association. May load dependent Blueprints, but never maps; preview does not edit or compile. Incomplete discovery, child-name collisions, unsupported bindings/reference nodes and child default overrides refuse. Dynamic/native references are outside inspection. No save or checkout.
     * @param Asset Blueprint asset path or unique name; prefer a full object path.
     * @param Name Exact case-sensitive owned member name. Inherited, local, component and widget-generated variables are not supported.
     * @param NewName New valid member identifier. Case-only renames are refused. Copy the returned confirmation token to EditTurboMCPVariableRename.
     */
    UFUNCTION(meta = (AICallable))
    static FString PreviewTurboMCPVariableRename(const FString& Asset, const FString& Name, const FString& NewName);

    /** Remove one unused local declaration from an exact owned function. Referenced locals refuse; no graph nodes are deleted. Preview loads the asset but does not edit it. Edit requires a fresh preview token. No save or checkout; structural refresh can compile and affect undo.
     * @param Asset Blueprint path or unique name; prefer the full path.
     * @param Function Exact owned function graph name. Event graphs and ambiguous names refuse.
     * @param Name Exact case-sensitive local variable name.
     */
    UFUNCTION(meta = (AICallable))
    static FString PreviewTurboMCPLocalVariableRemove(const FString& Asset, const FString& Function, const FString& Name);

    /** Rename one local variable and its exact local getter/setter references. Same-named members and other functions are unchanged. Preview lists affected nodes; edit requires fresh confirmation. No save or checkout. Structural refresh can compile and affect undo; inspect before saving.
     * @param Asset Blueprint asset path or unique name; prefer a full path.
     * @param Function Exact owned function graph name.
     * @param Name Exact current local declaration name.
     * @param NewName New identifier; collisions with function parameters or locals refuse.
     */
    UFUNCTION(meta = (AICallable))
    static FString PreviewTurboMCPLocalVariableRename(const FString& Asset, const FString& Function, const FString& Name, const FString& NewName);


	/**
	 * Preview an event or function override; loads the Blueprint and inspects editor metadata without creating the override, compiling or saving.
	 * Existing implementations are reported; edit will return unchanged without creating a duplicate. Preview is not a reservation; edit checks again.
	 * Edit creates only the override entry, not its behavior, and reports its exact node ID and graph. Connect the body, inspect parent-call requirements and tidy affected nodes before compiling.
	 * @param Asset Required Blueprint name or package/object path, for example /Game/Tests/BP_NodeTest; empty is refused.
	 * @param FunctionOrEvent Required reflected name from ReadTurboMCPOverrides, for example ReceiveActorOnClicked; case-insensitive with surrounding whitespace trimmed. Empty or unavailable names are refused; no default.
	 */
	UFUNCTION(meta = (AICallable))
	static FString PreviewTurboMCPOverrideAdd(
		const FString& Asset,
		const FString& FunctionOrEvent);

	/**
	 * Inspect widget bindings for missing widgets or delegates without validating function signatures, removing bindings, compiling or saving.
	 * Reports stale call nodes without editing them. Delegate presence does not validate the bound function or its signature.
	 * @param Asset Required Widget Blueprint path or unique name, e.g. /Game/Tests/WBP_NodeTest. Empty, unresolved and non-Widget Blueprint assets are refused.
	 */
	UFUNCTION(meta = (AICallable))
	static FString PreviewTurboMCPWidgetRetarget(const FString& Asset);

	/**
     * Plan comment changes; loads the Blueprint and may measure transient graph widgets, without applying edits, compiling or saving.
     * Deleting a box keeps its nodes. Explicit fit expands outward to preserve the old rectangle; fit without Nodes fits current contents. refit requires Box and Nodes and moves/resizes only the box around that explicit selection; unselected nodes remain in place and may be left outside. Inspect preview geometry before applying.
     * @param Asset Required Blueprint path or unique short name, e.g. /Game/Tests/BP_NodeTest. Empty or ambiguous input is refused.
     * @param GraphName Required existing graph name, e.g. EventGraph. Use the reader's name; missing targets are refused.
     * @param Action Required create, set, fit, refit or delete; case-insensitive and trimmed. Empty/unknown actions refuse.
     * @param Nodes Comma- or pipe-separated reader node IDs. Required for create; for fit selects nodes to enclose and requires Box. Empty fit uses current box contents. Unused for set/delete; pass empty.
     * @param Box Existing comment ID for set/delete. For fit, empty with empty Nodes selects all boxes; nonempty selects one box. Explicit fit with Nodes requires one Box. refit always requires one Box and explicit Nodes. Unused for create; pass empty.
     * @param Text Comment label for create/set; empty means no supplied label on create or leave unchanged on set, not clear. Whitespace is retained. Unused for fit/delete; pass empty. Example: Input handling.
     * @param Color Optional #RRGGBB or comma-separated RGB values 0..255, e.g. #3A6EA5. Empty uses the new box default or preserves an existing color. Used by create/set; otherwise pass empty.
     * @param Size Optional width x height in graph units, e.g. 400x200. Each side must be finite numeric text in 1..2147483647; fractional parts truncate. Empty create encloses selected nodes; empty set keeps size. Explicit create size overrides automatic enclosure. Automatic height includes wrapped-title clearance; new boxes have no floating comment bubble. Used by create/set; otherwise pass empty.
     */
    UFUNCTION(meta = (AICallable))
	static FString PreviewTurboMCPComment(
		const FString& Asset,
		const FString& GraphName,
		const FString& Action,
		const FString& Nodes,
		const FString& Box,
		const FString& Text = TEXT(""),
		const FString& Color = TEXT(""),
		const FString& Size = TEXT(""));

	/**
	 * Identify removal candidates using static graph reachability; loads the Blueprint without requesting edits, compilation or saving.
	 * Selection uses recognized entries and variable/call heuristics, not proof that nodes are unwanted or behaviorally irrelevant. Inspect the preview before editing; edit recomputes candidates.
	 * @param Asset Required Blueprint path or unique short name, e.g. /Game/Tests/BP_NodeTest. Empty or ambiguous values are refused; prefer a full path.
	 * @param GraphName Required event, function or macro graph name; case-insensitive and trimmed. Empty selects EventGraph or the first available graph. Example: EventGraph. Specify a name to avoid fallback selection.
	 */
	UFUNCTION(meta = (AICallable))
	static FString PreviewTurboMCPGraphDead(const FString& Asset, const FString& GraphName);

	/** Inspect repeated computed outputs, stable input-array counts and stock loop indexes in a Blueprint function; candidates and exclusions are advisory. Preview does not edit, compile or save. Edit supports stable scalar arithmetic at entry, explicitly selected immutable input-array counts at entry and scalar indexes at LoopBody after dominance, Completed, reference and lifetime checks; a clean compile baseline is required.
     * Pure-node classification is not proof of deterministic behavior or equivalent execution.
     * @param Asset Required Blueprint asset path or unique asset name, for example /Game/Blueprints/BP_Player; surrounding whitespace is trimmed and empty is refused. Prefer a full path to avoid ambiguity.
     * @param GraphName Function graph name, matched without case sensitivity after trimming whitespace; event graphs are refused. Pass empty to select the first function graph in the Blueprint's stored order; the result names the selected graph. Prefer an explicit name.
     * @param Names JSON object mapping exact output IDs from the local-variable preview to meaningful local names, for example {"<exact output ID>":"Subtotal"}. A nonempty map selects only its exact output IDs. Default {} preserves automatic selection of reviewed entry-stable arithmetic; input-array counts and loop values always require explicit selection. Unknown IDs, invalid names and name collisions are refused. Use the same mapping for preview and edit.
     */
	UFUNCTION(meta = (AICallable))
	static FString PreviewTurboMCPLocalVars(const FString& Asset, const FString& GraphName, const FString& Names = TEXT("{}"));

	/**
	 * Search available node types; loads Blueprint context and queries editor node actions, without requesting graph edits, compilation or saving.
	 * Results are ranked and capped; PreviewTurboMCPNodePins inspects a returned type.
	 * Searches include member getter/setter terms, arithmetic words such as plus/minus, and reflected dispatcher names.
	 * Check the declaring class before binding an event; similarly named dispatchers can belong to different classes.
	 * Discovery does not prove that a target-object pin connection is compatible. Use the returned exact creation ID.
	 * In function graphs, Function|Result creates another Return using the existing function output signature.
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
	 * For Function|Result, the temporary probe mirrors the function's existing Return output pins; it leaves the live graph unchanged.
	 */
	UFUNCTION(meta = (AICallable))
	static FString PreviewTurboMCPNodePins(
		const FString& Asset,
		const FString& GraphName,
		const FString& TypeId);

	/**
	 * Plan reroutes for crossing wires in one explicit Blueprint graph; measures native geometry without changing the live graph; straight-segment estimates do not certify rendered curves; no compile or save.
	 * Tries bounded forward routes, shares a knot when all affected destinations fit, and skips existing reroutes, nearby nodes and blocked positions; not a full graph tidy.
	 * Edit replans against current state. Rollback does not verify full graph restoration; inspect partial results before saving.
	 * @param Asset Required Blueprint path or unique short name, e.g. /Game/Tests/BP_NodeTest. Empty or ambiguous input is refused; use a full path when possible.
	 * @param GraphName Exact existing Blueprint graph name, case-insensitive with surrounding whitespace ignored; empty, missing or ambiguous names are refused. Example: EventGraph.
	 */
	UFUNCTION(meta = (AICallable))
	
static FString PreviewTurboMCPGraphKnots(const FString& Asset, const FString& GraphName);

	/**
     * Plan a layout for one graph without committing graph changes. Measures native node geometry.
     * Within safety limits, structural quality takes priority over total wire length. Unsupported graphs or unavailable measured
     * geometry are refused rather than estimated. The edit recomputes its plan against current state.
     * Tidy may duplicate variable getters near their consumers; it does not create local variables.
     * Inspect the preview and resulting capture: numeric layout scores do not prove human readability.
     *
	 * NodeIds accepts "@unboxed": alone selects all unboxed nodes; with node IDs filters that selection to unboxed nodes. Boxed nodes and comments stay fixed.
	 * `NodeIds` scopes the pass to a block. An empty list tidies the whole graph; otherwise every
	 * unselected node is a fixed anchor and only the named nodes may move. The block is moved to clear
	 * space rather than through its anchors. The preview reports the planned score and the edit reports
	 * the score it actually measured after writing.
	 *
	 * WrapAfter optionally lists comma-separated execution node IDs at natural breaks (e.g. a latent call); their continuations start a new row. No links change.
	 * HorizontalGap/VerticalGap set full-detail clearance; LanePitch is minimum Sequence row spacing, expanded for tall pure trees. MainPath accepts "auto" for the longest acyclic execution path, or comma-separated connected reader node IDs for the top exec lane. Auto refuses unsupported execution cycles.
	 * Structural quality (including requested row breaks) takes priority; total wire length breaks ties.
	 *
	 * @param Asset Existing Blueprint path or unique short name, e.g. /Game/Tests/BP_NodeTest. Use a full path to disambiguate.
	 * @param GraphName Existing graph name from the reader, including interface implementation and nested graphs. Matching is case-insensitive after trimming. Empty selects a unique EventGraph, or the only graph; missing or ambiguous names are refused.
	 * @param bForce Required boolean. false keeps quality-based acceptance; true permits the proposed layout even when its score does not improve. Does not bypass scope, collision or fixed-boundary wire-growth limits.
	 * @param NodeIds Array of reader node IDs; [] selects the whole graph. Unselected nodes remain fixed. ["@unboxed"] selects unboxed nodes; combine it with IDs to restrict those IDs to unboxed nodes.
	 * @param MainPath Empty uses normal layout. auto chooses the longest acyclic execution path; otherwise comma-separated connected execution node IDs in desired order. Unsupported cycles are refused in auto mode. With NodeIds selection, only the first explicit path node may be outside the selection: it stays fixed and must connect to the next selected node. All later path nodes must be selected.
	 * @param HorizontalGap Finite horizontal clearance in graph units, 40..2000; default 120. Example: 160.
	 * @param VerticalGap Finite vertical clearance in graph units, 40..2000; default 100. Example: 120.
	 * @param LanePitch Finite minimum Sequence-row spacing in graph units, 100..10000; default 280. Rows expand when needed for tall trees.
	 * @param LayoutMode Optional rows (default, existing layout) or blocks (compact control-flow placement). Blocks keeps loop Body on its row and stacks continuations below; whole-graph blocks mode may move and resize existing comments around their original members. CreateBoxes optionally adds nested block comments; locals remain a separate explicit operation. Reroutes require explicit RouteWires=orthogonal.
	 * @param RouteWires off (default) preserves existing connections. orthogonal plans measured reroute chains for blocked or long diagonal connections (at most 512 nodes and 4096 logical wires); skipped boundary/unrouteable links are reported. No automatic compile or save.
	 * @param bCreateBoxes Optional false (default) or true. Requires whole-graph blocks mode and at most 512 nodes; plans nested comments for control-flow blocks, preserving existing titles and refusing partial overlaps. Added comments share the layout undo.
	 * @param ChannelWidth Finite routing channel clearance in graph units, 20..400; default 60. Used only when RouteWires=orthogonal.
	 * @param WrapAfter Empty means no requested row breaks. Otherwise comma-separated unique execution node IDs within scope, each with a continuation. Following execution starts a new row. Logical connections are preserved; explicit routing may insert reroute nodes.
	 * @param GeometryOffset Zero-based row offset for optional measured scoring evidence in blocks mode. Use 0 for the first page. Each call replans the current graph; do not combine pages across an intervening edit.
	 * @param GeometryLimit 0 (default) omits per-rectangle and per-wire evidence. Use 1..40 to request a bounded page in blocks mode; the result reports the total and next offset. Seven measured scores remain in the default result.
	 * @param PlanOffset Zero-based offset for optional planned node, reader, generated-box and route-knot coordinates. Each call replans the current graph; do not combine pages across an intervening edit.
	 * @param PlanLimit 0 (default) returns the usual preview. Use 1..20 to request a bounded coordinate page with total and next offset. Request plan and geometry pages separately; no edit, compile or save occurs.
	 * Node references accept exact reader IDs or unique hexadecimal keys of at least six digits in the named graph. Ambiguous keys and mismatched qualified IDs are refused.
     * Stock ForLoopWithBreak/ForEachLoopWithBreak Break and DoOnce Reset wires are preserved as feedback, not forward layout dependencies. Other cycles in the selected dependencies still refuse. Unselected nodes remain fixed; unrelated fixed cycles do not prevent scoped tidy.
     */
	UFUNCTION(meta = (AICallable))
	static FString PreviewTurboMCPGraphTidy(
		const FString& Asset,
		const FString& GraphName,
		bool bForce,
		const TArray<FString>& NodeIds,
		const FString& MainPath = TEXT(""), double HorizontalGap = 120.0, double VerticalGap = 100.0, double LanePitch = 280.0, const FString& WrapAfter = TEXT(""), const FString& LayoutMode = TEXT("rows"), const FString& RouteWires = TEXT("off"), double ChannelWidth = 60.0, bool bCreateBoxes = false, int32 GeometryOffset = 0, int32 GeometryLimit = 0, int32 PlanOffset = 0, int32 PlanLimit = 0);

	/**
     * Preview supported position repairs across one Blueprint graph; loads and measures transient graph widgets, but does not move live nodes, compile or save.
     * Repairs selected backward execution patterns, exclusive pure chains, misplaced getters and linked origin strays. This is not a complete tidy or proof of graph correctness. Nodes and links are retained; comment boxes are not moved or resized.
     * @param Asset Required Blueprint package/object path or unique short name, e.g. /Game/Tests/BP_NodeTest. Prefer a full path. Loading may run editor callbacks; an idle editor and measured node geometry are required.
     * @param GraphName Required unique graph name from the reader, e.g. EventGraph. Empty, missing or ambiguous names are refused. The whole named graph is considered; there is no node-selection parameter. Exclusive pure chains are limited to 8 nodes; a chain that cannot fit safely is not partially carried.
     * @param bForce Required boolean. false writes only when estimated straight-segment wire length decreases; true permits an equal or longer result and reports it. Force never bypasses identity, collision, comment or coordinate guards, and a no-op stays unchanged. Preview first; edit replans current state. No automatic compilation or saving.
     */
	UFUNCTION(meta = (AICallable))
	static FString PreviewTurboMCPGraphFixBackward(
		const FString& Asset,
		const FString& GraphName,
		bool bForce);

	/** Check graph-text syntax, node types, pin names and supported literal types; loads the Blueprint for inspection but does not write graph changes, compile or save.
     * A clean report means only that these checks found no issue; it does not guarantee writer acceptance, compilation or correct behavior. Unjudged findings need separate inspection. Use the intended edit tool's preview before writing.
     * @param Asset Required Blueprint name or package/object path, for example /Game/Blueprints/BP_Player; surrounding whitespace is trimmed. Empty, ambiguous or non-Blueprint targets are refused.
     * @param GraphName Required unique graph name from the reader, for example EventGraph; trimmed and matched without case sensitivity. Empty names are refused rather than choosing a graph automatically.
     * @param Text Required graph text in the reader's dialect, using exact node type IDs and pin names. Pass complete forms, not truncated output; empty text, unbalanced parentheses, unclosed quotes, reader-error markers and partial-view markers are refused. Semicolon comments run to the end of the line, so closing parentheses inside comments do not close forms. No default text is supplied.
     */
	UFUNCTION(meta = (AICallable))
	static FString PreviewTurboMCPGraphCheck(
		const FString& Asset,
		const FString& GraphName,
		const FString& Text);

	/**
	 * Preview adding nodes without editing; Placement=local searches nearby; Placement=outside uses space beyond existing bounds and may create long wires.
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
	 * Use writer node forms and explicit input links, checked by P-GRAPH-CHECK, so
	 * the edit rechecks the current graph and its mutation guards.
	 *
	 * Plans this operation without applying it to the target graph. A successful preview
	 * does not guarantee a later edit: the target state and edit guards can change.
     * Local labels name the PRECEDING node form. Prefer `(TypeId) ; label Name` for a pinless node,
     * or put `; label Name` on its own line after the closed form. Never put a label before its node.
     * A multiline form may end with `Value 1) ; label Name`; one node cannot have conflicting labels.
     * Quote creation IDs containing spaces: `("Exact Type ID")`. Use flat node forms, not nested reader values.
     * Flat example: `(TypeId` then `InputPin 1) ; label ValueNode`, followed by a separate consumer form
     * containing `DestinationInput (-> ValueNode:OutputPin)`. Substitute discovered type IDs and pin names.
     * Do not write `(TypeId ; label Name)` on one line: the comment hides its closing parenthesis.
     * Every link is written on its destination INPUT: InputPin (-> SourceId:OutputPin).
     * Whitespace-only pin names are quoted exactly as shown by NodePins: `" " (-> Source:" ")`.
     * Preserve the spaces inside those quotes; they are engine pin names, not missing values.
     * To feed an existing node, copy its reader form header with its node ID, then add that input link.
     * Do not write a reverse link on a source output pin. Discover creation IDs with PreviewTurboMCPNodeType;
     * use PreviewTurboMCPNodePins for exact pin names. Native engine search IDs are not interchangeable.
     * In a function graph, use Function|Result for another Return; link execute from a reachable branch and set or link its result inputs.
     * Reader branch headings (:then/:else) are read-only and are refused. They do not create wires.
     * Instead, label the Branch Choice and write `execute (-> Choice:then)` inside the destination
     * node form (or Choice:else for the false output). Use the exact pin names from discovery.
     * Reference it as `(-> Name:ExactPinName)`. A pinless form needs no dummy pin. Semicolon comments
     * consume the rest of their line: put the closing parenthesis BEFORE a trailing label/comment.
	 *
	 * Continuation: auto (legacy unique exit), all (all reachable new unconnected exec exits), or label:pin (one named new node output). Applies to displaced existing exec wires; explicit submitted target links take precedence. Compile/save deferred.
	 *
	 * @param Asset Existing Blueprint path or unique short name, e.g. /Game/Tests/BP_NodeTest. Use a full path when names are ambiguous.
	 * @param GraphName Exact existing graph name, e.g. EventGraph. Use the reader to select the intended graph; do not guess.
	 * @param Text Nonempty append graph text. New forms create nodes; reader-derived IDs refer to existing nodes. Use Preview first and preserve exact type IDs and pin names. Partial reader pages are not standalone append input. Engine-created companion nodes are reported with IDs. Explicit delegate wiring removes only newly generated events left completely disconnected; existing events are preserved.
	 * Connected append placement searches nearby space above and below fixed connections. Existing nodes stay fixed. If no local slot fits, free space or submit a smaller connected block. Tight gaps may require bent wires; this is not whole-graph tidy.
	 *
	 * @param Placement local (default or empty) searches nearby space. outside places new nodes beyond existing graph bounds while keeping existing nodes fixed; boundary wires may be long. Preview and inspect before compiling/saving. Unknown values refuse.
	 * @param Continuation auto (default or empty), all, or label:pin. Controls reconnection of displaced execution wires to new exits; explicit target links take precedence, so Continuation is skipped for those targets and adds no second execution path. Remove the explicit target link if you want Continuation to reconnect it. Example: Done:then for a submitted local label.
	*/
	UFUNCTION(meta = (AICallable))
	static FString PreviewTurboMCPGraphAppend(
		const FString& Asset,
		const FString& GraphName,
		const FString& Text,
        const FString& Continuation = TEXT("auto"),
        const FString& Placement = TEXT("local"));

	/**
     * Plan graph replacement and list deletions; loads the Blueprint without applying the replacement, compiling or saving.
     * Removal considers the named graph, not just one execution branch. Entry/result/event nodes and comments are protected. Inspect the removal list before editing; successful preview does not guarantee a later edit.
     * @param Asset Required Blueprint path or unique short name, e.g. /Game/Tests/BP_NodeTest. Empty or ambiguous values are refused; prefer a full path.
     * @param GraphName Required target graph name, e.g. EventGraph. Use the reader's exact graph name; missing or ambiguous targets are refused.
     * @param Text Required nonempty graph text using the append grammar. Existing reader IDs identify nodes to retain; eligible nodes omitted from the text are deleted. Preserve exact type IDs and pin names. Use a complete ReadTurboMCPGraphLoad result as reference, not as an automatically writable payload. Convert the intended graph to append node forms: exact type ID, input pin lines, and explicit (-> NodeId:OutputPin) links. Keep existing node ID comments to retain those nodes. Reader branch headings such as :then/:else do not create execution links; express each destination execute input explicitly. Never use a truncated page as the replacement; preview and inspect every removal first.
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

	/**
     * Check a group translation; loads the Blueprint and measures transient graph widgets, without requesting live node moves, compilation or saving.
     * Requires an idle editor and usable node bounds. Selection includes nodes fully contained in selected comments. Collision checks use node rectangles; estimated wire lengths do not prove rendered layout quality.
     * @param Asset Required Blueprint path or unique short name, e.g. /Game/Tests/BP_NodeTest. Empty or ambiguous input is refused; prefer a full path.
     * @param GraphName Required unique graph name, e.g. EventGraph. Empty, missing or ambiguous names are refused; copy the name from the graph reader.
     * @param NodeIds Required nonempty array of exact full or short reader node IDs. IDs are case-sensitive and not trimmed; duplicates select the same node once. Comment IDs include their fully contained nodes and nested boxes. Example: ["/Game/Tests/BP_NodeTest.BP_NodeTest:EventGraph:abcdef"].
     * @param DeltaX Required signed 32-bit integer offset in graph units; positive moves right, negative left. Zero leaves X unchanged. Coordinate overflow is refused. Example: 128.
     * @param DeltaY Required signed 32-bit integer offset in graph units; positive moves down, negative up. Zero leaves Y unchanged. Coordinate overflow is refused. Example: -64.
     * @param DestinationBox Exact comment node ID, or empty to retain external comment membership. Nonempty permits only unboxed nodes to enter that comment and its containing ancestors; the selection must fit its padded interior without resizing. Use the ID returned by PreviewTurboMCPGraphGroupPlacement.
     */
    UFUNCTION(meta = (AICallable))
	static FString PreviewTurboMCPGraphGroupMove(const FString& Asset, const FString& GraphName, const TArray<FString>& NodeIds, int32 DeltaX, int32 DeltaY, const FString& DestinationBox = TEXT(""));

    /**
     * Suggest a group translation; loads the Blueprint and renders transient graph widgets for measurement, without requesting live node moves, compilation or saving.
     * Requires an idle editor and usable node bounds. Selection includes nodes fully contained in selected comments. Collision checks use node rectangles; estimated wire lengths do not prove rendered layout quality.
     * @param Asset Required Blueprint path or unique short name, e.g. /Game/Tests/BP_NodeTest. Empty or ambiguous input is refused; prefer a full path.
     * @param GraphName Required unique graph name, e.g. EventGraph. Empty, missing or ambiguous names are refused; copy the name from the graph reader.
     * @param NodeIds Required nonempty array of exact full or short reader node IDs. IDs are case-sensitive and not trimmed; duplicates select the same node once. Comment IDs include their fully contained nodes and nested boxes. Example: ["/Game/Tests/BP_NodeTest.BP_NodeTest:EventGraph:abcdef"].
     * Searches candidate positions near linked outside nodes; not a global optimum. No outside links or no shorter feasible candidate produces no proposal. Apply returned offsets and DestinationBox with EditTurboMCPGraphGroupMove; it rechecks current geometry.
     */
    UFUNCTION(meta = (AICallable))
    static FString PreviewTurboMCPGraphGroupPlacement(const FString& Asset, const FString& GraphName, const TArray<FString>& NodeIds);


	/**
	 * Preview discarding all unsaved changes in a saved Blueprint or data-asset package; does not reload or save. Returns a one-use confirmation valid for 60 seconds, bound to loaded package state and disk content. Refuses open asset editors, detected live-world references, PIE, active transactions and multiple top-level assets. The edit can reinstance references and reset global undo history; raw native references cannot all be detected.
	 * @param Asset Required saved Blueprint or data-asset path, or unique short name. Prefer an exact path such as /Game/Tests/BP_NodeTest. Empty, ambiguous, unsupported or unsaved assets are refused.
	 */
	UFUNCTION(meta = (AICallable))
	static FString PreviewTurboMCPAssetRevert(const FString& Asset);

	/** Preview call swap. Options is optional JSON: {"pin_map":{"Old":"New"},"new_inputs":{"NewInput":{"node":"reader node ID","pin":"OutputPin"}}}. Unmapped pins retain exact names; KeepOld defaults true, intentionally leaving the original disconnected. Validates connected pins and compatible unconnected input defaults using a probe. Preview does not apply the swap and is not a reservation; edit replans. No compile or save.
	 * @param Asset Existing Blueprint path or unique short name, e.g. /Game/Tests/BP_NodeTest. Use a full path when names are ambiguous.
	 * @param GraphName Exact existing graph name, e.g. EventGraph. Use the reader to select the intended graph; do not guess.
	 * @param NodeId Exact reader ID of the existing call node to replace; empty is invalid.
	 * @param ReplacementTypeId Exact replacement call type ID returned by node-type discovery for this graph. Do not use a display name or guessed C++ name.
	 * @param KeepOld true or yes (also empty) keeps the original node disconnected; false or no removes it after verification. Other values are refused.
	 * @param Options JSON object encoded as a string, or empty for exact-name pin matching. Optional target_self=true explicitly discards the old receiver link/default and calls the replacement on this Blueprint (instance functions only; receiver source nodes remain). It cannot be combined with a self pin mapping or new self input. Optional pin_map maps old pin names to new names; new_inputs maps new input names to objects with node and pin. Example: {"pin_map":{"Old":"New"}}. Unknown fields are refused.
	*/
	UFUNCTION(meta = (AICallable))
	static FString PreviewTurboMCPGraphSwapCall(
		const FString& Asset,
		const FString& GraphName,
		const FString& NodeId,
		const FString& ReplacementTypeId,
		const FString& KeepOld,
		const FString& Options = TEXT(""));

	/**
     * Plan moving consumers to another output; loads the Blueprint and may construct a probe node, without applying links, compiling or saving.
     * Both selected pins are outputs. Every consumer of SourcePin is redirected; this does not connect an output to an input or transfer the source node's other inputs/defaults. Inspect the preview and read back the graph after editing.
     * @param Asset Required Blueprint path or unique short name, e.g. /Game/Tests/BP_NodeTest. Empty or ambiguous input is refused.
     * @param GraphName Required unique existing graph name, e.g. EventGraph. Empty, missing or ambiguous names are refused.
     * @param SourceNodeId Required source reader node ID in the named graph. Empty or unresolved IDs are refused.
     * @param SourcePin Required exact output pin name, case-sensitive and not trimmed. Must have linked consumers. Example: ReturnValue when exposed by the source node.
     * @param TargetNodeId Existing target reader node ID, or empty to create a node using TargetTypeId. Must differ from SourceNodeId. A nonempty ID takes precedence over TargetTypeId.
     * @param TargetTypeId Supported creation type ID from discovery; required only when TargetNodeId is empty. Empty is valid with an existing target. Copy the exact type ID; unsupported types are refused.
     * @param TargetPin Required exact output pin name on the existing or new target, case-sensitive and not trimmed. Must be compatible with every source consumer. Example: ReturnValue; this is not an execute input.
     * @param KeepSource Required boolean text: true/1 retains the source; false/0 requests deletion. Empty defaults to true; whitespace trimmed and true/false case-insensitive. Other values refuse. Deletion refuses protected nodes or links on other source pins.
     */
    UFUNCTION(meta = (AICallable))
	static FString PreviewTurboMCPGraphRepoint(
		const FString& Asset, const FString& GraphName, const FString& SourceNodeId,
		const FString& SourcePin, const FString& TargetNodeId, const FString& TargetTypeId,
		const FString& TargetPin, const FString& KeepSource);

	/**
     * Simulate ordered repoints on a transient graph; loads the Blueprint without applying live edits, compiling or saving.
     * All operations must target one asset and graph. Later plans see preceding simulated changes. Preview does not guarantee apply. Failed edits attempt owned undo and compare restored source state and the dirty flag; undo does not certify compilation. If restoration cannot be verified, further TurboMCP edit calls are blocked for the editor session. Inspect and recover the asset before restarting; do not save unverified changes.
     * @param Operations Required nonempty JSON array of operation objects, or an object containing only a lowercase operations array. Each entry requires string asset, graphName (or graph, not both), sourceNodeId, sourcePin, targetNodeId and targetPin. Both pins are outputs. targetNodeId may be empty with targetTypeId for creation. Optional id defaults to operation-N and must be unique; kind defaults to exact graphRepoint, the only supported kind. Optional keepSource accepts boolean or true/false/1/0 text; empty or omitted retains the source. Other fields are strings. Operation field names are case-insensitive; unknown or case-duplicate fields refuse. Empty input/array and malformed JSON refuse. Example with actual reader IDs substituted: [{"asset":"/Game/Tests/BP_NodeTest","graphName":"EventGraph","sourceNodeId":"source reader ID","sourcePin":"ReturnValue","targetNodeId":"target reader ID","targetPin":"ReturnValue","keepSource":true}]. Both nodes must expose the named outputs.
     */
    UFUNCTION(meta = (AICallable))
	static FString PreviewTurboMCPBatchEdit(const FString& Operations);

    /** Connect exact existing node/pin IDs. Data replaces the target input source; exec replaces only the named source output destination and preserves other incoming target paths. No automatic continuation or casts. Same scratch preview and one-undo edit; compile/save deferred. Preview first.
	 * @param Asset Existing Blueprint path or unique short name, e.g. /Game/Tests/BP_NodeTest. Use a full path when names are ambiguous.
	 * @param GraphName Exact existing graph name, e.g. EventGraph. Use the reader to select the intended graph; do not guess.
     * @param SourceNodeId Exact reader ID or unique hexadecimal node key of at least six digits in GraphName. Missing or ambiguous IDs are refused. Qualified IDs must match the named graph.
	 * @param SourcePin Exact output pin name from the reader, e.g. then. No automatic continuation is added.
     * @param TargetNodeId Exact reader ID or unique hexadecimal node key of at least six digits in GraphName. Missing or ambiguous IDs are refused. Qualified IDs must match the named graph.
	 * @param TargetPin Exact input pin name from the reader, e.g. execute. Data replaces this input source; execution replaces only the named source output destination, preserving other incoming paths.
	*/
    UFUNCTION(meta = (AICallable))
    static FString PreviewTurboMCPGraphConnect(const FString& Asset,const FString& GraphName,const FString& SourceNodeId,const FString& SourcePin,const FString& TargetNodeId,const FString& TargetPin);

    /** Remove exact nodes and links with scratch preview and one undo. Nodes is a JSON array of reader node IDs. Links is a JSON array of objects with sourceNodeId, sourcePin, targetNodeId, targetPin. Use [] for either unused list. Node deletion also removes incident links; other nodes/positions are preserved. No automatic reconnection, compile or save.
	 *
	 * @param Asset Existing Blueprint path or unique short name, e.g. /Game/Tests/BP_NodeTest. Use a full path to disambiguate.
	 * @param GraphName Exact existing graph name, e.g. EventGraph. Select it from the reader; do not guess.
	 * @param Nodes JSON array encoded as a string, e.g. ["exact reader node ID"]. Use [] when removing only links, not an empty string. Duplicate, missing and non-deletable nodes are refused. Nodes also accepts exact current UObject paths returned by the engine finder, only within the named graph. Paths can change after reload or reconstruction; prefer fresh reader IDs. This removes graph nodes, not assets.
	 * @param Links JSON array encoded as a string. Each object requires sourceNodeId, sourcePin, targetNodeId and targetPin for an existing link. Use [] when removing only nodes. Do not repeat links already removed by a selected node deletion.
	 * Node references accept exact reader IDs or unique hexadecimal keys of at least six digits in the named graph. Ambiguous keys and mismatched qualified IDs are refused.
     */
    UFUNCTION(meta = (AICallable))
    static FString PreviewTurboMCPGraphRemove(const FString& Asset,const FString& GraphName,const FString& Nodes,const FString& Links);

    /**
     * Preview connecting an existing function through a Create Event node on a transient graph; no live graph edits, compile or save.
     * Requires an idle editor. Preview exercises graph/schema callbacks on a copy; successful preview does not guarantee a later edit. Inspect the result and arrange new nodes before compiling and saving.
     * Repeating the identical compatible function/object binding is unchanged. Replaced source nodes are retained and listed for inspection. This creates a delegate value; the owning Bind or Timer node must execute for registration. Read back, tidy the affected nodes, inspect a capture, then compile and save explicitly.
     * @param Asset Required Blueprint path or unique short name, e.g. /Game/Tests/BP_NodeTest. Empty or ambiguous values are refused; prefer a full path.
     * @param GraphName Required unique K2 graph name from the reader, e.g. EventGraph. Empty, missing and ambiguous names are refused.
     * @param TargetNodeId Exact reader ID or unique hexadecimal node key of at least six digits in GraphName. Missing or ambiguous IDs are refused. Qualified IDs must match the named graph.
     * @param TargetPin Required exact delegate input pin name, case-sensitive and not trimmed. Existing input links are replaced. Empty, orphaned, ambiguous or non-delegate pins are refused; use the reader's pin name.
     * @param FunctionName Required existing function name on self or the chosen object class, e.g. HandleClicked. Uses Unreal name matching; whitespace is not trimmed for lookup. Empty, missing or delegate-incompatible functions are refused. Does not create the function.
     * @param ObjectNodeId Exact reader ID or unique hexadecimal node key of at least six digits in GraphName. Missing or ambiguous IDs are refused. Qualified IDs must match the named graph. Supply with ObjectPin; both empty selects self.
     * @param ObjectPin Exact object-reference output pin name, case-sensitive and not trimmed. Supply together with ObjectNodeId; both empty selects self. Example: ReturnValue when the named node actually exposes that object output.
     */
    UFUNCTION(meta = (AICallable))
    
    static FString PreviewTurboMCPBindFunction(const FString& Asset,const FString& GraphName,const FString& TargetNodeId,const FString& TargetPin,const FString& FunctionName,const FString& ObjectNodeId = TEXT(""),const FString& ObjectPin = TEXT(""));

    /** Preview growth of variadic pins on Make Array, Sequence or associative operators; does not shrink, compile or save.
     * Workflow: append a base node, read its ID, preview/grow, then connect new pins explicitly.
     * Existing pins and links are preserved. No shrink, compile or save. An equal target is a clean no-op.
     * @param Asset Blueprint asset path or unique name. Loading may execute project callbacks.
     * @param GraphName Exact graph name in the Blueprint.
     * @param NodeId Exact full reader ID of the existing supported node.
     * @param Count Desired total, decimal integer 1..32. Sequence counts output branches; array/operator counts inputs. Unsupported native limits refuse.
     */
    UFUNCTION(meta = (AICallable))
    static FString PreviewTurboMCPNodePinGrowth(const FString& Asset,const FString& GraphName,const FString& NodeId,const FString& Count);

    /** Preview flags on an existing owned member variable; preserves identity and graph references, without compile or save.
     * @param Asset Blueprint path or unique name. Asset loading may run project callbacks.
     * @param Name Exact case-sensitive owned member name; inherited/native/component/widget-generated variables refuse.
     * @param InstanceEditable Exact true/false; empty preserves. Controls per-instance editing, not Blueprint access.
     * @param ExposeOnSpawn Exact true/false; empty preserves. Compile separately to update generated spawn behavior; inspect diagnostics before saving.
     * @param Private Exact true/false; empty preserves. Blueprint access metadata, not a security boundary. External references are not validated. Specify at least one flag.
     */
    UFUNCTION(meta = (AICallable))
    static FString PreviewTurboMCPVariableFlags(const FString& Asset,const FString& Name,const FString& InstanceEditable,const FString& ExposeOnSpawn,const FString& Private);

    /** Preview a designer widget delegate event; returns pins and an existing/new node ID on edit, without compile or save.
     * @param Asset Widget Blueprint path or unique name. Loading can execute project callbacks. Requires idle editor and available undo for edits.
     * @param GraphName Exact owned K2 event graph name. Existing binding in another graph refuses; duplicate matching binding is a clean no-op returning its ID.
     * @param WidgetName Exact owned designer widget name with Is Variable enabled and matching compiled skeleton property. Compile explicitly first if the property is missing/stale. Inherited widgets refuse.
     * @param EventName Exact BlueprintAssignable multicast delegate name on the widget class, e.g. OnClicked for UButton; not a display label. Return/writable-output signatures refuse. Connect the new event body separately, then compile and save explicitly. This differs from BindNewEvent on an existing runtime delegate input.
     */
    UFUNCTION(meta = (AICallable))
    static FString PreviewTurboMCPWidgetEvent(const FString& Asset,const FString& GraphName,const FString& WidgetName,const FString& EventName);

    /** Preview Blueprint reparenting; execution refreshes nodes and compiles but never saves, and undo is not guaranteed across reinstancing.
     * @param Asset Exact Blueprint asset path. Normal actor/object/widget Blueprints only; loading may execute project callbacks.
     * @param NewParent Exact class object path, including _C for Blueprint-generated classes. Incompatible families, cycles and declaration conflicts refuse.
     */
    UFUNCTION(meta = (AICallable))
    static FString PreviewTurboMCPBlueprintReparent(const FString& Asset, const FString& NewParent);

    /** Preview the function selected by an existing Create Event node in place; preserves its identity, object input and consumers, with no compile or save.
     * @param Asset Blueprint path or unique asset name. Loading may execute project callbacks. Requires an idle editor outside PIE or compilation.
     * @param GraphName Exact unique K2 graph name from the reader.
     * @param NodeId Exact full or short reader ID for a Create Event node in GraphName. Missing, ambiguous or unconnected delegate signatures refuse.
     * @param FunctionName Existing compatible function on the node's actual object scope. Same function is a clean no-op. Does not change the object or register a runtime delegate; the consuming Bind or Timer node must execute. Unlike BindFunction, this keeps the existing node and all consumers.
     */
    UFUNCTION(meta = (AICallable))
    static FString PreviewTurboMCPDelegateRetarget(const FString& Asset,const FString& GraphName,const FString& NodeId,const FString& FunctionName);



    /**
     * Preview creating a typed custom event wired to an existing delegate input, such as the Event pin of a Bind Event node. No live graph edits, compile or save.
     * Requires an idle editor. Preview exercises graph/schema callbacks on a copy; successful preview does not guarantee a later edit. Inspect the result and arrange new nodes before compiling and saving.
     * @param Asset Required Blueprint path or unique short name, e.g. /Game/Tests/BP_NodeTest. Empty or ambiguous values are refused; prefer a full path.
     * @param GraphName Required unique graph name, e.g. EventGraph. Empty or unresolved names are refused.
     * @param TargetNodeId Exact reader ID or unique hexadecimal node key of at least six digits in GraphName. Missing or ambiguous IDs are refused. Qualified IDs must match the named graph.
     * @param TargetPin Required exact delegate input pin name, case-sensitive and not trimmed. Existing input links are replaced. Empty, orphaned, ambiguous or non-delegate pins are refused; use the reader's pin name.
     * @param EventName Required unused identifier containing letters, digits or underscores, not starting with a digit; no whitespace. Example: OnSelectionChanged. Existing function/event names are refused. Event graphs only; parameters come from the target delegate. Return/output-value signatures are refused. The new event has no execution body. Connect its body separately and ensure the target Bind node executes at runtime. Replaced source nodes are retained; inspect remaining uses before cleanup.
     */
    UFUNCTION(meta = (AICallable))
    static FString PreviewTurboMCPBindNewEvent(const FString& Asset,const FString& GraphName,const FString& TargetNodeId,const FString& TargetPin,const FString& EventName);




    /** Preview copies of a shared pure value on a transient graph without editing, compiling or saving the asset; requires an idle editor.
     * Preserves defaults and existing source-input links. Zero or one connected input is unchanged. Supported sources are self, pure variable-get, object literals and whitelisted literal constructors; arbitrary pure functions, impure calls and split outputs are refused.
     * A consumer node using the source twice receives two copies. Copies are initially placed left of their consumers, may overlap and require layout. Read back and tidy the affected nodes, inspect a capture, then compile and save explicitly. Copying getters may change evaluation frequency; it is not a proof of equivalent project behavior.
     * @param Asset Required Blueprint path or unique short name, e.g. /Game/Tests/BP_NodeTest. Prefer a full path; empty or ambiguous assets are refused.
     * @param GraphName Required exact existing K2 graph name from the reader, e.g. EventGraph. Empty, missing and ambiguous names are refused.
     * @param NodeId Required exact reader ID or unique hexadecimal node key of at least six digits in the named graph. Missing or ambiguous IDs are refused. The source must have one unsplit data output and at most 4096 connected consumer inputs. Preview IDs for new copies are provisional; use edit-result IDs for subsequent calls. No compile or save is performed.
     */
    UFUNCTION(meta = (AICallable))
    static FString PreviewTurboMCPGraphSplitValue(const FString& Asset,const FString& GraphName,const FString& NodeId);

    /** Preview ordered Sequence rows on a transient graph without editing the asset; no compile or save, and requires an idle editor. Rows run in order, followed by the original continuation on the final output; this is not parallel execution. Preview first, edit, read back, tidy the affected nodes, inspect a capture, then compile and save explicitly.
	 *
	 * @param Asset Existing Blueprint path or unique short name, e.g. /Game/Tests/BP_NodeTest. Use a full path to disambiguate.
	 * @param GraphName Required exact existing K2 graph name from the reader, e.g. EventGraph. Empty, missing and ambiguous names are refused.
	 * @param Groups JSON array encoded as a string: 2 to 64 nonempty arrays, at most 4096 exact reader node IDs in total, e.g. [["first ID","second ID"],["third ID"]]. Groups must form an ordered linear chain of synchronous calls; latent, branching and cyclic groups are refused.
	 * @param bSplitValues Optional boolean, default true. Copy supported pure self, variable-get and literal sources per selected consumer when true; unrelated values are untouched. false preserves shared sources. Splitting may create nodes; use the edit result IDs, not provisional preview IDs.
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
	 * Plan material positions and descriptive column boxes; does not compile, apply layout or save; accepts Materials only, not functions or instances.
	 * Uses estimated expression sizes. Matching expression identities and compile errors do not prove identical rendered output.
	 * @param Asset Required material name or package/object path, e.g. /Game/Materials/M_Surface; empty is refused. Material instances are refused; name their parent explicitly.
	 * @param Force Legacy compatibility string; leave empty. Layout is always recomputed and an already matching result remains unchanged, including when Force is nonempty.
	 */
	UFUNCTION(meta = (AICallable))
	static FString PreviewTurboMCPMaterialTidy(
		const FString& Asset,
		const FString& Force);

	/**
	 * Plan named reroutes for exact material outputs read by multiple expressions. Loads the Material but does not compile, change connections or save. Material functions and instances are refused.
	 * Names use parameter/caption and output context. One exact output and mask must have at least two distinct direct readers; existing named-reroute readers are left alone. Existing positions are preserved; new nodes use conservative collision bounds, not full graph tidy or comment containment. Edit replans current state and compiles only when changing it. Compiler and identity checks do not prove identical rendered output.
	 * @param Asset Required material name or package/object path, e.g. /Game/Materials/M_Surface; empty is refused. Material instances are refused; explicitly name the parent to edit its graph.
	 */
	UFUNCTION(meta = (AICallable))
	static FString PreviewTurboMCPMaterialLabelPromote(
		const FString& Asset);

	/**
	 * Plan material label and reroute cleanup using the loaded graph. Does not compile, change graph data or save.
	 * Uses existing comment boxes as blocks. Adds labels for values read by multiple blocks, merges repeated usages per block, and removes eligible internal reroutes. Existing node positions are preserved; new usages need free space inside their block. This is cleanup, not full graph layout. Edit rebuilds the plan from current state; request another preview if the material changed. Compile and identity checks do not prove identical rendered output.
	 * @param Asset Required material name or package/object path, e.g. /Game/Materials/M_Surface; empty is refused. Material instances and material functions are refused. For an instance, explicitly name its parent material. Blocks are existing comment boxes; this tool does not create boxes or perform a full graph layout.
	 */
	UFUNCTION(meta = (AICallable))
	static FString PreviewTurboMCPMaterialBlocks(
		const FString& Asset);

	/** Inspect proposed row edits using temporary row data; loads the table and may load objects referenced by imported values; does not write rows, compile or save.
     * Validates the operation list before writing. Failed read-back attempts to undo this transaction; the result distinguishes verified row restoration from unverified or failed rollback. External callback effects are not covered by row restoration checks.
     * @param Asset Required DataTable name or package/object path, for example /Game/Data/DT_Items; empty, ambiguous, unresolved or non-DataTable targets are refused. Prefer a full path.
     * @param Rows Required array of operation strings, for example ["Potion = Health=100,Label=\"Small potion\"", "- Obsolete"]. Each string sets named fields or removes a named row. A set creates a missing row; unspecified fields keep existing values, or the row struct's defaults for a new row. Use internal field names when display labels are ambiguous and Unreal export-text syntax for values, including quoted text and parenthesized structs/containers. Empty arrays or all-blank lines are refused; blank lines within a nonempty request are skipped. Empty field values are passed to the type importer, not treated as omitted. Empty/None row names, duplicate fields in one operation and conflicting/repeated removals are refused. No implicit operation is supplied.
     */
	UFUNCTION(meta = (AICallable))
	static FString PreviewTurboMCPDataTableRows(
		const FString& Asset,
		const TArray<FString>& Rows);

	/**
     * Plan a node copy and report external links; loads both Blueprints without applying the copy, compiling or saving.
     * Required target members must already exist. External links that cannot resolve in the target are reported and left unconnected. Inspect the preview, then check layout, links and compilation after editing; preview does not guarantee edit success.
     * GraphCopy refuses the whole selection if it includes a local-variable node or a Format Text node. To copy the other nodes, leave these nodes out of the selection. Recreate them in the destination graph and use GraphConnect to restore their links.
     * @param SourceAsset Required source Blueprint path or unique short name, e.g. /Game/Tests/BP_NodeTest. Empty or ambiguous input is refused.
     * @param SourceGraph Required existing source graph name, e.g. EventGraph; use the reader's name. Missing targets are refused.
     * @param NodeIds Required array of source reader node IDs. Empty means nothing to copy. Use exact IDs from the source graph; unresolved or unsupported selections are refused. Structural nodes and comments are refused. Copies retain relative positions with a +40 offset; use GraphConnect for boundary wires and GraphTidy for layout afterward.
     * @param TargetAsset Required destination Blueprint path or unique short name. It must already exist and declare compatible members needed by copied nodes; self functions need compiled signatures with matching parameter names and types. Empty or ambiguous input is refused.
     * @param TargetGraph Required existing destination graph name. This is not a new graph name; choose a different graph from the source. Example: EventGraph in another Blueprint.
     * @param AnchorNodeId Required string; pass empty. Anchor wiring is not implemented and any nonempty value is refused.
     * Node references accept exact reader IDs or unique hexadecimal keys of at least six digits in the named graph. Ambiguous keys and mismatched qualified IDs are refused.
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
     * Extract a supported selection into a new function. Preview lists selected nodes, signature and external connections without changing graphs. Edit repeats planning, moves nodes and replaces them with a call. No automatic compile or save; inspect both graphs and compile before saving. Failed postconditions report partial changes rather than claiming restoration.
     * @param Asset Required Blueprint path or unique name, for example /Game/BP_Menu.
     * @param GraphName Required exact source graph name from the reader.
     * @param NodeIds Required selected reader node IDs. Entry/event/result, latent calls, local captures, macros and unsupported boundary pins refuse. Include all contents of a selected comment.
     * @param MemberName Required unused valid function name, for example FormatLabel.
     * @param MemberKind Required function. Macro extraction is not supported. Pure selections retain consumer-driven evaluation; execution selections require one incoming execution boundary and at most one continuation.
     */
    UFUNCTION(meta = (AICallable))
    static FString PreviewTurboMCPGraphCollapse(
		const FString& Asset,
		const FString& GraphName,
		const TArray<FString>& NodeIds,
		const FString& MemberName,
		const FString& MemberKind);

	/** Inspect a proposed user-defined struct; type resolution may load referenced assets; does not create, compile or save the struct.
     * Existing assets are refused. Preview validates field syntax and resolves types, but does not guarantee creation or validate every default. Edit lists observed fields; defaults and metadata are not fully verified. Failures may leave a partial unsaved asset; inspect before saving or retrying.
     * @param Path Required new package path including the asset name, such as /Game/Structs/S_Item; no object suffix or file extension. Whitespace is trimmed; empty, invalid and relative paths are refused. No automatic path is chosen.
     * @param Fields Required comma-separated Name: Type declarations, such as Count: int, default=1, Label: text, tooltip=Item label. Options default=, tooltip= and category= attach to the preceding field and may appear once each. Empty input is refused; empty option values are skipped during creation. Names must be unique ignoring case. Types include bool, byte, int, int64, float, real, double, name, string, text; vector, rotator, transform, vector2d, linearcolor, color; loadable struct/enum paths; array<T>, set<T>, map<K,V>; a full class path denotes an object reference, for example Array</Script/Engine.SoundBase>. Nested containers are refused and set elements/map keys must be hashable. Use Unreal literal syntax for defaults; plain top-level commas delimit fields or options.
     */
	UFUNCTION(meta = (AICallable))
	static FString PreviewTurboMCPStructNew(const FString& Path, const FString& Fields);

	/** Inspect proposed struct field edits; loads the struct and referenced types and queries source-control status; does not edit, compile, check out or save the struct.
     * Removal consent counts only loaded dependent assets, not all project references. Checkout failure is reported but does not block the in-memory edit. A failed operation may leave earlier changes applied; no automatic rollback is performed. Inspect the returned fields and dependent assets before saving.
     * @param Struct Required user-defined struct asset name or package/object path, for example /Game/Structs/S_Item; empty or unresolved values and native C++ structs are refused.
     * @param Changes Required comma-separated operations: Name: Type adds a missing field or changes an existing field; move:Name, above=Other or below=Other reorders; remove:Name deletes. Field declarations use the same types and default=, tooltip=, category= options as EditTurboMCPStructNew. Empty options leave existing settings unchanged, not cleared. Each field may be targeted once per call; empty input is refused. For removal, force=N must match the current loaded-dependent count reported by preview (integer 0 through 2147483647); omission is allowed only when there are no loaded dependents. Preview again if the count changes. Example: Count: int, default=1, move:Count, above=Label must be split into two calls because Count appears twice.
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
    /** Permanently delete one saved asset after fresh checks and preview confirmation; no undo or automatic retry. Refuses referenced, dirty, open, read-only or unsafe source-control targets, maps, redirectors and multi-asset packages. Never force-deletes or removes imported source files.
     * @param AssetPath Exact saved /Game asset object path, for example /Game/Tests/Unused.Unused. No short names, folders or package-only paths.
     * @param Confirmation Copy the separate preview token within 60 seconds. Target identity and file contents must remain unchanged. The token is consumed before deletion.
     */
    UFUNCTION(meta = (AICallable))
    static FString EditTurboMCPAssetDelete(const FString& AssetPath, const FString& Confirmation);


public:
    /** Create a new Blueprint function library at an unused destination; no disk save or overwrite. Never copies another asset. Preview creates nothing; edit uses the stock factory, which initializes the Blueprint class. No disk save or reliable creation-undo guarantee. Refuses PIE and active transactions.
     * @param PackagePath Exact new /Game package path, for example /Game/Utilities/BFL_Purchase. Do not pass an object suffix, filename, short name or existing package. Created assets remain unsaved; save separately after inspection.
     */
    UFUNCTION(meta = (AICallable))
    static FString EditTurboMCPFunctionLibraryCreate(const FString& PackagePath);

    /** Create a fresh typed Data Asset, preserving class defaults. Preview validates without allocation. Edit creates in memory; saving and source-control add are explicit. Constructors may execute project code. Never overwrites an existing package or compiles Blueprints.
     * @param ClassPath Exact already loaded concrete UDataAsset class path; Blueprint-generated paths end in _C. Unloaded, abstract, transient and superseded classes are refused.
     * @param PackagePath New /Game package path without object suffix or extension, for example /Game/Data/DA_Menu.
     * @param InitialFieldsJSON JSON object of editable field names to Unreal property-text strings. Empty means class defaults. Scalar/value-struct values and loaded external references are supported; containers and owned children use subsequent property edits.
     * @param Save Explicit disk save after creation. Default false. A saved file is not removed by Undo.
     * @param AddToSourceControl Add the saved file through the current provider; requires Save=true. Default false. Failure preserves the asset and reports partial completion. Never checks out or submits.
     */
    UFUNCTION(meta = (AICallable))
    static FString EditTurboMCPDataAssetCreate(const FString& ClassPath, const FString& PackagePath, const FString& InitialFieldsJSON = TEXT(""), bool Save = false, bool AddToSourceControl = false);



public:
    /** Validate a scoped PIE runtime call and execute it once with preview confirmation. Reference-bearing parameters, RPCs and latent functions are refused. No save or undo guarantee.
     * @param WorldPath Exact current PIE world object path.
     * @param ObjectPath Exact existing runtime object path in that world; local-player subsystems are supported.
     * @param FunctionName Exact Blueprint-callable function name. Inspect ReadTurboMCPPlayFunction first.
     * @param Arguments JSON object with every input parameter by exact name; use {} for no inputs. Use JSON numbers for numbers (integers must be exact and within +/-9007199254740991), booleans for bools, and strings for text/name/enum names. Arrays and sets use JSON arrays; reflected structs and maps use JSON objects. Map keys must be text/name/enum values. Object references, custom-imported structs and null values are unsupported. Supply all inputs, including defaults; omit output-only and return parameters. Example: {"Amount":3,"Label":"debug event"}. Unknown, missing or unsupported arguments refuse before dispatch.
     * @param Confirmation Copy the preview confirmation with identical arguments within 120 seconds. It is consumed before invocation; never automatically retry.
     */
    UFUNCTION(meta = (AICallable))
    static FString EditTurboMCPPlayInvoke(const FString& WorldPath,const FString& ObjectPath,const FString& FunctionName,const FString& Arguments,const FString& Confirmation);


    /** Create a standalone typed custom event in an existing event graph. Local, non-replicated; not an override or delegate binding. No compile or save. Duplicate names refuse. Loading assets can run engine load hooks.
     * @param Asset Blueprint object path or unique asset name.
     * @param GraphName Exact existing event graph name. Function and macro graphs refuse.
     * @param Name New event identifier, at most 128 characters; must not collide with existing or inherited callables.
     * @param Inputs Empty for no inputs, otherwise comma-separated Name:type pairs, for example Amount:float, Label:string. Container type commas stay inside angle brackets. At most 32 unique data parameters; no execution, delegate or wildcard types. Event parameters become output pins for subsequent append links.
     */
    UFUNCTION(meta = (AICallable))
    static FString EditTurboMCPCustomEventAdd(const FString& Asset, const FString& GraphName, const FString& Name, const FString& Inputs);


    /** Run a diagnostic query or a separately confirmed process-wide change; dispatch alone does not verify later effects. Log, Slate debugger and CVar effects are process-wide, even with a PIE target. No shell, file operations or arbitrary console commands. Dispatch is not proof of asynchronous effects; use ReadTurboMCPEditorLog.
     * @param Target Exact editor or pie:<instance number>. Missing or ambiguous worlds refuse.
     * @param Command One command: Log list [category], Log <exact category> <verbosity>, SlateDebugger.Event.Start/Stop, or an existing CVar name with an optional single value. No chaining or newlines. Preview reads current state without changing it.
     * @param Confirmation Empty for queries. For changes, copy the token from the latest separate preview within 60 seconds; each preview replaces the previous token. A changed target, command or current CVar/log value requires a fresh preview. Session changes are not saved; restore the previous value when finished.
     */
    UFUNCTION(meta = (AICallable))
    static FString EditTurboMCPDiagnosticConsole(const FString& Target, const FString& Command, const FString& Confirmation);


    /** Edit one data-asset property using temporary validation. Requires the current reader revision. No implicit save or compile. Edit notifications may run project code. The tool verifies resulting state and attempts restoration on mismatch; inspect the final result and rollback status.
     * @param Asset Exact asset path or uniquely resolved asset name; must resolve to UDataAsset.
     * @param Property Exact reflected property path from ReadTurboMCPDataAssetProperty. External objects are never implicit edit targets.
     * @param Operation set, array_remove, array_insert, map_upsert, map_remove, create_child or child_set. array_remove removes one existing array element and preserves all remaining values and their order; array_insert inserts before Key (0..count; count appends), preserving existing order. Whole-array set is unsupported. child_set edits an existing owned map child selected by Key. create_child creates a compatible instanced object in an empty property/map entry; it never replaces an existing child. Set changes one scalar/value-struct or existing object reference; map operations change one entry, preserving unrelated entries.
     * @param Key For array_remove/array_insert, an exact zero-based decimal index (e.g. 64 removes the 65th element). Otherwise, exact loaded object path for object keys or Unreal property text. Required for map operations and map-child creation; empty for set or direct child creation.
     * @param Value Unreal property text for value fields, or exact loaded object path for references. Empty for map_remove/array_remove. For array_insert, empty inserts a default element; explicit values support scalar/value structs or loaded compatible hard references, not reference-containing struct text. Use quoted empty string for a string element. Example integer: 42. Object example: /Game/Input/IA_Jump.IA_Jump. For child_set, use JSON: {"property":"Number","value":"42"}. For create_child, use JSON: {"class":"/Script/EnhancedInput.PlayerMappableKeySettings","fields":{"Name":"Jump"}}. Initial fields are scalar/value-struct property-text strings. Other operations do not accept stock engine JSON.
     * @param Revision Exact revision returned by the reader or preview; stale revisions refuse without writing. Read again after any change.
     */
    UFUNCTION(meta = (AICallable))
    static FString EditTurboMCPDataAssetProperty(const FString& Asset, const FString& Property, const FString& Operation, const FString& Key, const FString& Value, const FString& Revision);

    /**
     * Remove a directly implemented interface while keeping its function bodies as ordinary functions.
     *
     * Calls that require this interface may stop working. First preview, inspect affected callers, and pass the returned confirmation unchanged to the edit tool. Confirmation identifies the asset/interface; it is not a snapshot or stale-state lock.
     * Mutates the target in memory and requests structural refresh, which can compile dependencies. Does not save. Undo is not guaranteed across structural compilation.
     * Requires the editor game thread outside PIE and active transactions.
     *
     * Example: Preview the same Asset and Interface first; copy the complete confirmation value from its response into Confirm.
     *
     * @param Asset Required target Blueprint asset path, such as /Game/UI/WBP_Target.WBP_Target, or an unambiguous asset name. Prefer the full path.
     * @param Interface Required exact directly implemented interface class path, asset object path, or unique name. Inherited interfaces cannot be removed.
     * @param Confirm Required exact confirmation value returned by PreviewTurboMCPInterfaceRemove for this asset/interface. Empty or different values refuse.
     *
     * @return A preview/edited/unchanged summary or an error with the reason. Structural refresh is not proof of successful compilation. Read back and compile before saving.
     */
    UFUNCTION(meta = (AICallable))
    static FString EditTurboMCPInterfaceRemove(const FString& Asset, const FString& Interface, const FString& Confirm);

    /**
     * Request activation of one named runtime button through normal Slate input.
     *
     * Requires one in-process PIE world, one local player, an active editor application,
     * a constructed visible/enabled UMG or CommonUI button, and no tool-held input.
     * Focuses the button and sends Gamepad_FaceButton_Bottom for 0.1 seconds with
     * automatic release. Does not invoke click delegates directly. This executes
     * game/UI logic; there is no asset transaction, save or undo.
     *
     * @param Target Exact session-bound token from ReadTurboMCPPlayInputState(Target=list), or pie:0 for a single PIE world/player. The button must belong to that player. Expired tokens and external-process clients refuse.
     * @param Widget Required exact runtime object path or unique object name from ReadTurboMCPPlayInputState. Not a designer name, display label, asset path or Slate ref. Prefer the complete runtime path when names repeat.
     *
     * Workflow: read current state, select its runtime button path, call once, allow
     * normal frames for press/release, then read state and capture the viewport.
     * A disabled/hidden/ambiguous button refuses. If focus moves but input fails,
     * the error says so. Never retry blindly after an uncertain client outcome.
     * @return Input delivery details and a pending-activation message, not a claim
     * that the expected screen opened. Verify the resulting screen independently.
     */
    UFUNCTION(meta = (AICallable))
    static FString EditTurboMCPMenuActivate(const FString& Target, const FString& Widget);

    /**
     * Declare an event dispatcher with named, typed input parameters.
     *
     * Any existing member with this name refuses, including an identical dispatcher. This creates the signature only; it does not bind listeners, create call nodes or invoke the dispatcher.
     * Mutates the target in memory and requests structural refresh, which can compile dependencies. Does not save. Undo is not guaranteed across structural compilation.
     * Requires the editor game thread outside PIE and active transactions.
     *
     * Example: Name=OnPurchase; Inputs=Accepted:bool,Buyer:object</Script/Engine.Actor>.
     *
     * @param Asset Required target Blueprint asset path, such as /Game/UI/WBP_Target.WBP_Target, or an unambiguous asset name. Prefer the full path.
     * @param Name Required unused member identifier, for example OnPurchase. Letters, digits and underscores only; start with a letter or underscore. None is reserved.
     * @param Inputs Required string: comma-separated Name:Type pairs. Use an empty string for no parameters. Example: Accepted:bool,Buyer:object</Script/Engine.Actor>. Parameter names must be distinct; nested container types use shared type syntax.
     *
     * @return A preview/edited/unchanged summary or an error with the reason. Structural refresh is not proof of successful compilation. Read back and compile before saving.
     */
    UFUNCTION(meta = (AICallable))
    static FString EditTurboMCPDispatcherAdd(const FString& Asset, const FString& Name, const FString& Inputs);
    /**
     * Rename an owned Blueprint function and discovered callers. Preview loads candidates; edit repeats discovery using the same arguments. Requires a compiled owner and idle editor. Direct dependent maps and more than 4096 candidate packages refuse. No save or guaranteed rollback. Compile affected assets before saving; dynamic/native references are not verified.
     * Requires an idle editor outside play/transactions and a successfully compiled defining Blueprint. Direct dependent maps refuse; discovery refuses above 4096 candidate packages. Preview does not reserve a plan: edit discovers references again.
     * Example: Asset=/Game/UI/WBP_Menu, Function=RefreshState, NewName=RefreshLegacyState. Preview first, inspect affected assets, then edit with the same arguments.
     * Preview lists affected assets and call/delegate nodes. Dependencies are loaded; load callbacks may run. Edit repeats discovery, renames child overrides through stock editor APIs and updates references. No save or guaranteed rollback; compile affected assets before saving. Inherited/interface functions and name conflicts refuse. Dynamic strings, native code and unavailable content are not verified.
     * @param Asset Required defining Blueprint path or unique name. Example: /Game/UI/WBP_Menu with Function=RefreshState and NewName=RefreshLegacyState. Preview first and inspect affected assets. Requires a compiled owner and idle editor. Discovery inspects direct referencers of the owner and derived classes; dependent maps and more than 4096 direct candidate packages refuse.
     * @param Function Required exact owned function name; inherited overrides are not independently renamed.
     * @param NewName Required valid new member name, 1 to 100 characters. Same name is a no-op. Edit repeats discovery with the same arguments. No save or guaranteed rollback; compile affected assets before saving. Dynamic/native references are not verified.
     */
    UFUNCTION(meta = (AICallable))
    static FString EditTurboMCPFunctionRename(const FString& Asset, const FString& Function, const FString& NewName);
    /**
     * Inspect or change owned function purity and receiver constness. Preview first; edit repeats preflight. Requires an idle editor and compiled owner. Inspects direct referencers of the owner and derived classes, with a 4096-package limit. Refuses unsafe connected callers, overrides and delegate references. Candidate assets are loaded and callbacks may run. No automatic save; compile affected assets before saving. A failed postcondition explicitly reports partial changes.
     * @param Asset Required Blueprint path or unique asset name.
     * @param Function Required exact owned function name. Use FunctionAdd first to create a new helper.
     * @param Pure Required keep, true or false. Removing connected execution pins or introducing an unexecuted data producer refuses.
     * @param Const Required keep, true or false. Const constrains this receiver, not all external objects. Unsupported macro-body analysis refuses.
     */
    UFUNCTION(meta = (AICallable))
    static FString EditTurboMCPFunctionFlags(const FString& Asset, const FString& Function, const FString& Pure, const FString& Const);

    /**
     * Prepare missing member declarations before copying graph bodies between Blueprints.
     *
     * Creates declared variables with defaults, function signatures, dispatcher signatures and custom-event declarations on a new named event graph. Exact-name exclusions skip creation without deleting existing target members. GraphTransfer can replace matching empty event declarations with their bodies; connected or commented target events refuse. Compatible existing members are retained; differing variable types or callable signatures refuse. Compile both assets first. Does not copy widget trees, interfaces, function bodies, animations or bindings. Add required interfaces and copy widget subtrees separately, then use GraphTransfer. A failed edit can leave earlier declarations created; inspect the target before retrying.
     * Mutates the target in memory and requests structural refresh, which can compile dependencies. Does not save. Undo is not guaranteed across structural compilation.
     * Requires the editor game thread outside PIE and active transactions.
     *
     * Example: SourceAsset=/Game/UI/WBP_Source.WBP_Source; TargetAsset=/Game/UI/WBP_Target.WBP_Target.
     *
     * @param SourceAsset Required source Blueprint asset path or unambiguous name. Prefer its full object path; the source is not edited.
     * @param TargetAsset Required different target Blueprint asset path or unambiguous name. Its parent class must equal or derive from the source parent class.
     *
     * @param Exclude Optional comma- or pipe-separated exact source member names to skip, case-insensitive. Empty includes all. Unknown names refuse before changes. Skipped dependencies may need separate preparation before transferring bodies.
     * @return A preview/edited/unchanged summary or an error with the reason. Structural refresh is not proof of successful compilation. Read back and compile before saving.
     */
    UFUNCTION(meta = (AICallable))
    static FString EditTurboMCPMergeMembers(const FString& SourceAsset, const FString& TargetAsset, const FString& Exclude = TEXT(""));
    /**
     * Copy one complete owned graph, including its nodes, connections, layout, comments and locals.
     *
     * Compile both assets and prepare referenced members/widgets first. Existing executable logic is never overwritten. Source-owned macro dependencies must already exist in the target; unresolved calls, missing self members and conflicting custom events refuse. Matching empty custom-event declarations can be replaced; connected, commented or disabled declarations are protected. This does not perform a whole-asset merge or automatically tidy the copied layout.
     * Mutates the target in memory and requests structural refresh, which can compile dependencies. Does not save. Undo is not guaranteed across structural compilation.
     * Requires the editor game thread outside PIE and active transactions.
     *
     * Example: SourceGraph=CalculatePurchase; TargetGraph=""; use exact source/target asset paths.
     *
     * @param SourceAsset Required source Blueprint asset path or unambiguous name. Prefer its full object path; the source is not edited.
     * @param SourceGraph Required exact source function, interface-function, macro or event graph name. This selects the whole graph, not node IDs or an event label.
     * @param TargetAsset Required different target Blueprint asset path or unambiguous name. Its parent class must equal or derive from the source parent class.
     * @param TargetGraph Pass an empty string to keep SourceGraph, or a valid destination identifier. Existing event pages receive the whole source group below existing nodes, preserving internal links and relative positions; unmeasured nodes use a conservative 512-unit height. This does not splice execution chains or tidy. Nested source graphs refuse append. Existing macro graphs refuse. An existing function must contain only entry/result nodes, no locals, and have a compiled compatible signature.
     *
     * @return A preview/edited/unchanged summary or an error with the reason. Structural refresh is not proof of successful compilation. Read back and compile before saving.
     */
    UFUNCTION(meta = (AICallable))
    static FString EditTurboMCPGraphTransfer(const FString& SourceAsset, const FString& SourceGraph, const FString& TargetAsset, const FString& TargetGraph);
    /**
     * Copy one widget and its descendants into another Widget Blueprint.
     *
     * Widget names must be unused unless AutoRename is enabled. Copies widget properties, internal slots and the root slot layout; preserves direct/struct/array text identities. Designer bindings refuse. Graph logic and animations are not copied. Detected references outside the selected subtree refuse; prefer copying a common ancestor. Complex custom widget references and text map/set properties are not fully validated. Read the target tree and compile before saving.
     * Mutates the target in memory and requests structural refresh, which can compile dependencies. Does not save. Undo is not guaranteed across structural compilation.
     * Requires the editor game thread outside PIE and active transactions.
     *
     * Example: SourceWidget=PurchasePanel; Parent=RootCanvas; use exact source/target Widget Blueprint paths.
     *
     * @param SourceAsset Required source Blueprint asset path or unambiguous name. Prefer its full object path; the source is not edited.
     * @param SourceWidget Required exact designer object name, such as PurchasePanel. Use the widget-tree reader; this is not displayed text or a runtime PIE path.
     * @param TargetAsset Required different target Blueprint asset path or unambiguous name. Its parent class must equal or derive from the source parent class.
     * @param Parent Required string naming an existing target panel, such as RootCanvas. Pass an empty string only when the target widget tree is empty. A panel must have room; its class must match the source parent class when preserving the root slot.
     * @param AutoRename Optional, default false. Rename colliding copied widgets with _Copy, _Copy2, etc.; preview/edit list every mapping. The source and existing target widgets are untouched. Explicit navigation inside the subtree is remapped; outside navigation refuses. Graphs, animations and custom string references are not migrated. Edit recalculates names, so inspect its returned mapping.
     *
     * @return A preview/edited/unchanged summary or an error with the reason. Structural refresh is not proof of successful compilation. Read back and compile before saving.
     */
    UFUNCTION(meta = (AICallable))
    static FString EditTurboMCPWidgetSubtreeCopy(const FString& SourceAsset, const FString& SourceWidget, const FString& TargetAsset, const FString& Parent, bool AutoRename = false);

    /**
     * Add a Blueprint interface contract and its required function declarations.
     *
     * An already implemented or inherited interface is unchanged. Existing graph-name conflicts and animation-layer interfaces refuse. This does not copy source event bodies or function logic.
     * Mutates the target in memory and requests structural refresh, which can compile dependencies. Does not save. Undo is not guaranteed across structural compilation.
     * Requires the editor game thread outside PIE and active transactions.
     *
     * Example: Asset=/Game/UI/WBP_Target.WBP_Target; Interface=/Game/Interfaces/BPI_Menu.BPI_Menu.
     *
     * @param Asset Required target Blueprint asset path, such as /Game/UI/WBP_Target.WBP_Target, or an unambiguous asset name. Prefer the full path.
     * @param Interface Required compiled Blueprint interface asset path/name, such as /Game/Interfaces/BPI_Menu.BPI_Menu, or a Blueprint-implementable native /Script/Module.Interface class path.
     *
     * @return A preview/edited/unchanged summary or an error with the reason. Structural refresh is not proof of successful compilation. Read back and compile before saving.
     */
    UFUNCTION(meta = (AICallable))
    static FString EditTurboMCPInterfaceAdd(const FString& Asset, const FString& Interface);

    /**
     * Declare a typed local variable in one function; do not create getter/setter nodes.
     *
     * An existing local with the same type and default is unchanged; a conflicting declaration refuses. This declares storage only, not function behavior.
     * Mutates the target in memory and requests structural refresh, which can compile dependencies. Does not save. Undo is not guaranteed across structural compilation.
     * Requires the editor game thread outside PIE and active transactions.
     *
     * Example: Function=CalculatePurchase; Name=ItemCount; Type=int; Default=7.
     *
     * @param Asset Required target Blueprint asset path, such as /Game/UI/WBP_Target.WBP_Target, or an unambiguous asset name. Prefer the full path.
     * @param Function Required owned function graph name, for example CalculatePurchase, including implemented interface functions. Matching is case-insensitive and must be unique. Event graphs and inherited functions are not accepted.
     * @param Name Required identifier starting with a letter or underscore and containing only letters, digits or underscores. Must not conflict with parameters or members; None is reserved.
     * @param Type Required shared Blueprint type syntax, for example int, float, vector, or object</Script/Engine.Actor>. For structs, pass the plain object path, for example /Script/CoreUObject.Vector or /Game/Types/ST_Item.ST_Item; do not add a struct: prefix. The struct must support Blueprint variables.
     * @param Default Pass an empty string for the Unreal type default. Otherwise provide a schema-valid scalar/struct literal, for example 7 for int. Explicit object/class/container defaults refuse; assign them in graph logic.
     *
     * @return A preview/edited/unchanged summary or an error with the reason. Structural refresh is not proof of successful compilation. Read back and compile before saving.
     */
    UFUNCTION(meta = (AICallable))
    static FString EditTurboMCPLocalVariableAdd(const FString& Asset, const FString& Function, const FString& Name, const FString& Type, const FString& Default);

    /** Route bounded keyboard/gamepad input through Slate in PIE; executes game/UI code. Delivery is not gameplay success.
     * No Enhanced Input action injection, asset edits, save or undo. Use native Slate tools for mouse/text.
     * @param Target Exact session-bound token from ReadTurboMCPPlayInputState(Target=list), or pie:0 for a single PIE world/player. Tokens expire with the world/player. External-process clients are unsupported. Input scheduling is global: release the owning target before sending input to another target.
     * @param Action activate, activate_gameplay, focus, hold, sequence or release. Activate requests OS foreground while preserving menu focus (may refuse; unattended guard refuses). activate_gameplay also moves Slate focus to the gameplay viewport in the same call and displaces menu focus; OS activation is still not guaranteed. Activation actions require empty Key, Value and Seconds. Focus moves Slate focus to the viewport, displacing menus. Hold/sequence preserve current PIE menu focus. Release cancels all owned inputs and pending events. Existing active input refuses.
     * @param Key Hold: keyboard/gamepad button or 1D axis. Sequence: JSON array, 1..32 events, sorted by at; fields at, key, duration, analog-only value. Example: [{"at":0,"key":"Gamepad_RightX","value":0.5,"duration":2},{"at":0.3,"key":"Gamepad_FaceButton_Bottom","duration":0.1},{"at":0.8,"key":"Gamepad_FaceButton_Bottom","duration":0.1}]. Distinct keys may overlap; same-key overlap and editor shortcuts refuse. Empty for other actions.
     * @param Value Hold axis: finite -1..1; empty otherwise. Stock Slate Gamepad_LeftY=-1 Down, +1 Up; magnitude >0.5. UI routing may differ. Example: hold Gamepad_LeftY, Value=-1, Seconds=0.3. Inspect actual focus; delivery is not success.
     * @param Seconds Hold: finite 0.05..5; empty otherwise. Sequence at >=0 and duration 0.05..5, ending by 5 seconds. Normal ticks required; expired undispatched events abort, not replay. Focus loss, PIE end or deactivation release owned inputs. Holds may repeat, not exactly one row.
     */
    UFUNCTION(meta = (AICallable))
    static FString EditTurboMCPPlayInput(const FString& Target, const FString& Action, const FString& Key, const FString& Value, const FString& Seconds);

    /**
     * Inspect editor readiness for ordinary transactional asset edits; does not edit, compile or save.
     * Reports play/simulation, active transactions, transaction availability and recovery blocks.
     * This is a snapshot, not a queue, permission grant or reservation. Each edit still checks its
     * preferences, asset and safety requirements. Play-control tools have separate rules.
     */
	UFUNCTION(meta = (AICallable))
	static FString ReadTurboMCPEditQueue();

	/**
	 * Compile up to 25 named assets and report compiler and engine diagnostics; does not save.
	 * Compilation can reinstance classes and affect dependent objects. Public tool calls refuse during play or an active editor transaction. Earlier edits from other tools are not undone or queued: stop play, inspect ReadTurboMCPDirtyList, then compile and explicitly save wanted changes. Inspect each asset result before saving; this is not an atomic batch.
	 * @param Assets Required comma- or pipe-separated asset paths or unique short names, e.g. /Game/Tests/BP_NodeTest|/Game/Tests/WBP_NodeTest. Empty lists and lists over 25 entries are refused. Unsupported asset types are reported individually.
	 */
	UFUNCTION(meta = (AICallable))
	static FString EditTurboMCPCompile(const FString& Assets);

	/**
	 * Write named asset packages to disk, including packages whose dirty flag is clear; does not explicitly compile.
	 * Widget Blueprints use direct package serialization without refreshing their thumbnail. Other assets use the editor save path, which may render thumbnails and execute asset code. Saving is not an undoable graph edit.
	 * Refuses during play, simulation or an active editor transaction. A refusal does not undo earlier edits or queue a save. Stop play and inspect ReadTurboMCPDirtyList before compiling and saving wanted changes. Each asset is saved separately, with one retry on failure; earlier writes are not rolled back if another asset fails. Source-control checkout results are reported and do not guarantee a lock or block the save. Inspect each result.
	 * @param Assets Required comma- or pipe-separated asset paths or unique short names, e.g. /Game/Tests/BP_NodeTest. Empty lists and lists over 25 entries are refused. This is an explicit save list, not a dirty-asset filter.
	 */
	UFUNCTION(meta = (AICallable))
	static FString EditTurboMCPSave(const FString& Assets);

	/**
	 * Write named asset packages directly to disk without refreshing thumbnails; does not explicitly compile.
	 * Skips the editor thumbnail draw, but package serialization still runs engine and asset save callbacks. This is not a sandbox or an undoable graph edit. Existing thumbnails may remain stale. Mounted package filenames must resolve successfully.
	 * Refuses during play, simulation or an active editor transaction. A refusal does not undo earlier edits or queue a save. Stop play and inspect ReadTurboMCPDirtyList before compiling and saving wanted changes. Saves each asset separately with one retry on failure; earlier writes are not rolled back. Source-control checkout results are reported, not a guarantee that a lock was acquired.
	 * @param Assets Required comma- or pipe-separated asset paths or unique short names, e.g. /Game/Tests/BP_NodeTest. Empty lists and lists over 25 entries are refused. Explicitly saves even packages whose dirty flag is clear.
	 */
	UFUNCTION(meta = (AICallable))
	static FString EditTurboMCPSaveWithoutThumbnail(const FString& Assets);

	/** Create a new user-defined enum in memory in an idle editor outside PIE and active transactions; marks its package dirty and may run enum callbacks, with no tool-owned undo transaction, explicit Blueprint compile or save.
     * Existing assets are not overwritten. Preview does not guarantee creation or that the engine will accept every display name; edit reads names, numeric values and requested display names back. Failed creation may leave an unsaved asset for inspection.
     * @param Path Required new package path including the asset name, for example /Game/Enums/E_Colour; do not supply an object suffix or filesystem extension. Surrounding whitespace is trimmed; empty and relative paths are refused. Use a writable content mount.
     * @param Values Required comma- or pipe-separated entries in Name or Name=Display Name format, for example Red,Green=Dark Green,Blue. Names start with a letter or underscore and contain only letters, digits or underscores; duplicates ignoring case and _MAX are refused. Values receive consecutive integers starting at zero. Empty input is refused; empty separators are skipped. Empty display text uses the engine-generated display name. Commas and pipes cannot be escaped inside display text.
     */
	UFUNCTION(meta = (AICallable))
	static FString EditTurboMCPEnumAdd(const FString& Path, const FString& Values);

	/**
	 * Add Blueprint variables; may regenerate the skeleton repeatedly and run editor callbacks, can leave partial changes without rollback, and does not perform a full compile or save or create a dedicated undo transaction.
	 * Preview does not resolve the target Blueprint or check existing names. Edit performs those checks; preview acceptance does not guarantee edit success. Defaults are validated as Unreal property text before adding any variables; compile separately before saving.
	 * @param Asset Required Blueprint name or package/object path, e.g. /Game/Tests/BP_NodeTest; empty is refused. Preview only retains this identifier and does not check existing names; edit resolves it and checks conflicts.
	 * @param Variables Required comma-separated Name: Type declarations, e.g. Health: float default=100 category=Stats. Optional space-separated key=value fields: default (alias value), category, tooltip, private, readonly, instanceEditable, exposeOnSpawn. Flags default false and accept true/false, 1/0, yes/no, on/off. Other fields default empty; empty list is refused. Values cannot contain ungrouped spaces or commas. Types include basic scalars, named engine structs, enum/struct assets, object/class/softobject/softclass<ClassPath>, Array<T>, Set<T>, Map<K,V>; map values must be basic scalar types. Duplicate names/options are refused. Defaults are validated as Unreal property text before adding any variables; compile separately before saving.
	 */
	UFUNCTION(meta = (AICallable))
	static FString EditTurboMCPVarsAdd(const FString& Asset, const FString& Variables);
    /** Remove one unused Blueprint-owned member variable after reference inspection; never deletes graph nodes intentionally. Inspection loads dependent Blueprints and may run callbacks. Refuses referenced/inherited/generated variables and incomplete inspection. Dynamic/native/string references are outside proof. No save; structural refresh can compile dependencies and invalidate undo.
     * @param Asset Exact Blueprint asset path or unique name; prefer the full path.
     * @param Name Exact case-sensitive owned member name, not a local or widget-generated variable.
     * @param Confirm Copy the confirm token from preview. Missing or stale tokens refuse; references are checked again.
     */
    UFUNCTION(meta = (AICallable))
    static FString EditTurboMCPVariableRemove(const FString& Asset, const FString& Name, const FString& Confirm);

    /** Rename a member and supported getter/setter references using fresh confirmation; refreshes skeletons, so compile affected assets separately before saving. Preserves links, pin defaults, positions and OnRep association. Rechecks dependencies and defaults before editing. No save or checkout and no guaranteed undo across compilation. Unsupported cases refuse before mutation; see PreviewTurboMCPVariableRename.
     * @param Asset Same Blueprint asset used for preview.
     * @param Name Same exact current member name used for preview.
     * @param NewName Same new identifier used for preview.
     * @param Confirm Exact token from a fresh PreviewTurboMCPVariableRename result. Changed declarations, defaults, references or dependency coverage invalidate it.
     */
    UFUNCTION(meta = (AICallable))
    static FString EditTurboMCPVariableRename(const FString& Asset, const FString& Name, const FString& NewName, const FString& Confirm);

    /** Remove one unused local declaration from an exact owned function. Referenced locals refuse; no graph nodes are deleted. Preview loads the asset but does not edit it. Edit requires a fresh preview token. No save or checkout; structural refresh can compile and affect undo.
     * @param Asset Blueprint path or unique name; prefer the full path.
     * @param Function Exact owned function graph name. Event graphs and ambiguous names refuse.
     * @param Name Exact case-sensitive local variable name.
     * @param Confirm Copy the confirm token from a successful preview. Stale tokens refuse without edits.
     */
    UFUNCTION(meta = (AICallable))
    static FString EditTurboMCPLocalVariableRemove(const FString& Asset, const FString& Function, const FString& Name, const FString& Confirm);

    /** Rename one local variable and its exact local getter/setter references. Same-named members and other functions are unchanged. Preview lists affected nodes; edit requires fresh confirmation. No save or checkout. Structural refresh can compile and affect undo; inspect before saving.
     * @param Asset Blueprint asset path or unique name; prefer a full path.
     * @param Function Exact owned function graph name.
     * @param Name Exact current local declaration name.
     * @param NewName New identifier; collisions with function parameters or locals refuse.
     * @param Confirm Copy the confirm token from the latest preview. Stale tokens refuse without edits.
     */
    UFUNCTION(meta = (AICallable))
    static FString EditTurboMCPLocalVariableRename(const FString& Asset, const FString& Function, const FString& Name, const FString& NewName, const FString& Confirm);


    /**
     * Set one supported widget or parent-slot property in memory; returns before/after values read after native editor notification. Uses an undo transaction. Reference validation may load the named asset and native hooks may run. No compile or save is requested; rendered/runtime behavior is not verified.
     * @param Asset Required Widget Blueprint path or unique name; prefer /Game/UI/WBP_Menu.
     * @param Widget Required designer object name from ReadTurboMCPWidgetTree, not display text or a runtime instance path.
     * @param Target Required widget or slot. Slot means the named widget's parent slot; a root without a slot refuses.
     * @param Property Exact name or dotted struct-field path returned by ReadTurboMCPWidgetProperty, for example Visibility, LayoutData, Brush.ResourceObject or EntryWidgetClass. Paths cannot descend objects or containers.
     * @param Value Required Unreal property text for values, or None/exact Unreal object or generated-class path for a hard reference. Asset type and field metadata are checked before mutation. Other struct fields are preserved. Maximum 65536 characters. Requires an idle editor outside PIE.
     */
    UFUNCTION(meta = (AICallable))
    static FString EditTurboMCPWidgetProperty(const FString& Asset, const FString& Widget, const FString& Target, const FString& Property, const FString& Value);
    /**
     * Inspect or change explicit shared CommonButtonStyle Blueprint defaults.
     * @param Asset Required compiled CommonButtonStyle Blueprint path, not a widget. All buttons using this shared asset may change. No compile/save/runtime refresh is performed; recreate affected widgets after compiling to verify visuals.
     * @param Property Exact reflected path. Empty lists readable fields for read only. Examples: ButtonPadding or NormalHovered.TintColor.SpecifiedColor. Paths descend structs, never objects/containers. Edit conditions and color-use rules still apply. Native references and localized text are read-only.
     * @param Value Required Unreal property text, not JSON; up to 65536 characters. Example padding: (Left=8,Top=4,Right=8,Bottom=4). Example color: (R=0.2,G=0.5,B=0.8,A=1). Partial structs retain other fields. Preview first; edit reparses current state. Invalid/non-finite values refuse. Requires an idle editor outside PIE.
     */
    UFUNCTION(meta = (AICallable))
    static FString EditTurboMCPCommonButtonStyle(const FString& Asset, const FString& Property, const FString& Value);

    /**
     * Add one designer widget at the end of a named panel using Unreal's palette operation. Uses class defaults and a default parent slot.
     * Requires an idle editor outside PIE. Refuses missing/full parents, name conflicts and circular widget dependencies. Native construction/palette hooks and structural refresh may run on edit; loaded dependent Blueprints may be affected. No full compile or save is requested. Read back, configure layout separately and compile before saving.
     * @param Asset Required destination Widget Blueprint path or unique name; prefer /Game/UI/WBP_Menu.
     * @param Parent Required existing designer panel name from ReadTurboMCPWidgetTree, such as CardList. Empty does not select or create a root. Named-slot insertion is not supported.
     * @param WidgetClass Required compiled Widget Blueprint path/name, such as /Game/UI/WBP_Card, or concrete native class path such as /Script/UMG.TextBlock. Dirty source Blueprints must be compiled first.
     * @param Name Required unused identifier: 1 to 100 letters, digits or underscores, starting with a letter or underscore. Example: NewCard. Duplicate names refuse rather than adding a numbered copy.
     */
    UFUNCTION(meta = (AICallable))
    static FString EditTurboMCPWidgetAdd(const FString& Asset, const FString& Parent, const FString& WidgetClass, const FString& Name);

    /**
     * Remove the widget subtree through the stock editor operation, with an undo transaction and structural refresh. No full compile or save is requested.
     * Refuses graph references, designer bindings, animation bindings and explicit navigation targets in this Blueprint. External assets, dynamic string lookups and custom reference systems are not verified. Removing the root leaves an empty tree. Edit checks current state again; loaded dependent Blueprints may be refreshed. Compile and inspect before saving.
     * @param Asset Required Widget Blueprint path or unique name; prefer /Game/UI/WBP_Menu. Requires an idle editor outside PIE and transactions.
     * @param Widget Required exact designer object name from ReadTurboMCPWidgetTree. Includes its descendants in this tree, not the internals of embedded UserWidgets. Missing names refuse. Example: ObsoleteCard. Preview lists every affected name.
     */
    UFUNCTION(meta = (AICallable))
    static FString EditTurboMCPWidgetRemove(const FString& Asset, const FString& Widget);

    /**
     * Rename an existing designer widget using Unreal's undoable editor operation. Requires an idle editor outside PIE.
     * Refreshes graph/designer references handled by Unreal, including bindings, animation names and navigation. Arbitrary string lookups and external assets are not verified. Structural refresh may affect loaded dependent Blueprints. No full compile or save is requested; inspect and compile before saving.
     * @param Asset Required Widget Blueprint path or unique name; prefer /Game/UI/WBP_Menu.
     * @param Widget Required existing designer object name from ReadTurboMCPWidgetTree, not display text or a PIE instance path. Matching ignores case.
     * @param NewName Required unused identifier: 1 to 100 letters, digits or underscores, starting with a letter or underscore. Whitespace is trimmed; other invalid characters and member/name collisions refuse. Same name is a no-op; case-only renames are not supported. Example: PurchaseButton.
     */
    UFUNCTION(meta = (AICallable))
    static FString EditTurboMCPWidgetRename(const FString& Asset, const FString& Widget, const FString& NewName);

	/**
	 * Set named widgets' Is Variable flags; changes may regenerate the Blueprint skeleton and run editor callbacks, do not perform a full compile or save, and have no dedicated undo transaction.
	 * Resolves all names before changing flags. Existing requested values are not counted as changes; names are deduplicated ignoring case.
	 * Disabling is refused for the whole request if self-variable getter/setter nodes in this Blueprint still reference a selected widget. External/dynamic references are not checked. Repair references first.
	 * This exposes existing widgets to graph logic; it does not create widgets, change appearance or connect nodes.
	 * Review the result, then use EditTurboMCPCompile and EditTurboMCPSave as needed.
	 * @param Asset Required Widget Blueprint name or package/object path, for example /Game/UI/WBP_Menu; empty is refused.
	 * @param Widgets Required widget names separated by commas or pipes, for example TitleText,BodyText; whitespace is trimmed, duplicates are ignored, and empty never means all widgets.
	 * @param bIsVariable Required boolean: true exposes the named widgets as variables; false removes that flag. No default is assumed.
	 */
	UFUNCTION(meta = (AICallable))
	static FString EditTurboMCPWidgetsAsVars(
		const FString& Asset,
		const FString& Widgets,
		bool bIsVariable);

	/**
	 * Create an event or function override through the editor API; may regenerate the skeleton and run editor callbacks, uses engine-managed undo, and does not perform a full compile or save.
	 * Existing implementations return unchanged without creating a duplicate. Preview is not a reservation; edit checks again.
	 * Creates only the override entry, not its behavior. Event results include the exact node ID and graph. Connect the body, inspect parent-call requirements and tidy affected nodes before compiling.
	 * @param Asset Required Blueprint name or package/object path, for example /Game/Tests/BP_NodeTest; empty is refused.
	 * @param FunctionOrEvent Required reflected name from ReadTurboMCPOverrides, for example ReceiveActorOnClicked; case-insensitive with surrounding whitespace trimmed. Empty or unavailable names are refused; no default.
	 */
	UFUNCTION(meta = (AICallable))
	static FString EditTurboMCPOverrideAdd(
		const FString& Asset,
		const FString& FunctionOrEvent);

	/**
	 * Delete an override event node or an entire override function graph and its contents; uses an undo transaction, may regenerate the skeleton and run editor callbacks, and does not perform a full compile or save.
	 * Requires an idle editor with undo available. Event removal reports the exact removed node and detached connections. Other nodes remain; inspect their remaining uses before explicit GraphRemove cleanup. Function removal can close asset editors.
	 * Refuses ambiguous matches and local functions. Review the Blueprint after removal; this tool does not repair callers or prove compilation succeeds.
	 * @param Asset Required Blueprint name or package/object path, for example /Game/Tests/BP_NodeTest; empty or unresolved values are refused.
	 * @param FunctionOrEvent Required reflected override name from ReadTurboMCPOverrides with include=overridden, for example ReceiveActorOnClicked; case-insensitive, surrounding whitespace trimmed, no default. Empty is refused.
	 */
	UFUNCTION(meta = (AICallable))
	static FString EditTurboMCPOverrideRemove(
		const FString& Asset,
		const FString& FunctionOrEvent);

	/**
	 * Create a function graph and optional signature; loads types, regenerates the skeleton and refreshes matching self-call nodes, may run editor callbacks, and does not perform a full compile or save or provide a dedicated undo transaction.
	 * Requires an idle editor outside PIE. Reports lost caller links as partial changes; does not automatically roll them back.
	 * @param Asset Required Blueprint name or package/object path, for example /Game/Tests/BP_NodeTest; empty or unresolved values are refused.
	 * @param Name Required new identifier, for example ComputeScore; starts with a letter or underscore, then letters, digits or underscores. Empty, None, existing and parent function names are refused; whitespace is trimmed.
	 * @param Inputs Optional comma-separated Name: Type declarations, for example Score: int, Enabled: bool. Empty/default creates no inputs. Names must be unique across inputs/outputs. Types: bool, byte, int, int64, float, real, double, name, string, text; vector, rotator, transform, vector2d, linearcolor, color; loadable class/struct/enum paths; array<T>, set<T>, map<K,V>; object/class/softobject/softclass/interface<ClassPath>. No nested containers; set elements/map keys must be hashable.
	 * @param Outputs Optional comma-separated Name: Type declarations, for example Result: float. Empty/default creates no outputs. Uses the same type syntax and name rules as Inputs; no parameter default-value syntax is supported here.
	 */
	UFUNCTION(meta = (AICallable))
	static FString EditTurboMCPFunctionAdd(const FString& Asset, const FString& Name,
		const FString& Inputs = TEXT(""), const FString& Outputs = TEXT(""));

	/**
	 * Delete a function graph and all its nodes without repairing callers or bindings; no full compile, save or dedicated undo transaction, and editor callbacks may close asset editors and regenerate the skeleton.
	 * Requires an idle editor outside PIE. Does not repair callers or bindings. Removing the graph does not guarantee its name is immediately reusable; inspect the result before recreating it.
	 * @param Asset Required Blueprint name or package/object path, for example /Game/Tests/BP_NodeTest; empty or unresolved values are refused.
	 * @param Name Required function graph name, for example ComputeScore; case-insensitive with surrounding whitespace trimmed. Empty or ambiguous names are refused; no default. A missing name returns unchanged and lists available functions.
	 */
	UFUNCTION(meta = (AICallable))
	static FString EditTurboMCPFunctionRemove(const FString& Asset, const FString& Name);

	/**
	 * Remove bindings whose widget or delegate is missing without validating function signatures; changes stay in memory with no compile, save or dedicated undo transaction.
	 * Rechecks bindings before removal and reports remaining delegate failures. Keeps other bindings and leaves reported call nodes unchanged. Does not validate bound functions or signatures.
	 * @param Asset Required Widget Blueprint path or unique name, e.g. /Game/Tests/WBP_NodeTest. Empty, unresolved and non-Widget Blueprint assets are refused.
	 */
	UFUNCTION(meta = (AICallable))
	static FString EditTurboMCPWidgetRetarget(const FString& Asset);

	/**
     * Add or replace a single-cast designer binding after checking the reflected function signature and property purity. No full compile, save or dedicated undo transaction.
     * Invalid targets leave existing bindings intact. Identical requests return unchanged. Multicast events require graph binding tools instead.
     * @param Asset Existing Widget Blueprint path or unique name, for example /Game/UI/WBP_Menu.
     * @param Widget Existing widget name from ReadTurboMCPWidgetTree; matching ignores case.
     * @param Property Bindable property or single-cast delegate, for example Text or TextDelegate. Property aliases normalize to Text. Multicast OnClicked is refused.
     * @param Function Existing reflected function, for example GetPurchaseText. Property getters must be pure or const and signature-compatible. Empty selects WidgetName_OnPropertyName but does not create that function. Compile first if reflection is stale.
     */
	UFUNCTION(meta = (AICallable))
	static FString EditTurboMCPWidgetBindAdd(
		const FString& Asset,
		const FString& Widget,
		const FString& Property,
		const FString& Function);

	/**
     * Create, change, resize or delete comment boxes in an undo transaction; loads/measures the graph and does not compile or save.
     * Deleting a box keeps its nodes. Explicit fit expands outward to preserve the old rectangle; fit without Nodes fits current contents. refit requires Box and Nodes and moves/resizes only the box around that explicit selection; unselected nodes remain in place and may be left outside. Inspect preview geometry before applying.
     * @param Asset Required Blueprint path or unique short name, e.g. /Game/Tests/BP_NodeTest. Empty or ambiguous input is refused.
     * @param GraphName Required existing graph name, e.g. EventGraph. Use the reader's name; missing targets are refused.
     * @param Action Required create, set, fit, refit or delete; case-insensitive and trimmed. Empty/unknown actions refuse.
     * @param Nodes Comma- or pipe-separated reader node IDs. Required for create; for fit selects nodes to enclose and requires Box. Empty fit uses current box contents. Unused for set/delete; pass empty.
     * @param Box Existing comment ID for set/delete. For fit, empty with empty Nodes selects all boxes; nonempty selects one box. Explicit fit with Nodes requires one Box. refit always requires one Box and explicit Nodes. Unused for create; pass empty.
     * @param Text Comment label for create/set; empty means no supplied label on create or leave unchanged on set, not clear. Whitespace is retained. Unused for fit/delete; pass empty. Example: Input handling.
     * @param Color Optional #RRGGBB or comma-separated RGB values 0..255, e.g. #3A6EA5. Empty uses the new box default or preserves an existing color. Used by create/set; otherwise pass empty.
     * @param Size Optional width x height in graph units, e.g. 400x200. Each side must be finite numeric text in 1..2147483647; fractional parts truncate. Empty create encloses selected nodes; empty set keeps size. Explicit create size overrides automatic enclosure. Automatic height includes wrapped-title clearance; new boxes have no floating comment bubble. Used by create/set; otherwise pass empty.
     */
    UFUNCTION(meta = (AICallable))
	static FString EditTurboMCPComment(
		const FString& Asset,
		const FString& GraphName,
		const FString& Action,
		const FString& Nodes,
		const FString& Box,
		const FString& Text = TEXT(""),
		const FString& Color = TEXT(""),
		const FString& Size = TEXT(""));

	/**
	 * Delete statically selected node groups and their links in one undo transaction; requires an idle editor outside PIE; does not compile or save.
	 * Selection uses recognized entries and variable/call heuristics, not proof that nodes are unwanted or behaviorally irrelevant. Inspect the preview before editing; edit recomputes candidates.
	 * @param Asset Required Blueprint path or unique short name, e.g. /Game/Tests/BP_NodeTest. Empty or ambiguous values are refused; prefer a full path.
	 * @param GraphName Required event, function or macro graph name; case-insensitive and trimmed. Empty selects EventGraph or the first available graph. Example: EventGraph. Specify a name to avoid fallback selection.
	 */
	UFUNCTION(meta = (AICallable))
	static FString EditTurboMCPGraphDead(const FString& Asset, const FString& GraphName);

	/**
	 * Insert planned reroutes in one undo transaction without moving existing nodes; requires an idle editor and explicit graph; partial results require inspection before saving; no automatic compile or save.
	 * Tries bounded forward routes, shares a knot when all affected destinations fit, and skips existing reroutes, nearby nodes and blocked positions; not a full graph tidy.
	 * Edit replans against current state. Rollback does not verify full graph restoration; inspect partial results before saving.
	 * @param Asset Required Blueprint path or unique short name, e.g. /Game/Tests/BP_NodeTest. Empty or ambiguous input is refused; use a full path when possible.
	 * @param GraphName Exact existing Blueprint graph name, case-insensitive with surrounding whitespace ignored; empty, missing or ambiguous names are refused. Example: EventGraph.
	 */
	UFUNCTION(meta = (AICallable))
	
static FString EditTurboMCPGraphKnots(const FString& Asset, const FString& GraphName);

	/**
     * Recompute and apply a layout using the same options as PreviewTurboMCPGraphTidy.
     * A previous preview is not a reservation: the current graph is checked again before editing.
     * Reports measured results and may duplicate variable getters. Rows/scoped layouts preserve comment corners; whole-graph blocks mode refits comments around their original members.
     * Within safety limits, structural quality takes priority over total wire length. Force permits a non-improving score,
     * but does not bypass scope, collision or fixed-boundary wire-growth limits. Changes are undoable and are not compiled or saved.
     * Scoped groups are placed near their fixed connections. Boundary growth is checked before and after editing.
     * Each boundary wire may grow by at most its larger endpoint width clamped to 64..256 graph units;
     * aggregate boundary growth may not exceed the largest such allowance. Rejected edits are rolled back.
     * NodeIds uses the same fixed-anchor scope as the preview.
	 * Include "@unboxed" to leave boxed nodes fixed (alone: all unboxed; with IDs: only unboxed IDs).
	 * WrapAfter is the same comma-separated execution-node row-break option as the preview; links remain unchanged.
	 *
	 * @param Asset Existing Blueprint path or unique short name, e.g. /Game/Tests/BP_NodeTest. Use a full path to disambiguate.
	 * @param GraphName Existing graph name from the reader, including interface implementation and nested graphs. Matching is case-insensitive after trimming. Empty selects a unique EventGraph, or the only graph; missing or ambiguous names are refused.
	 * @param bForce Required boolean. false keeps quality-based acceptance; true permits the proposed layout even when its score does not improve. Does not bypass scope, collision or fixed-boundary wire-growth limits.
	 * @param NodeIds Array of reader node IDs; [] selects the whole graph. Unselected nodes remain fixed. ["@unboxed"] selects unboxed nodes; combine it with IDs to restrict those IDs to unboxed nodes.
	 * @param MainPath Empty uses normal layout. auto chooses the longest acyclic execution path; otherwise comma-separated connected execution node IDs in desired order. Unsupported cycles are refused in auto mode. With NodeIds selection, only the first explicit path node may be outside the selection: it stays fixed and must connect to the next selected node. All later path nodes must be selected.
	 * @param HorizontalGap Finite horizontal clearance in graph units, 40..2000; default 120. Example: 160.
	 * @param VerticalGap Finite vertical clearance in graph units, 40..2000; default 100. Example: 120.
	 * @param LanePitch Finite minimum Sequence-row spacing in graph units, 100..10000; default 280. Rows expand when needed for tall trees.
	 * @param LayoutMode Optional rows (default, existing layout) or blocks (compact control-flow placement). Blocks keeps loop Body on its row and stacks continuations below; whole-graph blocks mode may move and resize existing comments around their original members. CreateBoxes optionally adds nested block comments; locals remain a separate explicit operation. Reroutes require explicit RouteWires=orthogonal.
	 * @param RouteWires off (default) preserves existing connections. orthogonal plans measured reroute chains for blocked or long diagonal connections (at most 512 nodes and 4096 logical wires); skipped boundary/unrouteable links are reported. No automatic compile or save.
	 * @param bCreateBoxes Optional false (default) or true. Requires whole-graph blocks mode and at most 512 nodes; plans nested comments for control-flow blocks, preserving existing titles and refusing partial overlaps. Added comments share the layout undo.
	 * @param ChannelWidth Finite routing channel clearance in graph units, 20..400; default 60. Used only when RouteWires=orthogonal.
	 * @param WrapAfter Empty means no requested row breaks. Otherwise comma-separated unique execution node IDs within scope, each with a continuation. Following execution starts a new row. Logical connections are preserved; explicit routing may insert reroute nodes.
	 * Node references accept exact reader IDs or unique hexadecimal keys of at least six digits in the named graph. Ambiguous keys and mismatched qualified IDs are refused.
     * Stock ForLoopWithBreak/ForEachLoopWithBreak Break and DoOnce Reset wires are preserved as feedback, not forward layout dependencies. Other cycles in the selected dependencies still refuse. Unselected nodes remain fixed; unrelated fixed cycles do not prevent scoped tidy.
     */
	UFUNCTION(meta = (AICallable))
	static FString EditTurboMCPGraphTidy(
		const FString& Asset,
		const FString& GraphName,
		bool bForce,
		const TArray<FString>& NodeIds,
		const FString& MainPath = TEXT(""), double HorizontalGap = 120.0, double VerticalGap = 100.0, double LanePitch = 280.0, const FString& WrapAfter = TEXT(""), const FString& LayoutMode = TEXT("rows"), const FString& RouteWires = TEXT("off"), double ChannelWidth = 60.0, bool bCreateBoxes = false);

	/**
     * Replan and apply supported position repairs across one Blueprint graph in one undo transaction; marks the Blueprint modified, but does not compile or save.
     * Repairs selected backward execution patterns, exclusive pure chains, misplaced getters and linked origin strays. This is not a complete tidy or proof of graph correctness. Nodes and links are retained; comment boxes are not moved or resized.
     * @param Asset Required Blueprint package/object path or unique short name, e.g. /Game/Tests/BP_NodeTest. Prefer a full path. Loading may run editor callbacks; an idle editor and measured node geometry are required.
     * @param GraphName Required unique graph name from the reader, e.g. EventGraph. Empty, missing or ambiguous names are refused. The whole named graph is considered; there is no node-selection parameter. Exclusive pure chains are limited to 8 nodes; a chain that cannot fit safely is not partially carried.
     * @param bForce Required boolean. false writes only when estimated straight-segment wire length decreases; true permits an equal or longer result and reports it. Force never bypasses identity, collision, comment or coordinate guards, and a no-op stays unchanged. Preview first; edit replans current state. No automatic compilation or saving.
     */
	UFUNCTION(meta = (AICallable))
	static FString EditTurboMCPGraphFixBackward(
		const FString& Asset,
		const FString& GraphName,
		bool bForce);

	/** Promote reviewed scalar arithmetic or explicitly selected immutable input-array counts at entry, and selected stable stock loop indexes at LoopBody, retaining original calculations. Replans live state, compiles and verifies links; does not save. Requires a clean compile baseline and a single assignment continuation. Any selected excluded candidate refuses the whole edit. Unproven array-element, reference, latent or reentrant lifetimes are refused. Failure attempts owned undo but does not claim verified restoration.
     * Pure-node classification is not proof of deterministic behavior or equivalent execution.
     * @param Asset Required Blueprint asset path or unique asset name, for example /Game/Blueprints/BP_Player; surrounding whitespace is trimmed and empty is refused. Prefer a full path to avoid ambiguity.
     * @param GraphName Function graph name, matched without case sensitivity after trimming whitespace; event graphs are refused. Pass empty to select the first function graph in the Blueprint's stored order; the result names the selected graph. Prefer an explicit name.
     * @param Names JSON object mapping exact output IDs from the local-variable preview to meaningful local names, for example {"<exact output ID>":"Subtotal"}. A nonempty map selects only its exact output IDs. Default {} preserves automatic selection of reviewed entry-stable arithmetic; input-array counts and loop values always require explicit selection. Unknown IDs, invalid names and name collisions are refused. Use the same mapping for preview and edit.
     */
	UFUNCTION(meta = (AICallable))
	static FString EditTurboMCPLocalVars(const FString& Asset, const FString& GraphName, const FString& Names = TEXT("{}"));

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
     * Add nodes and connections without deleting existing nodes; Placement=local searches nearby; Placement=outside uses space beyond existing bounds and may create long wires.
     * Explicit links to occupied inputs can replace their current connections.
     * Does not change function signatures, compile, save or prove runtime behavior.
     * Preview validates a temporary graph; edit replans against the current graph.
     * A successful preview is not a reservation or a guarantee of later success.
     * Live edits use a transaction. Failures attempt undo and report whether it
     * completed; complete restoration is unverified, so inspect before retrying.
     * Success verifies created nodes and requested links, and reports their IDs.
     * Repeating new node forms creates another set of nodes; this is not idempotent.
     * Local labels name the PRECEDING node form. Prefer `(TypeId) ; label Name` for a pinless node,
     * or put `; label Name` on its own line after the closed form. Never put a label before its node.
     * A multiline form may end with `Value 1) ; label Name`; one node cannot have conflicting labels.
     * Quote creation IDs containing spaces: `("Exact Type ID")`. Use flat node forms, not nested reader values.
     * Flat example: `(TypeId` then `InputPin 1) ; label ValueNode`, followed by a separate consumer form
     * containing `DestinationInput (-> ValueNode:OutputPin)`. Substitute discovered type IDs and pin names.
     * Do not write `(TypeId ; label Name)` on one line: the comment hides its closing parenthesis.
     * Every link is written on its destination INPUT: InputPin (-> SourceId:OutputPin).
     * Whitespace-only pin names are quoted exactly as shown by NodePins: `" " (-> Source:" ")`.
     * Preserve the spaces inside those quotes; they are engine pin names, not missing values.
     * To feed an existing node, copy its reader form header with its node ID, then add that input link.
     * Do not write a reverse link on a source output pin. Discover creation IDs with PreviewTurboMCPNodeType;
     * use PreviewTurboMCPNodePins for exact pin names. Native engine search IDs are not interchangeable.
     * In a function graph, use Function|Result for another Return; link execute from a reachable branch and set or link its result inputs.
     * Reader branch headings (:then/:else) are read-only and are refused. They do not create wires.
     * Instead, label the Branch Choice and write `execute (-> Choice:then)` inside the destination
     * node form (or Choice:else for the false output). Use the exact pin names from discovery.
     * Reference it as `(-> Name:ExactPinName)`. A pinless form needs no dummy pin. Semicolon comments
     * consume the rest of their line: put the closing parenthesis BEFORE a trailing label/comment.
	 *
	 * Continuation: auto (legacy unique exit), all (all reachable new unconnected exec exits), or label:pin (one named new node output). Applies to displaced existing exec wires; explicit submitted target links take precedence. Compile/save deferred.
	 *
	 * @param Asset Existing Blueprint path or unique short name, e.g. /Game/Tests/BP_NodeTest. Use a full path when names are ambiguous.
	 * @param GraphName Exact existing graph name, e.g. EventGraph. Use the reader to select the intended graph; do not guess.
	 * @param Text Nonempty append graph text. New forms create nodes; reader-derived IDs refer to existing nodes. Use Preview first and preserve exact type IDs and pin names. Partial reader pages are not standalone append input. Engine-created companion nodes are reported with IDs. Explicit delegate wiring removes only newly generated events left completely disconnected; existing events are preserved.
	 * Connected append placement searches nearby space above and below fixed connections. Existing nodes stay fixed. If no local slot fits, free space or submit a smaller connected block. Tight gaps may require bent wires; this is not whole-graph tidy.
	 *
	 * @param Placement local (default or empty) searches nearby space. outside places new nodes beyond existing graph bounds while keeping existing nodes fixed; boundary wires may be long. Preview and inspect before compiling/saving. Unknown values refuse.
	 * @param Continuation auto (default or empty), all, or label:pin. Controls reconnection of displaced execution wires to new exits; explicit target links take precedence, so Continuation is skipped for those targets and adds no second execution path. Remove the explicit target link if you want Continuation to reconnect it. Example: Done:then for a submitted local label.
	*/
	UFUNCTION(meta = (AICallable))
	static FString EditTurboMCPGraphAppend(
		const FString& Asset,
		const FString& GraphName,
		const FString& Text,
        const FString& Continuation = TEXT("auto"),
        const FString& Placement = TEXT("local"));

	/**
     * Replace graph content and delete omitted eligible nodes in one undo transaction; loads the Blueprint, but does not compile or save.
     * Removal considers the named graph, not just one execution branch. Entry/result/event nodes and comments are protected. Inspect the removal list before editing; successful preview does not guarantee a later edit.
     * @param Asset Required Blueprint path or unique short name, e.g. /Game/Tests/BP_NodeTest. Empty or ambiguous values are refused; prefer a full path.
     * @param GraphName Required target graph name, e.g. EventGraph. Use the reader's exact graph name; missing or ambiguous targets are refused.
     * @param Text Required nonempty graph text using the append grammar. Existing reader IDs identify nodes to retain; eligible nodes omitted from the text are deleted. Preserve exact type IDs and pin names. Use a complete ReadTurboMCPGraphLoad result as reference, not as an automatically writable payload. Convert the intended graph to append node forms: exact type ID, input pin lines, and explicit (-> NodeId:OutputPin) links. Keep existing node ID comments to retain those nodes. Reader branch headings such as :then/:else do not create execution links; express each destination execute input explicitly. Never use a truncated page as the replacement; preview and inspect every removal first.
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

	/**
     * Move selected nodes and comment contents in one undo transaction; loads and measures the graph, marks the Blueprint modified, and does not compile or save.
     * Requires an idle editor and usable node bounds. Selection includes nodes fully contained in selected comments. Collision checks use node rectangles; estimated wire lengths do not prove rendered layout quality.
     * @param Asset Required Blueprint path or unique short name, e.g. /Game/Tests/BP_NodeTest. Empty or ambiguous input is refused; prefer a full path.
     * @param GraphName Required unique graph name, e.g. EventGraph. Empty, missing or ambiguous names are refused; copy the name from the graph reader.
     * @param NodeIds Required nonempty array of exact full or short reader node IDs. IDs are case-sensitive and not trimmed; duplicates select the same node once. Comment IDs include their fully contained nodes and nested boxes. Example: ["/Game/Tests/BP_NodeTest.BP_NodeTest:EventGraph:abcdef"].
     * @param DeltaX Required signed 32-bit integer offset in graph units; positive moves right, negative left. Zero leaves X unchanged. Coordinate overflow is refused. Example: 128.
     * @param DeltaY Required signed 32-bit integer offset in graph units; positive moves down, negative up. Zero leaves Y unchanged. Coordinate overflow is refused. Example: -64.
     * @param DestinationBox Exact comment node ID, or empty to retain external comment membership. Nonempty permits only unboxed nodes to enter that comment and its containing ancestors; the selection must fit its padded interior without resizing. Use the ID returned by PreviewTurboMCPGraphGroupPlacement.
     */
    UFUNCTION(meta = (AICallable))
	static FString EditTurboMCPGraphGroupMove(const FString& Asset, const FString& GraphName, const TArray<FString>& NodeIds, int32 DeltaX, int32 DeltaY, const FString& DestinationBox = TEXT(""));

	/**
	 * Reload a saved Blueprint or data-asset package, discarding ALL unsaved changes approved by preview; never saves. Memory or disk changes after preview invalidate confirmation. Refuses open asset editors and detected live-world references. May reinstance references and reset global undo history. Re-read asset IDs, values and references after reload. A failed verification reports unknown change state; inspect it before further edits.
	 * @param Asset Required saved Blueprint or data-asset path, or unique short name. Prefer an exact path such as /Game/Tests/BP_NodeTest. Empty, ambiguous, unsupported or unsaved assets are refused.
	 * @param Confirm Required exact DISCARD confirmation string from the latest PreviewTurboMCPAssetRevert for this asset. Expires after 60 seconds and is consumed before reload. Empty, expired, replaced or stale tokens are refused; preview again. Do not construct a token yourself.
	 */
	UFUNCTION(meta = (AICallable))
	static FString EditTurboMCPAssetRevert(const FString& Asset, const FString& Confirm);

	/**
	 * Set exact node positions in one undo step; not tidy and overlaps are allowed; moving a comment moves only its box, not its contents; requires an idle editor; no compile or save.
	 * Positions is a STRING containing a JSON array, not lines of text or a JSON object.
	 * Example: [{"node":"exact reader node ID","x":100,"y":200}]
	 * Each object must contain exactly node, x, y (case-sensitive); not id or nodeId.
	 * Use IDs from LookTurboMCPGraphPositions or the graph reader, not display titles.
	 * x and y are absolute integer graph coordinates, including negative coordinates,
	 * in the signed 32-bit range. Supply 1..4096 unique nodes; omit unchanged nodes.
	 * Validates the entire batch before writing; unlisted nodes stay in place.
	 * Numeric strings and booleans are refused. An already matching batch is a clean no-op.
	 * This is exact manual placement, not tidy: overlaps and backward wires are permitted.
	 * Moving a comment moves only its box, not its contents. Include contained nodes explicitly
	 * or use GraphGroupMove for a rigid group move. Links, defaults and sizes are unchanged.
	 * Requires an idle editor outside PIE and active transactions.
	 * @param Positions JSON array encoded as a string, with 1..4096 unique node IDs and integer x/y values in signed 32-bit range; booleans and numeric strings are refused; validate all entries before writing and omit unlisted nodes from movement; matching coordinates are a no-op. Example: [{"node":"exact reader node ID","x":100,"y":200}]
	 *
	 * @param Asset Existing Blueprint path or unique short name, e.g. /Game/Tests/BP_NodeTest. Use a full path when names are ambiguous.
	 * @param GraphName Exact existing graph name, e.g. EventGraph. Use the reader to select the intended graph; do not guess.
	* Node references accept exact reader IDs or unique hexadecimal keys of at least six digits in the named graph. Ambiguous keys and mismatched qualified IDs are refused.
     * Reports requested, moved and already_matching counts; nodes already at their requested position are successful no-ops.
     */
	UFUNCTION(meta = (AICallable))
	static FString EditTurboMCPGraphPositions(const FString& Asset, const FString& GraphName, const FString& Positions);

	/** Replace one call node and transfer its compatible links and unconnected input defaults. KeepOld=true deliberately retains the old node disconnected; false removes it after transfer checks. Replans against current state; preview first. A failure after mutation attempts undo but reports restoration as unverified. No automatic compile, save or tidy. Use the returned new ID for later calls; the deleted old ID is no longer valid.
	 * @param Asset Existing Blueprint path or unique short name, e.g. /Game/Tests/BP_NodeTest. Use a full path when names are ambiguous.
	 * @param GraphName Exact existing graph name, e.g. EventGraph. Use the reader to select the intended graph; do not guess.
	 * @param NodeId Exact reader ID of the existing call node to replace; empty is invalid.
	 * @param ReplacementTypeId Exact replacement call type ID returned by node-type discovery for this graph. Do not use a display name or guessed C++ name.
	 * @param KeepOld true or yes (also empty) keeps the original node disconnected; false or no removes it after verification. Other values are refused.
	 * @param Options JSON object encoded as a string, or empty for exact-name pin matching. Optional target_self=true explicitly discards the old receiver link/default and calls the replacement on this Blueprint (instance functions only; receiver source nodes remain). It cannot be combined with a self pin mapping or new self input. Optional pin_map maps old pin names to new names; new_inputs maps new input names to objects with node and pin. Example: {"pin_map":{"Old":"New"}}. Unknown fields are refused.
	*/
	UFUNCTION(meta = (AICallable))
	static FString EditTurboMCPGraphSwapCall(
		const FString& Asset,
		const FString& GraphName,
		const FString& NodeId,
		const FString& ReplacementTypeId,
		const FString& KeepOld,
		const FString& Options = TEXT(""));

	/**
     * Move consumers to another output, optionally creating a target and deleting the source, in one undo transaction; does not compile or save.
     * Both selected pins are outputs. Every consumer of SourcePin is redirected; this does not connect an output to an input or transfer the source node's other inputs/defaults. Inspect the preview and read back the graph after editing.
     * @param Asset Required Blueprint path or unique short name, e.g. /Game/Tests/BP_NodeTest. Empty or ambiguous input is refused.
     * @param GraphName Required unique existing graph name, e.g. EventGraph. Empty, missing or ambiguous names are refused.
     * @param SourceNodeId Required source reader node ID in the named graph. Empty or unresolved IDs are refused.
     * @param SourcePin Required exact output pin name, case-sensitive and not trimmed. Must have linked consumers. Example: ReturnValue when exposed by the source node.
     * @param TargetNodeId Existing target reader node ID, or empty to create a node using TargetTypeId. Must differ from SourceNodeId. A nonempty ID takes precedence over TargetTypeId.
     * @param TargetTypeId Supported creation type ID from discovery; required only when TargetNodeId is empty. Empty is valid with an existing target. Copy the exact type ID; unsupported types are refused.
     * @param TargetPin Required exact output pin name on the existing or new target, case-sensitive and not trimmed. Must be compatible with every source consumer. Example: ReturnValue; this is not an execute input.
     * @param KeepSource Required boolean text: true/1 retains the source; false/0 requests deletion. Empty defaults to true; whitespace trimmed and true/false case-insensitive. Other values refuse. Deletion refuses protected nodes or links on other source pins.
     */
    UFUNCTION(meta = (AICallable))
	static FString EditTurboMCPGraphRepoint(
		const FString& Asset, const FString& GraphName, const FString& SourceNodeId,
		const FString& SourcePin, const FString& TargetNodeId, const FString& TargetTypeId,
		const FString& TargetPin, const FString& KeepSource);

	/**
     * Apply ordered repoints in one undo transaction, including requested source deletion; loads the Blueprint and does not compile or save.
     * All operations must target one asset and graph. Later plans see preceding simulated changes. Preview does not guarantee apply. Failed edits attempt owned undo and compare restored source state and the dirty flag; undo does not certify compilation. If restoration cannot be verified, further TurboMCP edit calls are blocked for the editor session. Inspect and recover the asset before restarting; do not save unverified changes.
     * @param Operations Required nonempty JSON array of operation objects, or an object containing only a lowercase operations array. Each entry requires string asset, graphName (or graph, not both), sourceNodeId, sourcePin, targetNodeId and targetPin. Both pins are outputs. targetNodeId may be empty with targetTypeId for creation. Optional id defaults to operation-N and must be unique; kind defaults to exact graphRepoint, the only supported kind. Optional keepSource accepts boolean or true/false/1/0 text; empty or omitted retains the source. Other fields are strings. Operation field names are case-insensitive; unknown or case-duplicate fields refuse. Empty input/array and malformed JSON refuse. Example with actual reader IDs substituted: [{"asset":"/Game/Tests/BP_NodeTest","graphName":"EventGraph","sourceNodeId":"source reader ID","sourcePin":"ReturnValue","targetNodeId":"target reader ID","targetPin":"ReturnValue","keepSource":true}]. Both nodes must expose the named outputs.
     */
    UFUNCTION(meta = (AICallable))
	static FString EditTurboMCPBatchEdit(const FString& Operations);

    /** Connect exact existing node/pin IDs. Data replaces the target input source; exec replaces only the named source output destination and preserves other incoming target paths. No automatic continuation or casts. Same scratch preview and one-undo edit; compile/save deferred. Preview first.
	 * @param Asset Existing Blueprint path or unique short name, e.g. /Game/Tests/BP_NodeTest. Use a full path when names are ambiguous.
	 * @param GraphName Exact existing graph name, e.g. EventGraph. Use the reader to select the intended graph; do not guess.
     * @param SourceNodeId Exact reader ID or unique hexadecimal node key of at least six digits in GraphName. Missing or ambiguous IDs are refused. Qualified IDs must match the named graph.
	 * @param SourcePin Exact output pin name from the reader, e.g. then. No automatic continuation is added.
     * @param TargetNodeId Exact reader ID or unique hexadecimal node key of at least six digits in GraphName. Missing or ambiguous IDs are refused. Qualified IDs must match the named graph.
	 * @param TargetPin Exact input pin name from the reader, e.g. execute. Data replaces this input source; execution replaces only the named source output destination, preserving other incoming paths.
	*/
    UFUNCTION(meta = (AICallable))
    static FString EditTurboMCPGraphConnect(const FString& Asset,const FString& GraphName,const FString& SourceNodeId,const FString& SourcePin,const FString& TargetNodeId,const FString& TargetPin);

    /** Remove exact nodes and links with scratch preview and one undo. Nodes is a JSON array of reader node IDs. Links is a JSON array of objects with sourceNodeId, sourcePin, targetNodeId, targetPin. Use [] for either unused list. Node deletion also removes incident links; other nodes/positions are preserved. No automatic reconnection, compile or save.
	 *
	 * @param Asset Existing Blueprint path or unique short name, e.g. /Game/Tests/BP_NodeTest. Use a full path to disambiguate.
	 * @param GraphName Exact existing graph name, e.g. EventGraph. Select it from the reader; do not guess.
	 * @param Nodes JSON array encoded as a string, e.g. ["exact reader node ID"]. Use [] when removing only links, not an empty string. Duplicate, missing and non-deletable nodes are refused. Nodes also accepts exact current UObject paths returned by the engine finder, only within the named graph. Paths can change after reload or reconstruction; prefer fresh reader IDs. This removes graph nodes, not assets.
	 * @param Links JSON array encoded as a string. Each object requires sourceNodeId, sourcePin, targetNodeId and targetPin for an existing link. Use [] when removing only nodes. Do not repeat links already removed by a selected node deletion.
	 * Node references accept exact reader IDs or unique hexadecimal keys of at least six digits in the named graph. Ambiguous keys and mismatched qualified IDs are refused.
     */
    UFUNCTION(meta = (AICallable))
    static FString EditTurboMCPGraphRemove(const FString& Asset,const FString& GraphName,const FString& Nodes,const FString& Links);

    /**
     * Connect an existing function through a Create Event node in one undo transaction; replaces the target input and marks the asset modified, with no compile or save.
     * Requires an idle editor. Preview exercises graph/schema callbacks on a copy; successful preview does not guarantee a later edit. Inspect the result and arrange new nodes before compiling and saving.
     * Repeating the identical compatible function/object binding is unchanged. Replaced source nodes are retained and listed for inspection. This creates a delegate value; the owning Bind or Timer node must execute for registration. Read back, tidy the affected nodes, inspect a capture, then compile and save explicitly.
     * @param Asset Required Blueprint path or unique short name, e.g. /Game/Tests/BP_NodeTest. Empty or ambiguous values are refused; prefer a full path.
     * @param GraphName Required unique K2 graph name from the reader, e.g. EventGraph. Empty, missing and ambiguous names are refused.
     * @param TargetNodeId Exact reader ID or unique hexadecimal node key of at least six digits in GraphName. Missing or ambiguous IDs are refused. Qualified IDs must match the named graph.
     * @param TargetPin Required exact delegate input pin name, case-sensitive and not trimmed. Existing input links are replaced. Empty, orphaned, ambiguous or non-delegate pins are refused; use the reader's pin name.
     * @param FunctionName Required existing function name on self or the chosen object class, e.g. HandleClicked. Uses Unreal name matching; whitespace is not trimmed for lookup. Empty, missing or delegate-incompatible functions are refused. Does not create the function.
     * @param ObjectNodeId Exact reader ID or unique hexadecimal node key of at least six digits in GraphName. Missing or ambiguous IDs are refused. Qualified IDs must match the named graph. Supply with ObjectPin; both empty selects self.
     * @param ObjectPin Exact object-reference output pin name, case-sensitive and not trimmed. Supply together with ObjectNodeId; both empty selects self. Example: ReturnValue when the named node actually exposes that object output.
     */
    UFUNCTION(meta = (AICallable))
    
    static FString EditTurboMCPBindFunction(const FString& Asset,const FString& GraphName,const FString& TargetNodeId,const FString& TargetPin,const FString& FunctionName,const FString& ObjectNodeId = TEXT(""),const FString& ObjectPin = TEXT(""));

    /** Edit growth of variadic pins on Make Array, Sequence or associative operators; does not shrink, compile or save.
     * Workflow: append a base node, read its ID, preview/grow, then connect new pins explicitly.
     * Existing pins and links are preserved. No shrink, compile or save. An equal target is a clean no-op.
     * @param Asset Blueprint asset path or unique name. Loading may execute project callbacks.
     * @param GraphName Exact graph name in the Blueprint.
     * @param NodeId Exact full reader ID of the existing supported node.
     * @param Count Desired total, decimal integer 1..32. Sequence counts output branches; array/operator counts inputs. Unsupported native limits refuse.
     */
    UFUNCTION(meta = (AICallable))
    static FString EditTurboMCPNodePinGrowth(const FString& Asset,const FString& GraphName,const FString& NodeId,const FString& Count);

    /** Edit flags on an existing owned member variable; preserves identity and graph references, without compile or save.
     * @param Asset Blueprint path or unique name. Asset loading may run project callbacks.
     * @param Name Exact case-sensitive owned member name; inherited/native/component/widget-generated variables refuse.
     * @param InstanceEditable Exact true/false; empty preserves. Controls per-instance editing, not Blueprint access.
     * @param ExposeOnSpawn Exact true/false; empty preserves. Compile separately to update generated spawn behavior; inspect diagnostics before saving.
     * @param Private Exact true/false; empty preserves. Blueprint access metadata, not a security boundary. External references are not validated. Specify at least one flag.
     */
    UFUNCTION(meta = (AICallable))
    static FString EditTurboMCPVariableFlags(const FString& Asset,const FString& Name,const FString& InstanceEditable,const FString& ExposeOnSpawn,const FString& Private);

    /** Edit a designer widget delegate event; returns pins and an existing/new node ID on edit, without compile or save.
     * @param Asset Widget Blueprint path or unique name. Loading can execute project callbacks. Requires idle editor and available undo for edits.
     * @param GraphName Exact owned K2 event graph name. Existing binding in another graph refuses; duplicate matching binding is a clean no-op returning its ID.
     * @param WidgetName Exact owned designer widget name with Is Variable enabled and matching compiled skeleton property. Compile explicitly first if the property is missing/stale. Inherited widgets refuse.
     * @param EventName Exact BlueprintAssignable multicast delegate name on the widget class, e.g. OnClicked for UButton; not a display label. Return/writable-output signatures refuse. Connect the new event body separately, then compile and save explicitly. This differs from BindNewEvent on an existing runtime delegate input.
     */
    UFUNCTION(meta = (AICallable))
    static FString EditTurboMCPWidgetEvent(const FString& Asset,const FString& GraphName,const FString& WidgetName,const FString& EventName);

    /** Edit Blueprint reparenting; execution refreshes nodes and compiles but never saves, and undo is not guaranteed across reinstancing.
     * @param Asset Exact Blueprint asset path. Normal actor/object/widget Blueprints only; loading may execute project callbacks.
     * @param NewParent Exact class object path, including _C for Blueprint-generated classes. Incompatible families, cycles and declaration conflicts refuse.
     * @param ExpectedParent Exact current parent class path returned by preview. A changed parent refuses before mutation.
     * @param Confirmation Exact confirmation text returned by preview, authorizing this asset/parent change and compilation. Compilation may execute project code and reinstance objects. Failure after mutation is reported as changed; no rollback or save is implied.
     */
    UFUNCTION(meta = (AICallable))
    static FString EditTurboMCPBlueprintReparent(const FString& Asset, const FString& NewParent, const FString& ExpectedParent, const FString& Confirmation);

    /** Edit the function selected by an existing Create Event node in place; preserves its identity, object input and consumers, with no compile or save.
     * @param Asset Blueprint path or unique asset name. Loading may execute project callbacks. Requires an idle editor outside PIE or compilation.
     * @param GraphName Exact unique K2 graph name from the reader.
     * @param NodeId Exact full or short reader ID for a Create Event node in GraphName. Missing, ambiguous or unconnected delegate signatures refuse.
     * @param FunctionName Existing compatible function on the node's actual object scope. Same function is a clean no-op. Does not change the object or register a runtime delegate; the consuming Bind or Timer node must execute. Unlike BindFunction, this keeps the existing node and all consumers.
     */
    UFUNCTION(meta = (AICallable))
    static FString EditTurboMCPDelegateRetarget(const FString& Asset,const FString& GraphName,const FString& NodeId,const FString& FunctionName);



    /**
     * Create a typed custom event wired to an existing delegate input in one undo transaction. Replaces that input connection; no compile or save. This does not create the Bind Event node or execute registration.
     * Requires an idle editor. Preview exercises graph/schema callbacks on a copy; successful preview does not guarantee a later edit. Inspect the result and arrange new nodes before compiling and saving.
     * @param Asset Required Blueprint path or unique short name, e.g. /Game/Tests/BP_NodeTest. Empty or ambiguous values are refused; prefer a full path.
     * @param GraphName Required unique graph name, e.g. EventGraph. Empty or unresolved names are refused.
     * @param TargetNodeId Exact reader ID or unique hexadecimal node key of at least six digits in GraphName. Missing or ambiguous IDs are refused. Qualified IDs must match the named graph.
     * @param TargetPin Required exact delegate input pin name, case-sensitive and not trimmed. Existing input links are replaced. Empty, orphaned, ambiguous or non-delegate pins are refused; use the reader's pin name.
     * @param EventName Required unused identifier containing letters, digits or underscores, not starting with a digit; no whitespace. Example: OnSelectionChanged. Existing function/event names are refused. Event graphs only; parameters come from the target delegate. Return/output-value signatures are refused. The new event has no execution body. Connect its body separately and ensure the target Bind node executes at runtime. Replaced source nodes are retained; inspect remaining uses before cleanup.
     */
    UFUNCTION(meta = (AICallable))
    static FString EditTurboMCPBindNewEvent(const FString& Asset,const FString& GraphName,const FString& TargetNodeId,const FString& TargetPin,const FString& EventName);




    /** Copy a shared pure value per connected input in one undo transaction; removes the original and marks the asset modified, without compile, save or automatic tidy.
     * Preserves defaults and existing source-input links. Zero or one connected input is unchanged. Supported sources are self, pure variable-get, object literals and whitelisted literal constructors; arbitrary pure functions, impure calls and split outputs are refused.
     * A consumer node using the source twice receives two copies. Copies are initially placed left of their consumers, may overlap and require layout. Read back and tidy the affected nodes, inspect a capture, then compile and save explicitly. Copying getters may change evaluation frequency; it is not a proof of equivalent project behavior.
     * @param Asset Required Blueprint path or unique short name, e.g. /Game/Tests/BP_NodeTest. Prefer a full path; empty or ambiguous assets are refused.
     * @param GraphName Required exact existing K2 graph name from the reader, e.g. EventGraph. Empty, missing and ambiguous names are refused.
     * @param NodeId Required exact reader ID or unique hexadecimal node key of at least six digits in the named graph. Missing or ambiguous IDs are refused. The source must have one unsplit data output and at most 4096 connected consumer inputs. Preview IDs for new copies are provisional; use edit-result IDs for subsequent calls. No compile or save is performed.
     */
    UFUNCTION(meta = (AICallable))
    static FString EditTurboMCPGraphSplitValue(const FString& Asset,const FString& GraphName,const FString& NodeId);

    /** Replace an ordered synchronous call chain with Sequence rows in one undo transaction; marks the asset modified but does not compile, save or tidy it. Rows run in order, followed by the original continuation on the final output; this is not parallel execution. Preview first, edit, read back, tidy the affected nodes, inspect a capture, then compile and save explicitly.
	 *
	 * @param Asset Existing Blueprint path or unique short name, e.g. /Game/Tests/BP_NodeTest. Use a full path to disambiguate.
	 * @param GraphName Required exact existing K2 graph name from the reader, e.g. EventGraph. Empty, missing and ambiguous names are refused.
	 * @param Groups JSON array encoded as a string: 2 to 64 nonempty arrays, at most 4096 exact reader node IDs in total, e.g. [["first ID","second ID"],["third ID"]]. Groups must form an ordered linear chain of synchronous calls; latent, branching and cyclic groups are refused.
	 * @param bSplitValues Optional boolean, default true. Copy supported pure self, variable-get and literal sources per selected consumer when true; unrelated values are untouched. false preserves shared sources. Splitting may create nodes; use the edit result IDs, not provisional preview IDs.
	 */
    UFUNCTION(meta = (AICallable))
    static FString EditTurboMCPGraphSequence(const FString& Asset,const FString& GraphName,const FString& Groups,bool bSplitValues = true);


    /** Set one unconnected K2 input pin default; records an undo transaction and marks the Blueprint dirty; schema callbacks may run and object literals may load assets; does not explicitly compile or save.
     * Validates the literal before editing and reads the pin back afterward. Successful changed/unchanged results identify the actual node, pin and normalized display value; display text is not serialized edit syntax. A failed read-back attempts to undo only this tool's transaction; restoration is not verified.
     * @param Asset Required Blueprint asset path or unique asset name accepted by the asset resolver; empty is invalid. Prefer a full path such as /Game/Blueprints/BP_Player.
     * @param GraphName Required graph name from the reader, such as EventGraph; matches the whole name without case sensitivity. Empty or ambiguous names are refused; whitespace is not trimmed.
     * @param NodeId Required exact reader node ID, with or without its GUID suffix; surrounding whitespace is trimmed. Empty values, display labels and numeric indices are not supported.
     * @param PinName Required exact input pin name from the reader; case-sensitive and not trimmed. The pin must be unconnected, non-execution, non-orphaned and accept editable defaults; empty is not a default selector.
     * @param Value Required literal in the pin type's Unreal schema format, for example 42 for an integer or true for a Boolean. Empty text is passed to the schema as a literal, not a universal reset command; acceptance depends on the pin type. No value is supplied automatically.
     */
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
	 * One transaction for changed defaults; an identical request creates no transaction. Does not
	 * compile or save. Apply failures attempt undo and report its outcome; inspect failures before retrying.
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
	 * Move material expressions and write comment boxes in an undo transaction; recompiles before and after layout and runs editor callbacks, but does not save; failure cleanup is attempted, not certified restoration.
	 * Uses estimated expression sizes. Matching expression identities and compile errors do not prove identical rendered output.
	 * @param Asset Required material name or package/object path, e.g. /Game/Materials/M_Surface; empty is refused. Material instances are refused; name their parent explicitly.
	 * @param Force Legacy compatibility string; leave empty. Layout is always recomputed and an already matching result remains unchanged, including when Force is nonempty.
	 */
	UFUNCTION(meta = (AICallable))
	static FString EditTurboMCPMaterialTidy(
		const FString& Asset,
		const FString& Force);

	/**
	 * Promote each exact material output and mask read by at least two distinct direct expressions to named reroutes. Reuse matching declarations, preserve existing positions, and place new usages without conservative-bound overlaps. Replans current state; a nonempty edit compiles and runs editor callbacks in an undo transaction. Does not save. Material functions and instances are refused. Failed-edit restoration is not guaranteed.
	 * Names use parameter/caption and output context. One exact output and mask must have at least two distinct direct readers; existing named-reroute readers are left alone. Existing positions are preserved; new nodes use conservative collision bounds, not full graph tidy or comment containment. Edit replans current state and compiles only when changing it. Compiler and identity checks do not prove identical rendered output.
	 * @param Asset Required material name or package/object path, e.g. /Game/Materials/M_Surface; empty is refused. Material instances are refused; explicitly name the parent to edit its graph.
	 */
	UFUNCTION(meta = (AICallable))
	static FString EditTurboMCPMaterialLabelPromote(
		const FString& Asset);

	/**
	 * Add labels, rewire inputs and delete redundant usage/reroute nodes in an undo transaction; recompiles materials and runs editor callbacks, does not save, and verifies only expression identities after attempted rollback.
	 * Uses existing comment boxes as blocks. Adds labels for values read by multiple blocks, merges repeated usages per block, and removes eligible internal reroutes. Existing node positions are preserved; new usages need free space inside their block. This is cleanup, not full graph layout. Edit rebuilds the plan from current state; request another preview if the material changed. Compile and identity checks do not prove identical rendered output.
	 * @param Asset Required material name or package/object path, e.g. /Game/Materials/M_Surface; empty is refused. Material instances and material functions are refused. For an instance, explicitly name its parent material. Blocks are existing comment boxes; this tool does not create boxes or perform a full graph layout.
	 */
	UFUNCTION(meta = (AICallable))
	static FString EditTurboMCPMaterialBlocks(
		const FString& Asset);

	/** Set texture overrides on a Material Instance Constant; loads the instance and textures and runs PostEditChange, which may update rendering resources; no tool-owned undo transaction or save is performed.
     * Only texture parameters are supported. Validates all requested overrides before writing and checks effective texture values afterward; failures after writing may leave partial changes with no automatic rollback. Sampler checks cover color versus linear sRGB compatibility, not all texture constraints or rendered correctness.
     * @param Asset Required Material Instance Constant name or package/object path, for example /Game/Materials/MI_Item; empty, ambiguous or non-instance targets are refused. Prefer a full path.
     * @param Overrides Required array of Parameter = TextureAsset entries, for example ["BaseColor = /Game/Textures/T_Item"]. Names match without case sensitivity and surrounding whitespace is trimmed. Each name must identify one declared texture parameter and may appear once per call; ambiguous layer/blend names are refused. Empty arrays, empty entries and unresolved textures are refused. Empty texture values do not clear overrides; scalar, vector and static-switch edits are not supported.
     */
	UFUNCTION(meta = (AICallable))
	static FString EditTurboMCPMaterialInstanceSet(
		const FString& Asset,
		const TArray<FString>& Overrides);

	/** Set or remove DataTable rows in an undo transaction; removals discard row data and edits may run editor callbacks and mark the package dirty; does not explicitly compile or save.
     * Validates the operation list before writing. Failed read-back attempts to undo this transaction; the result distinguishes verified row restoration from unverified or failed rollback. External callback effects are not covered by row restoration checks.
     * @param Asset Required DataTable name or package/object path, for example /Game/Data/DT_Items; empty, ambiguous, unresolved or non-DataTable targets are refused. Prefer a full path.
     * @param Rows Required array of operation strings, for example ["Potion = Health=100,Label=\"Small potion\"", "- Obsolete"]. Each string sets named fields or removes a named row. A set creates a missing row; unspecified fields keep existing values, or the row struct's defaults for a new row. Use internal field names when display labels are ambiguous and Unreal export-text syntax for values, including quoted text and parenthesized structs/containers. Empty arrays or all-blank lines are refused; blank lines within a nonempty request are skipped. Empty field values are passed to the type importer, not treated as omitted. Empty/None row names, duplicate fields in one operation and conflicting/repeated removals are refused. No implicit operation is supplied.
     */
	UFUNCTION(meta = (AICallable))
	static FString EditTurboMCPDataTableRows(
		const FString& Asset,
		const TArray<FString>& Rows);

	/**
     * Copy selected nodes into a different graph in one undo transaction; preserves the source and does not compile or save.
     * Required target members must already exist. External links that cannot resolve in the target are reported and left unconnected. Inspect the preview, then check layout, links and compilation after editing; preview does not guarantee edit success.
     * GraphCopy refuses the whole selection if it includes a local-variable node or a Format Text node. To copy the other nodes, leave these nodes out of the selection. Recreate them in the destination graph and use GraphConnect to restore their links.
     * @param SourceAsset Required source Blueprint path or unique short name, e.g. /Game/Tests/BP_NodeTest. Empty or ambiguous input is refused.
     * @param SourceGraph Required existing source graph name, e.g. EventGraph; use the reader's name. Missing targets are refused.
     * @param NodeIds Required array of source reader node IDs. Empty means nothing to copy. Use exact IDs from the source graph; unresolved or unsupported selections are refused. Structural nodes and comments are refused. Copies retain relative positions with a +40 offset; use GraphConnect for boundary wires and GraphTidy for layout afterward.
     * @param TargetAsset Required destination Blueprint path or unique short name. It must already exist and declare compatible members needed by copied nodes; self functions need compiled signatures with matching parameter names and types. Empty or ambiguous input is refused.
     * @param TargetGraph Required existing destination graph name. This is not a new graph name; choose a different graph from the source. Example: EventGraph in another Blueprint.
     * @param AnchorNodeId Required string; pass empty. Anchor wiring is not implemented and any nonempty value is refused.
     * Node references accept exact reader IDs or unique hexadecimal keys of at least six digits in the named graph. Ambiguous keys and mismatched qualified IDs are refused.
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
     * Extract a supported selection into a new function. Preview lists selected nodes, signature and external connections without changing graphs. Edit repeats planning, moves nodes and replaces them with a call. No automatic compile or save; inspect both graphs and compile before saving. Failed postconditions report partial changes rather than claiming restoration.
     * @param Asset Required Blueprint path or unique name, for example /Game/BP_Menu.
     * @param GraphName Required exact source graph name from the reader.
     * @param NodeIds Required selected reader node IDs. Entry/event/result, latent calls, local captures, macros and unsupported boundary pins refuse. Include all contents of a selected comment.
     * @param MemberName Required unused valid function name, for example FormatLabel.
     * @param MemberKind Required function. Macro extraction is not supported. Pure selections retain consumer-driven evaluation; execution selections require one incoming execution boundary and at most one continuation.
     */
    UFUNCTION(meta = (AICallable))
    static FString EditTurboMCPGraphCollapse(
		const FString& Asset,
		const FString& GraphName,
		const TArray<FString>& NodeIds,
		const FString& MemberName,
		const FString& MemberKind);

	/** Create and compile a user-defined struct in memory within an undo transaction in an idle editor outside PIE; marks its package dirty and may run editor callbacks; does not save.
     * Existing assets are refused. Preview validates field syntax and resolves types, but does not guarantee creation or validate every default. Edit lists observed fields; defaults and metadata are not fully verified. Failures may leave a partial unsaved asset; inspect before saving or retrying.
     * @param Path Required new package path including the asset name, such as /Game/Structs/S_Item; no object suffix or file extension. Whitespace is trimmed; empty, invalid and relative paths are refused. No automatic path is chosen.
     * @param Fields Required comma-separated Name: Type declarations, such as Count: int, default=1, Label: text, tooltip=Item label. Options default=, tooltip= and category= attach to the preceding field and may appear once each. Empty input is refused; empty option values are skipped during creation. Names must be unique ignoring case. Types include bool, byte, int, int64, float, real, double, name, string, text; vector, rotator, transform, vector2d, linearcolor, color; loadable struct/enum paths; array<T>, set<T>, map<K,V>; a full class path denotes an object reference, for example Array</Script/Engine.SoundBase>. Nested containers are refused and set elements/map keys must be hashable. Use Unreal literal syntax for defaults; plain top-level commas delimit fields or options.
     */
	UFUNCTION(meta = (AICallable))
	static FString EditTurboMCPStructNew(const FString& Path, const FString& Fields);

	/** Add, change, reorder or remove struct fields in an undo transaction in an idle editor outside PIE; may check out files, compile structs and trigger dependent editor updates; removal or type changes may lose data; does not save.
     * Removal consent counts only loaded dependent assets, not all project references. Checkout failure is reported but does not block the in-memory edit. A failed operation may leave earlier changes applied; no automatic rollback is performed. Inspect the returned fields and dependent assets before saving.
     * @param Struct Required user-defined struct asset name or package/object path, for example /Game/Structs/S_Item; empty or unresolved values and native C++ structs are refused.
     * @param Changes Required comma-separated operations: Name: Type adds a missing field or changes an existing field; move:Name, above=Other or below=Other reorders; remove:Name deletes. Field declarations use the same types and default=, tooltip=, category= options as EditTurboMCPStructNew. Empty options leave existing settings unchanged, not cleared. Each field may be targeted once per call; empty input is refused. For removal, force=N must match the current loaded-dependent count reported by preview (integer 0 through 2147483647); omission is allowed only when there are no loaded dependents. Preview again if the count changes. Example: Count: int, default=1, move:Count, above=Label must be split into two calls because Count appears twice.
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
