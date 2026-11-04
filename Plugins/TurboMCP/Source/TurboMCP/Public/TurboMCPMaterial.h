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
#include "Materials/MaterialExpression.h"

class UMaterial;
class UMaterialExpression;
class UMaterialExpressionComment;
class UMaterialExpressionNamedRerouteDeclaration;
class UMaterialExpressionNamedRerouteUsage;
class UMaterialFunctionInterface;
class UMaterialInterface;
class UMaterialInstanceConstant;
class UObject;

/** Shared planning and text rendering for material Load, Trace, Labels and Blocks.
 * Reads do not assign GUIDs or edit graph objects. Missing GUIDs use a labelled fallback key.
 * Named reroutes use @Label references with one reachable definition in tree output.
 * Shared expressions use $key references. Comment membership uses the smallest box containing
 * the expression's position, not measured visual bounds. Token budgets are advisory; explicit
 * depth and expansion markers identify omitted tree content.
 */
class FTurboMCPMaterial
{
public:
	/** Which reader is being served. All four share the renderer below. */
	enum class ERead : uint8
	{
		/** `R-MAT-LABELS`: every named reroute, its readers and the unused ones. */
		Labels,
		/** `R-MAT-LOAD`: parameters, then every connected output as a tree. */
		Load,
		/** `R-MAT-TRACE`: one output only. */
		Trace,
		/** `R-MAT-BLOCKS`: the material by comment box. */
		Blocks,
	};

	/**
	 * A material expression's identifier.
	 *
	 * `04_FORMATS.md` 3.2: "Material expressions use the same form, with the expression's GUID.
	 * Expressions without a GUID get one (a Preview must not do this; it uses a stable fallback and
	 * says so)." A Read never writes, so this only ever reads `MaterialExpressionGuid` and falls
	 * back to a position-derived key when it is invalid.
	 */
	static FString KeyOf(const UMaterialExpression* InExpression);

	/** The full node ID, `<AssetName>:<GraphName>:<Key>`. */
	static FString IdOf(const UMaterialExpression* InExpression,
		const FString& InAssetName, const FString& InGraphName);

	/**
	 * Everything one reader needs, resolved and validated.
	 *
	 * THE RESOLUTION IS SHARED BY ALL FOUR, so a non-material asset is refused by the same code and
	 * with the same message whichever reader was pointed at it. Each tool's own report of what it
	 * read is not evidence, and neither is four copies of a refusal.
	 */
	struct FPlan
	{
		FString Asset;

		/** The material, when the asset is one. Null for a material function. */
		UMaterial* Material = nullptr;

		/** The material function, when the asset is one. Null for a material. */
		UMaterialFunctionInterface* Function = nullptr;

		/** The instance, when the asset is a material instance. */
		UMaterialInstanceConstant* Instance = nullptr;

		/** The instance's parent, so a refusal can name it. */
		FString ParentPath;

		/** The name used in every node ID: the asset's own name. */
		FString AssetName;

		/**
		 * The name used in every node ID where a graph name goes.
		 *
		 * A material has no graph name - it has one node graph - so a fixed word is used rather than
		 * the asset name repeated, which would read as `M:M:key` and look like a mistake. A material
		 * function uses its own name, because a function IS a separate graph.
		 */
		FString GraphName;

		/** Every expression, comments excluded, in a stable order. */
		TArray<UMaterialExpression*> Expressions;

		/** Every comment box, in a stable order. */
		TArray<UMaterialExpressionComment*> Comments;

		/** Label name to its declaration, for the labels the material defines. */
		TMap<FString, UMaterialExpressionNamedRerouteDeclaration*> DeclarationsByLabel;

		/** Label name to the declarations it names, INCLUDING ones with no declaration in this graph. */
		TMap<FString, int32> ReaderCountByLabel;

		/** The output property asked for by `R-MAT-TRACE`, empty for the others. */
		FString OutputProperty;

		/** True when the caller asked for instance overrides instead of the parent's graph. */
		bool bIncludeInstanceOverrides = false;
	};

	/**
	 * Resolves the asset and gathers the graph.
	 *
	 * Returns false with `OutError` set, which always NAMES what is wrong and, where it can, the
	 * alternative (`P7`). A material instance is refused here rather than later, because the refusal
	 * has to carry the parent's path and only this function knows it.
	 */
	static bool Plan(const FString& InAsset, ERead InKind, const FString& InOutputProperty,
		bool bInIncludeInstanceOverrides, FPlan& OutPlan, FString& OutError);

