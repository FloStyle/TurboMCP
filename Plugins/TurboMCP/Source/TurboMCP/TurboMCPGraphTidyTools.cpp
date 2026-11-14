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
 * Tidy preview and edit share planning and refusal rules. Edit plans again against
 * the current graph: a preview is not a reservation and cannot guarantee that a
 * later edit succeeds. Compilation and saving remain separate operations.
 */

FString UTurboMCPPreviewToolset::PreviewTurboMCPGraphTidy(const FString& Asset,
	const FString& GraphName, bool bForce, const TArray<FString>& NodeIds, const FString& MainPath, double HorizontalGap, double VerticalGap, double LanePitch, const FString& WrapAfter, const FString& LayoutMode, const FString& RouteWires, double ChannelWidth, bool bCreateBoxes, int32 GeometryOffset, int32 GeometryLimit)
{
	if (GeometryOffset < 0 || GeometryLimit < 0 || GeometryLimit > 40 || (GeometryLimit == 0 && GeometryOffset != 0))
	{
		return FTurboMCPToolError::AsError(TEXT("GeometryOffset must be nonnegative. GeometryLimit must be 0 (omit evidence) or 1..40; use offset 0 when the limit is 0."));
	}
	FTurboMCPGraphTidy::FPlan Plan;
	FString Error;
	if (!FTurboMCPGraphTidy::Plan(Asset, GraphName, bForce, /*bInMergeGetters=*/false,
		NodeIds, Plan, Error, MainPath, HorizontalGap, VerticalGap, LanePitch, WrapAfter, LayoutMode, RouteWires, ChannelWidth, bCreateBoxes))
	{
		return FTurboMCPToolError::AsError(Error);
	}
	return FTurboMCPGraphTidy::RenderPreview(Plan, GeometryOffset, GeometryLimit);
}

