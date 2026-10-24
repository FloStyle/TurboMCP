// Copyright (c) 2026 FloStyle
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "TurboMCPToolSchema.h"

#include "Containers/Map.h"
#include "Containers/Set.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Math/UnrealMathUtility.h"
#include "Serialization/JsonReader.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonSerializer.h"
#include "ToolsetRegistry/UToolsetRegistry.h"

#include "TurboMCPTraps.h"

namespace
{
	/**
	 * The bare tool name from the registry's published name.
	 *
	 * THE REGISTRY PUBLISHES `Toolset.Function`, NOT THE BARE NAME, and `call_tool` wants the
	 * BARE function name. `TurboMCPAgentHelp.cpp:66-82` records this as verified against the live
	 * server, and printing the published name would cost a caller a failed call. The bare name is
	 * taken from after the LAST dot rather than the first, because the engine's own toolset names
	 * themselves contain dots - the Blueprint tools publish as
	 * `editor_toolset.toolsets.blueprint.BlueprintTools.create_node`.
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

	/** A JSON string field, or an empty string. */
	FString Field(const TSharedPtr<FJsonObject>& InObject, const TCHAR* InName)
	{
		FString Value;
		if (InObject.IsValid())
		{
			InObject->TryGetStringField(InName, Value);
		}
		return Value;
	}

