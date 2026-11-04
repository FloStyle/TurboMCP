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
	/**
	 * How many distinct consuming expressions make a value worth a label.
	 *
	 * THE SPEC SAYS "used far away or in several boxes", AND TWO IS THE SMALLEST NUMBER THAT MAKES THAT
	 * TRUE. A value read in exactly one place is not shared, so naming it adds a node and a name and
	 * saves nothing; a value read in TWO places is shared, and that is the case the spec is describing.
	 * The threshold is a NAMED CONSTANT rather than a literal so a caller can see the rule and so a
	 * break test can move it.
	 */
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

		/**
		 * The input's own name, when the engine exposes one, so a report says WHICH pin rather than only
		 * its index. An index alone is not something a caller can act on in the material editor.
		 */
		FString InputName;

		/** The reader's column, which is what "in several boxes" is measured against. */
		int32 ReaderColumn = 0;
	};

	/** A value this pass would give a name to. */
	struct FPromotion
	{
		/** The expression producing the value. */
		UMaterialExpression* Source = nullptr;

		/** Exact selected output and channel mask forwarded by the label. */
		FExpressionInput SourceConnection;

		/** Its key, so the report and the check agree on identity. */
		FString SourceKey;

		/** The expression's own caption, which is what the proposed name is built from. */
		FString SourceCaption;

		/** Every input that reads it, in a stable order. */
		TArray<FConsumer> Consumers;

		/** How many DISTINCT reader expressions there are, which is the promotion test. */
		int32 DistinctReaders = 0;

		/** How many distinct columns the readers sit in, which is "in several boxes". */
		int32 DistinctReaderColumns = 0;

		/**
		 * The name this pass proposes, taken from context.
		 *
		 * A PROPOSAL RATHER THAN A DECISION: the spec asks the Preview to propose a name "from context
		 * (box title, parameter names, expression)", so this is derived from the expression's caption and
		 * made safe to use as an `FName`, AND IT IS PRINTED so the caller can see what they would get.
		 */
		FString ProposedName;

		/**
		 * True when a declaration for THIS VALUE already exists, so it is reused rather than duplicated.
		 *
		 * FOUND BY KEY RATHER THAN BY NAME, AND THAT IS THE CORRECT IDENTITY. The engine enforces its own
		 * naming rule through `MakeNameUnique`
		 * (`Runtime/Engine/Private/Materials/MaterialExpressions.cpp:18344`), which can turn a proposed
		 * `Value` into `Value_1` - so a second run that looked itself up BY NAME would not find what it
		 * made and would add a duplicate declaration for a value that already has one. A label is a
		 * property of the VALUE, so the value's own key is what decides whether it is already named.
		 */
		bool bAlreadyDeclared = false;

		/** The existing declaration when `bAlreadyDeclared`, so a second run is a no-op. */
		UMaterialExpressionNamedRerouteDeclaration* ExistingDeclaration = nullptr;
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

		/** The instance's parent, when the asset is a Material Instance, so a refusal can name it. */
		FString ParentPath;

		/** Every promotion this pass would make. */
		TArray<FPromotion> Promotions;

		/** Every value examined, so a preview can say how many were considered and rejected. */
		int32 ValuesExamined = 0;

		/** How many inputs in total would be retargeted by this plan. */
		int32 InputsToRetarget = 0;

		/**
		 * How many USAGE nodes this plan would add.
		 *
		 * ONE PER DISTINCT READER, which is the engine's own convention and the reason a label reports
		 * how many places read it. Counted separately from the declarations because the two numbers are
		 * different and a report that conflated them would hide a promotion that named a value nothing
		 * ends up reading.
		 */
		int32 UsagesToAdd = 0;

		/** How many labels the material already declares, so a report can say what it will reuse. */
		int32 ExistingLabels = 0;

		/**
		 * Which VALUE each existing declaration forwards, keyed by that value's key.
		 *
		 * THIS IS WHAT MAKES A SECOND RUN A NO-OP, and it is keyed by the VALUE rather than by the label's
		 * name for the reason recorded on `FPromotion::bAlreadyDeclared`: the engine may have made the
		 * name unique, so looking up by name would not find the declaration this pass already made.
		 */
		TMap<FString, UMaterialExpressionNamedRerouteDeclaration*> DeclarationBySourceKey;

		/**
		 * True when at least one expression has a graph node, which decides whether a pin-based rewire
		 * would even be possible on this asset.
		 *
		 * REPORTED RATHER THAN ASSUMED. The fixture has no graph, and a caller reading this can tell
		 * whether they are looking at a material the editor built or one built headlessly - which is the
		 * difference between a graph a human can open and one only the compiler can see.
		 */
		bool bHasGraphNodes = false;

		/** The engine's compiled-state identifier BEFORE the edit. Reported, never asserted. */
		FGuid StateIdBefore;

		/** The recompiler's own error lines before the edit, so a NEW error is what is refused. */
		TArray<FString> CompileErrorsBefore;

		/** Every expression key as this pass found them, so the caller can compare sets. */
		TArray<FString> KeysBefore;

		/** Anything that could not be done, named rather than silently skipped. */
		TArray<FString> Refusals;

		/** True when there is nothing to promote, so `Apply` can say "unchanged" honestly. */
		bool bNothingToPromote = false;

		/** True when there is nothing to refuse. */
		bool bAcceptable() const { return Refusals.Num() == 0; }
	};

	/**
	 * Plans the promotions and changes nothing.
	 *
	 * THE CHECKS LIVE HERE RATHER THAN IN `Apply`, so the Preview and its Edit twin refuse the SAME
	 * input - a rule this project states for every `P/E` pair because a Preview that accepts what the
	 * Edit rejects is worse than no Preview.
	 */
	static bool Plan(const FString& InAsset, FPlan& OutPlan, FString& OutError);

	/** What would happen, in text, WITH THE PROPOSED NAMES - which is what the spec asks for. */
	static FString RenderPreview(const FPlan& InPlan);

	/**
	 * Writes declarations and usages, then checks compiler errors and expression identities.
	 *
	 * Returns the report. An edit that lost an expression, added one it did not plan, or introduced a
	 * compile error is REPORTED AS A FAILURE with the difference named - never as a success with a note.
	 */
	static FString Apply(const FPlan& InPlan);
};
