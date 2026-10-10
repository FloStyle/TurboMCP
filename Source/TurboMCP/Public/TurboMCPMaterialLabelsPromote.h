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
#include "Math/Color.h"
#include "Misc/Guid.h"

class UMaterial;
class UMaterialExpression;
class UMaterialExpressionNamedRerouteDeclaration;
class UMaterialFunctionInterface;
struct FExpressionInput;

/**
 * `P/E-MAT-LABEL-PROMOTE`: give values names.
 *
 * `Spec/03_TOOLS.md` gives it three requirements and each one is load-bearing:
 *
 *   1. Preview lists "values worth a label (used far away or in several boxes), each with a proposed
 *      name from context (box title, parameter names, expression)".
 *   2. Edit creates "the declaration, rewire the readers".
 *   3. "The material's compiled result must be the same; check it and roll back if not."
 *
 * THE THIRD IS THE ONE THAT MAKES THIS CAPABILITY SAFE, AND THE REASON IS SPECIFIC RATHER THAN GENERAL.
 * A named reroute declaration COMPILES BY FORWARDING TO ITS INPUT - `Input.Compile(Compiler)`, read at
 * `Runtime/Engine/Private/Materials/MaterialExpressions.cpp:18220` - and a USAGE forwards to its
 * declaration, so A LABEL IS BYTECODE NEUTRAL: reading a value directly and reading it through a label
 * must compile to the same thing. That is what makes this edit legitimate at all, and it is also what
 * the check must confirm, because if the retarget missed an input or landed on the wrong one the
 * compiled result would differ and nothing else would notice.
 *
 * *** A LABEL'S READERS ARE USAGE NODES, AND GETTING THAT WRONG IS A SILENT FAILURE THAT WAS MEASURED
 * WHILE BUILDING THIS PASS. *** THE FIRST VERSION RETARGETED EACH READER'S INPUT STRAIGHT AT THE
 * DECLARATION. It compiled, it preserved the value, and every expression survived - and `R-MAT-LABELS`
 * then answered `read in 0 place(s)` ABOUT THE LABEL IT HAD JUST CREATED, BECAUSE THE READER COUNTS
 * `UMaterialExpressionNamedRerouteUsage` NODES, WHICH IS WHAT A MATERIAL LABEL IS. The edit produced a
 * label NOTHING READ and a naming story that was a lie, and NOTHING IN ITS OWN REPORT SAID SO. The
 * engine's own editor shows the shape: ONE USAGE PER READER, linked to the consumer
 * (`Editor/MaterialEditor/Private/MaterialEditor.cpp:4633-4643`). This pass does the same, and the
 * suite that tests it asks `R-MAT-LABELS` HOW MANY PLACES READ THE LABEL - which is the question that
 * caught this.
 *
 * WHAT IS MEASURED, AND WHY, FOLLOWING `FTurboMCPMaterialTidy` DELIBERATELY. `UMaterial::StateId`
 * (`Runtime/Engine/Public/Materials/Material.h:1239`) is the engine's identifier for the compiled state
 * and `UMaterialEditingLibrary::RecompileMaterial` (`MaterialEditingLibrary.h:267`) returns the
 * compiler's error lines. So the compiled-result check is:
 *
 *   - the engine reports NO NEW ERROR after the edit than before it, and
 *   - EVERY EXPRESSION THAT EXISTED BEFORE STILL EXISTS, which is the strongest statement available
 *     without comparing shader bytecode.
 *
 * `StateId` IS REPORTED RATHER THAN ASSERTED, for the reason `MaterialTidy` records: it can change for
 * reasons this pass does not control - a shader compiling in the background, a platform switch - so
 * asserting on it would make this pass fail for other people's reasons.
 *
 * AND THE SET COMPARISON HAS AN ASYMMETRY THAT IS DELIBERATE AND IS NAMED HERE. `MaterialTidy` asserts
 * the set is IDENTICAL because a layout cannot legitimately add an expression. THIS PASS LEGITIMATELY
 * ADDS A NODE PAIR FOR EACH PROMOTION - a declaration and one usage per reader - so the assertion is
 * that NO PRE-EXISTING EXPRESSION WAS LOST and that the expressions GAINED are exactly the ones this
 * pass planned, no more and no fewer. A one-directional check would have missed a dropped expression; a
 * bare "the sets differ" check would have failed every time it worked.
 *
 * WHAT A LABEL IS, AND THE TWO NODE CLASSES INVOLVED. A material label is a NAMED REROUTE:
 * `UMaterialExpressionNamedRerouteDeclaration` carries the value and its name
 * (`Runtime/Engine/Public/Materials/MaterialExpressionNamedReroute.h:34`), and
 * `UMaterialExpressionNamedRerouteUsage` is the node that reads it (`:97`). The reader side already
 * exists in `FTurboMCPMaterial` - `IsLabelDeclaration`, `IsLabelUsage`, `LabelOfUsage` and
 * `RenderLabels` - AND THIS PASS REUSES IT RATHER THAN WRITING A SECOND TRAVERSAL, for the same reason
 * the graph side has one lexer: two traversals of one structure drift.
 *
 * WHY THE REWIRING IS DONE ON `FExpressionInput` AND NOT ON GRAPH PINS, AND THIS IS A MEASURED
 * ENVIRONMENT FACT RATHER THAN A PREFERENCE. The engine's own editor converts a reroute to a named
 * reroute by creating the declaration and calling `MakeLinkTo` on graph pins
 * (`Editor/MaterialEditor/Private/MaterialEditor.cpp:4597-4645`). THAT PATH NEEDS `UEdGraph` PINS, and
 * the fixture this project tests against is built with `NewObject` and
 * `UMaterialEditingLibrary::CreateMaterialExpressionEx` and has NO GRAPH AT ALL - the fixture
 * commandlet never creates one. The compiler does not read pins either: `FExpressionInput::Compile`
 * reads `Expression` and `OutputIndex` (`MaterialExpression.h`), which is exactly what this pass
 * retargets. SO THE EDIT IS MADE WHERE THE COMPILER READS IT, and whether a graph node exists is
 * REPORTED rather than assumed, so a caller can see which kind of material they are looking at.
 *
 * THIS PASS NEVER REMOVES ANYTHING. Promotion adds a declaration and re-points inputs at it, so unlike
 * `E-GRAPH-REPLACE` it cannot destroy, and the worst outcome available to it is a wrong wire - which
 * the compiled-result check exists to catch.
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
	 * Writes the declarations and retargets the readers, then PROVES THE COMPILED RESULT IS UNCHANGED.
	 *
	 * Returns the report. An edit that lost an expression, added one it did not plan, or introduced a
	 * compile error is REPORTED AS A FAILURE with the difference named - never as a success with a note.
	 */
	static FString Apply(const FPlan& InPlan);
};