	/**
	 * Reads the whole registry into toolsets.
	 *
	 * Returns false ONLY when the registry answered with something unreadable, because an
	 * empty list and a broken list must not look the same (`P1`). `TurboMCPAgentHelp.cpp`
	 * reads the same JSON; this is a second reader of one published format rather than a
	 * second definition of it, and the shape it reads is the registry's own.
	 */
	bool ReadAllToolsets(TArray<FTurboMCPToolSchema::FToolset>& OutToolset, FString& OutError)
	{
		if (!UToolsetRegistry::IsAvailable())
		{
			OutError = TEXT("the toolset registry is not available, so no schema could be read. ")
				TEXT("It is only available in an editor build.");
			return false;
		}

		const FString AllSchemas = UToolsetRegistry::GetAllToolsetJsonSchemas();

		TArray<TSharedPtr<FJsonValue>> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(AllSchemas);
		if (!FJsonSerializer::Deserialize(Reader, Root))
		{
			OutError = FString::Printf(
				TEXT("the registry answered with %d character(s) of JSON that would not parse."),
				AllSchemas.Len());
			return false;
		}

		for (const TSharedPtr<FJsonValue>& Value : Root)
		{
			const TSharedPtr<FJsonObject>* Object = nullptr;
			if (!Value.IsValid() || !Value->TryGetObject(Object) || Object == nullptr)
			{
				continue;
			}

			FTurboMCPToolSchema::FToolset Toolset;
			(*Object)->TryGetStringField(TEXT("name"), Toolset.Name);
			(*Object)->TryGetStringField(TEXT("description"), Toolset.Description);

			const TArray<TSharedPtr<FJsonValue>>* Tools = nullptr;
			if ((*Object)->TryGetArrayField(TEXT("tools"), Tools) && Tools != nullptr)
			{
				for (const TSharedPtr<FJsonValue>& ToolValue : *Tools)
				{
					const TSharedPtr<FJsonObject>* ToolObject = nullptr;
					if (!ToolValue.IsValid() || !ToolValue->TryGetObject(ToolObject)
						|| ToolObject == nullptr)
					{
						continue;
					}

					FTurboMCPToolSchema::FTool Tool;
					// The published name is `Toolset.Function`; the bare function name is what
					// `call_tool` takes and what the traps file is keyed by, so it is the name
					// this tool prints and matches on.
					const FString PublishedName = Field(*ToolObject, TEXT("name"));
					Tool.Name = BareToolName(PublishedName);
					Tool.Description = Field(*ToolObject, TEXT("description"));
					Tool.FullName = PublishedName.IsEmpty()
						? FString::Printf(TEXT("%s.%s"), *Toolset.Name, *Tool.Name)
						: PublishedName;

					// The parameters, and which of them the schema calls required. A required
					// name the properties do not mention is reported through `OddSchemaNote`
					// rather than silently dropped, because that is a schema this reader does
					// not understand and the caller should know.
					TSet<FString> Required;
					const TSharedPtr<FJsonObject>* InputSchema = nullptr;
					if (ToolObject->Get()->TryGetObjectField(TEXT("inputSchema"), InputSchema)
						&& InputSchema != nullptr)
					{
						const TArray<TSharedPtr<FJsonValue>>* RequiredArray = nullptr;
						if ((*InputSchema)->TryGetArrayField(TEXT("required"), RequiredArray)
							&& RequiredArray != nullptr)
						{
							for (const TSharedPtr<FJsonValue>& RequiredValue : *RequiredArray)
							{
								FString RequiredName;
								if (RequiredValue.IsValid()
									&& RequiredValue->TryGetString(RequiredName))
								{
									Required.Add(RequiredName);
								}
							}
						}

						const TSharedPtr<FJsonObject>* Properties = nullptr;
						if ((*InputSchema)->TryGetObjectField(TEXT("properties"), Properties)
							&& Properties != nullptr)
						{
							// ONE PASS, SORTED, WITH NO SECOND LOOKUP.
							//
							// `TMap` HAS NO `GetKeys` IN THIS ENGINE - verified, the only `GetKeys` in
							// Core are on `TLruCache`, `TSortedMap` and `TStripedMap` - so the first
							// version collected names by iterating `Values` and then fetched each value
							// with `Values[Name]`. THAT DOES NOT COMPILE either: the map's key type is
							// not `FString` itself, only convertible to it, so neither an `FString`
							// index nor `Find(Name)` matches.
							//
							// Rather than hunt for a spelling that works, the PAIRS are collected and
							// sorted together, which removes the second lookup entirely - and with it
							// the case where a key vanishes between two passes. A lookup that cannot
							// fail cannot fail silently.
							//
							// The sort is kept because a TMap's iteration order is not the caller's
							// order, and a help text whose parameter list reorders between calls is a
							// help text nobody can compare.
							TArray<TPair<FString, TSharedPtr<FJsonValue>>> Sorted;
							for (const auto& Pair : (*Properties)->Values)
							{
								Sorted.Emplace(Pair.Key, Pair.Value);
							}
							Sorted.Sort([](const TPair<FString, TSharedPtr<FJsonValue>>& A,
										   const TPair<FString, TSharedPtr<FJsonValue>>& B)
							{
								return A.Key < B.Key;
							});

							for (const auto& Pair : Sorted)
							{
								const FString& Name = Pair.Key;
								const TSharedPtr<FJsonValue>& PropertyValue = Pair.Value;

								FTurboMCPToolSchema::FParam Param;
								Param.Name = Name;
								Param.bRequired = Required.Contains(Name);

								const TSharedPtr<FJsonObject>* PropObject = nullptr;
								if (PropertyValue.IsValid()
									&& PropertyValue->TryGetObject(PropObject)
									&& PropObject != nullptr)
								{
									Param.Type = Field(*PropObject, TEXT("type"));
									// Preserve JSON type and distinguish an explicit empty string from no default.
                                    const TSharedPtr<FJsonValue>* DefaultValue = (*PropObject)->Values.Find(TEXT("default"));
                                    if (DefaultValue && DefaultValue->IsValid())
                                    {
                                        FString JsonDefault;
                                        auto Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&JsonDefault);
                                        if (FJsonSerializer::Serialize(*DefaultValue, FString(), Writer))
                                            Param.Default = MoveTemp(JsonDefault);
                                    }
									Param.Opening = FTurboMCPToolSchema::OpeningSentence(
										Field(*PropObject, TEXT("description")));
								}

								if (Param.Type.IsEmpty())
								{
									// A parameter with no `type` is legal JSON Schema - it may be
									// `anyOf`, an enum or a `$ref` - so it is named as unknown
									// rather than guessed at.
									Param.Type = TEXT("any");
								}

								Tool.Params.Add(MoveTemp(Param));
							}
						}
						else
						{
							Tool.OddSchemaNote = TEXT("the schema has no properties object");
						}
					}
					else
					{
						Tool.OddSchemaNote = TEXT("the schema has no inputSchema object");
					}

					for (const FString& RequiredName : Required)
					{
						bool bFound = false;
						for (const FTurboMCPToolSchema::FParam& Param : Tool.Params)
						{
							if (Param.Name == RequiredName)
							{
								bFound = true;
								break;
							}
						}
						if (!bFound)
						{
							Tool.OddSchemaNote += Tool.OddSchemaNote.IsEmpty() ? TEXT("") : TEXT("; ");
							Tool.OddSchemaNote += FString::Printf(
								TEXT("'%s' is required but is not in the properties"),
								*RequiredName);
						}
					}

					Toolset.Tools.Add(MoveTemp(Tool));
				}
			}

			if (!Toolset.Name.IsEmpty())
			{
				Toolset.Tools.Sort([](const FTurboMCPToolSchema::FTool& A,
					const FTurboMCPToolSchema::FTool& B) { return A.Name < B.Name; });
				OutToolset.Add(MoveTemp(Toolset));
			}
		}

