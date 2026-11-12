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
#include "EdGraphSchema_K2.h"
#include "K2Node_CallFunction.h"
#include "K2Node_Event.h"
#include "K2Node_FunctionEntry.h"
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

	// KismetCompiler creates EventGraphs as transient function stubs. Require
	// the compiler's entry identity and self ubergraph call before redirecting.
	FName GetEventStubName(const UBlueprint& InBlueprint, const UEdGraph& InGraph)
	{
		if (!InGraph.HasAnyFlags(RF_Transient)
			|| !InBlueprint.EventGraphs.ContainsByPredicate([&InGraph](const TObjectPtr<UEdGraph>& Graph)
			{
				return Graph.Get() == &InGraph;
			}))
		{
			return NAME_None;
		}

		const FName UbergraphName(*(UEdGraphSchema_K2::FN_ExecuteUbergraphBase.ToString()
			+ TEXT("_") + InBlueprint.GetName()));
		const UK2Node_FunctionEntry* Entry = nullptr;
		bool bCallsUbergraph = false;
		for (const UEdGraphNode* Node : InGraph.Nodes)
		{
			if (const UK2Node_FunctionEntry* Candidate = Cast<UK2Node_FunctionEntry>(Node))
			{
				if (Entry != nullptr)
				{
					return NAME_None;
				}
				Entry = Candidate;
			}
			if (const UK2Node_CallFunction* Call = Cast<UK2Node_CallFunction>(Node))
			{
				bCallsUbergraph |= Call->FunctionReference.IsSelfContext()
					&& Call->FunctionReference.GetMemberName() == UbergraphName;
			}
		}
		return Entry != nullptr && bCallsUbergraph ? Entry->CustomGeneratedFunctionName : NAME_None;
	}

	void FindAuthoredEvents(const UBlueprint& InBlueprint, const FString& InFunctionName,
		TArray<const UK2Node_Event*>& OutEvents)
	{
		TArray<UEdGraph*> Graphs;
		for (UEdGraph* Page : InBlueprint.UbergraphPages)
		{
			if (Page != nullptr)
			{
				Graphs.Add(Page);
				Page->GetAllChildrenGraphs(Graphs);
			}
		}
		for (const UEdGraph* Graph : Graphs)
		{
			if (Graph == nullptr || Graph->HasAnyFlags(RF_Transient))
			{
				continue;
			}
			for (const UEdGraphNode* Node : Graph->Nodes)
			{
				if (const UK2Node_Event* Event = Cast<UK2Node_Event>(Node))
				{
					const FName Name = Event->GetFunctionName();
					if (!Name.IsNone() && Name.ToString().Equals(InFunctionName, ESearchCase::IgnoreCase))
					{
						OutEvents.AddUnique(Event);
					}
				}
			}
		}
	}

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
	const FString& Detail, int32 Offset, int32 MaxChars, const FString& InRevision, const FString& InEntryFilter)
{
    const FString Revision = InRevision == TEXT("first") ? FString() : InRevision;
    const FString EntryFilter = InEntryFilter == TEXT("*") ? FString() : InEntryFilter;
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

	int32 GraphMatches = 0;
	const UEdGraph* Graph = TurboMCPNodeId::FindGraphByName(Blueprint, GraphName, GraphMatches);
	if (GraphMatches > 1)
	{
		return FString::Printf(TEXT("error: %s has multiple graphs named '%s'; the graph name is ambiguous."),
			*Resolved.ObjectPath, *GraphName);
	}

	const FName StubName = Graph != nullptr ? GetEventStubName(*Blueprint, *Graph) : NAME_None;
	TArray<const UK2Node_Event*> Events;
	if (!StubName.IsNone() || Graph == nullptr)
	{
		FindAuthoredEvents(*Blueprint, Graph != nullptr ? StubName.ToString() : GraphName, Events);
	}
	if (Events.Num() > 1)
	{
		return FString::Printf(TEXT("error: '%s' matches multiple authored events in %s; read the desired event graph directly."),
			*GraphName, *Resolved.ObjectPath);
	}

	const UK2Node_Event* Event = Events.Num() == 1 ? Events[0] : nullptr;
	const UEdGraph* BodyGraph = Event != nullptr ? Event->GetGraph() : Graph;
	if (BodyGraph == nullptr)
	{
		return FString::Printf(
			TEXT("error: %s has no graph named '%s'. Its graphs are: %s"),
			*Resolved.ObjectPath, *GraphName, *ListGraphNames(Blueprint));
	}

	TArray<FString> Notes;
	const FTurboMCPGraphText Writer;
	FString Text;
	if (Event != nullptr)
	{
		Text = FString::Printf(TEXT("; %s is implemented by event %s in graph %s (%s, %d node(s) in body graph)\n"),
			*GraphName, *Event->GetFunctionName().ToString(), *BodyGraph->GetName(),
			*TurboMCPNodeId::MakeForNode(Event, true), BodyGraph->Nodes.Num());
	}
	else if (!StubName.IsNone())
	{
		Notes.Add(TEXT("; generated event stub; no matching authored event was found"));
		Text = Notes[0] + TEXT("\n");
	}
	Text += Writer.Render(BodyGraph, DetailLevel, Resolved.ObjectPath, Notes, EntryFilter, Event);
	const FString ReadName = Graph != nullptr ? Graph->GetName() : GraphName;
	const int32 NodeCount = Graph != nullptr ? Graph->Nodes.Num() : BodyGraph->Nodes.Num();

	const FString DetailLabel = Detail.IsEmpty() ? TEXT("bodies") : Detail;

	FString Result = FString::Printf(TEXT("read: %s graph %s, format read, detail %s, %d node(s) in the whole graph"),
		*Resolved.ObjectPath, *ReadName, *DetailLabel, NodeCount);

	Result += TEXT("\n") + Text;

	// Each limitation already appears in the body. Keep a count in the text
	// footer and structured notes in JSON without repeating node descriptions.
	if (!Notes.IsEmpty())
	{
		Result += FString::Printf(TEXT("\n\n; %d read limitation(s) noted above"), Notes.Num());
	}

    if (WantedFormat == TEXT("json"))
    {
        // Hash the complete snapshot and read options, never an individual page.
        const FString Snapshot=Result+TEXT("\njson\n")+EntryFilter+TEXT("\n")+FString::Join(Notes,TEXT("\n"));
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
        Root->SetStringField(TEXT("graph"),ReadName);
        if (Event != nullptr)
        {
            Root->SetStringField(TEXT("body_graph"),BodyGraph->GetName());
            Root->SetStringField(TEXT("event_node"),TurboMCPNodeId::MakeForNode(Event, true));
            Root->SetNumberField(TEXT("body_graph_node_count"),BodyGraph->Nodes.Num());
        }
        Root->SetStringField(TEXT("detail"),DetailLabel);
        Root->SetStringField(TEXT("entry_filter"),EntryFilter);
        Root->SetNumberField(TEXT("whole_graph_node_count"),NodeCount);
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
