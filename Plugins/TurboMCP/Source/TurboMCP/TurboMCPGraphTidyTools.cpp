// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "Tools/TurboMCPToolsets.h"

#include "TurboMCPToolError.h"

#include "Containers/UnrealString.h"

#include "TurboMCPGraphTidy.h"

/**
 * The two tool bodies for `P/E-GRAPH-TIDY`.
 *
 * THE PREVIEW AND THE EDIT SHARE ONE CODE PATH UP TO THE WRITE (`D-02`). Both call `FTurboMCPGraphTidy
 * ::Plan` and then differ by ONE call - `RenderPreview` or `Apply`. Every refusal is decided inside
 * `Plan`, so a Preview can never offer a plan the Edit then declines, which is a disagreement this
 * project has been caught by twice.
 *
 * THE DECLARATIONS LIVE IN THE ORCHESTRATOR'S HEADER. This file was gated while they were absent, because
 * a `UFUNCTION` declaration whose body is missing fails the whole module at LINK and breaks every other
 * workstream's build - the knots workstream did exactly that once. The gate is gone now that both
 * declarations are present, which is why this file is live rather than conditional.
 */

FString UTurboMCPPreviewToolset::PreviewTurboMCPGraphTidy(const FString& Asset,
	const FString& GraphName, bool bForce, const TArray<FString>& NodeIds)
{
	FTurboMCPGraphTidy::FPlan Plan;
	FString Error;
	if (!FTurboMCPGraphTidy::Plan(Asset, GraphName, bForce, /*bInMergeGetters=*/false,
		NodeIds, Plan, Error))
	{
		return FTurboMCPToolError::AsError(Error);
	}
	return FTurboMCPGraphTidy::RenderPreview(Plan);
}

FString UTurboMCPEditToolset::EditTurboMCPGraphTidy(const FString& Asset,
	const FString& GraphName, bool bForce, const TArray<FString>& NodeIds)
{
	FTurboMCPGraphTidy::FPlan Plan;
	FString Error;
	if (!FTurboMCPGraphTidy::Plan(Asset, GraphName, bForce, /*bInMergeGetters=*/false,
		NodeIds, Plan, Error))
	{
		return FTurboMCPToolError::AsError(Error);
	}
	return FTurboMCPGraphTidy::Apply(Plan);
}

#include "TurboMCPAssetResolver.h"
#include "TurboMCPNodeId.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "Editor.h"
#include "ScopedTransaction.h"
#include "Kismet2/BlueprintEditorUtils.h"

FString UTurboMCPEditToolset::EditTurboMCPGraphPositions(const FString& Asset, const FString& GraphName, const FString& Positions)
{
	if (!IsInGameThread() || !GEditor || GEditor->IsPlaySessionInProgress() || GEditor->IsTransactionActive())
		return TEXT("error: positions require an idle editor outside PIE and active transactions; unchanged");
	FTurboMCPAssetResolver::FResolved Resolved;
	const FString Error = FTurboMCPAssetResolver::Resolve(Asset, Resolved);
	if (!Error.IsEmpty()) return FTurboMCPToolError::AsError(Error);
	UBlueprint* BP = LoadObject<UBlueprint>(nullptr, *Resolved.ObjectPath);
	if (!BP) return TEXT("error: not a Blueprint; unchanged");
	int32 Matches = 0;
	UEdGraph* Graph = TurboMCPNodeId::FindGraphByName(BP, GraphName, Matches);
	if (!Graph || Matches != 1) return TEXT("error: graph missing or ambiguous; unchanged");
	TArray<TSharedPtr<FJsonValue>> Items;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Positions), Items) || Items.IsEmpty() || Items.Num() > 4096)
		return TEXT("error: Positions must be a JSON array with 1..4096 entries; unchanged");
	struct FMove { UEdGraphNode* Node; int32 X; int32 Y; };
	TArray<FMove> Moves; TSet<UEdGraphNode*> Seen;
	for (const auto& Item : Items)
	{
		const TSharedPtr<FJsonObject>* Obj = nullptr; FString Id; double X, Y;
		if (!Item || !Item->TryGetObject(Obj) || !Obj || !Obj->IsValid() || (*Obj)->Values.Num() != 3
			|| !(*Obj)->TryGetStringField(TEXT("node"), Id) || !(*Obj)->TryGetNumberField(TEXT("x"), X)
			|| !(*Obj)->TryGetNumberField(TEXT("y"), Y) || !FMath::IsFinite(X) || !FMath::IsFinite(Y)
			|| X < MIN_int32 || X > MAX_int32 || Y < MIN_int32 || Y > MAX_int32
			|| FMath::FloorToDouble(X) != X || FMath::FloorToDouble(Y) != Y)
			return TEXT("error: each position requires node, x and y with int32 coordinates; unchanged");
		UEdGraphNode* Found = nullptr;
		for (UEdGraphNode* Node : Graph->Nodes)
			if (Node && (TurboMCPNodeId::MakeForNode(Node, true) == Id || TurboMCPNodeId::MakeForNode(Node, false) == Id))
			{ if (Found) return TEXT("error: ambiguous node ID; unchanged"); Found = Node; }
		if (!Found || Seen.Contains(Found)) return TEXT("error: node missing or repeated; unchanged");
		Seen.Add(Found);
		if (Found->NodePosX != int32(X) || Found->NodePosY != int32(Y)) Moves.Add({Found, int32(X), int32(Y)});
	}
	if (Moves.IsEmpty()) return TEXT("unchanged: all positions already match");
	const FScopedTransaction Transaction(FText::FromString(TEXT("TurboMCP batch node positions")));
	BP->Modify(); Graph->Modify();
	for (const FMove& Move : Moves) { Move.Node->Modify(); Move.Node->NodePosX = Move.X; Move.Node->NodePosY = Move.Y; }
	Graph->NotifyGraphChanged();
	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	return FString::Printf(TEXT("changed: positioned %d nodes in one undo step; compile=not_run; save=not_requested"), Moves.Num());
}
