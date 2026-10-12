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

class UBlueprint;
class UEdGraph;
class UEdGraphNode;
class UEdGraphPin;

/**
 * `E-GRAPH-COPY` and `E-GRAPH-COLLAPSE`: the two structural graph writers.
 *
 * THE CHECKS ARE IN `Plan` AND NOT IN `Apply`, so a Preview and its Edit twin refuse the same input.
 * `Apply` consumes an `FPlan` and never re-decides anything.
 *
 * ---------------------------------------------------------------------------------------------
 * WHY THE BOUNDARY IS COMPUTED FROM CROSSING LINKS RATHER THAN FROM A PIN LIST, WHICH IS `D-47`
 * ---------------------------------------------------------------------------------------------
 *
 * `Spec/05_DECISIONS.md` `D-47` says the collapse boundary comes from the links that CROSS the selection.
 * That is not a convenience: it is the only definition that cannot disagree with the graph. A hand-entered
 * list of input and output names would be a SECOND DESCRIPTION of the same fact, and the two would drift
 * the first time someone linked a pin. So both writers below enumerate every pin of every selected node,
 * ask whether the link at the far end lands inside the selection, and treat the ones that do not as the
 * boundary - in a STABLE ORDER, because the new member's pin order must not depend on hash iteration.
 *
 * ---------------------------------------------------------------------------------------------
 * `SpawnNodeOfType` DOES NOT FIT A COPY, AND THAT IS MEASURED RATHER THAN ASSUMED
 * ---------------------------------------------------------------------------------------------
 *
 * A copy has a CLASS and a set of PROPERTY VALUES; it does not have a type id to resolve. `SpawnNodeOfType`
 * resolves a type id through the action database, so it would rebuild a node of the right class with the
 * RIGHT DEFAULT VALUES AND THE WRONG CONTENTS - a copied `UK2Node_CallFunction` would forget which function
 * it calls. `FEdGraphSchemaAction_K2NewNode::SpawnNodeFromTemplate` (`EdGraphSchema_K2_Actions.h:95`) is
 * static and carries the TEMPLATE'S OWN property values, so the source node IS the template and no
 * per-class copy code is needed. The engine uses it for exactly this at `EdGraphSchema_K2.cpp:2916`.
 *
 * `AllocateDefaultPins` IS MANDATORY on any node created outside the editor - create, add to the graph,
 * THEN allocate. It has already caused one crash and one silent corruption in this project, and
 * `SpawnNodeFromTemplate` is what performs it here rather than a hand-rolled sequence that might omit it.
 *
 * ---------------------------------------------------------------------------------------------
 * WHAT IS REPORTED RATHER THAN DROPPED
 * ---------------------------------------------------------------------------------------------
 *
 * `Spec/03_TOOLS.md` requires a link that crosses the selection and has NOTHING TO POINT AT in the target
 * to be REPORTED BY NAME and left unconnected, not silently dropped. A silently dropped wire is the worst
 * outcome this file can produce, because the graph still looks plausible - so every crossing link is
 * accounted for in exactly one of three lists: re-linked, refused, or reported.
 */
class FTurboMCPGraphStructure
{
public:
	/** Note a plan carries so a reader can tell a copy from a collapse without guessing from the text. */
	enum class EVerb : uint8
	{
		Copy,
		Collapse
	};

	/**
	 * One link that leaves the selection.
	 *
	 * `bHasTarget` is the CRITICAL FIELD. A crossing link whose far end is a pin of a node that exists in
	 * the destination can be re-linked; one whose far end is a node that was NOT copied and is NOT in the
	 * destination cannot, and `Reason` says which of those happened. Keeping the two apart is what stops
	 * "reported" from becoming a synonym for "quietly lost".
	 */
	struct FCrossingLink
	{
		/** Node key of the selected end, so a reader can find it. */
		FString InsideNodeKey;

		/** Pin name on the selected end. */
		FString InsidePin;

		/** Node key of the end outside the selection, when there is one. */
		FString OutsideNodeKey;

		/** Pin name on the outside end, when there is one. */
		FString OutsidePin;

		/** True when the outside end exists in the destination and the link can be made. */
		bool bHasTarget = false;

		/** Why it cannot be re-linked, or empty when it can. Always set when `bHasTarget` is false. */
		FString Reason;
	};

