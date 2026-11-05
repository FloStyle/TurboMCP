// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "Tools/TurboMCPToolsets.h"

#include "Containers/UnrealString.h"
#include "Misc/SecureHash.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "EdGraph/EdGraph.h"
#include "Engine/Blueprint.h"
#include "UObject/Object.h"
#include "UObject/UObjectGlobals.h"

#include "TurboMCPAssetResolver.h"
#include "TurboMCPGraphText.h"
#include "TurboMCPNodeId.h"

/**
 * Read one exact graph using the same graph writer as BlueprintLoad.
 * Validate arguments before loading, report missing targets, and paginate a
 * stable rendered snapshot. A revision identifies text, not all asset state.
 */
namespace
{
	/** How many characters of graph text are printed before the tool caps. */
	constexpr int32 MaxGraphTextChars = 24000;

	/** The graphs of a Blueprint, for a name that matched nothing. */
	FString ListGraphNames(const UBlueprint* InBlueprint)
	{
		TArray<FString> Names;
		auto Collect = [&Names](const TArray<TObjectPtr<UEdGraph>>& InGraphs)
		{
			for (const TObjectPtr<UEdGraph>& Graph : InGraphs)
			{
				if (Graph != nullptr)
				{
					Names.Add(Graph->GetName());
				}
			}
		};
		Collect(InBlueprint->UbergraphPages);
		Collect(InBlueprint->FunctionGraphs);
		Collect(InBlueprint->MacroGraphs);
		Collect(InBlueprint->EventGraphs);

		if (Names.Num() == 0)
		{
			return TEXT("(none)");
		}
		Names.Sort();
		return FString::Join(Names, TEXT(", "));
	}
}