		if (OutToolset.Num() == 0)
		{
			OutError = TEXT("the registry answered with no toolsets at all, which means nothing ")
				TEXT("is registered rather than that nothing exists.");
			return false;
		}
		return true;
	}

	/** How many characters two names share, longest common subsequence, for ranking. */
	int32 CommonSubsequenceLength(const FString& InA, const FString& InB)
	{
		const int32 NA = InA.Len();
		const int32 NB = InB.Len();
		if (NA == 0 || NB == 0)
		{
			return 0;
		}
		// Two rows rather than a full table: these strings are short and a full table per pair
		// would allocate on every candidate.
		TArray<int32> Previous;
		TArray<int32> Current;
		Previous.SetNumZeroed(NB + 1);
		Current.SetNumZeroed(NB + 1);

		for (int32 I = 1; I <= NA; ++I)
		{
			for (int32 J = 1; J <= NB; ++J)
			{
				if (InA[I - 1] == InB[J - 1])
				{
					Current[J] = Previous[J - 1] + 1;
				}
				else
				{
					Current[J] = FMath::Max(Previous[J], Current[J - 1]);
				}
			}
			Previous = Current;
		}
		return Previous[NB];
	}
}

FString FTurboMCPToolSchema::NormaliseForMatch(const FString& InText)
{
	// Case folded, and spaces, underscores and brackets removed, as `03_TOOLS.md` requires.
	// The category part before the LAST `|` is dropped because the spec says the category is
	// not part of what a caller is matching against: `Math|Float|sin` and `sin` are the same
	// request.
	FString Text = InText;
	Text.TrimStartAndEndInline();

	const int32 LastPipe = Text.Find(TEXT("|"), ESearchCase::CaseSensitive,
		ESearchDir::FromEnd);
	if (LastPipe != INDEX_NONE)
	{
		Text = Text.Mid(LastPipe + 1);
	}

	FString Out;
	Out.Reserve(Text.Len());
	for (const TCHAR Character : Text)
	{
		if (FChar::IsWhitespace(Character) || Character == TEXT('_')
			|| Character == TEXT('(') || Character == TEXT(')')
			|| Character == TEXT('[') || Character == TEXT(']')
			|| Character == TEXT('{') || Character == TEXT('}')
			|| Character == TEXT('<') || Character == TEXT('>')
			|| Character == TEXT('-'))
		{
			continue;
		}
		Out.AppendChar(FChar::ToLower(Character));
	}
	return Out;
}

FString FTurboMCPToolSchema::OpeningSentence(const FString& InText)
{
	FString Text = InText;
	Text.TrimStartAndEndInline();
	if (Text.IsEmpty())
	{
		return FString();
	}

	// The stop that ends a sentence is a '.' FOLLOWED BY A SPACE, because this project's spec
	// text is full of things like `03_TOOLS.md` and `P/E-COMMENT` where the stop is inside a
	// token. Cutting at the first bare stop printed "See `04" in an earlier tool.
	int32 Index = 0;
	while (Index < Text.Len())
	{
		if (Text[Index] == TEXT('\n'))
		{
			break;
		}
		if (Text[Index] == TEXT('.'))
		{
			const bool bEndOfText = Index + 1 >= Text.Len();
			if (bEndOfText || FChar::IsWhitespace(Text[Index + 1]))
			{
				return Text.Left(Index + 1).TrimStartAndEnd();
			}
		}
		++Index;
	}

	// No sentence stop: the first line, capped, because one runaway description must not
	// crowd out the signature and the parameters.
	FString FirstLine = Text.Left(Index);
	FirstLine.TrimStartAndEndInline();
	if (FirstLine.Len() > 160)
	{
		FirstLine = FirstLine.Left(160).TrimEnd() + TEXT("...");
	}
	return FirstLine;
}