	/** One node the plan will create, identified by the source node it came from. */
	struct FCopiedNode
	{
		/** The source node's key, as a caller wrote it. */
		FString SourceKey;

		/** The class the copy will have, read off the source. */
		FString ClassName;

		/** 1-based index among the selected nodes, so the report can count. */
		int32 Index = 0;
	};

	/**
	 * One member the target asset does not have, so the whole copy is refused.
	 *
	 * THE REFUSAL IS ALL-OR-NOTHING ON PURPOSE. `03_TOOLS.md`: "half a block is worse than none", because a
	 * half-copied block is a graph that looks like it works and does not.
	 */
	struct FMissingMember
	{
		/** `variable`, `function` or `event`. */
		FString Kind;

		/** The member's name as the source node refers to it. */
		FString Name;

		/** The source node's key, so the caller can find which node needed it. */
		FString SourceKey;
	};

	/** What the plan decided, so `Apply` can act without deciding anything. */
	struct FPlan
	{
		EVerb Verb = EVerb::Copy;

		/** Source side. */
		FString SourceAsset;
		FString SourceGraph;

		/** Target side. Empty for a collapse, whose target is a member of the source Blueprint. */
		FString TargetAsset;
		FString TargetGraph;

		/** The member a collapse will create or fill. Empty for a copy. */
		FString MemberName;
		FString MemberKind;

		/** Optional node in the target the copied block's entry links into. */
		FString TargetAnchorNodeKey;

		/** Every node the plan will create, in the order the caller named them. */
		TArray<FCopiedNode> Nodes;

		/** Every link INSIDE the selection, which is re-made between the copies. */
		TArray<FCrossingLink> InternalLinks;

		/** Every link that leaves the selection and CAN be re-made. */
		TArray<FCrossingLink> ReLinkable;

		/** Every link that leaves the selection and CANNOT be re-made. Reported, never dropped. */
		TArray<FCrossingLink> Unlinkable;

		/** Members the target lacks. Any entry here refuses the whole operation. */
		TArray<FMissingMember> Missing;

		/** Every way the request cannot be applied, as a sentence. */
		TArray<FString> Refusals;

		/** Node count in the target graph before, so the report can state the change. */
		int32 TargetNodeCountBefore = 0;

		/** True when the request is a no-op: nothing selected, or source and target are the same place. */
		bool bNothingToDo = false;

		/** True when the plan would be applied: no refusals, no missing members, and something to do. */
		bool bAcceptable() const
		{
			return Refusals.Num() == 0 && Missing.Num() == 0 && !bNothingToDo;
		}
	};

	/**
	 * Plans a copy of the named nodes from one graph to another. WRITES NOTHING.
	 *
	 * @param InNodeKeys  Source node keys, as `R-GRAPH-LOAD` prints them. At least one is required.
	 * @param OutError    Why the plan could not even be attempted, such as a missing asset. Empty on success.
	 */
	static bool PlanCopy(const FString& InSourceAsset, const FString& InSourceGraph,
		const TArray<FString>& InNodeKeys, const FString& InTargetAsset, const FString& InTargetGraph,
		const FString& InAnchorNodeKey, FPlan& OutPlan, FString& OutError);

	/**
	 * Plans extracting the named nodes into a function or a macro of the same Blueprint. WRITES NOTHING.
	 *
	 * @param InMemberKind  `function` or `macro`. Anything else is refused by name.
	 */
	static bool PlanCollapse(const FString& InAsset, const FString& InGraphName,
		const TArray<FString>& InNodeKeys, const FString& InMemberName, const FString& InMemberKind,
		FPlan& OutPlan, FString& OutError);

	/**
	 * Applies an accepted plan: one transaction, create, link, compile, then read back and compare.
	 *
	 * THE CALLER MUST HAVE CALLED A `Plan` AND CHECKED `bAcceptable`. This function does not re-decide, and
	 * its report's FIRST LINE is `changed` or `unchanged` as the spec requires.
	 */
	static bool Apply(const FPlan& InPlan, FString& OutReport, FString& OutError);

	/** Renders a plan for a Preview, which prints the same refusals an Edit would act on. */
	static FString RenderPreview(const FPlan& InPlan);
};
