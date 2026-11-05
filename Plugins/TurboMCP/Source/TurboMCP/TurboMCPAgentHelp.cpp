// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

// The tool declarations live with the rest of the toolsets, so that one header
// describes the whole tool surface and UnrealHeaderTool sees them together.
#include "Tools/TurboMCPToolsets.h"

#include "Containers/UnrealString.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonSerializer.h"
#include "ToolsetRegistry/UToolsetRegistry.h"

#include "Logging.h"
#include "TurboMCPToolsetsInternal.h"
#include "TurboMCPTraps.h"

/**
 * Shared helpers for the agent help tools.
 *
 * Everything here has internal linkage (an anonymous namespace) so it cannot
 * collide with another translation unit, and so only the two AICallable
 * functions above are part of the tool surface.
 */
namespace
{
	/**
	 * The four kinds, in the order a result must print them.
	 *
	 * `03_TOOLS.md` section 1.4 fixes this order: Read, Look, Preview, Edit.
	 */
	const TCHAR* KindNames[] = { TEXT("Read"), TEXT("Look"), TEXT("Preview"), TEXT("Edit") };

	/** Character budget for one tool's help, from `03_TOOLS.md` R-TOOL-SCHEMA. */
	constexpr int32 HelpBudgetChars = 2000;

	/** Where a too-long single description is cut when it has no sentence stop. */
	constexpr int32 MaxSentenceFallbackChars = 160;

    // Kinds describe only our four declared toolsets. Never infer effects from
    // a third-party name containing words such as Read or Edit.
    FString KindOf(const FString& InToolsetName)
    {
        for (const TCHAR* Kind : KindNames)
        {
            if (InToolsetName == FString::Printf(TEXT("TurboMCP.TurboMCP%sToolset"), Kind))
                return Kind;
        }
        return TEXT("Engine");
    }

	FString KindForTool(const FString& Toolset, const FString& Bare)
	{
		if (Toolset.StartsWith(TEXT("TurboMCP.")) && Bare == TEXT("ReadTurboMCPPlayCheck"))
			return TEXT("Execute (starts PIE / runs project code)");
		if (Toolset.StartsWith(TEXT("TurboMCP.")) && Bare == TEXT("LookTurboMCPWidgetPreview"))
            return TEXT("Execute (renders widget / runs project code)");
        return KindOf(Toolset);
	}

	/**
	 * The bare tool name.
	 *
	 * The registry publishes a tool's name as the full `Toolset.Function`, as
	 * verified against the live server, but `call_tool` wants the bare function
	 * name. Printing the wrong one costs a caller a failed call, which is what
	 * `P7` exists to prevent.
	 */
	FString BareToolName(const FString& InName)
	{
		int32 LastDot = INDEX_NONE;
		if (InName.FindLastChar(TEXT('.'), LastDot) && LastDot >= 0 && LastDot < InName.Len() - 1)
		{
			return InName.Mid(LastDot + 1);
		}
		return InName;
	}

	/** Source label: `TurboMCP` for our own tools, `Unreal MCP` for the engine's. */
	FString SourceOf(const FString& InName)
	{
		return InName.StartsWith(TEXT("TurboMCP."), ESearchCase::IgnoreCase)
			? TEXT("TurboMCP")
			: TEXT("Unreal MCP");
	}

	/**
	 * The first sentence of a description.
	 *
	 * `03_TOOLS.md` section 1.4 asks for the first sentence in a list, and `P6`
	 * exists because engine help runs to tens of thousands of characters, so a
	 * list must never carry a whole description.
	 */
	FString FirstSentence(const FString& InText)
	{
		FString Text = InText;
		Text.TrimStartAndEndInline();

		if (Text.IsEmpty())
		{
			return TEXT("(no description)");
		}

        // Dots inside paths, decimals and common abbreviations are not sentence ends.
        for (int32 Stop = 0; Stop < Text.Len(); ++Stop)
        {
            if (Text[Stop] != TEXT('.') || (Stop + 1 < Text.Len() && !FChar::IsWhitespace(Text[Stop + 1]))) continue;
            const FString Prefix = Text.Left(Stop + 1);
            if (Prefix.EndsWith(TEXT("e.g.")) || Prefix.EndsWith(TEXT("i.e."))) continue;
            return Prefix;
        }

		// No usable stop: take the first line, capped so one runaway description
		// cannot flood a list.
		FString FirstLine = Text;
		int32 Newline = INDEX_NONE;
		if (FirstLine.FindChar(TEXT('\n'), Newline))
		{
			FirstLine = FirstLine.Left(Newline);
		}
		FirstLine.TrimEndInline();

		if (FirstLine.Len() > MaxSentenceFallbackChars)
		{
			FirstLine = FirstLine.Left(MaxSentenceFallbackChars).TrimEnd() + TEXT("...");
		}
		return FirstLine;
	}

