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
#include "Misc/Guid.h"

class UMaterial;
class UMaterialExpression;
class UMaterialExpressionNamedRerouteDeclaration;
class UMaterialExpressionNamedRerouteUsage;
class UMaterialFunctionInterface;

/**
 * `P/E-MAT-BLOCKS`: make labels follow blocks.
 *
 * `Spec/03_TOOLS.md` gives it three requirements and each is load-bearing:
 *
 *   1. Preview: "the labels to add, usage nodes to merge, in-block reroutes to remove, following the
 *      block rule in `05_DECISIONS.md` D-17."
 *   2. Edit: "changes EXACTLY THE ITEMS IN THE PREVIEW LIST, NOTHING MORE."
 *   3. "Same compiled result check."
 *
 * *** D-17 IS THE WHOLE SHAPE OF THIS CAPABILITY, AND ITS SIX BULLETS ARE THE SPECIFICATION. ***
 *
 *   - each comment box holds one piece of logic (a block);
 *   - when a value LEAVES its block for another block it travels as a NAMED REROUTE - the producing
 *     block declares it, the consuming block reads it;
 *   - WITHIN a block use DIRECT WIRES, because READING A LABEL INSIDE THE BLOCK THAT DECLARES IT ADDS
 *     NOTHING;
 *   - A BLOCK READS EACH LABEL THROUGH A SINGLE USAGE NODE;
 *   - PLAIN REROUTE DOTS ARE FOR ROUTING LONG WIRES AND NEVER STAND FOR A VALUE;
 *   - LABEL NAMES DESCRIBE THE VALUE; BOX TITLES DESCRIBE WHAT THE BLOCK DOES.
 *
 * EACH OF THE THREE THINGS THIS PASS CHANGES IS ONE OF THOSE BULLETS VIOLATED, AND THE MAPPING IS THE
 * DESIGN RATHER THAN A COINCIDENCE:
 *
 *   - a value read from SEVERAL BLOCKS with no label breaks "it travels as a named reroute", so this
 *     pass ADDS a label. `FCrossing`.
 *   - a label read through TWO OR MORE usage nodes in ONE block breaks "each label through a single
 *     usage node", so this pass MERGES them onto one. `FMerge`.
 *   - a PLAIN reroute used as a value breaks "plain reroute dots never stand for a value", so this
 *     pass REMOVES it and wires its readers to what it passed along. `FInBlockReroute`.
 *
 * *** THE SECOND REQUIREMENT IS THE ONE THAT MAKES THIS CAPABILITY HONEST, AND IT IS A CONSTRAINT ON
 * WHAT THE EDIT MAY DO RATHER THAN ON WHAT IT REPORTS. *** "Changes exactly the items in the Preview
 * list, nothing more" means Applies reads the list it was given and touches those keys. IT ALSO MEANS
 * THE KEYS ARE RE-RESOLVED AT APPLY TIME, which is `05_DECISIONS.md` D-45: "it re-resolves every node
 * by key at apply time rather than trusting a plan from an earlier call", BECAUSE A PLAN IS MADE AND
 * APPLIED IN TWO CALLS AND THE MATERIAL CAN CHANGE BETWEEN THEM. A node this pass cannot re-resolve is
 * REFUSED BY NAME rather than skipped, because silently skipping it would make the edit differ from the
 * preview it was authorised by.
 *
 * *** AND D-45 ALSO GOVERNS THE REMOVAL, WHICH IS WHY THE INNER REROUTE LIST CARRIES KEYS AND CLASSES
 * RATHER THAN A COUNT: "a writer that removes anything lists each removal by key with its class and
 * title, never a count... a count says how much is gone and a list says what." ***
 *
 * WHAT IS MEASURED, FOLLOWING `FTurboMCPMaterialTidy` AND `FTurboMCPMaterialLabelsPromote` DELIBERATELY.
 * `UMaterial::StateId` (`Runtime/Engine/Public/Materials/Material.h:1239`) is the engine's identifier for
 * the compiled state and `UMaterialEditingLibrary::RecompileMaterial`
 * (`Editor/MaterialEditor/Public/MaterialEditingLibrary.h:267`) returns the compiler's own error lines.
 * So the compiled-result check is: the engine reports NO NEW ERROR, and NO EXPRESSION THAT EXISTED
 * BEFORE WAS LOST. `StateId` is REPORTED RATHER THAN ASSERTED, for the reason both earlier passes
 * record: it can change for reasons this pass does not control - a shader compiling in the background, a
 * platform switch - so asserting on it would make this pass fail for other people's reasons.
 *
 * THE SET COMPARISON HAS THE ASYMMETRY `LabelsPromote` RECORDS, AND FOR THE SAME REASON. This pass
 * legitimately ADDS declarations and usages, and it legitimately REMOVES usage nodes and reroutes. So
 * the assertion is that NO PRE-EXISTING EXPRESSION WAS LOST EXCEPT THE ONES THIS PASS PLANNED TO REMOVE,
 * and that the expressions GAINED are exactly the ones it planned to add. A bare "the sets differ" check
 * would fail every time it worked; a one-directional check would miss a dropped expression.
 *
 * WHY THE REWIRING IS DONE ON `FExpressionInput` AND NOT ON GRAPH PINS. `LabelsPromote` measured this:
 * the engine's own editor works through `UEdGraph` pins, and a material built headlessly by
 * `UMaterialEditingLibrary::CreateMaterialExpressionEx` HAS NO GRAPH AT ALL - the fixture commandlet
 * never creates one. The compiler does not read pins either: `FExpressionInput::Compile` reads
 * `Expression` and `OutputIndex`, which is exactly what this pass retargets. SO THE EDIT IS MADE WHERE
 * THE COMPILER READS IT, and whether graph nodes exist is REPORTED rather than assumed.
 */
