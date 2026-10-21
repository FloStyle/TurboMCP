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
	const FString& GraphName, bool bForce, const TArray<FString>& NodeIds, const FString& MainPath)
{
	FTurboMCPGraphTidy::FPlan Plan;
	FString Error;
	if (!FTurboMCPGraphTidy::Plan(Asset, GraphName, bForce, /*bInMergeGetters=*/false,
		NodeIds, Plan, Error, MainPath))
	{
		return FTurboMCPToolError::AsError(Error);
	}
	return FTurboMCPGraphTidy::RenderPreview(Plan);
}

FString UTurboMCPEditToolset::EditTurboMCPGraphTidy(const FString& Asset,
	const FString& GraphName, bool bForce, const TArray<FString>& NodeIds, const FString& MainPath)
{
	FTurboMCPGraphTidy::FPlan Plan;
	FString Error;
	if (!FTurboMCPGraphTidy::Plan(Asset, GraphName, bForce, /*bInMergeGetters=*/false,
		NodeIds, Plan, Error, MainPath))
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

#include "Editor/TransBuffer.h"
#include "UObject/Package.h"

namespace
{
	FString AssetUndo(const FString& Asset, const FString& Confirm, bool Edit)
	{
		if (!IsInGameThread() || !GEditor || GEditor->IsPlaySessionInProgress() || GEditor->IsTransactionActive())
			return TEXT("error: undo requires idle editor outside PIE and transactions");
		FTurboMCPAssetResolver::FResolved R;
		const FString Error = FTurboMCPAssetResolver::Resolve(Asset, R);
		if (!Error.IsEmpty()) return FTurboMCPToolError::AsError(Error);
		UBlueprint* BP = LoadObject<UBlueprint>(nullptr, *R.ObjectPath);
		if (!BP) return TEXT("error: undo currently supports Blueprint assets only");
		UTransBuffer* Buffer = Cast<UTransBuffer>(GEditor->Trans);
		const int32 Index = Buffer ? Buffer->GetQueueLength()-Buffer->GetUndoCount()-1 : -1;
		const FTransaction* Transaction = Index>=0 ? Buffer->GetTransaction(Index) : nullptr;
		if (!Transaction || !Transaction->GetTitle().ToString().Contains(TEXT("TurboMCP")))
			return TEXT("error: latest undo entry is not TurboMCP; no unrelated edit was undone");
		TArray<UObject*> Objects; Transaction->GetTransactionObjects(Objects); bool Found=false;
		for (UObject* Object : Objects)
		{
			if (!Object) continue;
			UPackage* Owner = Object->GetOutermost();
			if (Owner == BP->GetOutermost()) Found=true;
			else if (Owner && Owner != GetTransientPackage()) return TEXT("error: latest transaction touches another package; unchanged");
		}
		if (!Found) return TEXT("error: latest transaction does not touch the named asset; unchanged");
		const FString Id = Transaction->GetId().ToString();
		if (!Edit) return TEXT("preview: undo ") + Transaction->GetTitle().ToString() + TEXT(" on ") + R.ObjectPath + TEXT("; Confirm=") + Id;
		if (Confirm != Id) return TEXT("error: undo transaction changed or confirmation absent; preview again");
		return GEditor->UndoTransaction(false) ? TEXT("changed: latest TurboMCP transaction undone; editor Redo remains available") : TEXT("error: editor refused undo");
	}
}
FString UTurboMCPPreviewToolset::PreviewTurboMCPAssetUndo(const FString& Asset) { return AssetUndo(Asset, FString(), false); }
FString UTurboMCPEditToolset::EditTurboMCPAssetUndo(const FString& Asset,const FString& Confirm) { return AssetUndo(Asset, Confirm, true); }

#include "PackageTools.h"
#include "Misc/PackageName.h"
#include "HAL/PlatformTime.h"

namespace
{
 struct FRevertApproval { FString Token; double Expires = 0; };
 TMap<FString, FRevertApproval> RevertApprovals;
 FString AssetRevert(const FString& Asset, const FString& Confirm, bool Edit)
 {
  if (!IsInGameThread() || !GEditor || GEditor->IsPlaySessionInProgress() || GEditor->IsTransactionActive())
   return TEXT("error: revert requires idle editor outside PIE and transactions");
  FTurboMCPAssetResolver::FResolved R;
  const FString Error = FTurboMCPAssetResolver::Resolve(Asset, R);
  if (!Error.IsEmpty()) return FTurboMCPToolError::AsError(Error);
  UBlueprint* BP = LoadObject<UBlueprint>(nullptr, *R.ObjectPath);
  if (!BP || !FPackageName::DoesPackageExist(BP->GetOutermost()->GetName()))
   return TEXT("error: revert requires a Blueprint with an existing saved package; unchanged");
  if (!Edit)
  {
   FRevertApproval Approval;
   Approval.Token = TEXT("DISCARD ") + R.ObjectPath + TEXT(" ") + FGuid::NewGuid().ToString();
   Approval.Expires = FPlatformTime::Seconds() + 60.0;
   RevertApprovals.Add(R.ObjectPath, Approval);
   return TEXT("preview: discard ALL unsaved changes in package ") + BP->GetOutermost()->GetName()
    + TEXT(" and reload its saved version. This is not a selective graph undo. References can be reinstanced and global undo history can be reset. Changes made before confirmation are also discarded. No save will run. Confirmation expires in 60 seconds. Confirm=") + Approval.Token;
  }
  const FRevertApproval* Approval = RevertApprovals.Find(R.ObjectPath);
  if (!Approval || Approval->Token != Confirm || FPlatformTime::Seconds() > Approval->Expires)
   return TEXT("error: matching unexpired preview confirmation required; unchanged");
  RevertApprovals.Remove(R.ObjectPath);
  TArray<UPackage*> Packages; Packages.Add(BP->GetOutermost());
  FText ReloadError;
  const bool Changed = UPackageTools::ReloadPackages(Packages, ReloadError, EReloadPackagesInteractionMode::AssumePositive);
  return Changed ? TEXT("changed: requested package reloaded from disk; unsaved changes discarded; re-read asset IDs and references before further edits")
   : TEXT("error: package reload failed: ") + ReloadError.ToString();
 }
}
FString UTurboMCPPreviewToolset::PreviewTurboMCPAssetRevert(const FString& Asset) { return AssetRevert(Asset, FString(), false); }
FString UTurboMCPEditToolset::EditTurboMCPAssetRevert(const FString& Asset, const FString& Confirm) { return AssetRevert(Asset, Confirm, true); }

#include "EdGraphNode_Comment.h"
#include "TurboMCPLayoutReport.h"
namespace
{
 FString MoveGroup(const FString& Asset,const FString& GraphName,const TArray<FString>& NodeIds,int32 DeltaX,int32 DeltaY,bool Edit)
 {
  if(!IsInGameThread() || !GEditor || GEditor->IsPlaySessionInProgress() || GEditor->IsTransactionActive()) return TEXT("error: idle editor required; unchanged");
  if(NodeIds.IsEmpty()) return TEXT("error: explicit node or comment IDs required; unchanged");
  FTurboMCPAssetResolver::FResolved R; const FString Error=FTurboMCPAssetResolver::Resolve(Asset,R);
  if(!Error.IsEmpty()) return FTurboMCPToolError::AsError(Error);
  UBlueprint* BP=LoadObject<UBlueprint>(nullptr,*R.ObjectPath); int32 Matches=0;
  UEdGraph* Graph=BP?TurboMCPNodeId::FindGraphByName(BP,GraphName,Matches):nullptr;
  if(!Graph || Matches!=1) return TEXT("error: graph missing or ambiguous; unchanged");
  FString GeometryError;
  if(!FTurboMCPGraphTidy::PrepareAlignment(BP,Graph,GeometryError)) return TEXT("error: ")+GeometryError;
  TMap<UEdGraphNode*,FTurboMCPLayoutReport::FRect> Bounds;
  for(UEdGraphNode* N:Graph->Nodes) if(N)
  {
   const FVector2D Size=FTurboMCPGraphTidy::AlignmentNodeSize(N);
   if(Size.X<=0 || Size.Y<=0) return TEXT("error: group movement requires measured node bounds; unchanged");
   auto Rect=FTurboMCPLayoutReport::RectOf(N); Rect.MaxX=Rect.MinX+Size.X; Rect.MaxY=Rect.MinY+Size.Y;
   Bounds.Add(N,Rect);
  }
  TSet<UEdGraphNode*> Selected;
  for(const FString& Id:NodeIds)
  {
   UEdGraphNode* Found=nullptr;
   for(UEdGraphNode* N:Graph->Nodes)
    if(N && (TurboMCPNodeId::MakeForNode(N,true)==Id || TurboMCPNodeId::MakeForNode(N,false)==Id))
    { if(Found) return TEXT("error: ambiguous node ID; unchanged"); Found=N; }
   if(!Found) return TEXT("error: node ID not found; unchanged");
   Selected.Add(Found);
   if(Found->IsA<UEdGraphNode_Comment>())
   {
    const auto Box=Bounds[Found];
    for(UEdGraphNode* N:Graph->Nodes) if(N)
    {
     const auto Rect=Bounds[N];
     if(Rect.MinX>=Box.MinX && Rect.MinY>=Box.MinY && Rect.MaxX<=Box.MaxX && Rect.MaxY<=Box.MaxY) Selected.Add(N);
    }
   }
  }
  for(UEdGraphNode* N:Selected)
  {
   const int64 X=int64(N->NodePosX)+DeltaX,Y=int64(N->NodePosY)+DeltaY;
   if(X<MIN_int32 || X>MAX_int32 || Y<MIN_int32 || Y>MAX_int32) return TEXT("error: coordinate overflow; unchanged");
   auto New=Bounds[N]; New.MinX+=DeltaX; New.MaxX+=DeltaX; New.MinY+=DeltaY; New.MaxY+=DeltaY;
   const auto Old=Bounds[N];
   for(UEdGraphNode* Other:Graph->Nodes) if(Other && !Selected.Contains(Other))
   {
    const auto Rect=Bounds[Other];
    if(Other->IsA<UEdGraphNode_Comment>())
    {
     const bool WasInside=Old.MinX>=Rect.MinX && Old.MinY>=Rect.MinY && Old.MaxX<=Rect.MaxX && Old.MaxY<=Rect.MaxY;
     const bool IsInside=New.MinX>=Rect.MinX && New.MinY>=Rect.MinY && New.MaxX<=Rect.MaxX && New.MaxY<=Rect.MaxY;
     if(WasInside!=IsInside) return TEXT("error: move changes membership of an unselected comment; select its group too");
    }
    else if(New.MinX<Rect.MaxX && New.MaxX>Rect.MinX && New.MinY<Rect.MaxY && New.MaxY>Rect.MinY)
     return TEXT("error: moved group would overlap an unselected node; unchanged");
   }
  }
  const FString Summary=FString::Printf(TEXT("%d nodes/comments translated by (%d,%d); internal spacing, sizes, pins and links preserved"),Selected.Num(),DeltaX,DeltaY);
  if(!Edit) return TEXT("preview: ")+Summary;
  if(DeltaX==0 && DeltaY==0) return TEXT("unchanged: zero offset");
  FScopedTransaction Transaction(FText::FromString(TEXT("TurboMCP move graph group")));
  if(!Transaction.IsOutstanding()) return TEXT("error: no undo transaction; unchanged");
  BP->Modify(); Graph->Modify();
  for(UEdGraphNode* N:Selected) { N->Modify(); N->NodePosX+=DeltaX; N->NodePosY+=DeltaY; }
  Graph->NotifyGraphChanged(); FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
  return TEXT("changed: ")+Summary+TEXT("; undo=one; compile=not_run; save=not_requested");
 }
}
FString UTurboMCPPreviewToolset::PreviewTurboMCPGraphGroupMove(const FString& Asset,const FString& GraphName,const TArray<FString>& NodeIds,int32 DeltaX,int32 DeltaY) { return MoveGroup(Asset,GraphName,NodeIds,DeltaX,DeltaY,false); }
FString UTurboMCPEditToolset::EditTurboMCPGraphGroupMove(const FString& Asset,const FString& GraphName,const TArray<FString>& NodeIds,int32 DeltaX,int32 DeltaY) { return MoveGroup(Asset,GraphName,NodeIds,DeltaX,DeltaY,true); }