TArray<FString> FTurboMCPToolSchema::AllToolsetNames()
{
	TArray<FToolset> Toolsets;
	FString Error;
	TArray<FString> Names;
	if (ReadAllToolsets(Toolsets, Error))
	{
		for (const FToolset& Toolset : Toolsets)
		{
			Names.Add(Toolset.Name);
		}
		Names.Sort();
	}
	return Names;
}

TArray<FString> FTurboMCPToolSchema::ClosestNames(const FString& InGuess,
	const TArray<FString>& InCandidates, int32 InLimit)
{
	const FString Guess = NormaliseForMatch(InGuess);

	struct FScored
	{
		FString Name;
		int32 Score = 0;
		int32 Rank = 0;
	};
	TArray<FScored> Scored;

	for (const FString& Candidate : InCandidates)
	{
		const FString Normalised = NormaliseForMatch(Candidate);
		if (Normalised.IsEmpty())
		{
			continue;
		}

		FScored Entry;
		Entry.Name = Candidate;
		Entry.Score = Guess.IsEmpty() ? 0 : CommonSubsequenceLength(Guess, Normalised);

		// RANK before score, so a prefix match beats a longer subsequence somewhere in the
		// middle. "Print" should offer PrintString before CompoundPrint, whatever the lengths.
		if (!Guess.IsEmpty() && Normalised.StartsWith(Guess))
		{
			Entry.Rank = 0;
		}
		else if (!Guess.IsEmpty() && Normalised.Contains(Guess))
		{
			Entry.Rank = 1;
		}
		else
		{
			Entry.Rank = 2;
		}
		Scored.Add(MoveTemp(Entry));
	}

	// A rank, then the longest shared run, then the shorter name, so the order is stable and
	// a caller who runs the same failure twice sees the same list.
	Scored.Sort([](const FScored& A, const FScored& B)
	{
		if (A.Rank != B.Rank) { return A.Rank < B.Rank; }
		if (A.Score != B.Score) { return A.Score > B.Score; }
		if (A.Name.Len() != B.Name.Len()) { return A.Name.Len() < B.Name.Len(); }
		return A.Name < B.Name;
	});

	TArray<FString> Out;
	const int32 Limit = InLimit > 0 ? InLimit : 10;
	for (const FScored& Entry : Scored)
	{
		if (Out.Num() >= Limit)
		{
			break;
		}
		Out.Add(Entry.Name);
	}
	return Out;
}