	/**
	 * Splits a `Toolset.Tool` name.
	 *
	 * Splits at the LAST dot, because the toolset part itself contains a dot:
	 * `TurboMCP.TurboMCPReadToolset.ReadTurboMCPStatus` is toolset
	 * `TurboMCP.TurboMCPReadToolset` plus tool `ReadTurboMCPStatus`. Splitting at
	 * the first dot gives toolset `TurboMCP`, which no toolset answers to.
	 */
	void SplitName(const FString& InName, FString& OutToolset, FString& OutTool)
	{
		int32 LastDot = INDEX_NONE;
		if (InName.FindLastChar(TEXT('.'), LastDot) && LastDot > 0 && LastDot < InName.Len() - 1)
		{
			OutToolset = InName.Left(LastDot);
			OutTool = InName.Mid(LastDot + 1);
		}
		else
		{
			OutToolset.Reset();
			OutTool = InName;
		}
	}

	/** One registered toolset: its name, its description and its tools. */
	struct FToolsetEntry
	{
		FString Name;
		FString Description;
		TArray<TSharedPtr<FJsonObject>> Tools;
	};

	/**
	 * Reads every registered toolset out of the registry.
	 *
	 * Returns false only when the registry answered with something unreadable,
	 * because an empty list and a broken list must not look the same (`P1`).
	 */
	bool ReadAllToolsets(TArray<FToolsetEntry>& OutEntries)
	{
		const FString AllSchemas = UToolsetRegistry::GetAllToolsetJsonSchemas();

		TArray<TSharedPtr<FJsonValue>> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(AllSchemas);
		if (!FJsonSerializer::Deserialize(Reader, Root))
		{
			return false;
		}

		for (const TSharedPtr<FJsonValue>& Value : Root)
		{
			const TSharedPtr<FJsonObject>* Object = nullptr;
			if (!Value.IsValid() || !Value->TryGetObject(Object) || Object == nullptr)
			{
				continue;
			}

			FToolsetEntry Entry;
			(*Object)->TryGetStringField(TEXT("name"), Entry.Name);
			(*Object)->TryGetStringField(TEXT("description"), Entry.Description);

			const TArray<TSharedPtr<FJsonValue>>* Tools = nullptr;
			if ((*Object)->TryGetArrayField(TEXT("tools"), Tools) && Tools != nullptr)
			{
				for (const TSharedPtr<FJsonValue>& ToolValue : *Tools)
				{
					const TSharedPtr<FJsonObject>* ToolObject = nullptr;
					if (ToolValue.IsValid()
						&& ToolValue->TryGetObject(ToolObject)
						&& ToolObject != nullptr)
					{
						Entry.Tools.Add(*ToolObject);
					}
				}
			}

			Entry.Tools.Sort([](const TSharedPtr<FJsonObject>& A, const TSharedPtr<FJsonObject>& B)
			{
				FString AName, BName;
				A->TryGetStringField(TEXT("name"), AName);
				B->TryGetStringField(TEXT("name"), BName);
				return AName.Compare(BName, ESearchCase::CaseSensitive) < 0;
			});

			if (!Entry.Name.IsEmpty())
			{
				OutEntries.Add(MoveTemp(Entry));
			}
		}
		return true;
	}

	/** Reads one string field, or returns an empty string. */
	FString Field(const TSharedPtr<FJsonObject>& InObject, const TCHAR* InName)
	{
		FString Value;
		if (InObject.IsValid())
		{
			InObject->TryGetStringField(InName, Value);
		}
		return Value;
	}
}