class FTurboMCPMaterialBlocks
{
public:
	/** A value read from more than one block with no label, which D-17 says must travel as a label. */
	struct FCrossing
	{
		/** The expression producing the value. */
		UMaterialExpression* Source = nullptr;

		/** Its key, so the report, the plan and the check all agree on identity. */
		FString SourceKey;

		/** Its caption, which is what the proposed name is built from. */
		FString SourceCaption;

		/** The name this pass proposes, taken from the caption. */
		FString ProposedName;

		/** Every block that reads the value, as indices into the reader's box list. */
		TArray<int32> ReaderBlocks;

		/** How many inputs would be retargeted onto the new label. */
		int32 InputsToRetarget = 0;

		/**
		 * The keys of every reader input this would touch, as `<readerKey>#<inputIndex>`.
		 *
		 * CARRIED SO `Apply` CAN RE-RESOLVE THEM, which D-45 requires and which is the difference between
		 * an edit that does what was previewed and an edit that does what the material happens to look
		 * like now. An index alone would be wrong after any earlier change in the same run.
		 */
		TArray<FString> ConsumerRefs;
	};

	/** A label read through more than one usage node inside ONE block, which D-17 forbids. */
	struct FMerge
	{
		/** The label's name, which is what the report names. */
		FString Label;

		/** The block the redundant usages sit in. */
		int32 BlockIndex = INDEX_NONE;

		/**
		 * The usage node that is KEPT, by key.
		 *
		 * THE KEPT ONE IS THE FIRST IN A KEY-ORDERED LIST RATHER THAN AN ARBITRARY POINTER ORDER, so a
		 * preview and the edit that follows it choose the same node. Pointer order is stable within one
		 * run and meaningless across two, and a preview that disagrees with its own edit is worse than no
		 * preview - a rule this project states for every `P/E` pair.
		 */
		FString KeepKey;

		/** Every usage node to remove, by key, with the class and title D-45 requires. */
		TArray<FString> RemoveKeys;

		/** How many reader inputs would be re-pointed at the kept usage node. */
		int32 InputsToRetarget = 0;

		/** The reader inputs to retarget, as `<readerKey>#<inputIndex>`. */
		TArray<FString> ConsumerRefs;
	};

	/** A plain reroute standing for a value inside one block, which D-17 says is not what they are for. */
	struct FInBlockReroute
	{
		/** The reroute itself. */
		UMaterialExpression* Reroute = nullptr;