FString UTurboMCPEditToolset::EditTurboMCPGraphTidy(const FString& Asset,
	const FString& GraphName, bool bForce, const TArray<FString>& NodeIds, const FString& MainPath, double HorizontalGap, double VerticalGap, double LanePitch, const FString& WrapAfter, const FString& LayoutMode, const FString& RouteWires, double ChannelWidth, bool bCreateBoxes)
{
	FTurboMCPGraphTidy::FPlan Plan;
	FString Error;
	if (!FTurboMCPGraphTidy::Plan(Asset, GraphName, bForce, /*bInMergeGetters=*/false,
		NodeIds, Plan, Error, MainPath, HorizontalGap, VerticalGap, LanePitch, WrapAfter, LayoutMode, RouteWires, ChannelWidth, bCreateBoxes))
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
	// Validate the complete batch before opening a transaction. JSON conversion
	// accepts booleans as numbers, so require actual numeric values first.
	struct FMove { UEdGraphNode* Node; int32 X; int32 Y; };
	TArray<FMove> Moves; TSet<UEdGraphNode*> Seen;
	for (const auto& Item : Items)
	{
		const TSharedPtr<FJsonObject>* Obj = nullptr; FString Id; double X, Y;
		if (!Item || !Item->TryGetObject(Obj) || !Obj || !Obj->IsValid() || (*Obj)->Values.Num() != 3
			|| !(*Obj)->HasTypedField<EJson::String>(TEXT("node"))
            || !(*Obj)->HasTypedField<EJson::Number>(TEXT("x")) || !(*Obj)->HasTypedField<EJson::Number>(TEXT("y"))
            || !(*Obj)->TryGetStringField(TEXT("node"), Id) || !(*Obj)->TryGetNumberField(TEXT("x"), X)
			|| !(*Obj)->TryGetNumberField(TEXT("y"), Y) || !FMath::IsFinite(X) || !FMath::IsFinite(Y)
			|| X < MIN_int32 || X > MAX_int32 || Y < MIN_int32 || Y > MAX_int32
			|| FMath::FloorToDouble(X) != X || FMath::FloorToDouble(Y) != Y)
			return TEXT("error: each position requires exactly the keys node (reader ID), x and y (integer graph coordinates); example [{\"node\":\"exact reader node ID\",\"x\":100,\"y\":200}]; unchanged");
		UEdGraphNode* Found = nullptr;
		for (UEdGraphNode* Node : Graph->Nodes)
			if (TurboMCPNodeId::MatchesNode(Node, Id))
			{ if (Found) return TEXT("error: ambiguous node ID; unchanged"); Found = Node; }
		if (!Found || Seen.Contains(Found)) return TEXT("error: node missing or repeated; use a unique hexadecimal key of at least six digits or an exact reader ID in this graph; unchanged");
		Seen.Add(Found);
		if (Found->NodePosX != int32(X) || Found->NodePosY != int32(Y)) Moves.Add({Found, int32(X), int32(Y)});
	}
	if (Moves.IsEmpty()) return FString::Printf(TEXT("unchanged: requested=%d; moved=0; already_matching=%d; all positions already match; compile=not_run; save=not_requested"), Items.Num(), Items.Num());
	const FScopedTransaction Transaction(FText::FromString(TEXT("TurboMCP batch node positions")));
	BP->Modify(); Graph->Modify();
	for (const FMove& Move : Moves) { Move.Node->Modify(); Move.Node->NodePosX = Move.X; Move.Node->NodePosY = Move.Y; }
	Graph->NotifyGraphChanged();
	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	return FString::Printf(TEXT("changed: requested=%d; moved=%d; already_matching=%d; one undo step; compile=not_run; save=not_requested"), Items.Num(), Moves.Num(), Items.Num() - Moves.Num());
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
			// Transient objects can carry unrelated editor state too. Global undo
			// replays the entire transaction; it cannot undo just this package.
			else return TEXT("error: latest transaction touches another package or unrelated transient state; unchanged");
		}
		if (!Found) return TEXT("error: latest transaction does not touch the named asset; unchanged");
		const FString Id = Transaction->GetId().ToString();
		if (!Edit) return TEXT("preview: undo ") + Transaction->GetTitle().ToString() + TEXT(" on ") + R.ObjectPath + TEXT("; Confirm=") + Id;
		if (Confirm != Id) return TEXT("error: undo transaction changed or confirmation absent; preview again");
		return GEditor->UndoTransaction(false) ? FString::Printf(TEXT("changed: latest TurboMCP transaction undone for %s; editor Redo remains available; compile=not_run; save=not_requested; saved disk contents were not reverted"), *R.ObjectPath) : TEXT("error: editor refused undo");
	}
}
FString UTurboMCPPreviewToolset::PreviewTurboMCPAssetUndo(const FString& Asset) { return AssetUndo(Asset, FString(), false); }
FString UTurboMCPEditToolset::EditTurboMCPAssetUndo(const FString& Asset,const FString& Confirm) { return AssetUndo(Asset, Confirm, true); }

#include "PackageTools.h"
#include "Misc/PackageName.h"
#include "HAL/PlatformTime.h"
#include "Misc/SecureHash.h"
#include "Serialization/ObjectWriter.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "Engine/DataAsset.h"
#include "Subsystems/AssetEditorSubsystem.h"