	/** `R-MAT-LABELS`. */
	static FString RenderLabels(const FPlan& InPlan);

	/** `R-MAT-LOAD` and `R-MAT-TRACE`, which share every printing decision. */
	static FString RenderTree(FPlan& InPlan, bool bTraceOnly, int32 InTokenBudget);

	/** `R-MAT-BLOCKS`. */
	static FString RenderBlocks(FPlan& InPlan, int32 InTokenBudget);

	/** The connected output properties, in a stable order. */
	static TArray<FString> ConnectedOutputs(FPlan& InPlan);

	/**
	 * WHICH BLOCK AN EXPRESSION BELONGS TO, which is the innermost comment box that contains it, or
	 * `INDEX_NONE` when it is inside no box at all.
	 *
	 * *** THIS IS PUBLIC BECAUSE TWO TOOLS MUST AGREE ON IT AND THE MATERIAL COMMENT BOX DOES NOT STORE
	 * THE ANSWER. *** A comment box holds no list of members. This project measured that on the Blueprint
	 * side - the cache is filled from RENDERED widgets and is therefore empty headlessly - and a material
	 * box has no equivalent accessor at all. Membership is therefore GEOMETRIC: the box's rectangle is
	 * computed from its position and size and compared against each expression, which is what the editor
	 * itself does. `R-MAT-BLOCKS` already computes it, and `P/E-MAT-BLOCKS` must decide which values
	 * CROSS a block boundary - SO IT NEEDS THE SAME ANSWER. Two copies of a geometric rule drift the
	 * moment either is adjusted, and a writer that disagreed with the reader about which box holds what
	 * would move values between blocks the reader does not believe in.
	 *
	 * THE BOXES ARE ENUMERATED IN THE ORDER `InPlan.Comments` HOLDS THEM, so an index is stable for one
	 * plan. It is deliberately an INDEX rather than a pointer, because comparing indices is the whole
	 * question and a caller that wanted the box can index `InPlan.Comments` itself.
	 *
	 * THE RULE IS "SMALLEST CONTAINING BOX WINS", so a box inside a box owns its own members rather than
	 * both claiming them. That is the same innermost-wins rule this project reports for Blueprint comment
	 * nesting, and using a different one here would make the two tools disagree about one picture.
	 */
	static int32 BlockIndexOf(const FPlan& InPlan, const UMaterialExpression* InExpression);

	/** The number of blocks a plan has, which is its comment box count. */
	static int32 BlockCount(const FPlan& InPlan) { return InPlan.Comments.Num(); }

	/**
	 * True when an expression is a PLAIN reroute - `UMaterialExpressionReroute`, not a named one.
	 *
	 * PUBLIC BECAUSE `P/E-MAT-BLOCKS` MUST FIND IN-BLOCK REROUTES, and it must find them by the SAME
	 * definition the reader uses to decide that a value crosses a boundary. `2.5` says a plain reroute is
	 * invisible and links go through it, while a NAMED reroute has its own meaning and its own label -
	 * so the two classes are not interchangeable and a writer that conflated them would try to delete a
	 * label.
	 */
	static bool IsPlainReroute(const UMaterialExpression* InExpression);

	/**
	 * Follows plain reroutes to the expression a value really comes from, or null.
	 *
	 * PUBLIC FOR THE REASON ABOVE: the writer must know what a value's REAL SOURCE is before it can
	 * decide whether that source is in the same block, and it must follow the same reroutes the reader
	 * follows. `2.5`: "Plain reroutes are invisible: links go through them." A NAMED reroute is NOT
	 * followed, because a named one carries a label rather than passing a wire along.
	 */
	static UMaterialExpression* ResolveThroughReroutes(const UMaterialExpression* InExpression);

    /** Return the expression feeding a material or function output, or null.
     * Rendering resolves plain reroutes by default. Reachability passes false so the
     * walk retains the reroute nodes themselves. Named reroutes are not collapsed.
     */
	static UMaterialExpression* OutputSource(FPlan& InPlan, const FString& InProperty, bool bResolveReroutes = true);