		/** Its key, so it can be re-resolved and removed by identity. */
		FString RerouteKey;

		/** The engine class name, carried because D-45 requires a class beside every removal. */
		FString RerouteClass;

		/** The expression the reroute passes along, which is what its readers will point at instead. */
		UMaterialExpression* Source = nullptr;

		FString SourceKey;

		/** The block it sits in, which is also the block of its source and its readers. */
		int32 BlockIndex = INDEX_NONE;

		/** How many reader inputs would be re-pointed straight at the source. */
		int32 InputsToRetarget = 0;

		/** The reader inputs to retarget, as `<readerKey>#<inputIndex>`. */
		TArray<FString> ConsumerRefs;
	};

	/** The plan: what would change, or what did. */
	struct FPlan
	{
		FString Asset;
		FString AssetName;

		/** The material, when the asset is one. Null for a material function. */
		UMaterial* Material = nullptr;

		/** The function, when the asset is one. Null for a material. */
		UMaterialFunctionInterface* Function = nullptr;

		/**
		 * The instance's parent, when the asset is a Material Instance, so a refusal can name it.
		 *
		 * A refusal that does not say which asset to name instead is a refusal the caller cannot act on.
		 * `LabelsPromote` records the same requirement and the same reason.
		 */
		FString ParentPath;

		/** How many blocks the material has, so a report can say what it looked at. */
		int32 BlockCount = 0;

		/** Expressions that sit in no block, reported because a block view that dropped them would lie. */
		int32 UnboxedExpressions = 0;

		/** Every change this pass would make, in the three lists the spec names. */
		TArray<FCrossing> Crossings;
		TArray<FMerge> Merges;
		TArray<FInBlockReroute> InBlockReroutes;

		/** The engine's compiled-state identifier BEFORE any edit. Reported, never asserted. */
		FGuid StateIdBefore;

		/** The recompiler's own error lines before the edit, so a NEW error is what is refused. */
		TArray<FString> CompileErrorsBefore;

		/** Every expression key as this pass found them, so the caller can compare sets. */
		TArray<FString> KeysBefore;

		/**
		 * True when at least one expression carries a graph node.
		 *
		 * REPORTED RATHER THAN ASSUMED, because it tells a caller whether they are looking at a material
		 * the editor built or one built headlessly - the difference between a graph a human can open and
		 * one only the compiler can see.
		 */
		bool bHasGraphNodes = false;

		/** Anything that could not be done, named rather than silently skipped. */
		TArray<FString> Refusals;

		/** True when there is nothing to change, so `Apply` can say "unchanged" honestly. */
		bool bNothingToChange = false;

		/** True when there is nothing to refuse. */
		bool bAcceptable() const { return Refusals.Num() == 0; }

		/** How many items the Preview listed, which is exactly what `Apply` may change. */
		int32 ItemCount() const
		{
			return Crossings.Num() + Merges.Num() + InBlockReroutes.Num();
		}
	};

	/**
	 * Plans the changes and changes nothing.
	 *
	 * THE CHECKS LIVE HERE RATHER THAN IN `Apply`, so the Preview and its Edit twin refuse the SAME
	 * input - a rule this project states for every `P/E` pair because a Preview that accepts what the
	 * Edit rejects is worse than no Preview.
	 */
	static bool Plan(const FString& InAsset, FPlan& OutPlan, FString& OutError);

	/**
	 * What would happen, in text.
	 *
	 * IT PRINTS EVERY ITEM THE EDIT WOULD TOUCH, because "changes exactly the items in the Preview list"
	 * is only checkable if the list is in the text.
	 */
	static FString RenderPreview(const FPlan& InPlan);

	/**
	 * Makes exactly the planned changes, then PROVES THE COMPILED RESULT IS UNCHANGED.
	 *
	 * Returns the report. An edit that lost more than it planned, added something unplanned, or
	 * introduced a compile error is REPORTED AS A FAILURE with the difference named - never as a success
	 * with a note.
	 */
	static FString Apply(const FPlan& InPlan);
};
