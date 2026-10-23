// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "Tools/TurboMCPToolsets.h"

#include "Containers/UnrealString.h"
#include "Misc/SecureHash.h"
#include "EdGraph/EdGraph.h"
#include "Engine/Blueprint.h"
#include "UObject/Object.h"
#include "UObject/UObjectGlobals.h"

#include "TurboMCPAssetResolver.h"
#include "TurboMCPGraphText.h"
#include "TurboMCPNodeId.h"

/**
 * R-GRAPH-LOAD: read one graph as text.
 *
 * The graph text itself is written by `FTurboMCPGraphText`, which `R-BP-LOAD`
 * uses too. That is deliberate: `04_FORMATS.md` opens by saying most bugs in
 * earlier work came from two tools disagreeing about one of the shared formats,
 * so there is exactly one writer.
 *
 * This tool adds only what is specific to reading one graph: finding it, naming
 * the graphs that do exist when the name is wrong, and capping the output.
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

	// The detail level is validated before anything is loaded, so a typo costs
	// nothing and the error names every accepted value.
	FTurboMCPGraphText::EDetail DetailLevel;
	if (!FTurboMCPGraphText::ParseDetail(Detail, DetailLevel))
	{
		return FString::Printf(
			TEXT("error: '%s' is not a detail level. Accepted: %s."),
			*Detail, *FTurboMCPGraphText::DetailNames());
	}

	// Only `read` is implemented. `write` is the engine's own dialect
	// (`04_FORMATS.md` 2.6) and is refused by name rather than silently returning
	// the wrong format, which would be far worse than an error.
	const FString WantedFormat = Format.TrimStartAndEnd().ToLower();
	if (!WantedFormat.IsEmpty() && WantedFormat != TEXT("read"))
	{
		if (WantedFormat == TEXT("write"))
		{
            return TEXT("error: format 'write' is unsupported. Use Format=read to inspect the graph; read output is not the engine's graph-write dialect.");
		}
		return FString::Printf(
			TEXT("error: '%s' is not a format. Accepted: read."), *Format);
	}

	FTurboMCPAssetResolver::FResolved Resolved;
	const FString ResolveError = FTurboMCPAssetResolver::Resolve(Asset, Resolved);
	if (!ResolveError.IsEmpty())
	{
		return ResolveError;
	}

	UBlueprint* Blueprint = Cast<UBlueprint>(LoadObject<UObject>(nullptr, *Resolved.ObjectPath));
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

	FString Result = FString::Printf(TEXT("read: %s graph %s, format read, detail %s, %d node(s)"),
		*Resolved.ObjectPath, *Graph->GetName(), *DetailLabel, Graph->Nodes.Num());

	Result += TEXT("\n") + Text;

	// Anything that could not be printed is already an inline `; error:` line, and
	// it is repeated here so the caller cannot miss that the text is incomplete
	// (`2.1.12` makes the gap honest, this makes it visible).
	if (Notes.Num() > 0)
	{
		Result += FString::Printf(
			TEXT("\n\n; %d node(s) could not be printed in full:"), Notes.Num());
		for (const FString& Note : Notes)
		{
			Result += FString::Printf(TEXT("\n%s"), *Note);
		}
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
        const FString Header=FString::Printf(TEXT("; error: partial graph page; inspection fragment, not standalone write-back text\n; revision=%s offset=%d count=%d total=%d nextOffset=%d\n; Request the same Asset/GraphName/Format/Detail/EntryFilter with Revision and nextOffset; -1 means end. Payload follows:\n"),
            *CurrentRevision,Offset,Count,Result.Len(),Next<Result.Len()?Next:-1);
        return Header+Result.Mid(Offset,Count);
    }

	return Result;
}