	/**
	 * True when an expression is a label declaration, with the label it names.
	 *
	 * PUBLIC BECAUSE THE LABEL WRITER NEEDS THE SAME ANSWER THE READER GIVES. `P/E-MAT-LABEL-PROMOTE`
	 * must recognise a declaration and a usage for two reasons - it must not promote a label as if it
	 * were an unlabelled value, and it must dedupe a value it already named - and reimplementing that
	 * test in the writer would be the second-dialect mistake in a new place, the one this project has
	 * already paid for on the graph side with the lexer.
	 */
	static bool IsLabelDeclaration(const UMaterialExpression* InExpression, FString& OutLabel);

	/** True when an expression reads a label, with the label it reads. Public for the reason above. */
	static bool IsLabelUsage(const UMaterialExpression* InExpression, FString& OutLabel);

	/**
	 * The expression's caption, which is what the material editor draws for it.
	 *
	 * PUBLIC BECAUSE IT IS THE ONLY HONEST SOURCE FOR A PROPOSED LABEL NAME. The label writer derives
	 * the name it offers from the expression's own caption; deriving one from the class name instead
	 * would propose `MaterialExpressionConstant3Vector` where the editor prints the value.
	 */
	static FString CaptionOf(const UMaterialExpression* InExpression);

private:
	/**
	 * The per-render state.
	 *
	 * A struct rather than a pile of out-parameters, because the recursion needs to carry the same
	 * five things through every level and threading them by hand is how a shared-value set ends up
	 * being two sets.
	 *
	 * There is deliberately NO plan pointer here. One was added and then removed: it was assigned
	 * once and never read, because every renderer already takes the plan as its own argument. A field
	 * that is written and never read is the shape of a mistake that survives review, so it is gone
	 * rather than left in.
	 */
	struct FRenderState
	{
		/** Values already defined, so a second reference prints `$key` instead of the subtree. */
		TSet<const UMaterialExpression*> Defined;

		/** Labels already defined, so a second reference prints `@label`. */
		TSet<FString> DefinedLabels;

		/** Guards a cycle. A material graph is a DAG, but a malformed one can still be walked. */
		TSet<const UMaterialExpression*> InProgress;

		/** Values that reach an output. Anything else is flagged unreached. */
		TSet<const UMaterialExpression*> Reached;

		/** Anything that could not be printed, as `2.1.12` requires. */
		TArray<FString> Notes;
	};

	/** One expression as an S-expression, with its inputs. */
	static FString RenderExpression(const UMaterialExpression* InExpression,
		FRenderState& InOutState, int32 InDepth, int32 InTokenBudget);

	/** One input's value: the linked value, the label, or the expression's own constant. */
	static FString RenderInput(const UMaterialExpression* InOwner, int32 InInputIndex,
		const FExpressionInput& InInput, FRenderState& InOutState, int32 InDepth,
		int32 InTokenBudget);

	/**
	 * The constant an expression keeps for an unlinked input.
	 *
	 * READ BY REFLECTION, which `2.5` requires by name: "Read those constants by reflection, not from
	 * a hard coded list of expression classes." The input's name is matched against the owner's
	 * fields, a nested `FExpressionInput` is skipped into, and the constant fields inside it are
	 * printed. A hand-written list would silently miss every expression class added after it was
	 * written, and the miss would look like an input with no value.
	 */
	static FString ConstantForInput(const UMaterialExpression* InOwner, const FString& InInputName);

	/**
	 * Every output property the material has, whether connected or not.
	 *
	 * MUTABLE FOR THE SAME REASON AS `OutputSource`: the engine's property lookup is a non-const
	 * member, and this walks it. Keeping one constness convention across the two means a caller
	 * cannot pass a plan to one of them and not the other.
	 */
	static TArray<FString> AllOutputProperties(FPlan& InPlan);

	/** The label a usage node refers to, resolved through its declaration when the pointer is valid. */
	static FString LabelOfUsage(const UMaterialExpressionNamedRerouteUsage* InUsage);

	/**
	 * `IsPlainReroute` AND `ResolveThroughReroutes` USED TO BE DECLARED HERE AND ARE NOW PUBLIC, because
	 * `P/E-MAT-BLOCKS` needs the same two answers the reader uses. Their one definition each stays in
	 * `TurboMCPMaterial.cpp` - moving a declaration changes who may call a function, not what it does.
	 */

	/** Marks everything an output depends on as reached, so the unreached list is exact. */
	static void MarkReached(const UMaterialExpression* InExpression, FRenderState& InOutState);

	/** The token estimate and budget note that every Read result ends with. */
	static FString BudgetNote(const FString& InText, int32 InTokenBudget, const FString& InFullCall);
};

