// Copyright (c) 2026 Florian ESCHALIER (FloStyle)
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "CoreTypes.h"

class UBlueprint;
class UEdGraph;
class UEdGraphNode_Comment;

/**
 * Graph comment boxes, behind `P-COMMENT` and `E-COMMENT`.
 *
 * `03_TOOLS.md` names five operations and two facts that matter:
 *
 *  - create a box around given node IDs, rename, recolour and resize an existing one, delete a box
 *    (NEVER its nodes), fit every box to its contents, and list any box left with nothing inside;
 *  - the Preview and the Edit share one code path up to the write (`D-02`).
 *
 * HOW A BOX KNOWS WHAT IS INSIDE IT: THE ENGINE'S RULE, NOT THE ENGINE'S CACHE
 * -----------------------------------------------------------------------------
 * A comment node holds no list of members, and the engine's `GetNodesUnderComment` CANNOT BE USED
 * HEADLESSLY: it returns a CACHED set that `SGraphNodeComment::HandleSelection` fills from RENDERED
 * widgets, so an asset read without its graph editor open reports NOTHING inside every box. Measured:
 * a box drawn around two nodes reported `holds 0 node(s)`, which would have made "list every box with
 * nothing inside" call every box in every asset empty.
 *
 * So the RULE is reproduced rather than the cache read, from
 * `SGraphNodeComment::IsNodeUnderComment`, which is `FSlateRect::IsRectangleContained` in graph
 * space - and a box's rectangle starts `TitleBarHeight` BELOW its position, because the title bar's
 * own strip sits above it.
 *
 * THIS COMMENT USED TO SAY THE OPPOSITE, and it is corrected here because it is the first thing a
 * reader of the layout code sees. Anyone implementing a pass that moves nodes into or out of boxes
 * must use `MembersOf` below, which is this same rule, rather than `GetNodesUnderComment` or a
 * freshly written overlap test: three copies of a membership rule agree only until one of them is
 * changed.
 *
 * `fit` and `list empty boxes` therefore mean what the editor means by them.
 */
class FTurboMCPComment
{
public:
	/**
	 * The nodes a box encloses, by the editor's own geometric rule.
	 *
	 * PUBLIC SO THE LAYOUT PASSES SHARE ONE COPY. `P/E-GRAPH-TIDY` has to move nodes in and out of
	 * boxes and `P/E-GRAPH-FIX-BACKWARD` must never move a box, so both need this rule. It was in an
	 * anonymous namespace, which meant the only ways to reuse it were to copy it or to duplicate the
	 * bug the copy would eventually differ by.
	 *
	 * `InComment`'s own graph is used, and comments nested inside other comments are NOT members,
	 * because the engine treats nesting as its own relationship rather than as membership.
	 */
	static void MembersOf(UEdGraphNode_Comment* InComment, TArray<FString>& OutMembers);
	/** The operation the caller asked for. */
	enum class EAction : uint8
	{
		/** Make a new box around the named nodes. */
		Create,
		/** Change the text, the colour or the size of an existing box. */
		Set,
		/** Resize every box in the graph to enclose what is inside it. */
		FitAll,
		/** Remove a box, leaving its nodes alone. */
		Delete
	};

	/** One box, with what the editor says is inside it. */
	struct FBox
	{
		/** The box's own node key, so it can be named again. */
		FString Key;

		/** The node's name in the graph, for the message. */
		FString NodeName;

		/** The comment text. */
		FString Text;

		/** The box's rectangle, in graph units. */
		double X = 0.0;
		double Y = 0.0;
		double Width = 0.0;
		double Height = 0.0;

		/** How many nodes the editor says are inside it. */
		int32 MemberCount = 0;

		/** The members' names, so a caller can see what is in there. */
		TArray<FString> Members;
	};

	/** The parsed and checked request. */
	struct FPlan
	{
		FString Asset;
		FString GraphName;
		EAction Action = EAction::Create;

		/** The nodes the caller named, for `Create`. */
		TArray<FString> NodeKeys;

		/** The box the caller named, for `Set` and `Delete`. */
		FString BoxKey;

		FString Text;
		FString Color;
		bool bHasText = false;
		bool bHasColor = false;

		/** An explicit size, for `Set`. Zero means leave it alone. */
		double Width = 0.0;
		double Height = 0.0;

		/** Every box in the graph, for the preview and for `FitAll`. */
		TArray<FBox> Boxes;

		/** Boxes with nothing inside, which `03_TOOLS.md` asks to be listed. */
		TArray<FBox> EmptyBoxes;

		/** What `Create` would produce, before it exists. */
		FBox Planned;
	};

	/** Parses and validates the request, and reads the graph. */
	static bool Plan(const FString& InAsset, const FString& InGraphName,
		const FString& InAction, const FString& InNodes, const FString& InBox,
		const FString& InText, const FString& InColor,
		const FString& InSize, FPlan& OutPlan, FString& OutError);

	/** Prints what the call would do. */
	static FString RenderPreview(const FPlan& InPlan);

	/** Carries out exactly the plan. */
	static FString Apply(const FPlan& InPlan);

	/** The padding a fitted box leaves around its contents, in graph units. */
	static constexpr double FitPadding = 40.0;

	/** The height of a box's title bar, which a fit has to leave room for. */
	static constexpr double TitleBarHeight = 42.0;
};
