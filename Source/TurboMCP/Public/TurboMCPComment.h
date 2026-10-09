// Copyright FloStyle. All Rights Reserved.

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
 * HOW A BOX KNOWS WHAT IS INSIDE IT, AND WHY THAT IS NOT INVENTED HERE
 * -------------------------------------------------------------------
 * A comment node holds no list of members. The engine decides membership from GEOMETRY, through
 * `UEdGraphNode::GetNodesUnderComment`, and that is what this file calls rather than comparing
 * rectangles by hand. A hand-rolled overlap test would disagree with the editor the first time
 * either changed, and the disagreement would be invisible.
 *
 * `fit` and `list empty boxes` therefore mean what the editor means by them, because both are asked
 * of the same engine call.
 */
class FTurboMCPComment
{
public:
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
