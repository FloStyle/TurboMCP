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
	const FString& GraphName, bool bForce, const TArray<FString>& NodeIds, const FString& MainPath, double HorizontalGap, double VerticalGap, double LanePitch)
{
	FTurboMCPGraphTidy::FPlan Plan;
	FString Error;
	if (!FTurboMCPGraphTidy::Plan(Asset, GraphName, bForce, /*bInMergeGetters=*/false,
		NodeIds, Plan, Error, MainPath, HorizontalGap, VerticalGap, LanePitch))
	{
		return FTurboMCPToolError::AsError(Error);
	}
	return FTurboMCPGraphTidy::RenderPreview(Plan);
}

FString UTurboMCPEditToolset::EditTurboMCPGraphTidy(const FString& Asset,
	const FString& GraphName, bool bForce, const TArray<FString>& NodeIds, const FString& MainPath, double HorizontalGap, double VerticalGap, double LanePitch)
{
	FTurboMCPGraphTidy::FPlan Plan;
	FString Error;
	if (!FTurboMCPGraphTidy::Plan(Asset, GraphName, bForce, /*bInMergeGetters=*/false,
		NodeIds, Plan, Error, MainPath, HorizontalGap, VerticalGap, LanePitch))
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
		return TEXT("error: Positions must be a string containing a JSON array with 1..4096 entries, e.g. [{\"node\":\"exact reader node ID\",\"x\":100,\"y\":200}]; unchanged");
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
			return TEXT("error: each position requires exactly the keys node (reader ID), x and y (integer graph coordinates); example [{\"node\":\"exact reader node ID\",\"x\":100,\"y\":200}]; unchanged");
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
#include "EdGraph/EdGraphPin.h"
#include "Math/IntPoint.h"
#include "TurboMCPLayoutReport.h"
namespace
{
 FString MoveGroup(const FString& Asset,const FString& GraphName,const TArray<FString>& NodeIds,int32 DeltaX,int32 DeltaY,bool Edit,bool Suggest=false)
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
  auto MoveError = [&](int32 OffsetX,int32 OffsetY) -> FString
  {
   for(UEdGraphNode* N:Selected)
  {
   const int64 X=int64(N->NodePosX)+OffsetX,Y=int64(N->NodePosY)+OffsetY;
   if(X<MIN_int32 || X>MAX_int32 || Y<MIN_int32 || Y>MAX_int32) return TEXT("error: coordinate overflow; unchanged");
   auto New=Bounds[N]; New.MinX+=OffsetX; New.MaxX+=OffsetX; New.MinY+=OffsetY; New.MaxY+=OffsetY;
   const auto Old=Bounds[N];
   for(UEdGraphNode* Other:Graph->Nodes) if(Other && !Selected.Contains(Other))
   {
    const auto Rect=Bounds[Other];
    if(Other->IsA<UEdGraphNode_Comment>())
    {
     const bool WasInside=Old.MinX>=Rect.MinX && Old.MinY>=Rect.MinY && Old.MaxX<=Rect.MaxX && Old.MaxY<=Rect.MaxY;
     const bool IsInside=New.MinX>=Rect.MinX && New.MinY>=Rect.MinY && New.MaxX<=Rect.MaxX && New.MaxY<=Rect.MaxY;
     const bool Intersects=New.MinX<Rect.MaxX && New.MaxX>Rect.MinX && New.MinY<Rect.MaxY && New.MaxY>Rect.MinY;
     if(Suggest && Intersects && !IsInside && !N->IsA<UEdGraphNode_Comment>()) return TEXT("error: candidate crosses a comment boundary");
     if(WasInside!=IsInside) return TEXT("error: move changes membership of an unselected comment; select its group too");
    }
    else if(New.MinX<Rect.MaxX && New.MaxX>Rect.MinX && New.MinY<Rect.MaxY && New.MaxY>Rect.MinY)
     return TEXT("error: moved group would overlap an unselected node; unchanged");
   }
  }
   return FString();
  };
  FString PlacementDetails;
  if(Suggest)
  {
   struct FBoundaryWire { const UEdGraphPin* Moving; const UEdGraphPin* Fixed; FVector2D Start; FVector2D End; };
   TArray<FBoundaryWire> Boundary;
   for(UEdGraphNode* N:Graph->Nodes) if(N)
    for(const UEdGraphPin* Pin:N->Pins) if(Pin && Pin->Direction==EGPD_Output)
    {
     TSet<const UEdGraphPin*> Seen;
     for(const UEdGraphPin* Other:Pin->LinkedTo)
     {
      if(!Other || Seen.Contains(Other) || !Other->GetOwningNode() || Other->GetOwningNode()->GetGraph()!=Graph) continue;
      Seen.Add(Other);
      const bool SourceMoves=Selected.Contains(N), TargetMoves=Selected.Contains(Other->GetOwningNode());
      if(SourceMoves==TargetMoves) continue;
      const UEdGraphPin* Moving=SourceMoves?Pin:Other;
      const UEdGraphPin* Fixed=SourceMoves?Other:Pin;
      Boundary.Add({Moving,Fixed,FTurboMCPLayoutReport::PinPosition(Moving),FTurboMCPLayoutReport::PinPosition(Fixed)});
     }
    }
   if(Boundary.IsEmpty()) return TEXT("unchanged: selected group has no links to outside nodes; no connected placement to propose");
   FTurboMCPLayoutReport::FRect Group;
   bool First=true;
   for(UEdGraphNode* N:Selected)
   {
    const auto B=Bounds[N];
    if(First) {Group=B;First=false;}
    else {Group.MinX=FMath::Min(Group.MinX,B.MinX);Group.MinY=FMath::Min(Group.MinY,B.MinY);Group.MaxX=FMath::Max(Group.MaxX,B.MaxX);Group.MaxY=FMath::Max(Group.MaxY,B.MaxY);}
   }
   auto Score=[&](const FVector2D& Offset)
   {
    double Total=0;
    for(const FBoundaryWire& Wire:Boundary) Total+=FVector2D::Distance(Wire.Start+Offset,Wire.End);
    return Total;
   };
   const double Before=Score(FVector2D::ZeroVector);
   double Best=Before;
   FVector2D BestOffset=FVector2D::ZeroVector;
   int32 Evaluated=0,Feasible=0;
   TSet<FIntPoint> SeenOffsets;
   auto Consider=[&](double X,double Y)
   {
    if(!FMath::IsFinite(X) || !FMath::IsFinite(Y) || X<MIN_int32 || X>MAX_int32 || Y<MIN_int32 || Y>MAX_int32) return;
    const FIntPoint Offset(static_cast<int32>(FMath::RoundToDouble(X)),static_cast<int32>(FMath::RoundToDouble(Y)));
    if(SeenOffsets.Contains(Offset)) return;
    SeenOffsets.Add(Offset);++Evaluated;
    if(!MoveError(Offset.X,Offset.Y).IsEmpty()) return;
    const FVector2D D(Offset.X,Offset.Y);
    for(const FBoundaryWire& Wire:Boundary)
    {
     if(Wire.Moving->PinType.PinCategory!=TEXT("exec")) continue;
     const double Sign=Wire.Moving->Direction==EGPD_Output?1.0:-1.0;
     const double OldForward=Sign*(Wire.End.X-Wire.Start.X);
     const double NewForward=Sign*(Wire.End.X-Wire.Start.X-D.X);
     if(OldForward>=0.0 && NewForward<0.0) return;
    }
    ++Feasible;
    const double Cost=Score(D);
    if(Cost<Best-0.01 || (FMath::Abs(Cost-Best)<=0.01 && D.SizeSquared()<BestOffset.SizeSquared()))
    {Best=Cost;BestOffset=D;}
   };
   FVector2D Ideal=FVector2D::ZeroVector;
   for(const FBoundaryWire& Wire:Boundary) Ideal+=Wire.End-Wire.Start;
   Ideal/=Boundary.Num();
   Consider(Ideal.X,Ideal.Y);
   // Search the linked-node neighbourhood and obstacle edges, not just the
   // selected lane in isolation. All candidates keep internal offsets intact.
   for(UEdGraphNode* N:Graph->Nodes) if(N && !Selected.Contains(N))
   {
    const auto B=Bounds[N];
    const double Xs[]={B.MinX-Group.MaxX-64.0,B.MaxX-Group.MinX+64.0,Ideal.X};
    const double Ys[]={B.MinY-Group.MaxY-64.0,B.MaxY-Group.MinY+64.0,Ideal.Y};
    for(double X:Xs) for(double Y:Ys) Consider(X,Y);
   }
   for(const FBoundaryWire& Wire:Boundary)
   {
    const FVector2D D=Wire.End-Wire.Start;
    Consider(D.X+(Wire.Moving->Direction==EGPD_Output?-120.0:120.0),D.Y);
   }
   // Refine the best feasible region down to individual graph units.
   for(double Step=256.0;Step>=1.0;Step*=0.5)
   {
    const FVector2D Center=BestOffset;
    for(int32 X=-1;X<=1;++X) for(int32 Y=-1;Y<=1;++Y)
     Consider(Center.X+X*Step,Center.Y+Y*Step);
   }
   if(Best>=Before-0.01)
    return FString::Printf(TEXT("unchanged: no shorter collision-free placement found among %d candidates (%d feasible). Existing box membership retained; try a smaller selection or explicitly relocate into a destination box. No graph edits."),Evaluated,Feasible);
   DeltaX=static_cast<int32>(BestOffset.X);DeltaY=static_cast<int32>(BestOffset.Y);
   PlacementDetails=FString::Printf(TEXT("\nplacement proposal: DeltaX=%d DeltaY=%d; best feasible candidate searched, not a global optimum; %d candidates, %d feasible. Boundary-wire total %.1f -> %.1f estimated graph units. Apply with EditTurboMCPGraphGroupMove using the same NodeIds and offsets; positions and obstacles are rechecked. Existing comment membership preserved."),DeltaX,DeltaY,Evaluated,Feasible,Before,Best);
   for(const FBoundaryWire& Wire:Boundary)
   {
    const double OldLength=FVector2D::Distance(Wire.Start,Wire.End);
    const double NewLength=FVector2D::Distance(Wire.Start+BestOffset,Wire.End);
    PlacementDetails+=FString::Printf(TEXT("\n  %s.%s <-> %s.%s: %.1f -> %.1f (%s)"),*TurboMCPNodeId::MakeForNode(Wire.Moving->GetOwningNode()),*Wire.Moving->PinName.ToString(),*TurboMCPNodeId::MakeForNode(Wire.Fixed->GetOwningNode()),*Wire.Fixed->PinName.ToString(),OldLength,NewLength,NewLength<OldLength?TEXT("shorter"):NewLength>OldLength?TEXT("longer"):TEXT("unchanged"));
   }
  }
  const FString Validation=MoveError(DeltaX,DeltaY);
  if(!Validation.IsEmpty()) return Validation;
  const auto CrossBefore=FTurboMCPLayoutReport::MeasureCrossBox(Graph);
  TMap<const UEdGraphNode*,FTurboMCPLayoutReport::FRect> PredictedRects;
  for(UEdGraphNode* N:Selected)
  {
   auto Rect=FTurboMCPLayoutReport::RectOf(N);
   Rect.MinX+=DeltaX; Rect.MaxX+=DeltaX; Rect.MinY+=DeltaY; Rect.MaxY+=DeltaY;
   PredictedRects.Add(N,Rect);
  }
  const auto CrossPlanned=FTurboMCPLayoutReport::MeasureCrossBox(Graph,PredictedRects);
  const FString CrossPreview=FString::Printf(TEXT("; cross-box estimated straight-line length %.1f -> %.1f (delta %+.1f), connections %d -> %d"),CrossBefore.Length,CrossPlanned.Length,CrossPlanned.Length-CrossBefore.Length,CrossBefore.Count,CrossPlanned.Count);
  const FString Summary=FString::Printf(TEXT("%d nodes/comments translated by (%d,%d); internal spacing, sizes, pins and links preserved"),Selected.Num(),DeltaX,DeltaY);
  if(!Edit) return TEXT("preview: ")+Summary+CrossPreview+PlacementDetails;
  if(DeltaX==0 && DeltaY==0) return TEXT("unchanged: zero offset")+CrossPreview;
  FScopedTransaction Transaction(FText::FromString(TEXT("TurboMCP move graph group")));
  if(!Transaction.IsOutstanding()) return TEXT("error: no undo transaction; unchanged");
  BP->Modify(); Graph->Modify();
  for(UEdGraphNode* N:Selected) { N->Modify(); N->NodePosX+=DeltaX; N->NodePosY+=DeltaY; }
  Graph->NotifyGraphChanged(); FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
  const auto CrossAfter=FTurboMCPLayoutReport::MeasureCrossBox(Graph);
  return TEXT("changed: ")+Summary+FString::Printf(TEXT("; cross-box estimated straight-line length %.1f -> %.1f (delta %+.1f), connections %d -> %d; preview predicted %.1f"),CrossBefore.Length,CrossAfter.Length,CrossAfter.Length-CrossBefore.Length,CrossBefore.Count,CrossAfter.Count,CrossPlanned.Length)+TEXT("; undo=one; compile=not_run; save=not_requested");
 }
}
FString UTurboMCPPreviewToolset::PreviewTurboMCPGraphGroupMove(const FString& Asset,const FString& GraphName,const TArray<FString>& NodeIds,int32 DeltaX,int32 DeltaY) { return MoveGroup(Asset,GraphName,NodeIds,DeltaX,DeltaY,false); }
FString UTurboMCPEditToolset::EditTurboMCPGraphGroupMove(const FString& Asset,const FString& GraphName,const TArray<FString>& NodeIds,int32 DeltaX,int32 DeltaY) { return MoveGroup(Asset,GraphName,NodeIds,DeltaX,DeltaY,true); }

FString UTurboMCPPreviewToolset::PreviewTurboMCPGraphGroupPlacement(const FString& Asset,const FString& GraphName,const TArray<FString>& NodeIds) { return MoveGroup(Asset,GraphName,NodeIds,0,0,false,true); }