bool FTurboMCPToolSchema::Plan(const FString& InToolsetName, const FString& InToolName,
	FPlan& OutPlan, FString& OutError)
{
	OutError.Empty();
	OutPlan.RequestedToolset = InToolsetName.TrimStartAndEnd();
	const FString WantedTool = InToolName.TrimStartAndEnd();

	TArray<FToolset> Toolsets;
	if (!ReadAllToolsets(Toolsets, OutError))
	{
		return false;
	}

	for (const FToolset& Toolset : Toolsets)
	{
		OutPlan.AllToolsetNames.Add(Toolset.Name);
	}
	OutPlan.AllToolsetNames.Sort();

	if (OutPlan.RequestedToolset.IsEmpty())
	{
		OutError = FString::Printf(
			TEXT("no toolset given. Pass one of: %s"),
			*FString::Join(OutPlan.AllToolsetNames, TEXT(", ")));
		return false;
	}

	// THE TOOLSET IS MATCHED ON BOTH ITS FULL AND ITS SHORT FORM. The registry reports a
	// name like `TurboMCP.TurboMCPReadToolset`, and a caller who writes `TurboMCPReadToolset`
	// or `read` means the same thing. Matching is done on the normalised form so case, spaces
	// and underscores do not matter, and the first exact normalised match wins.
	const FString Guess = NormaliseForMatch(OutPlan.RequestedToolset);

	const FToolset* Found = nullptr;
	for (const FToolset& Toolset : Toolsets)
	{
		if (NormaliseForMatch(Toolset.Name) == Guess)
		{
			Found = &Toolset;
			break;
		}
	}

	if (Found == nullptr)
	{
		// The SHORT form: the last dot-separated part, so `TurboMCPReadToolset` finds
		// `TurboMCP.TurboMCPReadToolset`.
		for (const FToolset& Toolset : Toolsets)
		{
			FString Tail = Toolset.Name;
			int32 Dot = INDEX_NONE;
			if (Toolset.Name.FindLastChar(TEXT('.'), Dot))
			{
				Tail = Toolset.Name.Mid(Dot + 1);
			}
			if (NormaliseForMatch(Tail) == Guess)
			{
				Found = &Toolset;
				OutPlan.bUsedShortForm = true;
				break;
			}
		}
	}

	if (Found == nullptr)
	{
		// A SUFFIX MATCH LAST, so `read` finds `TurboMCPReadToolset`. This is the loosest rule
		// and is tried only after the two exact ones, so it cannot shadow them.
		//
		// THE TRAILING `Toolset` IS STRIPPED FIRST, AND WITHOUT THAT THIS RULE NEVER FIRES.
		// MEASURED: a call with toolset `Read` answered "'Read' is not a toolset the registry
		// serves" and listed every real name, because the normalised full name is
		// `turbomcpturbomcpreadtoolset` and THAT does not end with `read` - it ends with
		// `readtoolset`. Stripping the class suffix leaves `turbomcpread`, which does. The rule
		// looked obviously right and matched nothing, which is the failure this project keeps
		// recording.
		for (const FToolset& Toolset : Toolsets)
		{
			FString Stem = NormaliseForMatch(Toolset.Name);
			Stem.RemoveFromEnd(TEXT("toolset"));
			if (Stem.EndsWith(Guess))
			{
				Found = &Toolset;
				OutPlan.bUsedShortForm = true;
				break;
			}
		}
	}

	if (Found == nullptr)
	{
		const TArray<FString> Closest = ClosestNames(
			OutPlan.RequestedToolset, OutPlan.AllToolsetNames, 10);
		OutError = FString::Printf(
			TEXT("'%s' is not a toolset the registry serves. Closest names: %s. ")
			TEXT("ReadTurboMCPToolIndex lists registered toolsets."),
			*OutPlan.RequestedToolset,
			Closest.Num() > 0 ? *FString::Join(Closest, TEXT(", ")) : TEXT("(none)"));
		return false;
	}

	OutPlan.ResolvedToolset = Found->Name;
	OutPlan.SchemaToolsetName = Found->Name;
	OutPlan.Tools = Found->Tools;

	if (WantedTool.IsEmpty())
	{
		return true;
	}

	// The tool, matched the same way. `call_tool` takes the BARE name, which is what the schema
	// publishes after the last dot, so the bare name is what is matched. The PUBLISHED FULL NAME
	// is accepted too, because a caller who copied it out of `describe_toolset` wrote something
	// that names the tool exactly and should not be told it does not exist.
	const FString ToolGuess = NormaliseForMatch(WantedTool);
	for (const FTool& Tool : Found->Tools)
	{
		if (NormaliseForMatch(Tool.Name) == ToolGuess
			|| NormaliseForMatch(Tool.FullName) == ToolGuess)
		{
			OutPlan.ResolvedTool = Tool.Name;
			OutPlan.Single = Tool;
			break;
		}
	}

	if (OutPlan.ResolvedTool.IsEmpty())
	{
		TArray<FString> ToolNames;
		for (const FTool& Tool : Found->Tools)
		{
			ToolNames.Add(Tool.Name);
		}
		const TArray<FString> Closest = ClosestNames(WantedTool, ToolNames, 10);
		OutError = FString::Printf(
			TEXT("'%s' is not a tool of %s. Closest: %s. Call R-TOOL-SCHEMA with just the ")
			TEXT("toolset to list all %d."),
			*WantedTool, *Found->Name,
			Closest.Num() > 0 ? *FString::Join(Closest, TEXT(", ")) : TEXT("(none)"),
			Found->Tools.Num());
		return false;
	}

	// TRAPS, from the data file rather than from code (`D-07`). The name tried is the bare
	// tool name first, because that is what the traps file keys on, then the full name.
	const FTurboMCPTraps& Traps = FTurboMCPTraps::Get();
	const FTurboMCPTraps::FToolInfo* Info = Traps.Find(OutPlan.ResolvedTool);
	if (Info == nullptr)
	{
		Info = Traps.Find(OutPlan.Single.FullName);
	}

	if (Info != nullptr)
	{
		OutPlan.Traps = Info->Traps;
		for (const TPair<FString, FString>& Pair : Info->Parameters)
		{
			OutPlan.TrapParameterNotes.Add(FString::Printf(
				TEXT("a guess of '%s' means '%s'"), *Pair.Key, *Pair.Value));
		}
		if (OutPlan.TrapParameterNotes.Num() == 0)
		{
			OutPlan.TrapParameterNotes.Add(
				TEXT("no wrong parameter name is recorded for this tool"));
		}
	}
	else if (!Traps.GetLoadError().IsEmpty())
	{
		// `P1`: say the notes COULD NOT BE READ rather than implying that none exist.
		OutPlan.TrapsNote = FString::Printf(
			TEXT("trap notes are NOT AVAILABLE, so the absence of a trap here means nothing. %s"),
			*Traps.GetLoadError());
	}
	else
	{
		OutPlan.TrapsNote = TEXT(
			"no trap is recorded for this tool. That is not a promise that it has none.");
	}

	return true;
}

