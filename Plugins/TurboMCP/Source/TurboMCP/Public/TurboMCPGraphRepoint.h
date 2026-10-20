// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
#pragma once

#include "CoreMinimal.h"
#include "TurboMCPResult.h"

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
 /** Options used by the transaction-free apply core. */
 struct FApplyContext
 {
  bool bCompile = false;
  bool bSave = false;
 };

 /**
  * What actually happened, as named values rather than prose (ARC02).
  *
  * A caller that has to read the report to decide whether the asset changed is the defect R06 records, so the
  * outcome is set here and the tool boundary renders it.
  */
 struct FApplyOutcome
 {
  ETurboMCPResultOutcome Outcome = ETurboMCPResultOutcome::Unchanged;
  FString Code = TEXT("ok");
  /** Tri-state: 0 false, 1 true, 2 not known. */
  int32 Changed = 0;
  TArray<FString> Assets;
  ETurboMCPResultCompile Compile = ETurboMCPResultCompile::NotRun;
  ETurboMCPResultRollback Rollback = ETurboMCPResultRollback::NotNeeded;
  ETurboMCPResultSave Save = ETurboMCPResultSave::NotRequested;
  TArray<FString> Effects;
  /** Compile diagnostics the edit INTRODUCED, by normalized identity and multiplicity - never by count
   *  alone (ARC04). The comparison covers the WHOLE ASSET: a new error anywhere rolls the edit back. */
  TArray<FString> NewDiagnostics;
  /** Diagnostic occurrences already present before the edit, over the whole asset. */
  int32 BaselineDiagnosticCount = 0;
  /** Diagnostic occurrences after the edit, over the whole asset. */
  int32 AfterDiagnosticCount = 0;
  /** Whether the package was dirty before the edit, so a refusal can restore that exact state. */
  bool bDirtyBefore = false;
  bool bCompileRan = false;
  bool bCompileBaselineUsed = false;
  bool bChanged = false;
  double ElapsedMs = 0.0;
 };

 static bool Plan(const FString& Asset, const FString& GraphName, const FString& SourceNodeId,
  const FString& SourcePin, const FString& TargetNodeId, const FString& TargetTypeId,
  const FString& TargetPin, const FString& KeepSource, FPlan& OutPlan, FString& OutError);
 static FString RenderPreview(const FPlan& Plan);
 /**
  * ONE transaction for the whole public edit: resolve, Modify, apply, compare, commit-or-restore. Compile/save must be separate.
  *
  * MEASURED: the previous pair of entry points gave one edit two transaction owners. `ApplyCore` remains as a
  * thin wrapper so existing callers keep compiling, and it now delegates here.
  */
 static bool ApplyTransacted(const FPlan& Plan, const FApplyContext& Context, FApplyOutcome& OutOutcome,
  FString& OutReport, FString& OutError, bool bJoinCallerTransaction = false);
 /** Read the compile diagnostics of one Blueprint, normalized to stable identity strings (ARC04). */
 static TArray<FString> ReadCompileDiagnostics(UBlueprint* Blueprint);
 /** Severity + message text for every error and warning, sorted. Duplicates are KEPT, so the comparison counts multiplicity: two identical errors before and three after is one new diagnostic (ARC04). Extends the identity-only rule to multiplicity. */
 static TArray<FString> NormalizeDiagnostics(const class FCompilerResultsLog& Log);
 /** Apply using a caller-owned transaction; never starts or rolls back a transaction. */
 static bool ApplyCore(const FPlan& Plan, const FApplyContext& Context, FString& OutReport, FString& OutError);
 /** Standalone edit transaction; compilation and saving are explicit separate operations. */
 static bool Apply(const FPlan& Plan, FString& OutReport, FString& OutError);
};
