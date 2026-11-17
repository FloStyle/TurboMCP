// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#include "Misc/Guid.h"
#include "TurboMCPGraphAppend.h"

class UBlueprint;
class UEdGraph;

/** Applies a complete, fresh ARC07 edit document in one undo transaction. */
class FTurboMCPGraphReplace
{
	struct FLink
	{
		int32 From = INDEX_NONE;
		FString Output;
	};

	struct FInput
	{
		FString Name;
		TOptional<FString> Default;
		TArray<FLink> Links;
	};

	struct FForm
	{
		FGuid Id; // Invalid only for a new node.
		FString Type;
		FString Label;
		TArray<FInput> Inputs;
	};

	struct FRemoval
	{
		FGuid Id;
		FString ClassName;
		FString Title;
	};

public:
	class FPlan
	{
		friend class FTurboMCPGraphReplace;
		FString Asset;
		FString Graph;
		FGuid GraphGuid;
		FString BaseFingerprint;
		TArray<FForm> Forms;
		TArray<FRemoval> Removals;
		TArray<FString> Updates;
		TArray<FTurboMCPGraphAppend::FPlannedPosition> Positions;
		TArray<FString> Refusals;
		bool bValidated = false;
		bool HasChanges() const { return !Removals.IsEmpty() || !Positions.IsEmpty() || !Updates.IsEmpty(); }
	public:
		bool bAcceptable() const { return bValidated && Refusals.IsEmpty(); }
	};

	static bool Plan(const FString& InAsset, const FString& InGraphName,
		const FString& InText, FPlan& OutPlan, FString& OutError);
	static bool Apply(const FPlan& InPlan, FString& OutReport, FString& OutError);
	static FString RenderPreview(const FPlan& InPlan);

private:
	friend class FTurboMCPReplacePlanTest;
	static bool ApplyInternal(const FPlan& Plan, FString& OutReport, FString& OutError, bool bFailAfterRemoval);
	static bool Execute(const FPlan& Plan, UBlueprint& Blueprint, UEdGraph& Graph,
		UEdGraph* DiscoveryGraph, FString& OutError, bool bFailAfterRemoval = false);
};