FString UTurboMCPReadToolset::ReadTurboMCPToolIndex()
{
	// The server state leads, because whether the server runs and whether tool
	// search is on changes what a caller can even reach.
	FString Result = TurboMCPToolsets::DescribeServerState();

	if (!UToolsetRegistry::IsAvailable())
	{
		// P1: say why the list is empty instead of printing an empty list.
		Result += TEXT("\nno toolsets: the toolset registry is not available, ")
			TEXT("so no engine or TurboMCP tool could be listed.");
		return Result;
	}

	TArray<FToolsetEntry> Toolsets;
	if (!ReadAllToolsets(Toolsets))
	{
		Result += TEXT("\nno tools: the registry answered, but its schema list could not be read.");
		return Result;
	}

	int32 ToolCount = 0;
    int32 RegisteredTurboMCPCount = 0;
	FString Lines;

	for (const FToolsetEntry& Toolset : Toolsets)
	{
        if (Toolset.Name.StartsWith(TEXT("TurboMCP."))) ++RegisteredTurboMCPCount;
		Lines += FString::Printf(TEXT("\n%s (source: %s)"),
			*Toolset.Name, *SourceOf(Toolset.Name));

		for (const TSharedPtr<FJsonObject>& Tool : Toolset.Tools)
		{
			// The published name is the full Toolset.Function; print the bare name
			// because that is what call_tool accepts.
			const FString Bare = BareToolName(Field(Tool, TEXT("name")));
			const FString Description = Field(Tool, TEXT("description"));

			Lines += FString::Printf(TEXT("\n  %s %s - %s - %s"),
				*KindForTool(Toolset.Name, Bare), *Bare, *SourceOf(Toolset.Name),
				*FirstSentence(Description));
			++ToolCount;
		}
	}

    // Count registry entries, not classes that might have failed registration.
	Result += FString::Printf(TEXT("\n%d toolsets in total, %d of them TurboMCP, %d tools"),
		Toolsets.Num(),
		RegisteredTurboMCPCount,
		ToolCount);
	Result += Lines;
	Result += TEXT("\nOrder is Read, Look, Preview, Edit. ")
		TEXT("Use ReadTurboMCPToolHelp with a toolset name and a bare tool name ")
		TEXT("for its parameters and known traps. Search using ToolName=find:0:<words> (ToolsetName empty searches all registered tools).");

	return Result;
}

