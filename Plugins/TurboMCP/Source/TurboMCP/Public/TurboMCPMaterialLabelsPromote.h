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
#include "Math/Color.h"
#include "Misc/Guid.h"
#include "MaterialExpressionIO.h"
#include "Math/IntPoint.h"

class UMaterial;
class UMaterialExpression;
class UMaterialExpressionNamedRerouteDeclaration;
class UMaterialFunctionInterface;
struct FExpressionInput;

/** Plans named reroutes for exact material outputs shared by multiple readers.
 * Preview does not compile or mutate the material. A nonempty edit creates declarations
 * and usage nodes, preserves selected outputs and masks, and compiles before and after.
 * Compiler and expression-identity checks do not prove identical rendered output.
 * Failed edits request undo; restoration must be inspected before retrying or saving.
 */
class FTurboMCPMaterialLabelsPromote
{
public:
	/** Minimum number of distinct readers for one exact output and mask. */
	static constexpr int32 MinimumConsumers = 2;

	/** One place a value is read from, which is one input this pass would retarget. */
	struct FConsumer
	{
		/** The expression that reads the value. Never the declaration and never the value itself. */
		UMaterialExpression* Reader = nullptr;

		/** Its key, so a report names it the way the readers do. */
		FString ReaderKey;

		/** Which input of the reader points at the value, so the report can name the pin. */
		int32 InputIndex = 0;

		/** The input's own name, when the engine exposes one, so a report says which pin rather than only its index. An index alone is not something a caller can act on in the material editor.  */
		FString InputName;

	};

	/** A value this pass would give a name to. */
	struct FPromotion
	{
		/** The expression producing the value. */
		UMaterialExpression* Source = nullptr;

		/** Exact selected output and channel mask forwarded by the label. */
		FExpressionInput SourceConnection;
		FIntPoint DeclarationPosition;
		TMap<FString, FIntPoint> UsagePositions;

		/** Its key, so the report and the check agree on identity. */
		FString SourceKey;

		/** The expression's own caption, which is what the proposed name is built from. */
		FString SourceCaption;

		/** Every input that reads it, in a stable order. */
		TArray<FConsumer> Consumers;

		/** How many distinct reader expressions there are, which is the promotion test. */
		int32 DistinctReaders = 0;

		/** The name this pass proposes, taken from context. */
		FString ProposedName;

		/** True when a declaration for this exact output and mask already exists, so it is reused rather than duplicated. */
		bool bAlreadyDeclared = false;

		/** Existing declaration reused without renaming or rewiring its input. */
		UMaterialExpressionNamedRerouteDeclaration* ExistingDeclaration = nullptr;
	};

	/** The plan: what would change, or what did. */
	struct FPlan
	{
		FString Asset;
		FString AssetName;

		/** Wiring and placement used to reject a stale in-memory plan. */
		FString ConnectionSnapshot;

		/** The material, when the asset is one. Null for a material function. */
		UMaterial* Material = nullptr;

		/** The function, when the asset is one. Null for a material. */
		UMaterialFunctionInterface* Function = nullptr;

		/** The instance's parent, when the asset is a Material Instance, so a refusal can name it. */
		FString ParentPath;

		/** Every promotion this pass would make. */
		TArray<FPromotion> Promotions;

		/** Every value examined, so a preview can say how many were considered and rejected. */
		int32 ValuesExamined = 0;

		/** How many inputs in total would be retargeted by this plan. */
		int32 InputsToRetarget = 0;

		/** How many usage nodes this plan would add. */
		int32 UsagesToAdd = 0;

		/** How many labels the material already declares, so a report can say what it will reuse. */
		int32 ExistingLabels = 0;

		/** True when at least one expression has a graph node, which decides whether a pin-based rewire would even be possible on this asset. */
		bool bHasGraphNodes = false;

		/** The engine's compiled-state identifier before the edit. Reported, never asserted. */
		FGuid StateIdBefore;

		/** Every expression key as this pass found them, so the caller can compare sets. */
		TArray<FString> KeysBefore;

		/** Anything that could not be done, named rather than silently skipped. */
		TArray<FString> Refusals;

		/** True when there is nothing to promote, so `Apply` can say "unchanged" honestly. */
		bool bNothingToPromote = false;

		/** True when there is nothing to refuse. */
		bool bAcceptable() const { return Refusals.Num() == 0; }
	};

	/** Plans the promotions and changes nothing. */
	static bool Plan(const FString& InAsset, FPlan& OutPlan, FString& OutError);

	/** What would happen, in text, including proposed names. */
	static FString RenderPreview(const FPlan& InPlan);

	/** Writes declarations and usages, then checks compiler errors and expression identities. */
	static FString Apply(const FPlan& InPlan);
};
