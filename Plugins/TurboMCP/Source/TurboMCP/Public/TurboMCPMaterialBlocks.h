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
#include "MaterialExpressionIO.h"

class UMaterial;
class UMaterialExpression;
class UMaterialExpressionNamedRerouteDeclaration;
class UMaterialExpressionNamedRerouteUsage;
class UMaterialFunctionInterface;

/** Plans and applies material comment-block cleanup.
 * Preview only reads the loaded graph. Apply recompiles, edits in a transaction,
 * and checks expression identities and new compiler errors. These checks do not
 * prove identical rendered output. Failed edits attempt undo and report its limits.
 */
class FTurboMCPMaterialBlocks
{
public:
	/** A value read from more than one block with no label, to promote to a named reroute. */
	struct FCrossing
	{
		/** The expression producing the value. */
		UMaterialExpression* Source = nullptr;

		/** Its key, so the report, the plan and the check all agree on identity. */
		FString SourceKey;

		/** Its caption, which is what the proposed name is built from. */
		FString SourceCaption;

		/** Complete source connection, including output selection and component mask. */
		FExpressionInput SourceConnection;
		TMap<FString, FExpressionInput> ExpectedConsumers;

		/** The name this pass proposes, taken from the caption. */
		FString ProposedName;
		FIntPoint DeclarationPosition;
		TMap<FString, FIntPoint> UsagePositions;

		/** Every block that reads the value, as indices into the reader's box list. */
		TArray<int32> ReaderBlocks;

		/** How many inputs would be retargeted onto the new label. */
		int32 InputsToRetarget = 0;

		TArray<FString> ConsumerRefs;
	};

	/** A label read through more than one usage node inside one block. */
	struct FMerge
	{
		/** The label's name, which is what the report names. */
		FString Label;

		/** The block the redundant usages sit in. */
		int32 BlockIndex = INDEX_NONE;

		FString KeepKey;

		/** Keys of the usage nodes to remove. */
		TArray<FString> RemoveKeys;

		/** How many reader inputs would be re-pointed at the kept usage node. */
		int32 InputsToRetarget = 0;

		/** The reader inputs to retarget, as `<readerKey>#<inputIndex>`. */
		TArray<FString> ConsumerRefs;
		/** Material property inputs that also consume a removed usage. */
		TArray<FString> OutputProperties;
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

		/** Final source connection retained when bypassing the reroute chain. */
		FExpressionInput SourceConnection;

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

		/** Connection and layout state used to reject stale plans before mutation. */
		FString ConnectionSnapshot;

		/** Every expression key as this pass found them, so the caller can compare sets. */
		TArray<FString> KeysBefore;

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

	static bool Plan(const FString& InAsset, FPlan& OutPlan, FString& OutError);

	/**
	 * What would happen, in text.
	 *
	 * IT PRINTS EVERY ITEM THE EDIT WOULD TOUCH, because "changes exactly the items in the Preview list"
	 * is only checkable if the list is in the text.
	 */
	static FString RenderPreview(const FPlan& InPlan);

    /** Apply the current plan after rechecking connections and layout.
     * Compiles before and after a nonempty edit; does not save.
     * OutRollbackNote reports identity checks after attempted undo, not full restoration.
     */
	static FString Apply(const FPlan& InPlan, FString& OutRollbackNote);
};