FString UTurboMCPReadToolset::ReadTurboMCPToolHelp(
	const FString& ToolsetName,
	const FString& ToolName)
{
	FString WantedToolset = ToolsetName;
	FString WantedTool = ToolName;
	WantedToolset.TrimStartAndEndInline();
	WantedTool.TrimStartAndEndInline();
	// A continuation uses the existing string field, preserving the public call schema.
	int32 ListOffset = 0;
	if (WantedTool.StartsWith(TEXT("list:")))
	{
		const FString OffsetText = WantedTool.Mid(5);
		if (WantedToolset.IsEmpty() || OffsetText.IsEmpty() || OffsetText.Len() > 8)
			return TEXT("error: use ToolsetName with ToolName=list:<nonnegative offset> from the previous page");
		for (TCHAR Ch : OffsetText)
			if (Ch < TEXT('0') || Ch > TEXT('9'))
				return TEXT("error: list offset must contain decimal digits only");
		ListOffset = FCString::Atoi(*OffsetText);
		WantedTool.Empty();
	}


	if (WantedToolset.IsEmpty() && WantedTool.IsEmpty())
	{
		return TEXT("error: no tool named. Pass a toolset name and a tool name, or one joined ")
			TEXT("name such as TurboMCP.TurboMCPReadToolset.ReadTurboMCPStatus.");
	}

	if (!UToolsetRegistry::IsAvailable())
	{
		return TEXT("error: the toolset registry is not available, so no tool help could be read.");
	}

	TArray<FToolsetEntry> Toolsets;
	if (!ReadAllToolsets(Toolsets))
	{
		return TEXT("error: the registry answered, but its schema list could not be read.");
	}

	TArray<FString> KnownToolsetNames;
	for (const FToolsetEntry& Entry : Toolsets)
	{
		KnownToolsetNames.Add(Entry.Name);
	}

	// Does this name already name a toolset exactly? Checked before any split,
	// because a toolset name may itself contain dots, for example
	// `editor_toolset.toolsets.asset.AssetTools`. Splitting that on the last dot
	// would truncate it to a name no toolset answers to.
	auto IsKnownToolset = [&KnownToolsetNames](const FString& InName)
	{
		for (const FString& Known : KnownToolsetNames)
		{
			if (Known.Equals(InName, ESearchCase::IgnoreCase))
			{
				return true;
			}
		}
		return false;
	};

	// Only treat the toolset argument as a joined `Toolset.Tool` when the tool
	// name was left empty AND the argument does not name a toolset as given. That
	// keeps both supported: a toolset name with dots, and one joined name.
	if (WantedTool.IsEmpty() && !WantedToolset.IsEmpty() && !IsKnownToolset(WantedToolset))
	{
		FString JoinedToolset;
		FString JoinedTool;
		SplitName(WantedToolset, JoinedToolset, JoinedTool);
		if (!JoinedToolset.IsEmpty() && IsKnownToolset(JoinedToolset))
		{
			WantedToolset = JoinedToolset;
			WantedTool = JoinedTool;
		}
	}

	const FTurboMCPTraps& Traps = FTurboMCPTraps::Get();

	// Match a toolset by its full name or by its short form, because the full
	// name carries the module prefix and nobody types that from memory.
	auto ToolsetMatches = [&WantedToolset](const FString& InName)
	{
		return WantedToolset.IsEmpty()
			|| InName.Equals(WantedToolset, ESearchCase::IgnoreCase)
			|| InName.EndsWith(TEXT(".") + WantedToolset, ESearchCase::IgnoreCase);
	};

    // Read-only discovery over the live registry; sorted pages do not imply routing preference.
    // Syntax: find:<offset>:<words>. Keep the query intact after the first separator.
    if (WantedTool.StartsWith(TEXT("find:")))
    {
        FString OffsetText, Query;
        if (!WantedTool.Mid(5).Split(TEXT(":"), &OffsetText, &Query)
            || OffsetText.IsEmpty() || OffsetText.Len() > 8 || Query.Len() > 256 || Query.TrimStartAndEnd().IsEmpty())
            return TEXT("error: use ToolName=find:0:<search words>; ToolsetName may be empty; query maximum 256 characters");
        for (TCHAR Ch : OffsetText)
            if (Ch < TEXT('0') || Ch > TEXT('9')) return TEXT("error: search offset must contain decimal digits only");
        const int32 Offset = FCString::Atoi(*OffsetText);
        TArray<FString> Words;
        Query.ParseIntoArrayWS(Words);
        TArray<FString> Matches;
        for (const FToolsetEntry& Entry : Toolsets)
        {
            if (!ToolsetMatches(Entry.Name)) continue;
            for (const TSharedPtr<FJsonObject>& Tool : Entry.Tools)
            {
                const FString Name = Entry.Name + TEXT(".") + BareToolName(Field(Tool, TEXT("name")));
                const FString Searchable = Name + TEXT(" ") + Field(Tool, TEXT("description"));
                bool bMatches = true;
                for (const FString& Word : Words)
                    if (!Searchable.Contains(Word, ESearchCase::IgnoreCase)) { bMatches = false; break; }
                if (bMatches) Matches.Add(Name);
            }
        }
        Matches.Sort();
        if (Matches.IsEmpty()) return TEXT("read: no registered tools match all search words in the requested toolset scope");
        if (Offset >= Matches.Num()) return TEXT("error: search offset outside current results; restart with find:0:<search words>");
        FString Result = FString::Printf(TEXT("read: %d matching registered tools; offset %d; alphabetical, not a recommendation"), Matches.Num(), Offset);
        int32 End = Offset;
        // Reserve the bounded query plus routing guidance; never split a tool identity.
        const int32 FooterReserve = Query.Len() + 220;
        for (; End < Matches.Num() && End < Offset + 20; ++End)
        {
            const FString Row = TEXT("\n  ") + Matches[End];
            if (Result.Len() + Row.Len() + FooterReserve > HelpBudgetChars)
            {
                if (End == Offset)
                    return TEXT("error: a tool name exceeds the search budget; inspect describe_toolset directly");
                break;
            }
            Result += Row;
        }
        if (End < Matches.Num()) Result += FString::Printf(TEXT("\nnext: keep ToolsetName; ToolName=find:%d:%s"), End, *Query);
        Result += TEXT("\nUse an exact tool name for parameters and traps. Search does not authorize edits or bypass refusals.");
        return Result;
    }

	// Summary mode: one line per tool in the toolset.
	if (WantedTool.IsEmpty())
	{
		for (const FToolsetEntry& Entry : Toolsets)
		{
			if (!ToolsetMatches(Entry.Name))
			{
				continue;
			}

			if (ListOffset > 0 && ListOffset >= Entry.Tools.Num())
				return FString::Printf(TEXT("error: list offset %d is outside this toolset's %d tools; restart with ToolName empty"), ListOffset, Entry.Tools.Num());
			FString Summary = FString::Printf(TEXT("read: %s, %d tools, offset %d"),
				*Entry.Name, Entry.Tools.Num(), ListOffset);
			int32 Next = ListOffset;
			for (; Next < Entry.Tools.Num() && Next - ListOffset < 12; ++Next)
			{
				const TSharedPtr<FJsonObject>& Tool = Entry.Tools[Next];
				const FString Bare = BareToolName(Field(Tool, TEXT("name")));
				FString Description = FirstSentence(Field(Tool, TEXT("description")));
				Description.ReplaceInline(TEXT("\r"), TEXT(" "));
				Description.ReplaceInline(TEXT("\n"), TEXT(" "));
				if (Description.Len() > 120) Description = Description.Left(117) + TEXT("...");
				const FString Row = FString::Printf(TEXT("\n%s %s - %s"), *KindForTool(Entry.Name, Bare), *Bare, *Description);
				// Reserve space for the continuation, never cut a tool name in half.
				if (Summary.Len() + Row.Len() > HelpBudgetChars - 250)
				{
					if (Next == ListOffset)
						return TEXT("error: a tool name exceeds the summary budget; inspect the registry schema directly");
					break;
				}
				Summary += Row;
			}
			if (Next < Entry.Tools.Num())
				Summary += FString::Printf(TEXT("\n%d more. Continue with the same ToolsetName and ToolName=\"list:%d\"."), Entry.Tools.Num() - Next, Next);
			else Summary += TEXT("\nEnd of toolset.");
			Summary += TEXT("\nPass a listed bare tool name for full parameters and traps.");

			return Summary;
		}

		FString Error = FString::Printf(
			TEXT("error: no toolset named '%s'. Real toolsets: %s"),
			*WantedToolset, *FString::Join(KnownToolsetNames, TEXT(", ")));
		return Error;
	}

	// One tool: full help.
	for (const FToolsetEntry& Entry : Toolsets)
	{
		if (!ToolsetMatches(Entry.Name))
		{
			continue;
		}

		for (const TSharedPtr<FJsonObject>& Tool : Entry.Tools)
		{
			const FString Bare = BareToolName(Field(Tool, TEXT("name")));
			if (!Bare.Equals(WantedTool, ESearchCase::IgnoreCase))
			{
				continue;
			}

			FString Help = FString::Printf(TEXT("read: %s.%s"), *Entry.Name, *Bare);
			Help += FString::Printf(TEXT("\nkind: %s   source: %s"),
				*KindForTool(Entry.Name, Bare), *SourceOf(Entry.Name));
            // Keep execution and destructive effects before optional detail and truncation.
            if (Entry.Name.StartsWith(TEXT("TurboMCP.")))
            {
                if (Bare == TEXT("ReadTurboMCPPlayCheck"))
                    Help += TEXT("\neffects: starts PIE and executes project code; NOT a passive read.");
                else if (Bare == TEXT("LookTurboMCPWidgetPreview"))
                    Help += TEXT("\neffects: Execution=allow runs widget/project code, including construction. The requested callback runs after construction; it does not simulate pointer/focus input. Empty/deny refuses. Effects are not sandboxed or reversible; this is NOT a passive image read.");
                else if (Bare == TEXT("EditTurboMCPSave"))
                    Help += TEXT("\neffects: writes packages even when not dirty. Widgets skip thumbnail drawing; other assets may execute thumbnail code. Partial saves are not rolled back; inspect each result. No explicit compile.");
                else if (Bare == TEXT("EditTurboMCPSaveWithoutThumbnail"))
                    Help += TEXT("\neffects: writes packages without thumbnail drawing; save callbacks still run. Partial saves are not rolled back; inspect each result. No explicit compile. Thumbnails may remain stale.");
                else if (Bare == TEXT("EditTurboMCPCompile"))
                    Help += TEXT("\neffects: compilation can reinstance classes and affect dependents. Does not save; inspect each asset's diagnostics before saving.");
                else if (Bare == TEXT("PreviewTurboMCPGraphTidy") || Bare == TEXT("EditTurboMCPGraphTidy"))
                    Help += TEXT("\nlayout: LayoutMode=rows preserves the existing default. LayoutMode=blocks requests compact control-flow placement. Whole-graph blocks mode may move and resize existing comments around their original members; empty notes stay fixed. No automatic reroutes, new boxes or local promotion.\nscope: unselected nodes stay fixed. Only the first explicit MainPath node may be an outside-selection fixed predecessor, connected to the next selected node; later path nodes must be selected. Scoped groups are placed near their connections. Force does not bypass collision, comment membership or fixed-boundary wire-growth limits (64..256 graph units of clearance per wire; aggregate allowance is the largest individual allowance). Preview does not move graph nodes. Edit rechecks the current graph and rolls back rejected geometry; no compile or save.");
                else if (Bare == TEXT("EditTurboMCPAssetUndo"))
                    Help += TEXT("\neffects: undoes only the latest matching TurboMCP transaction. Requires its preview GUID; never skips unrelated edits. No explicit compile or save. Redo remains available after success.");
                else if (Bare == TEXT("PreviewTurboMCPAssetRevert"))
                    Help += TEXT("\neffects: preview only; does not discard edits. Returns a one-use confirmation for EditTurboMCPAssetRevert, valid for 60 seconds and invalidated by memory or disk changes. Supports saved Blueprint and data-asset packages only; refuses open asset editors and detected live-world references. The edit discards ALL unsaved edits in the named package and may reset undo.");
                else if (Bare == TEXT("EditTurboMCPAssetRevert"))
                    Help += TEXT("\neffects: discards ALL unsaved edits in the named saved Blueprint or data-asset package; may reset undo. Confirm must be the current preview confirmation, expires after 60 seconds and is consumed before reload. Changes to memory or disk invalidate it. Refuses open asset editors and detected live-world references. Re-read values and references afterward. This is not a save or a normal undo.");
            }
            // Usage comes from the current tool contract; placeholders are not real asset IDs.
            if (Entry.Name.StartsWith(TEXT("TurboMCP.")))
            {
                if (Bare == TEXT("ReadTurboMCPTrace"))
                    Help += TEXT("\nusage: Action=status, NodeId empty. This reads trace state; arm/clear require EditTurboMCPTrace.");
                else if (Bare == TEXT("EditTurboMCPTrace"))
                    Help += TEXT("\nusage: Action=arm, NodeId=<actual node ID from the reader>; Action=clear, NodeId empty clears the trace registry. Action=status is also accepted. No start/stop action exists. Arming does not itself execute the graph.");
                else if (Bare == TEXT("PreviewTurboMCPGraphAppend") || Bare == TEXT("EditTurboMCPGraphAppend"))
                    Help += TEXT("\nusage: choose the existing Asset and exact GraphName. Discover creation IDs with PreviewTurboMCPNodeType and pin names with PreviewTurboMCPNodePins; engine search IDs may differ.\nLink grammar: write DestinationInput (-> SourceNode:SourceOutput). Example template:\n(<creation ID>\n    InputPin (-> <existing source ID>:OutputPin)) ; label Added\n(<existing destination type ID> ; <existing destination node ID>\n    InputPin (-> Added:OutputPin))\nReplace placeholders with exact discovered types, reader IDs and pins. The second form refers to an existing node, not a copy. Labels name preceding forms. Do not write reverse links on output pins. Occupied inputs may be rewired: inspect the preview. Enhanced Input events are supported; unreachable calls are refused. Preview first; edit replans. No compile or save. New forms are not idempotent; do not blindly retry an uncertain edit.");
                else if (Bare == TEXT("PreviewTurboMCPGraphPatch") || Bare == TEXT("EditTurboMCPGraphPatch"))
                    Help += TEXT("\nusage template: Asset=<existing Blueprint path>, GraphName=<explicit existing graph>. For an existing PrintString call, replace <full reader node ID> below with that node's actual identity:\n(Development|PrintString ; <full reader node ID>\n    InString \"Hello\")\nPreview first. Patch changes existing literal defaults; this is not an append or an execution-link rewrite. The type and pin must match the target node; placeholders are not accepted IDs.");
            }

            // Batch recovery and scope restrictions are part of safe use, not
            // optional detail. Keep them in compact help as well as the schema.
            const bool bBatch = Bare == TEXT("PreviewTurboMCPBatchEdit") || Bare == TEXT("EditTurboMCPBatchEdit");
            const FString Description = Field(Tool, TEXT("description"));
            Help += FString::Printf(TEXT("\nwhat: %s"), *(bBatch ? Description : FirstSentence(Description)));

            FString RequiredSummary = TEXT("\nrequired parameters (live schema):");
            bool bHasRequiredParameters = false;
			// Parameters, straight out of the schema the registry published.
			const TSharedPtr<FJsonObject>* InputSchema = nullptr;
			if (Tool->TryGetObjectField(TEXT("inputSchema"), InputSchema) && InputSchema != nullptr)
			{
				const TSharedPtr<FJsonObject>* Properties = nullptr;
				if ((*InputSchema)->TryGetObjectField(TEXT("properties"), Properties)
					&& Properties != nullptr)
				{
					TArray<FString> RequiredParameters;
					(*InputSchema)->TryGetStringArrayField(TEXT("required"), RequiredParameters);
					const int32 ParamCount = (*Properties)->Values.Num();
					Help += FString::Printf(TEXT("\nparameters: %d"), ParamCount);

					// JsonObject keys can be FString or FSharedString depending on engine configuration.
					TArray<TPair<FString, TSharedPtr<FJsonValue>>> OrderedProperties;
					OrderedProperties.Reserve(ParamCount);
					for (const auto& Property : (*Properties)->Values)
					{
						OrderedProperties.Emplace(FString(*Property.Key), Property.Value);
					}
					OrderedProperties.Sort([](const auto& A, const auto& B) { return A.Key < B.Key; });
					for (const TPair<FString, TSharedPtr<FJsonValue>>& Property : OrderedProperties)
					{
						FString Type = TEXT("any");
						FString PropDescription;
						const TSharedPtr<FJsonObject>* PropObject = nullptr;
						if (Property.Value.IsValid()
							&& Property.Value->TryGetObject(PropObject)
							&& PropObject != nullptr)
						{
							Type = Field(*PropObject, TEXT("type"));
							if (Type.IsEmpty())
							{
								Type = TEXT("any");
							}
							PropDescription = Field(*PropObject, TEXT("description"));
						}

                        if (RequiredParameters.Contains(Property.Key))
                        {
                            RequiredSummary += FString::Printf(TEXT("\n  %s: %s"), *Property.Key, *Type);
                            bHasRequiredParameters = true;
                        }
						const TCHAR* Requirement = RequiredParameters.Contains(Property.Key)
							? TEXT("required") : TEXT("optional");
						Help += PropDescription.IsEmpty()
							? FString::Printf(TEXT("\n  %s: %s [%s]"), *Property.Key, *Type, Requirement)
							: FString::Printf(TEXT("\n  %s: %s [%s] - %s"),
								*Property.Key, *Type, Requirement, *PropDescription.TrimStartAndEnd());
						if (PropObject && (*PropObject).IsValid())
						{
							TSharedRef<FJsonObject> Constraints = MakeShared<FJsonObject>();
							for (const TCHAR* Key : { TEXT("enum"), TEXT("const"), TEXT("default"), TEXT("minimum"), TEXT("maximum"), TEXT("pattern") })
							{
								const TSharedPtr<FJsonValue>* Value = (*PropObject)->Values.Find(Key);
								if (Value && Value->IsValid()) Constraints->SetField(Key, *Value);
							}
							if (Constraints->Values.Num() > 0)
							{
								FString ConstraintText;
								auto Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&ConstraintText);
								if (FJsonSerializer::Serialize(Constraints, Writer))
									Help += TEXT("\n    schema: ") + ConstraintText;
							}
						}

					}

					if (ParamCount == 0)
					{
						Help += TEXT(" (this tool takes no parameters)");
					}
				}
				else
				{
					Help += TEXT("\nparameters: unavailable (schema has no properties object). Use describe_toolset; do not assume this tool takes no arguments.");
				}
            }
            else
            {
                Help += TEXT("\nparameters: unavailable (schema has no inputSchema object). Use describe_toolset; do not guess arguments.");
            }

			// Traps come from the data file, never from code (D-07).
			const FTurboMCPTraps::FToolInfo* Info = Traps.Find(Bare);
			if (Info != nullptr && Info->Parameters.Num() > 0)
			{
				// Supplemental notes describe engine-specific forms; the registry
				// above remains authoritative for parameter names and requiredness.
				TArray<FString> NoteNames;
				Info->Parameters.GetKeys(NoteNames);
				NoteNames.Sort();
				Help += TEXT("\nparameter notes (supplemental; use the live schema above):");
				for (const FString& NoteName : NoteNames)
				{
					Help += FString::Printf(TEXT("\n  %s: %s"), *NoteName, *Info->Parameters.FindChecked(NoteName));
				}
			}
			if (Info != nullptr && Info->Traps.Num() > 0)
			{
				Help += FString::Printf(TEXT("\ntraps: %d"), Info->Traps.Num());
				for (const FString& Trap : Info->Traps)
				{
					Help += TEXT("\n  - ") + Trap;
				}
			}
			else if (Info != nullptr)
			{
				Help += TEXT("\ntraps: none recorded for this tool.");
			}
			else if (!Traps.GetLoadError().IsEmpty())
			{
				// P1: say the notes could not be read rather than implying that
				// none exist.
				Help += FString::Printf(TEXT("\ntraps: not available. %s"), *Traps.GetLoadError());
			}
			else
			{
				Help += FString::Printf(
					TEXT("\ntraps: none recorded for this tool. Notes were read from %s."),
					*Traps.GetLoadedFrom());
			}

            // Required names and types must survive compact-help truncation.
            // Reserve their space before shortening descriptive details. If the
            // schema itself exceeds the budget, completeness takes precedence.
            if (!bHasRequiredParameters) RequiredSummary.Empty();
            const int32 DetailBudget = FMath::Max(0, HelpBudgetChars - RequiredSummary.Len());
            if (Help.Len() > DetailBudget)
            {
                const FString Notice = TEXT("\n[Compact help shortened. Use describe_toolset with this toolset_name for full parameter descriptions and constraints.]");
                FString Prefix = Help.Left(FMath::Max(0, DetailBudget - Notice.Len()));
                int32 LastLine = INDEX_NONE;
                if (Prefix.FindLastChar(TEXT('\n'), LastLine)) Prefix.LeftInline(LastLine);
                Help = Prefix + Notice;
            }

            Help += RequiredSummary;
			return Help;
		}
	}

	// Nothing matched. P2: refuse and name what would work.
	FString Error = FString::Printf(TEXT("error: no tool named '%s'"), *WantedTool);
	if (!WantedToolset.IsEmpty())
	{
		Error += FString::Printf(TEXT(" in toolset '%s'"), *WantedToolset);
	}
	Error += TEXT(". Real toolsets: ") + FString::Join(KnownToolsetNames, TEXT(", "));
	Error += TEXT(". Use ReadTurboMCPToolIndex for registered tool names.");

	// Name close toolset matches, which is the usual mistake: the module prefix
	// is easy to drop.
	TArray<FString> Close;
	for (const FString& Known : KnownToolsetNames)
	{
		if ((!WantedToolset.IsEmpty() && Known.Contains(WantedToolset, ESearchCase::IgnoreCase))
			|| (!WantedToolset.IsEmpty() && WantedToolset.Contains(Known, ESearchCase::IgnoreCase)))
		{
			Close.Add(Known);
		}
	}
	if (Close.Num() > 0)
	{
		Error += TEXT(" Closest toolset names: ") + FString::Join(Close, TEXT(", "));
	}

	return Error;
}
