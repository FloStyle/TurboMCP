// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#include "Containers/UnrealString.h"
#include "CoreTypes.h"
#include "UObject/NameTypes.h"

class UBlueprint;
class UEdGraph;
class UEdGraphNode;

/**
 * Node IDs, the `<AssetName>:<GraphName>:<NodeKey>` form from `04_FORMATS.md`
 * section 3.
 *
 * One implementation, used by every reader and every writer, because the spec
 * says most bugs in earlier work came from two tools disagreeing about one of
 * these formats.
 *
 * The ID deliberately carries no runtime pointer or array index. `3.2` requires
 * that an ID never changes while the node exists, whatever is added or removed
 * around it, and an index would change the moment a node was deleted.
 */
namespace TurboMCPNodeId
{
	/** How many hex digits a node key uses unless that is ambiguous. */
	constexpr int32 StandardKeyDigits = 6;

	/**
	 * A parsed node ID.
	 *
	 * `AssetName` is the asset's own name, or a full object path when the caller
	 * wrote the long form. `GraphName` and `NodeKey` are empty when the ID was
	 * written with fewer than three parts, which is allowed for the short forms
	 * the spec's rules mention.
	 */
	struct FParsed
	{
		FString AssetName;
		FString GraphName;
		FString NodeKey;

		/** True when the caller wrote a full `/Game/...` path. */
		bool bIsLongForm = false;

		/** True when all three parts were present. */
		bool IsComplete() const
		{
			return !AssetName.IsEmpty() && !GraphName.IsEmpty() && !NodeKey.IsEmpty();
		}
	};

	/**
	 * Builds the node key: the leading hex digits of the node's GUID, lower case.
	 *
	 * @param InDigits  How many digits to print. More than the standard six is
	 *                  used only to break a tie inside one graph.
	 */
	FString MakeKey(const FGuid& InGuid, int32 InDigits = StandardKeyDigits);

	/**
	 * Builds a full ID from an asset, a graph and a node.
	 *
	 * @param InAssetPath  The asset's object path, for example
	 *                     `/Game/UI/WBP_ShopCard.WBP_ShopCard`. The object path is
	 *                     accepted and reduced to the asset name, because `3.2`
	 *                     requires every reader to start its output with the long
	 *                     asset path and the name alone does not carry it.
	 * @param bLongForm    When true, keeps the full object path in the ID.
	 */
	FString Make(const FString& InAssetPath, const FString& InGraphName,
		const FGuid& InNodeGuid, bool bLongForm = false,
		int32 InDigits = StandardKeyDigits);

	/** Makes an ID for a node that already knows its graph and outer asset. */
	FString MakeForNode(const UEdGraphNode* InNode, bool bLongForm = false);

	/**
	 * Splits an ID into its parts.
	 *
	 * A graph name containing spaces or colons is quoted, and separators inside
	 * those quotes are ignored. The spec allows a graph name with spaces, which
	 * are kept as written; the same is true of colons.
	 */
	FParsed Parse(const FString& InId);

	/**
	 * The node key an ID carries, trimmed and validated.
	 *
	 * Returns an empty string when the key is empty, non-hexadecimal, or shorter
	 * than the standard six digits, so a caller can refuse rather than compare
	 * against a partial key.
	 */
	FString NormaliseKey(const FString& InKey);

	/**
	 * Finds every node in a graph whose normalised key starts with InKey.
	 * Keys shorter than the standard width or containing non-hex characters are
	 * refused and produce no matches.
	 *
	 * Returns how many matched, rather than a bool, because "none" and "more than
	 * one" need different answers from the caller: one says the node is gone, the
	 * other says to widen the key. A bool cannot tell those apart, and the first
	 * version of this returned a bool, which made the ambiguity branch in
	 * `R-NODE-RESOLVE` dead code that never ran.
	 *
	 * @param OutMatches  Every matching node, in graph order.
	 */
	int32 FindNodesByKey(const UEdGraph* InGraph, const FString& InKey,
		TArray<UEdGraphNode*>& OutMatches);

	/**
	 * The shortest key length that is unique for this node inside its graph.
	 *
	 * Six unless another node in the same graph shares its first six digits, in
	 * which case it grows until the prefix is unique. A graph with thousands of
	 * nodes is the case this exists for.
	 */
	int32 UniqueKeyDigits(const UEdGraph* InGraph, const FGuid& InGuid);