FString FTurboMCPToolSchema::Render(const FPlan& InPlan)
{
	FString Out;

	// ---------------------------------------------------------------------------------
	// A whole toolset: one line per tool.
	// ---------------------------------------------------------------------------------
	if (InPlan.ResolvedTool.IsEmpty())
	{
		Out += FString::Printf(TEXT("read: tool schema for %s"), *InPlan.ResolvedToolset);
		Out += FString::Printf(TEXT("\n%d tool(s)"), InPlan.Tools.Num());
		if (InPlan.bUsedShortForm)
		{
			// Named, so a caller who wrote the short form learns the name the registry uses.
			Out += FString::Printf(TEXT("\nfull name: %s"), *InPlan.ResolvedToolset);
		}

		for (const FTool& Tool : InPlan.Tools)
		{
			Out += FString::Printf(TEXT("\n%-38s %s"), *Tool.Name,
				*OpeningSentence(Tool.Description));
		}

		if (InPlan.Tools.Num() == 0)
		{
			// `P1`: an empty list says WHY it is empty. A toolset with no tools is a
			// registration problem, not a query that found nothing.
			Out += TEXT("\nthis toolset serves NO tools, which is a registration problem ")
				TEXT("rather than a query that matched nothing.");
		}

		Out += FString::Printf(
			TEXT("\n; pass tool= to get one tool's signature and its trap notes. ")
			TEXT("R-TOOL-SCHEMA is capped: a single tool's help stays under %d characters."),
			MaxToolChars);
		return Out;
	}

	// ---------------------------------------------------------------------------------
	// One tool: signature, parameters, traps. Kept under the cap.
	// ---------------------------------------------------------------------------------
	const FTool& Tool = InPlan.Single;

	// The signature, built from the parameter names in schema order so it reads the way a
	// caller writes the call. Required parameters are not marked here because the table below
	// marks them, and two markings for one fact is how a reader learns to trust neither.
	FString Signature = FString::Printf(TEXT("%s("), *Tool.Name);
	for (int32 Index = 0; Index < Tool.Params.Num(); ++Index)
	{
		if (Index > 0)
		{
			Signature += TEXT(", ");
		}
		Signature += Tool.Params[Index].Name;
	}
	Signature += TEXT(")");

	Out += FString::Printf(TEXT("read: tool schema for %s"), *Tool.FullName);
	Out += FString::Printf(TEXT("\nsignature: %s"), *Signature);
	if (InPlan.bUsedShortForm)
	{
		Out += FString::Printf(TEXT("\ntoolset full name: %s"), *InPlan.ResolvedToolset);
	}
	Out += FString::Printf(TEXT("\nwhat: %s"), *OpeningSentence(Tool.Description));

	if (!Tool.OddSchemaNote.IsEmpty())
	{
		// Named rather than hidden: a schema this reader could not fully understand may have
		// parameters it is not showing, and a caller must know that rather than infer it.
		Out += FString::Printf(TEXT("\n; the schema is unusual: %s"), *Tool.OddSchemaNote);
	}

	Out += FString::Printf(TEXT("\nparameters: %d"), Tool.Params.Num());

	// The cap. The signature, the description and the trap notes are the parts a caller acts
	// on, so the PARAMETER TABLE is what gets shortened, and it says so.
	int32 DroppedParams = 0;
	for (int32 Index = 0; Index < Tool.Params.Num(); ++Index)
	{
		const FParam& Param = Tool.Params[Index];

		FString Row = FString::Printf(TEXT("\n  %s : %s%s"),
			*Param.Name, *Param.Type, Param.bRequired ? TEXT("  REQUIRED") : TEXT(""));
		if (!Param.Default.IsEmpty())
		{
			Row += FString::Printf(TEXT("  default=%s"), *Param.Default);
		}
		if (!Param.Opening.IsEmpty())
		{
			Row += FString::Printf(TEXT("  - %s"), *Param.Opening);
		}

		// A reserve for the traps and the closing note, so the cap cannot be blown by a long
		// parameter list before the traps are even appended.
		constexpr int32 TrapReserve = 900;
		if (Out.Len() + Row.Len() + TrapReserve > MaxToolChars)
		{
			DroppedParams = Tool.Params.Num() - Index;
			break;
		}
		Out += Row;
	}

	if (Tool.Params.Num() == 0)
	{
		Out += TEXT("\n  (this tool takes no parameters)");
	}

	if (DroppedParams > 0)
	{
		Out += FString::Printf(
			TEXT("\n  ... %d more parameter(s) did not fit the %d character budget. ")
			TEXT("Call describe_toolset on %s for the full schema."),
			DroppedParams, MaxToolChars, *InPlan.ResolvedToolset);
	}

	// The traps, from the data file.
	if (InPlan.Traps.Num() > 0)
	{
		Out += FString::Printf(TEXT("\ntraps: %d"), InPlan.Traps.Num());
		for (const FString& Trap : InPlan.Traps)
		{
			const FString Line = TEXT("\n  - ") + Trap;
			constexpr int32 TrapFloor = 60;
			if (Out.Len() + Line.Len() + TrapFloor > MaxToolChars)
			{
				Out += FString::Printf(
					TEXT("\n  ... more traps did not fit the %d character budget."),
					MaxToolChars);
				break;
			}
			Out += Line;
		}

		if (InPlan.TrapParameterNotes.Num() > 0)
		{
			Out += TEXT("\nparameter names:");
			for (const FString& Note : InPlan.TrapParameterNotes)
			{
				Out += TEXT("\n  - ") + Note;
			}
		}
	}
	else
	{
		Out += FString::Printf(TEXT("\ntraps: %s"),
			InPlan.TrapsNote.IsEmpty()
				? TEXT("none recorded for this tool.")
				: *InPlan.TrapsNote);
	}

	// The final guard. If the natural rendering still overshoots - one enormous trap
	// sentence, say - the text is cut and the cut is ANNOUNCED, because a silently truncated
	// help text teaches a caller that the tool has no more to say.
	if (Out.Len() > MaxToolChars)
	{
		const FString Suffix = FString::Printf(
			TEXT("\n[cut at %d characters; the rest is available from describe_toolset]"),
			MaxToolChars);
		Out = Out.Left(FMath::Max(0, MaxToolChars - Suffix.Len())) + Suffix;
	}

	return Out;
}
