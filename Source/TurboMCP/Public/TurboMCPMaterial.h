// Copyright (c) 2026 Florian ESCHALIER (FloStyle)
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

/**
 * The material readers, behind `R-MAT-LABELS`, `R-MAT-LOAD`, `R-MAT-TRACE` and `R-MAT-BLOCKS`.
 *
 * ONE RENDERER FOR ALL FOUR, for the reason `04_FORMATS.md` gives and this project keeps
 * rediscovering: two tools that render the same thing agree until one of them is changed. Every
 * shared decision - how a node is named, when a value is defined once, how a label is written - is
 * made once here.
 *
 * THE TEXT IS `04_FORMATS.md` SECTION 2.5, WHICH IS SECTION 2 APPLIED TO MATERIALS. So the
 * S-expression shape, the `;` comments and the node-ID form are the ones `R-BP-LOAD` already uses:
 *
 *   (Constant3Vector ; 3fa91c
 *       ; class MaterialExpressionConstant3Vector
 *       Constant 0.5 0.5 0.5)
 *
 * A SECOND DIALECT THAT AGREES UNTIL IT DOES NOT IS THIS PROJECT'S RECURRING FAILURE, so where a
 * rendering question has an answer in `TurboMCPGraphText.cpp` the same answer is used here:
 *
 *  - a node's identifier rides in a comment on its head line, as `RenderNode` does;
 *  - a shared value is referred to as `$<key>`, which is `RenderPinValue`'s own marker;
 *  - a plain value reference is `(-> <key>)`;
 *  - an unlinked input at its default is not printed, which is `2.1.8`.
 *
 * WHAT IS DELIBERATELY DIFFERENT, AND WHY
 * ---------------------------------------
 * Materials have no exec chain, so there is no top-level "entry point" and `2.1.2` does not apply.
 * `2.5` gives the material shape instead: connected output properties as the top-level forms, a
 * labels section, parameters at the top. That is this file's structure.
 *
 * A LABEL REPLACES A NODE ID (`2.5`). Where `R-BP-LOAD` would print `$3fa91c`, a labelled value
 * prints `@SkyColour` and nothing else, because the spec says a value that has a label is referred
 * to ONLY by its label. That is the whole point of the labels feature: the material stays one
 * connected picture instead of islands.
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
	 * The expression feeding one output property, or null.
	 *
	 * THE PLAN IS MUTABLE HERE, AND THAT IS FORCED BY THE ENGINE. The output lookup goes through
	 * `UMaterial::GetExpressionInputForProperty` (`Material.h:1890`), which is a NON-CONST member, so
	 * a `const FPlan&` cannot reach it. The alternatives were to hold a second non-const `UMaterial*`
	 * in the plan - two pointers to one object, which is the shape of bug this project keeps fixing -
	 * or to make the one pointer honest about what the engine requires. This takes the second.
	 *
	 * The property is found by asking the engine about EVERY `EMaterialProperty` in turn, rather than
	 * from a hand-written switch or a list this file maintains, so an output added by a future engine
	 * version is found without this file being touched. Deriving it from a switch is what the audit of
	 * `TurboMCPLocalVars.cpp` found to be this project's most expensive habit.
	 */
	static UMaterialExpression* OutputSource(FPlan& InPlan, const FString& InProperty);

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

	/** True when this expression is a label declaration, and its label. */
	static bool IsLabelDeclaration(const UMaterialExpression* InExpression, FString& OutLabel);

	/** True when this expression only reads a label, and the label it reads. */
	static bool IsLabelUsage(const UMaterialExpression* InExpression, FString& OutLabel);

	/** The label a usage node refers to, resolved through its declaration when the pointer is valid. */
	static FString LabelOfUsage(const UMaterialExpressionNamedRerouteUsage* InUsage);

	/** A short human label for an expression: its caption, or its class when it has none. */
	static FString CaptionOf(const UMaterialExpression* InExpression);

	/** True when this expression is a plain reroute, which `2.5` says the reader looks through. */
	static bool IsPlainReroute(const UMaterialExpression* InExpression);

	/**
	 * Follows plain reroutes to the expression a value really comes from.
	 *
	 * `2.5`: "Plain reroutes are invisible: links go through them." A NAMED reroute is NOT followed,
	 * because a named one has its own meaning and its own label.
	 */
	static UMaterialExpression* ResolveThroughReroutes(const UMaterialExpression* InExpression);

	/** Marks everything an output depends on as reached, so the unreached list is exact. */
	static void MarkReached(const UMaterialExpression* InExpression, FRenderState& InOutState);

	/** The token estimate and budget note that every Read result ends with. */
	static FString BudgetNote(const FString& InText, int32 InTokenBudget, const FString& InFullCall);
};

