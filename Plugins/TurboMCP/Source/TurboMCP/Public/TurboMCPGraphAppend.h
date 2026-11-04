// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "CoreTypes.h"
#include "TurboMCPGraphTextLex.h"

class UBlueprint;
class UEdGraph;
class UEdGraphNode;
class UEdGraphPin;

/**
 * Add nodes and explicitly requested connections to an existing graph.
 * Existing nodes are retained; connecting an occupied input may replace its link.
 * Engine-created companions are included in the result. A newly generated delegate
 * event is removed only if requested wiring leaves it completely disconnected.
 * Plan parses the shared graph-text grammar and validates identities and pins.
 * Public preview also executes on a scratch graph. Edit replans against current
 * state and checks the live result. A failed live edit attempts undo but cannot
 * certify full restoration of editor callbacks or external state.
 */
class FTurboMCPGraphAppend
{
public:
	/** One node the plan will create. */
	struct FNewNode
	{
		/** The type id from the head of the form. */
		FString TypeId;

		/** The node id the form carried, or empty when the form describes a NEW node. */
		FString ExistingNodeId;

		/** A unique local label from `; label Name`, usable by later links in this text block. */
		FString LocalLabel;

		/** True when this form names a node that is already in the graph, so it is linked to, not made. */
		bool bLinksToExisting = false;

		/** 1-based line of the head, so a refusal can point at it. */
		int32 Line = 0;

		/** 1-based column of the type id. */
		int32 Column = 0;

		/** The pins as written, in order. */
		TArray<TurboMCPGraphTextLex::FPinForm> Pins;

		/** Filled by `Plan` when the type id resolves: the node class the engine will build. */
		FString ResolvedClass;

		/**
		 * The pins the node will HAVE, read off the probe `Plan` built and then removed.
		 *
		 * WHY A FORM NEEDS THIS. A link is `(-> Node:Pin)` and its SOURCE may be a node this same text creates,
		 * which does not exist yet when `Plan` runs. The probe is the only place the pin set of a not-yet-created
		 * node can be READ while still keeping `Plan` free of any change to the asset, so the names are CAPTURED
		 * here and the link is validated against them. Without it, a link onto a created node could only be
		 * refused at apply time, and `P1` requires the Preview and the Edit to refuse the SAME input.
		 */
		TArray<FString> ProbePinNames;
        TArray<FString> ProbeExecInputs;
        TArray<FString> ProbeExecOutputs;
        int32 MeasuredWidth = 0;
        int32 MeasuredHeight = 0;
	};

	/** One link the plan will make, by node id or by a node this same text creates. */
	struct FNewLink
	{
		/**
		 * EACH END IS EITHER A NODE ALREADY IN THE GRAPH OR A FORM THIS SAME TEXT CREATES, and which one it is is a
		 * FIELD RATHER THAN A GUESS. The earlier shape carried `bUsesCreatedNode`, a SINGLE BOOL FOR A TWO-SIDED
		 * FACT, so a link FROM a created node TO an existing one was indistinguishable from the reverse, and the
		 * apply step would have had to re-derive what the plan already knew.
		 */
		FString FromNodeKey;
		int32 FromFormIndex = INDEX_NONE;
		FString FromPin;

		FString ToNodeKey;
		int32 ToFormIndex = INDEX_NONE;
		FString ToPin;

		/** 1-based line of the pin value that wrote the link, so a refusal can point at it. */
		int32 Line = 0;

		/** 1-based column of the pin value. */
		int32 Column = 0;
	};

	/** One planned top-left position, keyed by the index of the form it creates. */
	struct FPlannedPosition
	{
		/** Index into `FPlan::Nodes`. Only forms that create a node have an entry. */
		int32 FormIndex = INDEX_NONE;

		int32 X = 0;
		int32 Y = 0;

		/** True when the created footprint uses the fallback estimate rather than scratch Slate measurement. */
		bool bEstimatedSize = true;
	};

	/** What the plan decided, so `Apply` can act without deciding anything. */
	struct FPlan
	{
        FString Continuation = TEXT("auto");
		FString Asset;
		FString GraphName;

		/** Every form the text contains, in order. */
		TArray<FNewNode> Nodes;

