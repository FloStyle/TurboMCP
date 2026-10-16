// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
#include "Tools/TurboMCPToolsets.h"
#include "TurboMCPGraphRepoint.h"
#include "TurboMCPToolError.h"

FString UTurboMCPPreviewToolset::PreviewTurboMCPGraphRepoint(const FString& Asset,const FString& GraphName,const FString& SourceNodeId,const FString& SourcePin,const FString& TargetNodeId,const FString& TargetTypeId,const FString& TargetPin,const FString& KeepSource){
 FTurboMCPGraphRepoint::FPlan Plan; FString Error;
 if(!FTurboMCPGraphRepoint::Plan(Asset,GraphName,SourceNodeId,SourcePin,TargetNodeId,TargetTypeId,TargetPin,KeepSource,Plan,Error)) return FTurboMCPToolError::AsError(Error);
 return FTurboMCPGraphRepoint::RenderPreview(Plan);
}
FString UTurboMCPEditToolset::EditTurboMCPGraphRepoint(const FString& Asset,const FString& GraphName,const FString& SourceNodeId,const FString& SourcePin,const FString& TargetNodeId,const FString& TargetTypeId,const FString& TargetPin,const FString& KeepSource){
 FTurboMCPGraphRepoint::FPlan Plan; FString Error;
 if(!FTurboMCPGraphRepoint::Plan(Asset,GraphName,SourceNodeId,SourcePin,TargetNodeId,TargetTypeId,TargetPin,KeepSource,Plan,Error)) return FTurboMCPToolError::AsError(Error);
 FString Report; if(!FTurboMCPGraphRepoint::Apply(Plan,Report,Error)) return FTurboMCPToolError::AsError(Error); return Report;
}
