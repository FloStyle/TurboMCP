// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
#pragma once

#include "CoreMinimal.h"

class UBlueprint;
class UEdGraph;
class UEdGraphNode;
class UEdGraphPin;

/** Plan and apply a safe fan-out rewire from one output to an existing or newly spawned source. */
class FTurboMCPGraphRepoint
{
public:
 struct FReader
 {
  FString ConsumerNodeId;
  FString ConsumerPin;
  FString OldSourceNodeId;
  FString OldSourcePin;
 };
 struct FPlan
 {
  FString Asset;
  FString GraphName;
  FString SourceNodeId;
  FString SourcePin;
  FString TargetNodeId;
  FString TargetTypeId;
  FString TargetPin;
  bool bKeepSource = true;
  bool bCreateTarget = false;
  int32 NodeCountBefore = 0;
  TArray<FReader> Readers;
  TArray<FString> Refusals;
  bool bAcceptable() const { return Refusals.Num() == 0 && Readers.Num() > 0; }
 };
 static bool Plan(const FString& Asset, const FString& GraphName, const FString& SourceNodeId,
  const FString& SourcePin, const FString& TargetNodeId, const FString& TargetTypeId,
  const FString& TargetPin, const FString& KeepSource, FPlan& OutPlan, FString& OutError);
 static FString RenderPreview(const FPlan& Plan);
 static bool Apply(const FPlan& Plan, FString& OutReport, FString& OutError);
};
