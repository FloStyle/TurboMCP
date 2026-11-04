// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Containers/Set.h"
#include "Containers/UnrealString.h"
#include "CoreTypes.h"

class UMaterial;
class UMaterialExpression;
class UMaterialExpressionComment;
class UMaterialFunctionInterface;

/**
 * Plans left-to-right dependency columns and one descriptive comment box per column.
 * Preview loads a Material without compiling or editing it. Apply uses a transaction,
 * recompiles before and after changes, and checks expression identities and new errors.
 * Those checks do not prove identical rendered output or guaranteed rollback.
 * Node bounds are estimates; existing boxes outside planned column edges are preserved.
 */
class FTurboMCPMaterialTidy
{
public:
	/**
	 * The layout rules, from `05_DECISIONS.md` D-07 to D-12 as section 6 restates them for graphs.
	 *
	 * THESE ARE COPIED FROM `FTurboMCPGraphTidy` DELIBERATELY RATHER THAN RE-DERIVED, because section 6
	 * is ONE set of rules and a material that laid out differently from a graph would be a second
	 * dialect of layout. Where the two genuinely differ the difference is named in a comment below.
	 */
	static constexpr double HorizontalNodeGap = 60.0;
	static constexpr double VerticalNodeGap = 40.0;

	/**
	 * The gap between one column's box and the next.
	 *
	 * LARGER THAN THE GRAPH'S `BoxGap`, AND THE REASON IS A MATERIAL FACT RATHER THAN A PREFERENCE: a
	 * material expression is drawn as a wide node with its inputs stacked down the left, so a material
	 * column is far taller than a Blueprint column and two boxes that merely touch read as one. This is
	 * the ONE layout constant that differs from `FTurboMCPGraphTidy`, and it is stated rather than
	 * buried.
	 */
	static constexpr double BoxGap = 60.0;

	/** How far a box is inset from the expressions it surrounds. */
	static constexpr double FitPadding = 40.0;

	/** A comment box's title bar, which sits ABOVE the box's own Y. */
	static constexpr double TitleBarHeight = 42.0;

	/**
	 * The extent used for an expression the engine has not measured.
	 *
	 * A HEADLESS EDITOR DOES NOT MEASURE NODES. This project measured that for `UEdGraphNode` in
	 * `P/E-GRAPH-KNOTS` and the same is true of `UMaterialExpression`, which has NO stored size at all -
	 * `MaterialExpressionEditorX` and `MaterialExpressionEditorY` (`MaterialExpression.h:159,162`) are
	 * the only geometry an expression owns. So a size is SUBSTITUTED, it is a FALLBACK, and EVERY REPORT
	 * SAYS SO. UNDER-REPORTING IS ACCEPTABLE HERE; INVENTING A MEASUREMENT IS NOT.
	 */
	static constexpr double FallbackWidth = 240.0;
	static constexpr double FallbackHeight = 140.0;

	/** One expression in the layout. */
	struct FNode
	{
		FString Key;
		UMaterialExpression* Expression = nullptr;

		/** The position as the engine holds it, before this pass plans anything. */
		int32 X = 0;
		int32 Y = 0;

		/**
		 * The size this pass USES, which is the fallback because a material expression stores none.
		 *
		 * Kept apart from any measured value for the reason `FTurboMCPGraphTidy` records: a pass that
		 * compares a substituted geometry against a measured one concludes things about layouts it never
		 * touched.
		 */
		double Width = 0.0;
		double Height = 0.0;

		/** Where this pass would put it. */
		double PlannedX = 0.0;
		double PlannedY = 0.0;

		/** Its column, which is its depth from the output roots. */
		int32 Column = 0;

		/** True when nothing reaches this expression from any output. */
		bool bUnreached = false;

		/** True when the size is the fallback rather than anything the engine said. */
		bool bSizeWasFallback = true;
	};

	/** One column, which becomes one comment box. */
	struct FBlock
	{
		int32 Column = 0;

		/** The expressions in it, by index into `Nodes`. */
		TArray<int32> MemberIndices;

		/** The rectangle this pass plans for the box, title bar included. */
		double Left = 0.0;
		double Top = 0.0;
		double Right = 0.0;
		double Bottom = 0.0;

		/** The text the box carries, which NAMES the expressions it holds. */
		FString Text;
	};

	/** The plan: what would change, or what did. */
	struct FPlan
	{
		FString Asset;
		FString AssetName;
		FString ConnectionSnapshot;
		int32 PlannedOutputX = 0;
		int32 PlannedOutputY = 0;

		/** The material, when the asset is one. Null for a material function. */
		UMaterial* Material = nullptr;

		/** The function, when the asset is one. Null for a material. */
		UMaterialFunctionInterface* Function = nullptr;

		/**
		 * The instance's parent, when the asset is a Material Instance.
		 *
		 * CARRIED SO THE REFUSAL CAN NAME IT. An instance has no graph of its own, so laying one out
		 * would move its PARENT's expressions - an edit the caller did not ask for - and a refusal that
		 * does not say which asset to name instead is a refusal the caller cannot act on.
		 */
		FString ParentPath;

		/** The comment boxes as the reader found them, so a report can say what it reused. */
		TArray<UMaterialExpressionComment*> Comments;

		TArray<FNode> Nodes;
		TArray<FBlock> Blocks;

		/** The comment boxes that already existed, so a report can say what it kept. */
		int32 ExistingBoxes = 0;

		/**
		 * The engine's compiled-state identifier BEFORE the layout.
		 *
		 * REPORTED RATHER THAN ASSERTED, because it can legitimately change for reasons that have nothing
		 * to do with a layout - a background shader compile, a platform switch. The assertion rests on
		 * the expression set instead.
		 */
		FGuid StateIdBefore;

		/** The recompiler's own error lines before the layout, so a NEW error is what is refused. */
		TArray<FString> CompileErrorsBefore;

		/** True when the engine substituted a size for at least one expression. */
		bool bUsedFallbackGeometry = false;

		/** True when the material was already laid out the way this pass would lay it out. */
		bool bAlreadyTidy = false;

		/** Anything that could not be done, named rather than silently skipped. */
		TArray<FString> Refusals;

		/** The expression keys as this pass found them, so the caller can compare sets. */
		TArray<FString> KeysBefore;

		/** True when there is nothing to refuse. */
		bool bAcceptable() const { return Refusals.Num() == 0; }
	};

	/**
	 * Plans a layout and changes nothing.
	 *
	 * THE CHECKS LIVE HERE RATHER THAN IN `Apply`, so the Preview and its Edit twin refuse the SAME
	 * input - a rule this project states for every `P/E` pair because a Preview that accepts what the
	 * Edit rejects is worse than no Preview.
	 */
	static bool Plan(const FString& InAsset, bool bInForce, FPlan& OutPlan, FString& OutError);

	/** What would happen, in text. */
	static FString RenderPreview(const FPlan& InPlan);

	/**
	 * Writes the layout and checks expression identities and new compiler errors.
	 *
	 * Returns the report. A layout that added or removed an expression, or that introduced a compile
	 * error, is REPORTED AS A FAILURE with the difference named - never as a success with a note.
	 */
	static FString Apply(const FPlan& InPlan);
};