namespace
{
 // Serialize every loaded package object, not only the root asset: a graph or
 // owned subobject can change without changing the package's dirty flag.
 // DuplicateVerbatim prevents ObjectWriter from assigning a localization namespace.
 bool RecoveryFingerprint(UPackage* Package, FString& MemoryHash, FString& DiskHash)
 {
  FString Filename;
  if (!FPackageName::DoesPackageExist(Package->GetName(), &Filename)) return false;
  const FMD5Hash FileHash = FMD5Hash::HashFile(*Filename);
  if (!FileHash.IsValid()) return false;
  DiskHash = LexToString(FileHash);
  TArray<UObject*> Objects;
  GetObjectsWithOuter(Package, Objects, EGetObjectsFlags::IncludeNestedObjects);
  Objects.Sort([](const UObject& A, const UObject& B) { return A.GetPathName() < B.GetPathName(); });
  FString State = Package->IsDirty() ? TEXT("dirty\n") : TEXT("clean\n");
  for (UObject* Object : Objects)
  {
   if (!IsValid(Object)) return false;
   TArray<uint8> Bytes;
   FObjectWriter Writer(Object, Bytes, false, false, false, PPF_DuplicateVerbatim);
   if (Writer.IsError()) return false;
   State += Object->GetPathName() + TEXT("|") + Object->GetClass()->GetPathName()
    + TEXT("|") + FString::FromInt(static_cast<int32>(Object->GetFlags()))
    + TEXT("|") + FMD5::HashBytes(Bytes.GetData(), Bytes.Num()) + TEXT("\n");
  }
  FTCHARToUTF8 Utf8(*State);
  MemoryHash = FMD5::HashBytes(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());
  return true;
 }
 struct FRevertApproval { FString Token; FString MemoryHash; FString DiskHash; double Expires = 0; };
 TMap<FString, FRevertApproval> RevertApprovals;
 FString AssetRevert(const FString& Asset, const FString& Confirm, bool Edit)
 {
  if (!IsInGameThread() || !GEditor || GEditor->IsPlaySessionInProgress() || GEditor->IsTransactionActive())
   return TEXT("error: recovery requires an idle editor outside PIE and active transactions; no reload was attempted");
  FTurboMCPAssetResolver::FResolved R;
  const FString Error = FTurboMCPAssetResolver::Resolve(Asset, R);
  if (!Error.IsEmpty()) return FTurboMCPToolError::AsError(Error);
  UObject* Target = LoadObject<UObject>(nullptr, *R.ObjectPath);
  if (!Target || (!Target->IsA<UBlueprint>() && !Target->IsA<UDataAsset>()))
   return TEXT("error: recovery supports saved Blueprint and data assets only; no reload was attempted");
  UPackage* Package = Target->GetOutermost();
  FString Filename;
  if (!FPackageName::DoesPackageExist(Package->GetName(), &Filename))
   return TEXT("error: no saved package exists; recovery cannot restore an unsaved asset");
  TArray<UObject*> TopLevel;
  GetObjectsWithOuter(Package, TopLevel, EGetObjectsFlags::None);
  const UBlueprint* TargetBlueprint = Cast<UBlueprint>(Target);
  for (UObject* Object : TopLevel)
   // These classes are saved parts of this Blueprint, not independent assets.
   if (Object != Target && Object->IsAsset()
    && !(TargetBlueprint && (Object == TargetBlueprint->GeneratedClass || Object == TargetBlueprint->SkeletonGeneratedClass)))
    return TEXT("error: package contains multiple top-level assets; recovery refuses to discard unrelated asset changes");
  UAssetEditorSubsystem* Editors = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
  if (!Editors || Editors->FindEditorForAsset(Target, false))
   return TEXT("error: close the asset editor before previewing recovery; no editor was closed and no reload was attempted");
  // Stock reference collection sees UObject references, not arbitrary raw native pointers.
  // Reject world-backed consumers; persistent asset references may be reinstanced by reload.
  for (TObjectIterator<UObject> It; It; ++It)
  {
   UObject* Consumer = *It;
   if (!IsValid(Consumer) || Consumer->IsTemplate() || Consumer->GetOutermost() == Package || !Consumer->GetWorld()) continue;
   // Generated-class identity is not reliably surfaced by the generic reference scan.
   for (UClass* Class = Consumer->GetClass(); Class; Class = Class->GetSuperClass())
    if (Class->GetOutermost() == Package)
     return TEXT("error: a live world object uses a class in this package; remove that use before recovery; no reload was attempted");
   TArray<UObject*> DirectReferences;
   FReferenceFinder Finder(DirectReferences, nullptr, false, false, false, false);
   Finder.FindReferences(Consumer);
   for (UObject* Reference : DirectReferences)
    if (Reference && Reference->GetOutermost() == Package)
     return TEXT("error: a live world object references this package; remove that use before recovery; no reload was attempted");
  }
  FReferencerInformationList References;
  UObject* ReferenceTarget = Package;
  IsReferenced(ReferenceTarget, RF_NoFlags, EInternalObjectFlags::None, true, &References);
  for (const FReferencerInformation& Reference : References.ExternalReferences)
   if (Reference.Referencer && Reference.Referencer->GetOutermost() != Package && Reference.Referencer->GetWorld())
    return TEXT("error: a live world object references this asset; remove that use before recovery; no reload was attempted");
  FString MemoryHash, DiskHash;
  if (!RecoveryFingerprint(Package, MemoryHash, DiskHash))
   return TEXT("error: could not fingerprint the loaded package and saved file; no reload was attempted");
  if (!Edit)
  {
   FRevertApproval Approval;
   Approval.MemoryHash = MemoryHash;
   Approval.DiskHash = DiskHash;
   Approval.Token = TEXT("DISCARD ") + R.ObjectPath + TEXT(" ") + FGuid::NewGuid().ToString();
   Approval.Expires = FPlatformTime::Seconds() + 60.0;
   RevertApprovals.Add(R.ObjectPath, Approval);
   return TEXT("preview: discard ALL unsaved changes in package ") + Package->GetName()
    + TEXT("; asset=") + R.ObjectPath + TEXT("; disk_file=") + Filename
    + TEXT("; dirty=") + (Package->IsDirty() ? TEXT("true") : TEXT("false"))
    + TEXT(". Reload the saved version, not a selective undo. References may be reinstanced and global undo history may be reset. Raw native references cannot all be detected. Changes to memory or disk invalidate this approval. No save will run. Confirmation expires in 60 seconds. Confirm=") + Approval.Token;
  }
  const FRevertApproval* Approval = RevertApprovals.Find(R.ObjectPath);
  if (!Approval || Approval->Token != Confirm || FPlatformTime::Seconds() > Approval->Expires)
   return TEXT("error: matching unexpired preview confirmation required; no reload was attempted");
  if (Approval->MemoryHash != MemoryHash || Approval->DiskHash != DiskHash)
  {
   RevertApprovals.Remove(R.ObjectPath);
   return TEXT("error: the loaded package or saved file changed after preview; confirmation was invalidated; preview again; no reload was attempted");
  }
  const FString ExpectedClass = Target->GetClass()->GetPathName();
  RevertApprovals.Remove(R.ObjectPath);
  TArray<UPackage*> Packages;
  Packages.Add(Package);
  const TWeakObjectPtr<UPackage> PreviousPackage(Package);
  FText ReloadError;
  const bool Reloaded = UPackageTools::ReloadPackages(Packages, ReloadError, EReloadPackagesInteractionMode::AssumePositive);
  // Reload can destroy/reinstance the original objects. Only use freshly resolved pointers below.
  UObject* Fresh = FindObject<UObject>(nullptr, *R.ObjectPath);
  const FMD5Hash FreshDisk = FMD5Hash::HashFile(*Filename);
  if (!Reloaded || PreviousPackage.IsValid() || !Fresh || Fresh->GetClass()->GetPathName() != ExpectedClass
   || Fresh->GetOutermost()->IsDirty() || !FreshDisk.IsValid() || LexToString(FreshDisk) != DiskHash)
   return TEXT("error: recovery verification failed; changed=unknown; inspect the asset before further edits. Confirmation was consumed. Global undo history may have been reset. No save was requested. Engine detail: ") + ReloadError.ToString();
  return TEXT("changed: reloaded saved package; asset=") + R.ObjectPath
   + TEXT("; class=") + Fresh->GetClass()->GetPathName() + TEXT("; dirty=false; disk_hash=") + DiskHash
   + TEXT("; disk unchanged; save=not_requested. Global undo history may have been reset and references reinstanced. Re-read asset IDs and values before editing. Project-specific PostLoad behavior is not verified by this result.");
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
 FString MoveGroup(const FString& Asset,const FString& GraphName,const TArray<FString>& NodeIds,int32 DeltaX,int32 DeltaY,bool Edit,bool Suggest=false,const FString& DestinationBox=FString())
 {
  if(!IsInGameThread() || !GEditor || GEditor->IsPlaySessionInProgress() || GEditor->IsTransactionActive()) return TEXT("error: idle editor required; unchanged");
  if(NodeIds.IsEmpty()) return TEXT("error: explicit node or comment IDs required; unchanged");
  FTurboMCPAssetResolver::FResolved R; const FString Error=FTurboMCPAssetResolver::Resolve(Asset,R);
  if(!Error.IsEmpty()) return FTurboMCPToolError::AsError(Error);
  UBlueprint* BP=LoadObject<UBlueprint>(nullptr,*R.ObjectPath); int32 Matches=0;
  if (!BP) return TEXT("error: Blueprint could not be loaded; unchanged");
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
  auto ContainsRect=[](const FTurboMCPLayoutReport::FRect& Outer,const FTurboMCPLayoutReport::FRect& Inner)
  { return Inner.MinX>=Outer.MinX && Inner.MinY>=Outer.MinY && Inner.MaxX<=Outer.MaxX && Inner.MaxY<=Outer.MaxY; };
  bool AllUnboxed=true;
  for(UEdGraphNode* N:Selected)
  {
   if(N->IsA<UEdGraphNode_Comment>()) {AllUnboxed=false;break;}
   for(UEdGraphNode* Box:Graph->Nodes)
    if(Box && Box->IsA<UEdGraphNode_Comment>() && ContainsRect(Bounds[Box],Bounds[N])) {AllUnboxed=false;break;}
  }
  UEdGraphNode* Destination=nullptr;
  if(!DestinationBox.IsEmpty())
  {
   for(UEdGraphNode* N:Graph->Nodes)
    if(N && N->IsA<UEdGraphNode_Comment>() && (TurboMCPNodeId::MakeForNode(N,true)==DestinationBox || TurboMCPNodeId::MakeForNode(N,false)==DestinationBox))
    {if(Destination) return TEXT("error: destination box is ambiguous; unchanged");Destination=N;}
   if(!Destination) return TEXT("error: destination comment ID not found; unchanged");
   if(!AllUnboxed) return TEXT("error: destination-box transfer requires unboxed nodes only; unchanged");
  }
  auto MoveError = [&](int32 OffsetX,int32 OffsetY) -> FString
  {
   for(UEdGraphNode* N:Selected)
  {
   const int64 X=int64(N->NodePosX)+OffsetX,Y=int64(N->NodePosY)+OffsetY;
   if(X<MIN_int32 || X>MAX_int32 || Y<MIN_int32 || Y>MAX_int32) return TEXT("error: coordinate overflow; unchanged");
   auto New=Bounds[N]; New.MinX+=OffsetX; New.MaxX+=OffsetX; New.MinY+=OffsetY; New.MaxY+=OffsetY;
   const auto Old=Bounds[N];
   if(Destination)
   {
    auto Interior=Bounds[Destination]; Interior.MinX+=24.0;Interior.MaxX-=24.0;Interior.MinY+=48.0;Interior.MaxY-=24.0;
    if(!ContainsRect(Interior,New)) return TEXT("error: selected nodes do not fit inside destination comment with padding; unchanged");
   }

   for(UEdGraphNode* Other:Graph->Nodes) if(Other && !Selected.Contains(Other))
   {
    const auto Rect=Bounds[Other];
    if(Other->IsA<UEdGraphNode_Comment>())
    {
     const bool WasInside=Old.MinX>=Rect.MinX && Old.MinY>=Rect.MinY && Old.MaxX<=Rect.MaxX && Old.MaxY<=Rect.MaxY;
     const bool IsInside=New.MinX>=Rect.MinX && New.MinY>=Rect.MinY && New.MaxX<=Rect.MaxX && New.MaxY<=Rect.MaxY;
     const bool Intersects=New.MinX<Rect.MaxX && New.MaxX>Rect.MinX && New.MinY<Rect.MaxY && New.MaxY>Rect.MinY;
     // Preview and explicit edits must reject the same partial comment entry.
     if(Intersects && !IsInside && !N->IsA<UEdGraphNode_Comment>()) return TEXT("error: moved node would cross an unselected comment boundary; unchanged. Move fully outside the comment or use DestinationBox for an unboxed node that fits inside.");
     const bool AllowedDestination=Destination && !WasInside && IsInside && (Other==Destination || ContainsRect(Rect,Bounds[Destination]));
     if(WasInside!=IsInside && !AllowedDestination) return TEXT("error: move changes membership of an unselected comment; specify its DestinationBox for unboxed nodes or select its group too");
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
      // Use the geometry measured above. Stored NodeWidth can be zero and
      // would put the output pin on the wrong side of a rendered node.
      Boundary.Add({Moving,Fixed,FTurboMCPGraphTidy::AlignmentPinPosition(Moving),FTurboMCPGraphTidy::AlignmentPinPosition(Fixed)});
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
   UEdGraphNode* BestDestination=Destination;
   int32 Evaluated=0,Feasible=0;
   TSet<FString> SeenOffsets;
   auto Consider=[&](double X,double Y)
   {
    if(!FMath::IsFinite(X) || !FMath::IsFinite(Y) || X<MIN_int32 || X>MAX_int32 || Y<MIN_int32 || Y>MAX_int32) return;
    const FIntPoint Offset(static_cast<int32>(FMath::RoundToDouble(X)),static_cast<int32>(FMath::RoundToDouble(Y)));
    const FString CandidateKey=FString::Printf(TEXT("%s:%d:%d"),Destination?*Destination->GetName():TEXT("free"),Offset.X,Offset.Y);
    if(SeenOffsets.Contains(CandidateKey)) return;
    SeenOffsets.Add(CandidateKey);++Evaluated;
    if(!MoveError(Offset.X,Offset.Y).IsEmpty()) return;
    const FVector2D D(Offset.X,Offset.Y);
    for(const FBoundaryWire& Wire:Boundary)
    {
     if(Wire.Moving->PinType.PinCategory!=TEXT("exec")) continue;
     const double Sign=Wire.Moving->Direction==EGPD_Output?1.0:-1.0;
     const double OldForward=Sign*(Wire.End.X-Wire.Start.X);
     const double NewForward=Sign*(Wire.End.X-Wire.Start.X-D.X);
     // Keep a readable forward run; do not shorten a valid execution
     // connection into a hairpin just to improve the distance score.
     if(OldForward>=0.0 && NewForward<FMath::Min(64.0,OldForward)) return;
    }
    ++Feasible;
    const double Cost=Score(D);
    if(Cost<Best-0.01 || (FMath::Abs(Cost-Best)<=0.01 && D.SizeSquared()<BestOffset.SizeSquared()))
    {Best=Cost;BestOffset=D;BestDestination=Destination;}
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
   // A connected destination comment is considered explicitly; the same
   // validation is used when applying the returned DestinationBox and offsets.
   if(AllUnboxed)
   {
    for(UEdGraphNode* Box:Graph->Nodes)
    {
     if(!Box || !Box->IsA<UEdGraphNode_Comment>()) continue;
     bool Connected=false;
     for(const FBoundaryWire& Wire:Boundary)
      if(ContainsRect(Bounds[Box],Bounds[Wire.Fixed->GetOwningNode()])) {Connected=true;break;}
     if(!Connected) continue;
     const auto B=Bounds[Box];
     const double MinDX=B.MinX+24.0-Group.MinX, MaxDX=B.MaxX-24.0-Group.MaxX;
     const double MinDY=B.MinY+48.0-Group.MinY, MaxDY=B.MaxY-24.0-Group.MaxY;
     if(MinDX>MaxDX || MinDY>MaxDY) continue;
     Destination=Box;
     auto InsideCandidate=[&](double X,double Y) {Consider(FMath::Clamp(X,MinDX,MaxDX),FMath::Clamp(Y,MinDY,MaxDY));};
     const double Xs[]={MinDX,MaxDX,FMath::Clamp(Ideal.X,MinDX,MaxDX)};
     const double Ys[]={MinDY,MaxDY,FMath::Clamp(Ideal.Y,MinDY,MaxDY)};
     for(double X:Xs) for(double Y:Ys) InsideCandidate(X,Y);
     for(UEdGraphNode* N:Graph->Nodes) if(N && !Selected.Contains(N) && N!=Box)
     {
      const auto Obstacle=Bounds[N];
      const double EdgeX[]={Obstacle.MinX-Group.MaxX-32.0,Obstacle.MaxX-Group.MinX+32.0,Ideal.X};
      const double EdgeY[]={Obstacle.MinY-Group.MaxY-32.0,Obstacle.MaxY-Group.MinY+32.0,Ideal.Y};
      for(double X:EdgeX) for(double Y:EdgeY) InsideCandidate(X,Y);
     }
    }
   }
   Destination=BestDestination;
   // Refine the best feasible region down to individual graph units.
   for(double Step=256.0;Step>=1.0;Step*=0.5)
   {
    const FVector2D Center=BestOffset;
    for(int32 X=-1;X<=1;++X) for(int32 Y=-1;Y<=1;++Y)
     Consider(Center.X+X*Step,Center.Y+Y*Step);
   }
   if(Best>=Before-0.01)
    return FString::Printf(TEXT("unchanged: no shorter collision-free placement found among %d candidates (%d feasible). Free space and fitting connected boxes considered; try a smaller selection if needed. No graph edits."),Evaluated,Feasible);
   Destination=BestDestination;
   DeltaX=static_cast<int32>(BestOffset.X);DeltaY=static_cast<int32>(BestOffset.Y);
   PlacementDetails=FString::Printf(TEXT("\nplacement proposal: DeltaX=%d DeltaY=%d; best feasible candidate searched, not a global optimum; %d candidates, %d feasible. Boundary-wire total %.1f -> %.1f estimated graph units. Apply with EditTurboMCPGraphGroupMove using the same NodeIds, offsets and DestinationBox; positions and obstacles are rechecked. Only the explicitly returned DestinationBox and its containing ancestors may gain the selected unboxed nodes."),DeltaX,DeltaY,Evaluated,Feasible,Before,Best);
   PlacementDetails+=TEXT("\nDestinationBox=")+(Destination?TurboMCPNodeId::MakeForNode(Destination):TEXT("\"\" (empty string; retain membership)"));
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
  const FString DestinationSummary=Destination?TEXT("; DestinationBox=")+TurboMCPNodeId::MakeForNode(Destination)+TEXT("; selected unboxed nodes enter this box; box size and other contents unchanged"):FString();
  const FString Summary=FString::Printf(TEXT("%d nodes/comments translated by (%d,%d); internal spacing, sizes, pins and links preserved"),Selected.Num(),DeltaX,DeltaY);
  if(!Edit) return TEXT("preview: proposed movement; ")+Summary+DestinationSummary+CrossPreview+PlacementDetails+TEXT("; no live node positions changed; graph widgets measured; compilation and save not requested");
  if(DeltaX==0 && DeltaY==0) return TEXT("unchanged: zero offset")+CrossPreview;
  FScopedTransaction Transaction(FText::FromString(TEXT("TurboMCP move graph group")));
  if(!Transaction.IsOutstanding()) return TEXT("error: no undo transaction; unchanged");
  BP->Modify(); Graph->Modify();
  for(UEdGraphNode* N:Selected) { N->Modify(); N->NodePosX+=DeltaX; N->NodePosY+=DeltaY; }
  Graph->NotifyGraphChanged(); FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
  const auto CrossAfter=FTurboMCPLayoutReport::MeasureCrossBox(Graph);
  return TEXT("changed: ")+Summary+DestinationSummary+FString::Printf(TEXT("; cross-box estimated straight-line length %.1f -> %.1f (delta %+.1f), connections %d -> %d; preview predicted %.1f"),CrossBefore.Length,CrossAfter.Length,CrossAfter.Length-CrossBefore.Length,CrossBefore.Count,CrossAfter.Count,CrossPlanned.Length)+TEXT("; undo=one; compile=not_run; save=not_requested");
 }
}
FString UTurboMCPPreviewToolset::PreviewTurboMCPGraphGroupMove(const FString& Asset,const FString& GraphName,const TArray<FString>& NodeIds,int32 DeltaX,int32 DeltaY,const FString& DestinationBox) { return MoveGroup(Asset,GraphName,NodeIds,DeltaX,DeltaY,false,false,DestinationBox); }
FString UTurboMCPEditToolset::EditTurboMCPGraphGroupMove(const FString& Asset,const FString& GraphName,const TArray<FString>& NodeIds,int32 DeltaX,int32 DeltaY,const FString& DestinationBox) { return MoveGroup(Asset,GraphName,NodeIds,DeltaX,DeltaY,true,false,DestinationBox); }

FString UTurboMCPPreviewToolset::PreviewTurboMCPGraphGroupPlacement(const FString& Asset,const FString& GraphName,const TArray<FString>& NodeIds) { return MoveGroup(Asset,GraphName,NodeIds,0,0,false,true); }
