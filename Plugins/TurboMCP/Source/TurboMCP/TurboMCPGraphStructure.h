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
 * Plan graph copies and supported same-Blueprint function extractions.
 * Copy preserves existing pin trees, recreates supported links and reports links
 * that cannot be recreated. Apply rechecks eligibility and uses an owned undo
 * transaction; it does not compile or save. Failed undo is not proof of restoration.
 * Collapse moves the selected nodes into a function and reconnects their boundaries through a replacement call. Unsupported selections are refused before mutation.
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
		bool bInsideInput = false;

		/** Node key of the end outside the selection, when there is one. */
		FString OutsideNodeKey;

		/** Pin name on the outside end, when there is one. */
		FString OutsidePin;
		bool bOutsideInput = false;

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

		/** Zero-based index in the source selection. */
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

	/** Selection and expected state for Apply to recheck. */
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

        /** Exact proposed signature, generated without editing the source graph. */
        TArray<FString> Signature;

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
	 * Plans extracting the named nodes into a function of the same Blueprint without changing it.
	 *
	 * @param InMemberKind  `function`. Macro extraction is explicitly unsupported.
	 */
	static bool PlanCollapse(const FString& InAsset, const FString& InGraphName,
		const TArray<FString>& InNodeKeys, const FString& InMemberName, const FString& InMemberKind,
		FPlan& OutPlan, FString& OutError);

	/** Apply a copy or function extraction with current-state guards and read-back; compile/save are separate.
	 * Failure diagnostics distinguish refusal from potentially partial changes.
	 */
	static bool Apply(const FPlan& InPlan, FString& OutReport, FString& OutError);

	/** Renders a plan for a Preview, which prints the same refusals an Edit would act on. */
	static FString RenderPreview(const FPlan& InPlan);
};