		/** Every link the text describes. */
		TArray<FNewLink> Links;

		/** Every way the text cannot be applied, as a sentence naming its line. */
		TArray<FString> Refusals;

		/** How many nodes the graph had before, so the report can state the change. */
		int32 NodeCountBefore = 0;

		/** True when there is nothing to do: no forms at all, or every form is a link to an existing node. */
		bool bNothingToDo = false;

		/**
		 * True when the caller asked for placement. Preview and Edit set it; shared callers such as Replace
		 * leave it false and keep the legacy origin placement.
		 */
		bool bPlacementRequested = false;

		/** One entry per created form, in input order. Empty when placement was not requested. */
		TArray<FPlannedPosition> PlannedPositions;

		/** True when at least one existing or created bound dimension was estimated. */
		bool bPlacementUsedEstimatedSize = false;

		/** True when the text would be applied: no refusals AND something to do. */
		bool bAcceptable() const { return Refusals.Num() == 0 && !bNothingToDo; }
	};

	/**
	 * Parses, checks, resolves and decides. WRITES NOTHING, so a Preview and an Edit twin behave alike.
	 *
	 * THE CHECKER IS CALLED FIRST. `P-GRAPH-CHECK` is the gate every writer depends on, and this plan
	 * refuses whatever the checker refuses rather than re-deciding the same questions differently.
	 *
	 * @param OutError  Why the plan could not even be attempted, such as a missing asset. Empty on success.
	 * @param bRequestPlacement  When true, plan deterministic positions for the created forms. False keeps
	 *                           the legacy behavior and no positions are computed.
	 */
	static bool Plan(const FString& InAsset, const FString& InGraphName, const FString& InText,
		FPlan& OutPlan, FString& OutError, bool bRequestPlacement = false, const FString& Continuation = FString());

	/**
	 * Plans an attachment-relative block when links identify an existing exec source; otherwise a deterministic column.
	 *
	 * Every edge, extent and addition is widened to int64 and checked before the int32 cast. Each positive
	 * stored dimension is kept and only a missing dimension is estimated. Both the top-left position and the
	 * measured or fallback footprint must fit int32; otherwise the plan refuses before mutation. The result is
	 * scratch-measured created geometry with stored/estimated existing bounds; live visual non-overlap is unverified.
	 *
	 * @param OutRefusal  Why the geometry cannot be represented. Empty on success.
	 */
	static bool PlanPlacement(const UEdGraph* InGraph, const TArray<FNewNode>& InNodes,
		TArray<FPlannedPosition>& OutPositions, bool& bOutUsedEstimatedSize, FString& OutRefusal,
        const TArray<FNewLink>* InLinks = nullptr);

	/**
	 * Applies an accepted plan: one transaction, create, link, lay out, then read back and compare. Compile is separate.
	 *
	 * THE CALLER MUST HAVE CALLED `Plan` AND CHECKED `bAcceptable`. This function does not re-decide, and it
	 * returns a report whose FIRST LINE is `changed` or `unchanged` as the spec requires.
	 *
	 * @param OutError  Why it could not be applied at all. Empty when a report was produced.
	 */
	// Internal composition only: the caller owns the sole active transaction and handles early failures.
	static bool Apply(const FPlan& InPlan, FString& OutReport, FString& OutError, bool bJoinCallerTransaction = false);

	/** Renders a plan for a Preview, which prints the same refusals an Edit would act on. */
	static FString RenderPreview(const FPlan& InPlan);

    /** Execute the same mutation pipeline on a transient graph, never the live asset. */
    static bool Preflight(const FPlan& InPlan, FString& OutReport, FString& OutError, FPlan* OutMeasuredPlan = nullptr);
private:
#if WITH_DEV_AUTOMATION_TESTS
    // Exercise failures after scratch preflight without exposing a runtime bypass.
    friend class FTurboMCPAppendFailureTest;
#endif
    static bool ExecuteGraph(const FPlan& InPlan, UBlueprint* Blueprint, UEdGraph* Graph,
        FString& OutReport, FString& OutError, bool bScratch, bool bJoinCallerTransaction, TArray<UEdGraphNode*>* OutCreatedByForm = nullptr);

};
