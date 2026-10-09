// Copyright FloStyle. All Rights Reserved.

// The tool declarations live with the rest of the toolsets, so that one header
// describes the whole tool surface and UnrealHeaderTool sees them together.
#include "Tools/TurboMCPToolsets.h"

#include "Containers/UnrealString.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
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

	/**
	 * The kind of a tool.
	 *
	 * A toolset holds one kind, which is why a Read toolset cannot write. Our own
	 * toolset names embed their kind, for example `TurboMCP.TurboMCPReadToolset`,
	 * so the kind is read from the toolset name rather than guessed from a tool.
	 *
	 * The match is a substring search, not a prefix test, because the name starts
	 * with the module rather than the kind. Kind words are checked in the fixed
	 * order Read, Look, Preview, Edit so the answer does not depend on map order.
	 *
	 * For an engine toolset no kind can be proven, so "Engine" is printed instead
	 * of a guess. A confident but wrong kind is what `P1` and `D-05` forbid.
	 */
	FString KindOf(const FString& InToolsetName)
	{
		for (const TCHAR* Kind : KindNames)
		{
			if (InToolsetName.Contains(Kind, ESearchCase::IgnoreCase))
			{
				return Kind;
			}
		}
		return TEXT("Engine");
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

		int32 Stop = INDEX_NONE;
		if (Text.FindChar(TEXT('.'), Stop))
		{
			// A stop inside a short leading token is usually an abbreviation or a
			// dotted file name rather than a sentence end. The spec text is full
			// of both, and cutting at the first would print things like "See `04".
			const bool bLooksLikeAbbreviation =
				Stop < 12
				&& Text.Len() > Stop + 1
				&& !FChar::IsWhitespace(Text[Stop + 1]);

			if (!bLooksLikeAbbreviation)
			{
				return Text.Left(Stop + 1);
			}

			int32 NextStop = INDEX_NONE;
			// FString::FindChar takes no search direction, so search the tail
			// and add the offset back.
			const FString Tail = Text.Mid(Stop + 1);
			if (Tail.FindChar(TEXT('.'), NextStop))
			{
				return Text.Left(Stop + 1 + NextStop + 1);
			}
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
	FString Lines;

	for (const FToolsetEntry& Toolset : Toolsets)
	{
		Lines += FString::Printf(TEXT("\n%s (source: %s)"),
			*Toolset.Name, *SourceOf(Toolset.Name));

		for (const TSharedPtr<FJsonObject>& Tool : Toolset.Tools)
		{
			// The published name is the full Toolset.Function; print the bare name
			// because that is what call_tool accepts.
			const FString Bare = BareToolName(Field(Tool, TEXT("name")));
			const FString Description = Field(Tool, TEXT("description"));

			Lines += FString::Printf(TEXT("\n  %s %s - %s - %s"),
				*KindOf(Toolset.Name), *Bare, *SourceOf(Toolset.Name),
				*FirstSentence(Description));
			++ToolCount;
		}
	}

	// The counts print even at zero, so "nothing registered" never reads the
	// same as "the check did not run". Our own toolset count comes from the class
	// list rather than the registry, so a toolset the registry silently refused
	// shows up as a gap between the two numbers.
	Result += FString::Printf(TEXT("\n%d toolsets in total, %d of them TurboMCP, %d tools"),
		Toolsets.Num(),
		TurboMCPToolsets::GetToolsetClasses().Num(),
		ToolCount);
	Result += Lines;
	Result += TEXT("\nOrder is Read, Look, Preview, Edit. ")
		TEXT("Use ReadTurboMCPToolHelp with a toolset name and a bare tool name ")
		TEXT("for its parameters and known traps.");

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

	// A caller may pass the joined form, because that is how tools/list prints
	// it. Accept both rather than failing on the more obvious of the two.
	if (WantedTool.IsEmpty())
	{
		FString JoinedToolset;
		FString JoinedTool;
		SplitName(WantedToolset, JoinedToolset, JoinedTool);
		if (!JoinedToolset.IsEmpty())
		{
			WantedToolset = JoinedToolset;
			WantedTool = JoinedTool;
		}
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

	const FTurboMCPTraps& Traps = FTurboMCPTraps::Get();

	// Match a toolset by its full name or by its short form, because the full
	// name carries the module prefix and nobody types that from memory.
	auto ToolsetMatches = [&WantedToolset](const FString& InName)
	{
		return WantedToolset.IsEmpty()
			|| InName.Equals(WantedToolset, ESearchCase::IgnoreCase)
			|| InName.EndsWith(TEXT(".") + WantedToolset, ESearchCase::IgnoreCase);
	};

	// Summary mode: no tool name given, so one line per tool in the toolset.
	if (WantedTool.IsEmpty())
	{
		for (const FToolsetEntry& Entry : Toolsets)
		{
			if (!ToolsetMatches(Entry.Name))
			{
				continue;
			}

			FString Summary = FString::Printf(TEXT("read: %s, %d tools"),
				*Entry.Name, Entry.Tools.Num());
			for (const TSharedPtr<FJsonObject>& Tool : Entry.Tools)
			{
				const FString Bare = BareToolName(Field(Tool, TEXT("name")));
				Summary += FString::Printf(TEXT("\n%s %s - %s"),
					*KindOf(Entry.Name), *Bare,
					*FirstSentence(Field(Tool, TEXT("description"))));
			}
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
				*KindOf(Entry.Name), *SourceOf(Entry.Name));
			Help += FString::Printf(TEXT("\nwhat: %s"),
				*FirstSentence(Field(Tool, TEXT("description"))));

			// Parameters, straight out of the schema the registry published.
			const TSharedPtr<FJsonObject>* InputSchema = nullptr;
			if (Tool->TryGetObjectField(TEXT("inputSchema"), InputSchema) && InputSchema != nullptr)
			{
				const TSharedPtr<FJsonObject>* Properties = nullptr;
				if ((*InputSchema)->TryGetObjectField(TEXT("properties"), Properties)
					&& Properties != nullptr)
				{
					const int32 ParamCount = (*Properties)->Values.Num();
					Help += FString::Printf(TEXT("\nparameters: %d"), ParamCount);

					for (const TPair<FString, TSharedPtr<FJsonValue>>& Property : (*Properties)->Values)
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

						Help += PropDescription.IsEmpty()
							? FString::Printf(TEXT("\n  %s: %s"), *Property.Key, *Type)
							: FString::Printf(TEXT("\n  %s: %s - %s"),
								*Property.Key, *Type, *FirstSentence(PropDescription));
					}

					if (ParamCount == 0)
					{
						Help += TEXT(" (this tool takes no parameters)");
					}
				}
				else
				{
					Help += TEXT("\nparameters: none");
				}
			}

			// Traps come from the data file, never from code (D-07).
			const FTurboMCPTraps::FToolInfo* Info = Traps.Find(Bare);
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

			// The budget is a hard requirement in R-TOOL-SCHEMA. Say when it is
			// exceeded rather than cutting text in the middle (P6), because a
			// truncated trap is a trap an agent will not read.
			if (Help.Len() > HelpBudgetChars)
			{
				Help += FString::Printf(
					TEXT("\nwarning: %d characters, over the %d character budget. ")
					TEXT("Shorten the traps for this tool in the traps file."),
					Help.Len(), HelpBudgetChars);
			}

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
	Error += TEXT(". Use R-TOOL-INDEX for every tool name.");

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
