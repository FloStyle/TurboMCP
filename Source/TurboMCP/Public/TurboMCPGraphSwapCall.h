// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
#pragma once

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "CoreTypes.h"

class UBlueprint;
class UEdGraph;
class UEdGraphNode;

/** Plan and apply a safe, named-pin call-node replacement. */
class FTurboMCPGraphSwapCall
{
public:
	struct FLink
	{
		FString PinName;
		FString OtherNodeId;
		FString OtherPinName;
		bool bOldPinIsOutput = false;
	};

	struct FPlan
	{
		FString Asset;
		FString GraphName;
		FString OldNodeId;
		FString ReplacementTypeId;
		FString OldClass;
		FString OldTitle;
		FString NewClass;
		bool bKeepOld = true;
		TArray<FLink> Links;
		TArray<FString> UnmatchedPins;
		TArray<FString> Refusals;
		int32 NodeCountBefore = 0;

		bool bAcceptable() const { return Refusals.Num() == 0; }
	};

	static bool Plan(const FString& InAsset, const FString& InGraphName, const FString& InNodeId,
		const FString& InReplacementTypeId, const FString& InKeepOld, FPlan& OutPlan, FString& OutError);
	static bool Apply(const FPlan& InPlan, FString& OutReport, FString& OutError);
	static FString RenderPreview(const FPlan& InPlan);
};