FString UTurboMCPReadToolset::ReadTurboMCPGraphLoad(
	const FString& Asset,
	const FString& GraphName,
	const FString& Format,
	const FString& Detail, int32 Offset, int32 MaxChars, const FString& Revision, const FString& EntryFilter)
{
    if (Offset < 0 || MaxChars < 256 || MaxChars > MaxGraphTextChars) return TEXT("error: Offset must be nonnegative and MaxChars must be 256..24000");
    if (Offset > 0 && Revision.IsEmpty()) return TEXT("error: subsequent pages require the revision token from page zero");
	if (Asset.IsEmpty())
	{
		return TEXT("error: no asset given. Pass a Blueprint in any path form, for ")
			TEXT("example BP_NodeTest or /Game/Tests/BP_NodeTest.");
	}
	if (GraphName.IsEmpty())
	{
		return TEXT("error: no graph given. Pass a graph name; the tool refuses ")
			TEXT("rather than guessing which graph you meant.");
	}

	// Reject invalid detail before loading the asset.
	FTurboMCPGraphText::EDetail DetailLevel;
	if (!FTurboMCPGraphText::ParseDetail(Detail, DetailLevel))
	{
		return FString::Printf(
			TEXT("error: '%s' is not a detail level. Accepted: %s."),
			*Detail, *FTurboMCPGraphText::DetailNames());
	}

	// JSON wraps the same readable body; it is not a second graph dialect.
	const FString WantedFormat = Format.TrimStartAndEnd().ToLower();
	if (!WantedFormat.IsEmpty() && WantedFormat != TEXT("read") && WantedFormat != TEXT("json"))
	{
		if (WantedFormat == TEXT("write"))
		{
            return TEXT("error: format 'write' is unsupported. Use Format=read to inspect the graph; read output is not the engine's graph-write dialect.");
		}
		return FString::Printf(
			TEXT("error: '%s' is not a format. Accepted: read, json."), *Format);
	}

	FTurboMCPAssetResolver::FResolved Resolved;
	const FString ResolveError = FTurboMCPAssetResolver::Resolve(Asset, Resolved);
	if (!ResolveError.IsEmpty())
	{
		return ResolveError;
	}

	UObject* Loaded = LoadObject<UObject>(nullptr, *Resolved.ObjectPath);
	if (Loaded == nullptr)
	{
		return FString::Printf(TEXT("error: could not load %s. Check that the asset is available and inspect the editor log; its asset type could not be verified."), *Resolved.ObjectPath);
	}
	UBlueprint* Blueprint = Cast<UBlueprint>(Loaded);
	if (Blueprint == nullptr)
	{
		return FString::Printf(
			TEXT("error: %s is not a Blueprint, so it has no graphs to read."),
			*Resolved.ObjectPath);
	}

	const UEdGraph* Graph = TurboMCPNodeId::FindGraphByName(Blueprint, GraphName);
	if (Graph == nullptr)
	{
		// `R-GRAPH-LOAD` requires a graph name that does not exist to list the real
		// names, so the caller can correct itself in one step (`P7`).
		return FString::Printf(
			TEXT("error: %s has no graph named '%s'. Its graphs are: %s"),
			*Resolved.ObjectPath, *GraphName, *ListGraphNames(Blueprint));
	}

	TArray<FString> Notes;
	const FTurboMCPGraphText Writer;
	const FString Text = Writer.Render(Graph, DetailLevel, Resolved.ObjectPath, Notes, EntryFilter);

	const FString DetailLabel = Detail.IsEmpty() ? TEXT("bodies") : Detail;

	FString Result = FString::Printf(TEXT("read: %s graph %s, format read, detail %s, %d node(s) in the whole graph"),
		*Resolved.ObjectPath, *Graph->GetName(), *DetailLabel, Graph->Nodes.Num());

	Result += TEXT("\n") + Text;

	// Anything that could not be printed is already an inline `; error:` line, and
	// it is repeated here so the caller cannot miss that the text is incomplete
	// (`2.1.12` makes the gap honest, this makes it visible).
	if (Notes.Num() > 0)
	{
		Result += FString::Printf(
			TEXT("\n\n; %d read limitation(s); inspect these before planning edits:"), Notes.Num());
		for (const FString& Note : Notes)
		{
			Result += FString::Printf(TEXT("\n%s"), *Note);
		}
	}

    if (WantedFormat == TEXT("json"))
    {
        // Hash the complete snapshot and read options, never an individual page.
        const FString Snapshot=Result+TEXT("\njson\n")+EntryFilter;
        FSHAHash Hash;
        FSHA1::HashBuffer(*Snapshot,Snapshot.Len()*sizeof(TCHAR),Hash.Hash);
        const FString Token=Hash.ToString();
        if (!Revision.IsEmpty() && !Revision.Equals(Token,ESearchCase::IgnoreCase))
            return TEXT("error: graph read changed since the previous page; restart with Offset=0 and empty Revision");
        if (Offset>Text.Len()) return TEXT("error: Offset exceeds the complete body length; restart at zero");
        int32 Count=FMath::Min(MaxChars,Text.Len()-Offset);
        if (Count>0 && Offset+Count<Text.Len() && uint32(Text[Offset+Count-1])>=0xD800 && uint32(Text[Offset+Count-1])<=0xDBFF) --Count;
        const int32 Next=Offset+Count<Text.Len()?Offset+Count:-1;
        TSharedRef<FJsonObject> Root=MakeShared<FJsonObject>();
        Root->SetNumberField(TEXT("schema"),1);
        Root->SetStringField(TEXT("asset"),Resolved.ObjectPath);
        Root->SetStringField(TEXT("graph"),Graph->GetName());
        Root->SetStringField(TEXT("detail"),DetailLabel);
        Root->SetStringField(TEXT("entry_filter"),EntryFilter);
        Root->SetNumberField(TEXT("whole_graph_node_count"),Graph->Nodes.Num());
        Root->SetStringField(TEXT("body"),Text.Mid(Offset,Count));
        Root->SetStringField(TEXT("revision"),Token);
        Root->SetNumberField(TEXT("offset"),Offset);
        Root->SetNumberField(TEXT("count"),Count);
        Root->SetNumberField(TEXT("total"),Text.Len());
        Root->SetNumberField(TEXT("nextOffset"),Next);
        Root->SetBoolField(TEXT("partial"),Offset>0 || Next!=-1);
        Root->SetStringField(TEXT("usage"),TEXT("Inspection only. Concatenate body fields in offset order. Request the same read options with revision and nextOffset; -1 means end. Offsets count UTF-16 code units before JSON escaping. Envelope and escaping exceed MaxChars. This is not write-back input."));
        TArray<TSharedPtr<FJsonValue>> JsonNotes;
        for (const FString& Note:Notes) JsonNotes.Add(MakeShared<FJsonValueString>(Note));
        Root->SetArrayField(TEXT("body_notes"),JsonNotes);
        FString Json;
        FJsonSerializer::Serialize(Root,TJsonWriterFactory<>::Create(&Json));
        return Json;
    }

    FSHAHash Digest;
    FSHA1::HashBuffer(*Result, Result.Len()*sizeof(TCHAR), Digest.Hash);
    const FString CurrentRevision = Digest.ToString();
    if (!Revision.IsEmpty() && !Revision.Equals(CurrentRevision, ESearchCase::IgnoreCase))
        return TEXT("error: graph read changed since the previous page; restart with Offset=0 and empty Revision");
    if (Offset > Result.Len()) return TEXT("error: Offset exceeds the complete read length; restart at zero");
    if (Offset > 0 || Result.Len() > MaxChars)
    {
        int32 Count=FMath::Min(MaxChars,Result.Len()-Offset);
        // Avoid splitting a UTF-16 surrogate pair at the page boundary.
        if (Count>0 && Offset+Count<Result.Len() && uint32(Result[Offset+Count-1])>=0xD800 && uint32(Result[Offset+Count-1])<=0xDBFF) --Count;
        const int32 Next=Offset+Count;
        const FString Header=FString::Printf(TEXT("; partial: graph page; inspection fragment, not standalone write-back text\n; revision=%s offset=%d count=%d total=%d nextOffset=%d\n; Request the same Asset/GraphName/Format/Detail/EntryFilter with Revision and nextOffset; -1 means end. Payload follows:\n"),
            *CurrentRevision,Offset,Count,Result.Len(),Next<Result.Len()?Next:-1);
        return Header+Result.Mid(Offset,Count);
    }

	return Result;
}