	/**
	 * The graph inside a Blueprint whose name matches, case insensitively.
	 *
	 * Blueprints hold several graph kinds: the event graph, function graphs,
	 * macro graphs and the like. They are all searched, because an ID names a
	 * graph and not a kind of graph.
	 */
	UEdGraph* FindGraphByName(const UBlueprint* InBlueprint, const FString& InGraphName);

	/**
	 * The same resolution, and it also reports how many graphs matched.
	 *
	 * *** WHY THIS EXISTS: A NAME IS NOT AN IDENTITY. *** `FindGraphByName` returns the FIRST matching graph and
	 * never counts the rest, and a node id is `...:GraphName:<hash>` - the graph NAME ONLY. So when a blueprint
	 * holds two graphs of the same name in different arrays, a tool can resolve one while the reader shows the
	 * other and the report names the same `name:hash` for either. That is the ARC06 family: identity that cannot
	 * be verified across calls.
	 *
	 * `OutMatchCount > 1` means the name is AMBIGUOUS. A MUTATING caller must REFUSE and name the candidates
	 * rather than guess; a READER should report the count and stay available. The returned graph is still the
	 * first match, so this overload changes nothing by itself - it only makes the ambiguity observable.
	 */
	UEdGraph* FindGraphByName(const UBlueprint* InBlueprint, const FString& InGraphName, int32& OutMatchCount);

	/**
	 * The node type ID the engine's own graph write tool accepts.
	 *
	 * Format is `<category>|<title>` with every whitespace character removed from
	 * both, for example `Development|PrintString` or `Math|Float|sin(degrees)`.
	 * A custom event is special-cased to `AddEvent|Custom|<title>`.
	 *
	 * This is deliberately the same rule the engine's own Blueprint toolset uses
	 * in `EditorToolset/Content/Python/editor_toolset/toolsets/blueprint.py`,
	 * function `_get_node_type_id`. Matching it is what makes a node this plugin
	 * reports a node the engine's writer can actually create, which
	 * `03_TOOLS.md` requires of `R-NODE-RESOLVE`: "the node type ID (the one the
	 * engine's node creation tool accepts)".
	 *
	 * Returns an empty string when either half is missing, so a caller refuses
	 * rather than printing a type ID nothing will accept.
	 */
	FString GetNodeTypeId(const UEdGraphNode* InNode);

	/**
	 * The node's title with its whitespace collapsed, for matching and printing.
	 */
	FString GetNodeTitle(const UEdGraphNode* InNode);

	/**
	 * The unlocalised source of an FText, when it has one.
	 *
	 * `04_FORMATS.md` section 2 and 7 describe the graph text as something agents
	 * write and tools read back, so the node type ID has to be the same on every
	 * machine. The engine builds a node's title and menu category as localised
	 * `FText`, so on a French editor `PrintString` becomes `Afficher|...` and a
	 * type ID captured on one machine would not resolve on another.
	 *
	 * `FText::GetSourceString` gives the string before translation, which is
	 * English, so this is the form a stable ID is built from.
	 *
	 * Falls back to the display string when the text is a literal with no separate
	 * source, because some nodes build their title at runtime.
	 */
	FString Unlocalised(const FText& InText);

	/**
	 * The type ID of a member-variable get or set node.
	 *
	 * Variable nodes are not palette nodes, so they have no menu category, and the
	 * generic rule in GetNodeTypeId cannot build their ID. The engine's own writer
	 * uses the canonical form `Variables|<variable category>|Set<Name>` or
	 * `Get<Name>`, and its `_expand_type_id` in `blueprint_dsl.py` resolves a
	 * shorthand `|SetName` back to exactly that.
	 *
	 * Matching it is what lets an ID this plugin prints be handed straight back to
	 * the engine's graph writer, which is what `03_TOOLS.md` requires.
	 *
	 * Returns an empty string when the variable cannot be found on the Blueprint,
	 * so the caller refuses rather than printing an ID that will not resolve.
	 */
	FString GetVariableNodeTypeId(const class UK2Node_Variable* InNode);
}
