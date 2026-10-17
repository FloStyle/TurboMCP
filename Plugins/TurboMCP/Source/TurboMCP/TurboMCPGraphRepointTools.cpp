// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// *** THE FIRST MIGRATED TOOL, AND THE FIRST PUBLIC ANSWER THAT CARRIES A RESULT LINE. ***
//
// MEASURED, and this is why the file changed: `EditTurboMCPGraphRepoint` returned the report text and nothing
// else, so a client had to read prose to learn whether the asset had changed, whether the compile had passed,
// and whether a failure had restored the original state. Release findings R06 and R07 are exactly that, and
// ARC02 settles it: "Only the outer tool boundary appends this line."
//
// Every exit from both tools now goes through `FTurboMCPResult::AppendTo`, including the refusals and the
// errors. A refusal is `outcome=refused` with a named code rather than an `error:` prefix; a failure that
// restored the original state is `failed` with `rollback=complete`; and a failure whose restoration could not
// be verified is `outcome_unknown` with `changed=null`, which is the one case a caller must never read as
// "nothing happened" (finding G05).

#include "Tools/TurboMCPToolsets.h"
#include "TurboMCPGraphRepoint.h"
#include "TurboMCPResult.h"
#include "TurboMCPToolError.h"

namespace
{
	/** Every exit from these tools carries a result line, so there is exactly one boundary shape. */
	FString WithResult(const FString& Body, const FTurboMCPResult& Result)
	{
		return Result.AppendTo(Body);
	}

	/** A request that never reached a plan: refused, nothing attempted, and that is a refusal not a failure. */
	FTurboMCPResult RefusedResult(const FString& Code)
	{
		FTurboMCPResult Result;
		Result.Outcome = ETurboMCPResultOutcome::Refused;
		Result.Code = Code;
		Result.SetChanged(false);
		Result.Rollback = ETurboMCPResultRollback::NotNeeded;
		Result.Save = ETurboMCPResultSave::NotRequested;
		Result.OperationId = FTurboMCPResult::NewOperationId();
		return Result;
	}

	/** The result a plan's own refusals produce: refused, and nothing was written. */
	FTurboMCPResult PlanRefusalResult(const FTurboMCPGraphRepoint::FPlan& Plan)
	{
		FTurboMCPResult Result = RefusedResult(TEXT("plan_refused"));
		Result.Assets.Add(Plan.Asset);
		for (const FString& Refusal : Plan.Refusals)
		{
			Result.Diagnostics.Add(Refusal);
		}
		return Result;
	}
}

FString UTurboMCPPreviewToolset::PreviewTurboMCPGraphRepoint(const FString& Asset,const FString& GraphName,const FString& SourceNodeId,const FString& SourcePin,const FString& TargetNodeId,const FString& TargetTypeId,const FString& TargetPin,const FString& KeepSource){
 FTurboMCPGraphRepoint::FPlan Plan; FString Error;
 if(!FTurboMCPGraphRepoint::Plan(Asset,GraphName,SourceNodeId,SourcePin,TargetNodeId,TargetTypeId,TargetPin,KeepSource,Plan,Error))
 {
  return WithResult(FTurboMCPToolError::AsError(Error), RefusedResult(TEXT("plan_unavailable")));
 }
 if (Plan.Refusals.Num() > 0)
 {
  return WithResult(FTurboMCPGraphRepoint::RenderPreview(Plan), PlanRefusalResult(Plan));
 }
 // A PREVIEW DOES NOT CERTIFY COMPILATION OR RUNTIME SEMANTICS (ARC06), so its compile is `not_run` and its
 // outcome is `planned`. Reporting `changed` here would be a lie about an edit that has not happened.
 FTurboMCPResult Result;
 Result.Outcome = ETurboMCPResultOutcome::Planned;
 Result.Code = TEXT("ok");
 Result.SetChanged(false);
 Result.Assets.Add(Plan.Asset);
 Result.Compile = ETurboMCPResultCompile::NotRun;
 Result.Rollback = ETurboMCPResultRollback::NotNeeded;
 Result.Effects.Add(TEXT("none"));
 Result.OperationId = FTurboMCPResult::NewOperationId();
 return WithResult(FTurboMCPGraphRepoint::RenderPreview(Plan), Result);
}
FString UTurboMCPEditToolset::EditTurboMCPGraphRepoint(const FString& Asset,const FString& GraphName,const FString& SourceNodeId,const FString& SourcePin,const FString& TargetNodeId,const FString& TargetTypeId,const FString& TargetPin,const FString& KeepSource){
 FTurboMCPGraphRepoint::FPlan Plan; FString Error;
 if(!FTurboMCPGraphRepoint::Plan(Asset,GraphName,SourceNodeId,SourcePin,TargetNodeId,TargetTypeId,TargetPin,KeepSource,Plan,Error))
 {
  return WithResult(FTurboMCPToolError::AsError(Error), RefusedResult(TEXT("plan_unavailable")));
 }

 // ARC04: the write compiles ONCE and compares the diagnostics around it. ARC05: a memory edit does not save
 // unless its public contract says so, and this one's does not.
 FTurboMCPGraphRepoint::FApplyContext Context;
 Context.bCompile = true;
 Context.bSave = false;
 FTurboMCPGraphRepoint::FApplyOutcome Outcome;
 FString Report;
 const bool bApplied = FTurboMCPGraphRepoint::ApplyTransacted(Plan, Context, Outcome, Report, Error);

 FTurboMCPResult Result;
 Result.Outcome = Outcome.Outcome;
 Result.Code = Outcome.Code;
 Result.Assets = Outcome.Assets;
 Result.Compile = Outcome.Compile;
 Result.Rollback = Outcome.Rollback;
 Result.Save = Outcome.Save;
 Result.Effects = Outcome.Effects;
 Result.OperationId = FTurboMCPResult::NewOperationId();
 for (const FString& Diagnostic : Outcome.NewDiagnostics)
 {
  Result.Diagnostics.Add(Diagnostic);
 }
 if (bApplied && Outcome.Outcome == ETurboMCPResultOutcome::Changed)
 {
  // Only a verified change sets this, so `changed=true` on the wire means the readback confirmed it.
  Result.SetChanged(true);
 }
 else if (Outcome.Outcome == ETurboMCPResultOutcome::Unchanged
  || Outcome.Outcome == ETurboMCPResultOutcome::Refused)
 {
  Result.SetChanged(false);
 }
 else if (Outcome.Rollback == ETurboMCPResultRollback::Complete)
 {
  // A failure whose restoration WAS verified: the asset is as it was, and the line says so.
  Result.SetChanged(false);
 }
 else
 {
  // *** ARC05: "If restoration fails, report `failed` with `rollback=failed` and `changed=null`. Do not report
  // unchanged." A caller told `changed: false` here would conclude the asset is safe, and it may not be. ***
  Result.SetChangedUnknown();
 }

 const FString Body = bApplied ? Report : FTurboMCPToolError::AsError(Error);
 return WithResult(Body, Result);
}
