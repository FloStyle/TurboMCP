// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "Tools/TurboMCPToolsets.h"

#include "TurboMCPToolError.h"

#include "TurboMCPGraphAppend.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "HAL/PlatformTime.h"
#include "Misc/Guid.h"

namespace
{
    struct FInsertionPreview
    {
        FString Token, Asset, Graph, Text, Continuation, Scope, Comment;
        FTurboMCPGraphAppend::FPlan Plan;
        double Expires = 0;
    };
    // Editor game-thread access only. Bounded, short-lived plans contain IDs and
    // value data, never owning references that keep project assets alive.
    TArray<FInsertionPreview> InsertionPreviews;
    bool ReadScope(const FString& Json, TArray<FString>& Ids, FString& Error)
    {
        TArray<TSharedPtr<FJsonValue>> Values;
        if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Values))
        { Error = TEXT("MovableNodeIdsJson must be a JSON array of node ID strings."); return false; }
        for (const auto& Value : Values)
        {
            FString Id;
            if (!Value.IsValid() || !Value->TryGetString(Id) || Id.TrimStartAndEnd().IsEmpty())
            { Error = TEXT("Every movable node ID must be a nonempty string."); return false; }
            Ids.Add(Id.TrimStartAndEnd());
        }
        return true;
    }
    void PrunePreviews()
    {
        const double Now = FPlatformTime::Seconds();
        InsertionPreviews.RemoveAll([&](const FInsertionPreview& Entry) { return Entry.Expires <= Now; });
    }
}

// Both entry points share planning and scratch validation. Planning can load assets
// and construct temporary nodes; toolset membership does not enforce side effects.
// Apply performs additional live checks and reports execution failures separately.
FString UTurboMCPPreviewToolset::PreviewTurboMCPGraphAppend(
	const FString& Asset, const FString& GraphName, const FString& Text, const FString& Continuation, const FString& Placement,
    const FString& MovableNodeIdsJson, const FString& ContainingCommentId)
{
	FTurboMCPGraphAppend::FPlan Plan;
	FString Error;
    if (!IsInGameThread()) return FTurboMCPToolError::AsError(TEXT("Graph append preview requires the editor game thread."));
    TArray<FString> Scope;
    if (!ReadScope(MovableNodeIdsJson, Scope, Error)) return FTurboMCPToolError::AsError(Error);
	if (!FTurboMCPGraphAppend::Plan(Asset, GraphName, Text, Plan, Error, /*bRequestPlacement=*/true, Continuation, Placement, Scope, ContainingCommentId))
	{
		return FTurboMCPToolError::AsError(Error);
	}


    FString ExecutionReport;
    if (Plan.bAcceptable() && !FTurboMCPGraphAppend::Preflight(Plan, ExecutionReport, Error))
        return FTurboMCPToolError::AsError(Error);
    FString TokenReport;
    if (Plan.bInsertion && Plan.bAcceptable())
    {
        PrunePreviews();
        if (InsertionPreviews.Num() >= 32) InsertionPreviews.RemoveAt(0);
        FInsertionPreview Entry;
        Entry.Token = FGuid::NewGuid().ToString(EGuidFormats::Digits);
        Entry.Asset = Asset; Entry.Graph = GraphName; Entry.Text = Text; Entry.Continuation = Continuation;
        Entry.Scope = MovableNodeIdsJson; Entry.Comment = ContainingCommentId; Entry.Plan = Plan;
        Entry.Expires = FPlatformTime::Seconds() + 600.0;
        TokenReport = TEXT("\npreview_token=") + Entry.Token + TEXT("; expires_in_seconds=600; apply with identical arguments and this PreviewToken.");
        InsertionPreviews.Add(MoveTemp(Entry));
    }
    return FTurboMCPGraphAppend::RenderPreview(Plan) + TokenReport + (ExecutionReport.IsEmpty() ? FString() : TEXT("\n") + ExecutionReport);
}

FString UTurboMCPEditToolset::EditTurboMCPGraphAppend(
	const FString& Asset, const FString& GraphName, const FString& Text, const FString& Continuation, const FString& Placement,
    const FString& MovableNodeIdsJson, const FString& ContainingCommentId, const FString& PreviewToken)
{
    if (!IsInGameThread()) return FTurboMCPToolError::AsError(TEXT("Graph append requires the editor game thread."));
    if (Placement.TrimStartAndEnd() == TEXT("insert"))
    {
        PrunePreviews();
        const int32 Index = InsertionPreviews.IndexOfByPredicate([&](const FInsertionPreview& Entry) { return Entry.Token == PreviewToken; });
        if (Index == INDEX_NONE) return FTurboMCPToolError::AsError(TEXT("Insertion requires a current PreviewToken from PreviewTurboMCPGraphAppend. Preview again; nothing was written."));
        const FInsertionPreview Entry = InsertionPreviews[Index];
        if (Entry.Asset != Asset || Entry.Graph != GraphName || Entry.Text != Text || Entry.Continuation != Continuation
            || Entry.Scope != MovableNodeIdsJson || Entry.Comment != ContainingCommentId)
            return FTurboMCPToolError::AsError(TEXT("Insertion arguments differ from the preview. Preview again; nothing was written."));
        InsertionPreviews.RemoveAt(Index);
        FString Report, Error;
        if (!FTurboMCPGraphAppend::Apply(Entry.Plan, Report, Error)) return FTurboMCPToolError::AsError(Error);
        return Report;
    }
    if (!PreviewToken.IsEmpty()) return FTurboMCPToolError::AsError(TEXT("PreviewToken is supported only for insert placement."));
	// Rebuild the plan against current state; a previous preview may be stale.
	FTurboMCPGraphAppend::FPlan Plan;
	FString Error;
    TArray<FString> Scope;
    if (!ReadScope(MovableNodeIdsJson, Scope, Error)) return FTurboMCPToolError::AsError(Error);
	if (!FTurboMCPGraphAppend::Plan(Asset, GraphName, Text, Plan, Error, /*bRequestPlacement=*/true, Continuation, Placement, Scope, ContainingCommentId))
	{
		return FTurboMCPToolError::AsError(Error);
	}


	if (!Plan.bAcceptable())
	{
		// A REFUSED PLAN IS RENDERED RATHER THAN RESTATED. The refusals name their lines, and the first line
		// is `unchanged`, so a caller that reads only the first line still learns nothing was written.
		return FTurboMCPGraphAppend::RenderPreview(Plan);
	}

	FString Report;
	FString ApplyError;
	if (!FTurboMCPGraphAppend::Apply(Plan, Report, ApplyError))
	{
		// Preserve the execution failure, including partial-change or rollback details.
		return FTurboMCPToolError::AsError(ApplyError);
	}

	return Report;
}
